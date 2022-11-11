//--------------------------------------------------------------
// Free Hand Tool
// 自由曲線ツール
//
// マウスドラッグで離散的な点をトレースしていき、
// マウスリリースでベジエ曲線に近似してシェイプを作る。
// 設定ダイアログで近似の精度などを調整できる。
//--------------------------------------------------------------

#include "freehandtool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwtoolhandle.h"
#include "iwproject.h"
#include "colorsettings.h"
#include "viewsettings.h"
#include "iwundomanager.h"

#include "iwshapepairselection.h"
#include "reshapetool.h"
#include "sceneviewer.h"
#include "pentool.h"

#include "iwlayer.h"
#include "iwlayerhandle.h"

#include <QMouseEvent>
#include <QPair>
#include <math.h>
#include <QMessageBox>

/*
Some part of this source is based on the Graphics Gems code by Eric Haines
https://github.com/erich666/GraphicsGems/blob/master/gems/FitCurves.c

An Algorithm for Automatically Fitting Digitized Curves
by Philip J. Schneider
from "Graphics Gems", Academic Press, 1990
*/

//-------------
// ベクトル演算関係

namespace {

/* returns squared length of input vector */
double V2SquaredLength(QPointF* a) {
  return ((a->x() * a->x()) + (a->y() * a->y()));
}

/* returns length of input vector */
double V2Length(QPointF* a) { return (sqrt(V2SquaredLength(a))); }

/* negates the input vector and returns it */
QPointF* V2Negate(QPointF* v) {
  v->setX(-v->x());
  v->setY(-v->y());
  return v;
}

/* scales the input vector to the new length and returns it */
QPointF* V2Scale(QPointF* v, double newlen) {
  double len = V2Length(v);
  if (len != 0.0) {
    v->setX(v->x() * newlen / len);
    v->setY(v->y() * newlen / len);
  }
  return v;
}

/* return vector sum c = a+b */
QPointF* V2Add(QPointF* a, QPointF* b, QPointF* c) {
  c->setX(a->x() + b->x());
  c->setY(a->y() + b->y());
  return c;
}

/* return the dot product of vectors a and b */
double V2Dot(QPointF* a, QPointF* b) {
  return (a->x() * b->x()) + (a->y() * b->y());
}

/* normalizes the input vector and returns it */
QPointF* V2Normalize(QPointF* v) {
  double len = V2Length(v);
  if (len != 0.0) {
    v->setX(v->x() / len);
    v->setY(v->y() / len);
  }
  return v;
}

/* return the distance between two points */
double V2DistanceBetween2Points(QPointF* a, QPointF* b) {
  double dx = a->x() - b->x();
  double dy = a->y() - b->y();
  return sqrt((dx * dx) + (dy * dy));
}

//-------------------
// 指定degree、指定ベジエ位置での座標を返す
// Bezier :
//	Evaluate a Bezier curve at a particular parameter value
//-------------------
QPointF Bezier(int degree,         // The degree of the bezier curve
               QList<QPointF>& V,  // Array of control points
               double t)           // Parametric value to find point for
{
  QList<QPointF> Vtemp;  // Local copy of control points
  for (int i = 0; i <= degree; i++) {
    Vtemp.push_back(V[i]);
  }

  // Triangle computation
  for (int i = 1; i <= degree; i++) {
    for (int j = 0; j <= degree - i; j++) {
      Vtemp[j].setX((1.0 - t) * Vtemp[j].x() + t * Vtemp[j + 1].x());
      Vtemp[j].setY((1.0 - t) * Vtemp[j].y() + t * Vtemp[j + 1].y());
    }
  }

  return Vtemp[0];
}

//-------------------
//  B0, B1, B2, B3 : ベジエの各項
//	Bezier multipliers
//-------------------
double B0(double u) {
  double tmp = 1.0 - u;
  return tmp * tmp * tmp;
}

double B1(double u) {
  double tmp = 1.0 - u;
  return 3 * u * tmp * tmp;
}

double B2(double u) {
  double tmp = 1.0 - u;
  return 3 * u * u * tmp;
}

double B3(double u) { return u * u * u; }

};  // namespace

//-------------

FreeHandTool::FreeHandTool()
    : IwTool("T_Freehand")
    , m_isClosed(false)
    , m_isDragging(false)
    , m_error(0.010001) {}

FreeHandTool::~FreeHandTool() {}

void FreeHandTool::onDeactivate() {
  // シェイプがある場合は、それをプロジェクトに登録する
  finishShape();

  // 既に線を描いている場合は、新たにする
  if (!m_points.isEmpty()) m_points.clear();
}

bool FreeHandTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return false;
  if (layer->isLocked()) {
    QMessageBox::critical(m_viewer, tr("Critical"),
                          tr("The current layer is locked."));
    return false;
  }

  // シェイプがある場合は、それをプロジェクトに登録する
  finishShape();

  // 既に線を描いている場合は、新たにする
  if (!m_points.isEmpty()) m_points.clear();

  m_points.push_back(pos);

  m_lastMousePos = e->localPos();

  m_isDragging = true;

  return true;
}

bool FreeHandTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  // ドラッグ中で無ければreturn
  if (!m_isDragging) return false;

  QPointF newMousePos = e->localPos();

  QPointF mouseMoveVec = newMousePos - m_lastMousePos;

  // 新しいマウス位置のマンハッタン距離が３ピクセル以上離れていたら、
  // 新たなトレース位置として登録する
  if (mouseMoveVec.manhattanLength() >= 3.0) {
    m_points.push_back(pos);
    m_lastMousePos = newMousePos;
    return true;
  }

  return false;
}

bool FreeHandTool::leftButtonUp(const QPointF&, const QMouseEvent*) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return false;

  // 線が完成していたらベジエを作る(ポイントが２つ以上)
  if (m_points.count() >= 2) {
    createShape();
    m_isDragging = false;
    return true;
  }

  m_isDragging = false;
  return false;
}

//--------------------------------------------------------
bool FreeHandTool::rightButtonDown(const QPointF&, const QMouseEvent*,
                                   bool& canOpenMenu, QMenu& menu) {
  // シェイプの完成
  canOpenMenu = finishShape();

  if (m_points.isEmpty()) return false;

  m_points.clear();
  return true;
}

void FreeHandTool::draw() {
  // トレースした点が無ければreturn
  if (m_points.isEmpty()) return;

  if (!m_viewer) return;
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  //--- トレースした点を描画
  // 色の取得

  // シェイプの現在の状況に合わせて色を得る
  double tracedLineColor[3];
  ColorSettings::instance()->getColor(tracedLineColor,
                                      Color_FreeHandToolTracedLine);
  glColor3d(tracedLineColor[0], tracedLineColor[1], tracedLineColor[2]);

  glBegin((m_isClosed) ? GL_LINE_LOOP : GL_LINE_STRIP);
  for (int p = 0; p < m_points.size(); p++) {
    glVertex3d(m_points.at(p).x(), m_points.at(p).y(), 0.0);
  }
  glEnd();

  if (!m_currentShape.shapePairP) return;

  // 描き終わっていたらベジエも表示

  // 現在のフレーム
  int frame = project->getViewFrame();

  // シェイプの現在の状況に合わせて色を得る
  glColor3d(1.0, 1.0, 1.0);

  GLdouble* vertexArray = m_currentShape.shapePairP->getVertexArray(
      frame, m_currentShape.fromTo, project);

  // VertexArrayの有効化
  glEnableClientState(GL_VERTEX_ARRAY);
  // 1頂点は3つで構成、double型、オフセット０、データ元
  glVertexPointer(3, GL_DOUBLE, 0, vertexArray);
  // シェイプは開いている
  glDrawArrays(GL_LINE_STRIP, 0,
               m_currentShape.shapePairP->getVertexAmount(project));

  // VertexArrayの無効化
  glDisableClientState(GL_VERTEX_ARRAY);

  // データを解放
  delete[] vertexArray;

  // コントロールポイントを描画

  // コントロールポイントの色を得ておく
  double cpColor[3], cpSelected[3];
  ColorSettings::instance()->getColor(cpColor, Color_CtrlPoint);
  ColorSettings::instance()->getColor(cpSelected, Color_ActiveCtrl);
  // それぞれのポイントについて、選択されていたらハンドル付きで描画
  // 選択されていなければ普通に四角を描く
  BezierPointList bPList = m_currentShape.shapePairP->getBezierPointList(
      frame, m_currentShape.fromTo);

  for (int p = 0; p < bPList.size(); p++) {
    // 単にコントロールポイントを描画する
    glColor3d(cpColor[0], cpColor[1], cpColor[2]);
    ReshapeTool::drawControlPoint(m_currentShape, bPList, p, false,
                                  m_viewer->getOnePixelLength());
  }
}

//(ダイアログから呼ばれる) 精度が変わったらシェイプを変更する
void FreeHandTool::onPrecisionChanged(int prec) {
  // 精度値から許される誤差値を計算
  m_error = std::pow((float)(100 - prec) / 100.f, 7.f) * 0.01f + 0.000001f;

  // 現在のツールがカレント、かつシェイプがあれば更新する
  if (IwApp::instance()->getCurrentTool()->getTool() == this &&
      m_currentShape.shapePairP) {
    createShape();

    m_viewer->update();
  }
}

// 現在のパラメータからシェイプを作る
void FreeHandTool::createShape() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  if (m_points.count() < 2) return;

  if (m_currentShape.shapePairP) delete m_currentShape.shapePairP;

  BezierPointList bpList = fitCurve(m_points, m_error, m_isClosed);

  // 現在のフレーム
  int frame = project->getViewFrame();

  int bpCount = bpList.count();

  CorrPointList cpList;
  for (int cp = 0; cp < 4; cp++) {
    cpList.push_back((double)((bpCount - 1) * cp) / ((m_isClosed) ? 4.0 : 3.0));
  }

  m_currentShape =
      OneShape(new ShapePair(frame, m_isClosed, bpList, cpList, 0.01), 0);

  // ポイント数をエミット
  emit shapeChanged(bpCount);
}

// シェイプがある場合は、それをプロジェクトに登録する
//  もしシェイプが完成したら、メニューを出さないためにfalseを返す
bool FreeHandTool::finishShape() {
  if (!m_currentShape.shapePairP)
    // if(!m_currentShape)
    return true;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new CreateFreeHandShapeUndo(m_currentShape.shapePairP, project, layer));

  // シェイプの実体はUndoに渡したので、ポインタには０を代入
  m_currentShape = 0;

  // Close情報を初期化
  m_isClosed = false;

  // ダイアログ更新
  emit shapeChanged(0);

  return false;
}

void FreeHandTool::toggleCloseShape(bool close) {
  if (!m_currentShape.shapePairP) return;

  m_isClosed = close;

  createShape();
  m_viewer->update();
}

void FreeHandTool::onDeleteCurrentShape() {
  if (!m_currentShape.shapePairP) return;

  delete m_currentShape.shapePairP;
  // delete m_currentShape;
  m_currentShape = 0;

  // Close情報を初期化
  m_isClosed = false;

  m_points.clear();

  m_viewer->update();

  // ダイアログ更新
  emit shapeChanged(0);
}

//------- 描線からベジエカーブを近似する部分

BezierPointList FreeHandTool::fitCurve(
    QList<QPointF>& points,  // マウス位置の軌跡
    double error,  // （ユーザが指定する）精度、ベジエカーブに許された誤差
    bool closed) {
  QPointF tHat1, tHat2;  // 端点の接線ベクトル(正規化したもの)を求める
  tHat1 = computeLeftTangent(points, 0);
  tHat2 = computeRightTangent(points, points.size() - 1);

  QList<QPointF> outList;
  // ここから再帰的に分割しつつベジエに近似していく
  fitCubic(points, 0, points.size() - 1, tHat1, tHat2, error, outList);

  // outListをbpListに移す
  BezierPointList bpList;

  if (outList.size() >= 2) {
    QPointF firstHandle;
    int lastIndex = outList.size() - 1;

    if (closed)  // 閉じたシェイプの場合は終点に向かうハンドルにする
      firstHandle = outList[0] + (outList[lastIndex] - outList[0]) / 3.0;
    else
      firstHandle = outList[0] + outList[0] - outList[1];

    outList.push_front(firstHandle);

    lastIndex = outList.size() - 1;

    QPointF lastHandle;

    if (closed)
      lastHandle = outList[lastIndex] + (outList[1] - outList[lastIndex]) / 3.0;
    else
      lastHandle =
          outList[lastIndex] + outList[lastIndex] - outList[lastIndex - 1];

    outList.push_back(lastHandle);

    // 3つずつまとめてBezierPointListに登録していく
    for (int index = 0; index < outList.size(); index += 3) {
      BezierPoint bp = {
          outList[index + 1],  // pos
          outList[index],      // firstHandle
          outList[index + 2]   // secondHandle
      };
      bpList.push_back(bp);
    }
  }
  return bpList;
}

QPointF FreeHandTool::computeLeftTangent(
    QList<QPointF>& points,  // Digitized points 軌跡データ
    int end  // Index to "left" end of region //左端のインデックス
) {
  QPointF tHat1;
  tHat1 = points[end + 1] - points[end];
  tHat1 = *V2Normalize(&tHat1);
  return tHat1;
}

QPointF FreeHandTool::computeRightTangent(
    QList<QPointF>& points,  // Digitized points 軌跡データ
    int end  // Index to "left" end of region //左端のインデックス
) {
  QPointF tHat2;
  tHat2 = points[end - 1] - points[end];
  tHat2 = *V2Normalize(&tHat2);
  return tHat2;
}

QPointF FreeHandTool::computeCenterTangent(
    QList<QPointF>& points,  // Digitized points 軌跡データ
    int center  // 分割点のインデックス Index to point inside region
) {
  QPointF tHatCenter;
  tHatCenter = points[center - 1] - points[center + 1];
  V2Normalize(&tHatCenter);
  return tHatCenter;
}

void FreeHandTool::computeCenterTangents(
    QList<QPointF>& points,  // Digitized points 軌跡データ
    int center,  // 分割点のインデックス Index to point inside region
    QPointF* V1, QPointF* V2) {
  QPointF tHatCenter;

  *V1 = points[center - 1] - points[center];
  *V2 = points[center + 1] - points[center];

  V2Normalize(V1);
  V2Normalize(V2);

  // 鋭角にする条件
  if (V2Dot(V1, V2) > -0.7) return;

  // スムースな曲線にする
  *V1 = *V2 = computeCenterTangent(points, center);
  V2Negate(V2);
}

// 結果にベジエポイント4点を追加
void FreeHandTool::addBezierCurve(QList<QPointF>& outList,
                                  QList<QPointF>& bezierCurve) {
  // outListの最後の点をbezierCurveの１点目と置き換え、残りはPushする
  // まだ点が無い場合は最初からPush
  if (outList.isEmpty())
    outList.push_back(bezierCurve[0]);
  else
    outList.last() = bezierCurve[0];

  for (int i = 1; i < 4; i++) outList.push_back(bezierCurve[i]);
}

//------------------------------------------
// 各点の距離を０～１に正規化したものを格納
// chordLengthParameterize :
// Assign parameter values to digitized points
// using relative distances between points.
//------------------------------------------
QList<double> FreeHandTool::chordLengthParameterize(QList<QPointF>& points,
                                                    int first, int last) {
  QList<double> u; /*  Parameterization */

  // 始点からの距離を積算して格納していく
  u.push_back(0.0);
  for (int i = first + 1; i <= last; i++) {
    u.push_back(u.last() +
                V2DistanceBetween2Points(&points[i], &points[i - 1]));
  }

  // 0-1に正規化
  for (int i = first + 1; i <= last; i++) {
    u[i - first] = u[i - first] / u[last - first];
  }

  return u;
}

//------------------------------------------
//  FitCubic :
//  ポイントの（サブ）セットに、ベジエ曲線をフィットさせる
//  Fit a Bezier curve to a (sub)set of digitized points
//------------------------------------------

void FreeHandTool::fitCubic(
    QList<QPointF>&
        points,  // 軌跡データ			Array of digitized points
    int first,
    int last,  // 端点のインデックス	Indices of first and last pts in region
    QPointF& tHat1,
    QPointF&
        tHat2,     // 端点の接線ベクトル	Unit tangent vectors at endpoints
    double error,  // ユーザの決めた精度（許される誤差値）	User-defined
                   // error squared
    QList<QPointF>& outList  // ここに結果を入れていく
) {
  QList<QPointF>
      bezCurve;  // QPointFの配列にしよう。Control points of fitted Bezier curve

  double iterationError =
      error * error;  // 誤差値 Error below which you try iterating
  int nPts =
      last - first + 1;  // セット内にあるポイント数 Number of points in subset

  //  二点しかない場合は発見的方法を使う Use heuristic if region only has two
  //  points in it
  if (nPts == 2) {
    // ハンドルの長さを2点間の距離の1/3にする
    double dist = V2DistanceBetween2Points(&points[last], &points[first]) / 3.0;
    // 4点追加
    bezCurve.push_back(points[first]);
    bezCurve.push_back(points[first] + *V2Scale(&tHat1, dist));
    bezCurve.push_back(points[last] + *V2Scale(&tHat2, dist));
    bezCurve.push_back(points[last]);
    addBezierCurve(outList, bezCurve);
    return;
  }

  QList<double>
      u;  // 各点の距離を０～１に正規化したものを格納 Parameter values for point

  // 始点からの各点距離を積算して正規化したものを格納
  //  Parameterize points, and attempt to fit curve
  u = chordLengthParameterize(points, first, last);

  // ベジエの生成 最小二乗法で誤差を最小化
  bezCurve = generateBezier(points, first, last, u, tHat1, tHat2);

  // 適用したベジエで、最もズレている点のインデックスと、誤差の幅(二乗)を得る
  double maxError;  // 最大誤差値 Maximum fitting error
  int splitPoint;   // 分割すべき点のインデックス Point to split point set at
                    //  Find max deviation of points to fitted curve
  maxError = computeMaxError(points, first, last, bezCurve, u, &splitPoint);

  // 誤差値が許容量よりも小さかったら、これ以上点は追加せず
  if (maxError < error) {
    addBezierCurve(outList, bezCurve);
    return;
  }

  // パラメータの再設定、、いるのか？？ まあちょっと入れてみよう
  //  If error not too large, try some reparameterization
  //  and iteration

  int maxIterations =
      4;  //  パラメータ再設定の試行数 Max times to try iterating
  if (maxError < iterationError) {
    for (int i = 0; i < maxIterations; i++) {
      QList<double> uPrime;
      // パラメータを再設定する(ニュートン法)
      reparameterize(uPrime, points, first, last, u, bezCurve);
      bezCurve = generateBezier(points, first, last, uPrime, tHat1, tHat2);
      maxError =
          computeMaxError(points, first, last, bezCurve, uPrime, &splitPoint);

      if (maxError < error) {
        addBezierCurve(outList, bezCurve);
        return;
      }
      u = uPrime;
    }
  }

  // フィットがうまくいかなかったので、分割する
  // Fitting failed -- split at max error point and fit recursively

  QList<char> splitarr;  // yes or no split

  // longはコンパイラに関わらず必ず4バイト
  for (int i = first; i < last; i++) {
    long c    = 0;
    double dx = points[i].x() - points[i + 1].x();
    double dy = points[i].y() - points[i + 1].y();
    if (dx < 0.0)
      c |= 1;
    else if (dx > 0.0)
      c |= 2;
    if (dy < 0.0)
      c |= 4;
    else if (dy > 0.0)
      c |= 8;
    splitarr.push_back(c);
  }
  // 排他論理和にする
  for (int i = 0; i < nPts - 2; i++)
    // for (int i = 0; i < nPts - 1; i++)
    splitarr[i] ^= splitarr[i + 1];

  long best  = splitPoint;
  long mdist = 0xffffff;

  for (int i = 1; i < nPts - 3; i++) {
    if (splitarr[i]) {
      long dist = (i + first + 1) - splitPoint;
      if (dist < 0) dist = -dist;
      if (dist < mdist) {
        best  = i + first + 1;
        mdist = dist;
      }
    }
  }
  splitPoint = best;

  QPointF tHatLeft, tHatRight;
  // 分割点での接線ベクトルを得る
  computeCenterTangents(points, splitPoint, &tHatLeft, &tHatRight);
  // ベジエを分割して再帰的に計算
  fitCubic(points, first, splitPoint, tHat1, tHatLeft, error, outList);
  fitCubic(points, splitPoint, last, tHatRight, tHat2, error, outList);
}

//-----------------------------------------------------
// 最小二乗法でベジエのコントロールポイントを探す
//  GenerateBezier :
//  Use least-squares method to find Bezier control points for region.
//-----------------------------------------------------
// ベジエの生成
QList<QPointF> FreeHandTool::generateBezier(
    QList<QPointF>& points,  //  軌跡のトレースデータ Array of digitized points
    int first, int last,  //  これからベジエを当てる範囲のインデックス Indices
                          //  defining region
    QList<double>&
        uPrime,  //  軌跡の各点の、始点からの距離を0-1に正規化したもの
                 //  Parameter values for region
    QPointF tHat1, QPointF tHat2  //  Unit tangents at endpoints
) {
  QList<QPointF>
      bezCurve;  // 戻り値を格納するための入れ物 RETURN bezier curve ctl pts

  int nPts = last - first +
             1;  // このサブカーブ内のトレース点の数 Number of pts in sub-curve

  // 両端点のベクトルの、各軌跡点での影響 的なパラメータ Precomputed rhs for eqn
  QList<QPair<QPointF, QPointF>> A;
  // 行列Aの計算 Compute the A's
  for (int i = 0; i < nPts; i++) {
    QPointF v1, v2;
    v1 = tHat1;
    v2 = tHat2;
    V2Scale(&v1, B1(uPrime[i]));
    V2Scale(&v2, B2(uPrime[i]));
    A.push_back(QPair<QPointF, QPointF>(v1, v2));
    // A[i].first	= v1;
    // A[i].second = v2;
  }

  // 行列 CとX
  double C[2][2];  // Matrix C
  double X[2];     // Matrix X
  // Create the C and X matrices
  C[0][0] = C[0][1] = C[1][0] = C[1][1] = 0.0;
  X[0] = X[1] = 0.0;

  for (int i = 0; i < nPts; i++) {
    C[0][0] += V2Dot(&A[i].first, &A[i].first);
    C[1][0] = C[0][1] += V2Dot(&A[i].first, &A[i].second);
    C[1][1] += V2Dot(&A[i].second, &A[i].second);

    QPointF tmp =
        points[first + i] -
        (points[first] * B0(uPrime[i]) + points[first] * B1(uPrime[i]) +
         points[last] * B2(uPrime[i]) + points[last] * B3(uPrime[i]));

    X[0] += V2Dot(&A[i].first, &tmp);
    X[1] += V2Dot(&A[i].second, &tmp);
  }

  // Determinants of matrices
  double det_C0_C1 = C[0][0] * C[1][1] - C[1][0] * C[0][1];
  double det_C0_X  = C[0][0] * X[1] - C[0][1] * X[0];
  double det_X_C1  = X[0] * C[1][1] - X[1] * C[0][1];

  // Finally, derive alpha values
  if (det_C0_C1 == 0.0) {
    det_C0_C1 = (C[0][0] * C[1][1]) * 10e-12;
  }

  // Alpha values, left and right
  // 両端から伸びるハンドルの長さ
  double alpha_l = det_X_C1 / det_C0_C1;
  double alpha_r = det_C0_X / det_C0_C1;

  //  If alpha negative, use the Wu/Barsky heuristic (see text)
  if (alpha_l < 0.0 || alpha_r < 0.0) {
    double dist = V2DistanceBetween2Points(&points[last], &points[first]) / 3.0;

    bezCurve.push_back(points[first]);                           // 0
    bezCurve.push_back(points[first] + *V2Scale(&tHat1, dist));  // 1
    bezCurve.push_back(points[last] + *V2Scale(&tHat2, dist));   // 2
    bezCurve.push_back(points[last]);                            // 3
    return bezCurve;
  }

  //  First and last control points of the Bezier curve are
  //  positioned exactly at the first and last data points
  //  Control points 1 and 2 are positioned an alpha distance out
  //  on the tangent vectors, left and right, respectively
  bezCurve.push_back(points[first]);                              // 0
  bezCurve.push_back(points[first] + *V2Scale(&tHat1, alpha_l));  // 1
  bezCurve.push_back(points[last] + *V2Scale(&tHat2, alpha_r));   // 2
  bezCurve.push_back(points[last]);                               // 3

  return bezCurve;
}

//-----------------------------------------------
// 適用したベジエで、最もズレている点のインデックスと、誤差の幅(二乗)を得る
//  ComputeMaxError :
//	Find the maximum squared distance of digitized points
//	to fitted curve.
//-----------------------------------------------

double FreeHandTool::computeMaxError(
    QList<QPointF>& points,  // 軌跡のトレースデータArray of digitized points
    int first, int last,     // これからベジエを当てる範囲のインデックス Indices
                             // defining region
    QList<QPointF>& bezCurve,  // さっきフィットさせたベジエ Fitted Bezier curve
    QList<double>& u,          // 始点からの各点距離を積算して正規化したもの
                               // Parameterization of points
    int* splitPoint)  // 最もズレている点のインデックスを入れて返す Point of
                      // maximum error
{
  // とりあえず中点
  *splitPoint = (last - first + 1) / 2;

  // 誤差の最大値
  double maxDist = 0.0;

  // 順番にインデックスを進めて、ずれの距離が最大になる位置を探す
  for (int i = first + 1; i < last; i++) {
    QPointF P   = Bezier(3, bezCurve, u[i - first]);  //  Point on curve
    QPointF v   = P - points[i];        //  Vector from point to curve
    double dist = V2SquaredLength(&v);  //  Current error

    // 誤差がより大きかったらインデックスを更新
    if (dist >= maxDist) {
      maxDist     = dist;
      *splitPoint = i;
    }
  }

  return maxDist;
}

//------------------------------------------------
// パラメータを再設定する
//  Reparameterize:
//	Given set of points and their parameterization, try to find
//   a better parameterization.
//------------------------------------------------

void FreeHandTool::reparameterize(
    QList<double>& uPrime,    // ここに結果を収める
    QList<QPointF>& points,   //  Array of digitized points
    int first, int last,      //  Indices defining region
    QList<double>& u,         //  Current parameter values
    QList<QPointF>& bezCurve  //  Current fitted curve
) {
  for (int i = first; i <= last; i++) {
    // 以下、newtonRaphsonRootFindの内容を入れる
    QPointF current_p = points[i];
    double current_u  = u[i - first];

    // Compute Q(u)
    QPointF Q_u = Bezier(3, bezCurve, current_u);

    QList<QPointF> Q1, Q2;  //  Q' and Q''
    // Generate control vertices for Q'
    for (int j = 0; j <= 2; j++) {
      Q1.push_back((bezCurve[j + 1] - bezCurve[j]) * 3.0);
    }
    // Generate control vertices for Q''
    for (int j = 0; j <= 1; j++) {
      Q2.push_back((Q1[j + 1] - Q1[j]) * 2.0);
    }
    // Compute Q'(u) and Q''(u)
    QPointF Q1_u = Bezier(2, Q1, current_u);
    QPointF Q2_u = Bezier(1, Q2, current_u);

    // Compute f(u)/f'(u)
    double numerator = (Q_u.x() - current_p.x()) * (Q1_u.x()) +
                       (Q_u.y() - current_p.y()) * (Q1_u.y());
    double denominator = (Q1_u.x()) * (Q1_u.x()) + (Q1_u.y()) * (Q1_u.y()) +
                         (Q_u.x() - current_p.x()) * Q2_u.x() +
                         (Q_u.y() - current_p.y()) * Q2_u.y();

    double improved_u = current_u - (numerator / denominator);

    uPrime.push_back(improved_u);
  }
}

//--------------------------------------------------------

int FreeHandTool::getCursorId(const QMouseEvent* e) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer || layer->isLocked()) {
    IwApp::instance()->showMessageOnStatusBar(
        tr("Current layer is locked or not exist."));
    return ToolCursor::ForbiddenCursor;
  }

  IwApp::instance()->showMessageOnStatusBar(
      tr("[Drag and draw] to create a new shape."));
  return ToolCursor::ArrowCursor;
  ;
}

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

CreateFreeHandShapeUndo::CreateFreeHandShapeUndo(ShapePair* shape,
                                                 IwProject* prj, IwLayer* layer)
    // CreateFreeHandShapeUndo::CreateFreeHandShapeUndo( IwShape* shape,
    // IwProject* prj)
    : m_project(prj), m_shape(shape), m_layer(layer) {
  // シェイプを挿入するインデックスは、シェイプリストの最後
  m_index = m_layer->getShapePairCount();
  // m_index = m_project->getShapeAmount();
}

CreateFreeHandShapeUndo::~CreateFreeHandShapeUndo() {
  // シェイプをdelete
  delete m_shape;
  // delete m_shape;
}

void CreateFreeHandShapeUndo::undo() {
  // シェイプを消去する
  m_layer->deleteShapePair(m_index);
  // m_project->deleteShape(m_index);
  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void CreateFreeHandShapeUndo::redo() {
  // シェイプをプロジェクトに追加
  m_layer->addShapePair(m_shape, m_index);
  // m_project->addShape(m_shape,m_index);

  // もしこれがカレントなら、
  if (m_project->isCurrent()) {
    // 作成したシェイプを選択状態にする
    IwShapePairSelection::instance()->selectOneShape(OneShape(m_shape, 0));
    // シグナルをエミット
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

FreeHandTool freehandTool;