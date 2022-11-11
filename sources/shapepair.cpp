//---------------------------------------
// ShapePair
// From-Toの形状をペアで持つ
//---------------------------------------

#include "shapepair.h"

#include "iwproject.h"
#include "preferences.h"
#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"

#include "iwselectionhandle.h"
#include "iwtimelinekeyselection.h"

#include "iwselectionhandle.h"
#include "iwshapepairselection.h"

#include "viewsettings.h"
#include "iwtrianglecache.h"
#include "projectutils.h"

#include "keydragtool.h"
#include "outputsettings.h"

#include <cassert>

#include <QRectF>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QMessageBox>
#include <QList>

#include <QPainter>
#include <QPolygon>
#include <QVector2D>

#include <QMap>
#include <QMutexLocker>
#include <QMouseEvent>

#include "logger.h"

#define PRINT_LOG(message)                                                     \
  { Logger::Write(message); }

namespace {

const int LayerIndent = 10;
const int LockPos     = 40;
const int PinPos      = 70;
const int CorrIndent  = 100;

// ●直線を作る

// verticePに、Start, Endをリニア補間した座標を格納していく
// endPointのいっこ手前まで
void makeLinearPoints(double* vertexP, QPointF startPoint, QPointF endPoint,
                      int precision) {
  double ratioStep = 1.0 / (double)precision;
  double tmpR      = 0.0;
  for (int p = 0; p < precision; p++) {
    // リニア補間した座標を取得
    QPointF tmpPoint = startPoint * (1.0 - tmpR) + endPoint * tmpR;
    // 値を格納、ポインタを進める
    vertexP[0] = tmpPoint.x();
    vertexP[1] = tmpPoint.y();
    vertexP[2] = 0.0;
    vertexP += 3;

    // ratioをインクリメント
    tmpR += ratioStep;
  }
}

// 片方のチャンネルのベジエ値を計算する
double calcBezier(double t, double v0, double v1, double v2, double v3) {
  return v0 * (1.0 - t) * (1.0 - t) * (1.0 - t) +
         v1 * (1.0 - t) * (1.0 - t) * t * 3.0 + v2 * (1.0 - t) * t * t * 3.0 +
         v3 * t * t * t;
}

// ベジエ座標を計算する
QPointF calcBezier(double t, QPointF& v0, QPointF& v1, QPointF& v2,
                   QPointF& v3) {
  return v0 * (1.0 - t) * (1.0 - t) * (1.0 - t) +
         v1 * (1.0 - t) * (1.0 - t) * t * 3.0 + v2 * (1.0 - t) * t * t * 3.0 +
         v3 * t * t * t;
}

// ●ベジエ曲線を作る
void makeBezierPoints(double* vertexP, QPointF p0, QPointF p1, QPointF p2,
                      QPointF p3, int precision) {
  double ratioStep = 1.0 / (double)precision;
  double tmpR      = 0.0;
  for (int p = 0; p < precision; p++) {
    // リニア補間した座標を取得
    QPointF tmpPoint = p0 * (1.0 - tmpR) * (1.0 - tmpR) * (1.0 - tmpR) +
                       p1 * (1.0 - tmpR) * (1.0 - tmpR) * tmpR * 3.0 +
                       p2 * (1.0 - tmpR) * tmpR * tmpR * 3.0 +
                       p3 * tmpR * tmpR * tmpR;
    // 値を格納、ポインタを進める
    vertexP[0] = tmpPoint.x();
    vertexP[1] = tmpPoint.y();
    vertexP[2] = 0.0;
    vertexP += 3;

    // ratioをインクリメント
    tmpR += ratioStep;
  }
}

// ベジエのバウンディングボックスを計算する
QRectF getBezierBBox(QPointF p0, QPointF p1, QPointF p2, QPointF p3) {
  double left, right, top, bottom;
  for (int xy = 0; xy < 2; xy++) {
    double v0, v1, v2, v3;
    // X方向
    if (xy == 0) {
      v0 = p0.x();
      v1 = p1.x();
      v2 = p2.x();
      v3 = p3.x();
    } else {
      v0 = p0.y();
      v1 = p1.y();
      v2 = p2.y();
      v3 = p3.y();
    }

    double a, b, c;

    std::vector<double> valVec;

    a = v3 - 3.0 * v2 + 3.0 * v1 - v0;
    b = (v0 - v1) * v3 + v2 * v2 + (-v1 - v0) * v2 + v1 * v1;
    c = v2 - 2.0 * v1 + v0;

    valVec.push_back(v0);
    valVec.push_back(v3);

    // 重解の場合
    if (a == 0.0) {
      double d = 2.0 * (v2 - 2.0 * v1 + v0);
      if (d != 0.0) {
        double t = -(v1 - v0) / d;
        if (0.0 < t && t < 1.0) {
          valVec.push_back(calcBezier(t, v0, v1, v2, v3));
        }
      }
    }
    // 解が存在する場合
    else if (b >= 0.0) {
      double bb = sqrt(b);
      // 解１つめ
      double t = (bb - c) / a;
      if (0.0 < t && t < 1.0) valVec.push_back(calcBezier(t, v0, v1, v2, v3));

      // 解２つめ
      t = -(bb + c) / a;
      if (0.0 < t && t < 1.0) valVec.push_back(calcBezier(t, v0, v1, v2, v3));
    }

    // 最大、最小値を得る
    double minPos = v0;
    double maxPos = v0;
    for (int i = 0; i < valVec.size(); i++) {
      if (valVec[i] < minPos) minPos = valVec[i];
      if (valVec[i] > maxPos) maxPos = valVec[i];
    }

    if (xy == 0) {
      left  = minPos;
      right = maxPos;
    } else {
      top    = minPos;
      bottom = maxPos;
    }
  }

  return QRectF(QPointF(left, top), QPointF(right, bottom));
}

// 最近傍距離とその t の値を得る(範囲つき)
double getNearestPos(QPointF& p0, QPointF& p1, QPointF& p2, QPointF& p3,
                     const QPointF& pos, double& t, double rangeBefore,
                     double rangeAfter) {
  double firstDist2         = 10000.0;
  double secondDist2        = 10000.0;
  double firstNearestRatio  = 0.0;
  double secondNearestRatio = 0.0;

  int beforeIndex = (int)(rangeBefore * 100.0);
  int afterIndex  = (int)(rangeAfter * 100.0);

  // tの各分割点ごとに、距離を測っていく
  // まず100分割
  for (int i = beforeIndex; i <= afterIndex; i++) {
    double tmpR = (double)i / 100.0;
    // リニア補間した座標を取得
    QPointF tmpPoint = calcBezier(tmpR, p0, p1, p2, p3);
    QPointF tmpVec   = pos - tmpPoint;
    // 基準点からの距離の二乗を計測
    double tmp_dist2 = tmpVec.x() * tmpVec.x() + tmpVec.y() * tmpVec.y();
    // 距離の二乗が短かったら記録を更新
    if (tmp_dist2 < firstDist2) {
      secondDist2        = firstDist2;
      secondNearestRatio = firstNearestRatio;
      firstDist2         = tmp_dist2;
      firstNearestRatio  = tmpR;
    }
    // 2番目だけ更新の可能性
    else if (tmp_dist2 < secondDist2) {
      secondDist2        = tmp_dist2;
      secondNearestRatio = tmpR;
    }
  }

  // 続いて、上位2点を、距離に合わせて線形補間して結果を得る
  //  Ratioが２つ以上離れている場合、一番近いやつを返す
  if (std::abs(firstNearestRatio - secondNearestRatio) > 0.019) {
    t = firstNearestRatio;
    return std::sqrt(firstDist2);
  }

  double firstDist  = std::sqrt(firstDist2);
  double secondDist = std::sqrt(secondDist2);

  // 距離に合わせて線形補間
  t = firstNearestRatio * secondDist / (firstDist + secondDist) +
      secondNearestRatio * firstDist / (firstDist + secondDist);

  if (t < rangeBefore)
    t = rangeBefore;
  else if (t > rangeAfter)
    t = rangeAfter;

  QPointF nearestPoint = calcBezier(t, p0, p1, p2, p3);
  QPointF nearestVec   = pos - nearestPoint;
  // 基準点からの距離を返す
  return std::sqrt(nearestVec.x() * nearestVec.x() +
                   nearestVec.y() * nearestVec.y());
}

// 最近傍距離とその t の値を得る
double getNearestPos(QPointF& p0, QPointF& p1, QPointF& p2, QPointF& p3,
                     const QPointF& pos, double& t) {
  return getNearestPos(p0, p1, p2, p3, pos, t, 0.0, 1.0);
}

// 挿入されたハンドルの座標を得る
QPointF calcAddedHandlePos(double t, QPointF& p0, QPointF& p1, QPointF& p2) {
  return t * t * p0 + 2.0 * t * (1.0 - t) * p1 + (1.0 - t) * (1.0 - t) * p2;
}

// ベジエ値ｔでの微分値を求める
double getBezierDifferntial(double t, double v0, double v1, double v2,
                            double v3) {
  return 3.0 * t * t * (v3 - v2) + 6.0 * (1.0 - t) * t * (v2 - v1) +
         3.0 * (1.0 - t) * (1.0 - t) * (v1 - v0);
}

// ベジエの距離測定に用いる精度。
const double BEZ_PREC = 1000.0;

// ベジエの長さを求める(ピクセル長で)
double getBezierLength(const QPointF& onePix, QPointF& p0, QPointF& p1,
                       QPointF& p2, QPointF& p3) {
  double amount = 0.0;
  // 1000分割で求めよう
  for (int i = 0; i <= (int)BEZ_PREC; i++) {
    double ratio = (double)i / BEZ_PREC;
    double dx    = getBezierDifferntial(ratio, p0.x(), p1.x(), p2.x(), p3.x()) /
                onePix.x();
    double dy = getBezierDifferntial(ratio, p0.y(), p1.y(), p2.y(), p3.y()) /
                onePix.y();
    double tmpLen = std::sqrt(dx * dx + dy * dy) / BEZ_PREC;
    if (i == 0 || i == (int)BEZ_PREC) tmpLen /= 2.0;
    amount += tmpLen;
  }

  return amount;
}

// ベジエの長さを求める(ピクセル長で) (StartとEndのRatioを指定できる版)
double getBezierLength(const double start, const double end,
                       const QPointF& onePix, QPointF& p0, QPointF& p1,
                       QPointF& p2, QPointF& p3) {
  double amount     = 0.0;
  int startIndex    = (int)(start * BEZ_PREC);
  int endIndex      = (int)(end * BEZ_PREC);
  double startRatio = BEZ_PREC * start - (double)startIndex;
  double endRatio   = BEZ_PREC * end - (double)endIndex;

  for (int i = startIndex; i <= endIndex; i++) {
    double ratio = (double)i / BEZ_PREC;
    double dx    = getBezierDifferntial(ratio, p0.x(), p1.x(), p2.x(), p3.x()) /
                onePix.x();
    double dy = getBezierDifferntial(ratio, p0.y(), p1.y(), p2.y(), p3.y()) /
                onePix.y();
    double tmpLen = std::sqrt(dx * dx + dy * dy) / BEZ_PREC;
    if (i == start)
      tmpLen *= (1.0 - startRatio);
    else if (i == end)
      tmpLen *= endRatio;
    amount += tmpLen;
  }

  return amount;
}

// ピクセル長からベジエ座標を計算。あふれた場合は1を返す
double getBezierRatioByLength(const QPointF& onePix, double start,
                              double& remainLength, QPointF& p0, QPointF& p1,
                              QPointF& p2, QPointF& p3) {
  double amount     = 0.0;
  int startIndex    = (int)(start * BEZ_PREC);
  double startRatio = BEZ_PREC * start - (double)startIndex;

  // 1000分割で求めよう
  for (int i = startIndex; i < (int)BEZ_PREC; i++) {
    double ratio = (double)i / BEZ_PREC;
    double dx    = getBezierDifferntial(ratio, p0.x(), p1.x(), p2.x(), p3.x()) /
                onePix.x();
    double dy = getBezierDifferntial(ratio, p0.y(), p1.y(), p2.y(), p3.y()) /
                onePix.y();
    double tmpLen = std::sqrt(dx * dx + dy * dy) / BEZ_PREC;
    if (i == startIndex) tmpLen *= 1.0 - startRatio;

    // 超えたら
    if (remainLength <= amount + tmpLen) {
      // 線形補間で返す
      return ratio + (1.0 / BEZ_PREC) * (remainLength - amount) / tmpLen;
    }

    amount += tmpLen;
  }

  remainLength -= amount;

  return 1.0;
}

const QColor selectedColor(128, 128, 250, 100);

};  // namespace

ShapePair::ShapePair(int frame, bool isClosed, BezierPointList bPointList,
                     CorrPointList cPointList,
                     double dstShapeOffset)  // オフセットはデフォルト０
    : m_precision(3)
    , m_isClosed(isClosed)
    , m_isExpanded({false, true})
    , m_isLocked({false, false})
    , m_isPinned({false, false})
    , m_name(tr("Shape"))
    , m_isParent(
          isClosed)  // 親シェイプフラグの初期値は、閉じているかどうかで決める
    , m_isVisible(true)
    , m_animationSmoothness(0.) {
  // 指定したフレームに形状キーを入れる
  if (dstShapeOffset == 0.0) {
    for (int i = 0; i < 2; i++) m_formKeys[i].setKeyData(frame, bPointList);
  } else  // オフセットがある場合、ずらして格納
  {
    m_formKeys[0].setKeyData(frame, bPointList);
    for (int bp = 0; bp < bPointList.size(); bp++) {
      bPointList[bp].firstHandle += QPointF(dstShapeOffset, dstShapeOffset);
      bPointList[bp].pos += QPointF(dstShapeOffset, dstShapeOffset);
      bPointList[bp].secondHandle += QPointF(dstShapeOffset, dstShapeOffset);
    }
    m_formKeys[1].setKeyData(frame, bPointList);
  }
  // ベジエの頂点数を初期データから格納
  m_bezierPointAmount = bPointList.size();

  // 指定したフレームに対応点キーを入れる
  for (int i = 0; i < 2; i++) m_corrKeys[i].setKeyData(frame, cPointList);
  // 対応点の数を初期データから格納
  m_corrPointAmount = cPointList.size();
}

//-----------------------------------------------------------------------------
// (ファイルのロード時のみ使う)空のシェイプのコンストラクタ
//-----------------------------------------------------------------------------
ShapePair::ShapePair()
    : m_precision(1)
    , m_isClosed(true)
    , m_isExpanded({false, true})
    , m_isLocked({false, false})
    , m_isPinned({false, false})
    , m_isParent(true)
    , m_isVisible(true)
    , m_animationSmoothness(0.) {}

//-----------------------------------------------------------------------------
// コピーコンストラクタ
//-----------------------------------------------------------------------------
ShapePair::ShapePair(ShapePair* src)
    : m_precision(src->getEdgeDensity())
    , m_isClosed(src->isClosed())
    , m_bezierPointAmount(src->getBezierPointAmount())
    , m_corrPointAmount(src->getCorrPointAmount())
    , m_isExpanded({false, true})
    , m_isLocked({false, false})
    , m_isPinned({false, false})
    , m_shapeTag({src->getShapeTags(0), src->getShapeTags(1)})
    , m_isParent(src->isParent())
    , m_isVisible(src->isVisible())
    , m_animationSmoothness(src->getAnimationSmoothness()) {
  for (int i = 0; i < 2; i++) {
    m_formKeys[i].setData(src->getFormData(i));
    m_formKeys[i].setInterpolation(src->getFormInterpolation(i));
    m_corrKeys[i].setData(src->getCorrData(i));
    m_corrKeys[i].setInterpolation(src->getCorrInterpolation(i));
  }

  m_name = src->getName();
}

//-----------------------------------------------------------------------------

ShapePair* ShapePair::clone() { return new ShapePair(this); }

//-----------------------------------------------------------------------------
// あるフレームでのベジエ形状を返す
//-----------------------------------------------------------------------------
BezierPointList ShapePair::getBezierPointList(int frame, int fromTo) {
  return m_formKeys[fromTo].getData(frame, 0, m_animationSmoothness);
}

//-----------------------------------------------------------------------------
// あるフレームでの対応点を返す
//-----------------------------------------------------------------------------
CorrPointList ShapePair::getCorrPointList(int frame, int fromTo) {
  return m_corrKeys[fromTo].getData(
      frame, (m_isClosed) ? m_bezierPointAmount : m_bezierPointAmount - 1);
}

double ShapePair::getBezierInterpolation(int frame, int fromTo) {
  assert(isFormKey(frame, fromTo));
  return m_formKeys[fromTo].getInterpolation().value(frame, 0.5);
}

double ShapePair::getCorrInterpolation(int frame, int fromTo) {
  assert(isCorrKey(frame, fromTo));
  return m_corrKeys[fromTo].getInterpolation().value(frame, 0.5);
}

void ShapePair::setBezierInterpolation(int frame, int fromTo, double interp) {
  assert(isFormKey(frame, fromTo));
  if (interp == 0.5)
    m_formKeys[fromTo].getInterpolation().remove(frame);
  else
    m_formKeys[fromTo].getInterpolation().insert(frame, interp);
}

void ShapePair::setCorrInterpolation(int frame, int fromTo, double interp) {
  assert(isCorrKey(frame, fromTo));
  if (interp == 0.5)
    m_corrKeys[fromTo].getInterpolation().remove(frame);
  else
    m_corrKeys[fromTo].getInterpolation().insert(frame, interp);
}

//-----------------------------------------------------------------------------
// 表示用の頂点数を求める。線が閉じているか空いているかで異なる
//-----------------------------------------------------------------------------
int ShapePair::getVertexAmount(IwProject* project) {
  // ベジエの分割数を求める
  int bezierPrec = Preferences::instance()->getBezierActivePrecision();

  return (m_isClosed) ? (bezierPrec * m_bezierPointAmount)
                      : (bezierPrec * (m_bezierPointAmount - 1) + 1);
}

//-----------------------------------------------------------------------------
// 指定したフレームのベジエ頂点情報を返す
//-----------------------------------------------------------------------------
double* ShapePair::getVertexArray(int frame, int fromTo, IwProject* project) {
  PRINT_LOG("   getVertexArray frame " + std::to_string(frame) + "  start")
  BezierPointList pointList = getBezierPointList(frame, fromTo);

  // ベジエの分割数を求める
  int bezierPrec = (int)Preferences::instance()->getBezierActivePrecision();

  // 頂点数を求める。線が閉じているか空いているかで異なる
  int vertSize = getVertexAmount(project);

  double* vertices = new double[vertSize * 3];

  double* v_p = vertices;

  // 閉じたシェイプの場合
  if (m_isClosed) {
    // 各ポイントについて
    for (int p = 0; p < m_bezierPointAmount; p++) {
      // ベジエの４つのコントロールポイントを得る
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = (p == m_bezierPointAmount - 1)
                                   ? pointList.at(0)
                                   : pointList.at(p + 1);

      // ２つのハンドル長が０のとき、リニアで繋ぐ
      if (startPoint.pos == startPoint.secondHandle &&
          endPoint.pos == endPoint.firstHandle) {
        makeLinearPoints(v_p, startPoint.pos, endPoint.pos, bezierPrec);
        v_p += 3 * bezierPrec;
      }
      // ４つのコントロールポイントから、各ベジエの点を分割数分格納する
      else {
        makeBezierPoints(v_p, startPoint.pos, startPoint.secondHandle,
                         endPoint.firstHandle, endPoint.pos, bezierPrec);
        v_p += 3 * bezierPrec;
      }
    }
  }
  // 開いたシェイプの場合
  else {
    // 各ポイントについて
    for (int p = 0; p < m_bezierPointAmount - 1; p++) {
      // ベジエの４つのコントロールポイントを得る
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = pointList.at(p + 1);

      // ２つのハンドル長が０のとき、リニアで繋ぐ
      if (startPoint.pos == startPoint.secondHandle &&
          endPoint.pos == endPoint.firstHandle) {
        makeLinearPoints(v_p, startPoint.pos, endPoint.pos, bezierPrec);
        v_p += 3 * bezierPrec;
      }
      // ４つのコントロールポイントから、各ベジエの点を分割数分格納する
      else {
        makeBezierPoints(v_p, startPoint.pos, startPoint.secondHandle,
                         endPoint.firstHandle, endPoint.pos, bezierPrec);
        v_p += 3 * bezierPrec;
      }
    }
    // 最後に端点の座標を追加
    BezierPoint lastPoint = pointList.at(pointList.size() - 1);
    v_p[0]                = lastPoint.pos.x();
    v_p[1]                = lastPoint.pos.y();
    v_p[2]                = 0.0;
  }
  PRINT_LOG("   getVertexArray frame " + std::to_string(frame) + "  end")

  return vertices;
}

//-----------------------------------------------------------------------------
// シェイプのバウンディングボックスを返す
//-----------------------------------------------------------------------------
QRectF ShapePair::getBBox(int frame, int fromTo) {
  BezierPointList pointList = getBezierPointList(frame, fromTo);

  // 各セグメントのバウンディングボックスを合成していく
  QRectF resultRect = QRectF(pointList.at(0).pos, pointList.at(0).pos);

  // 閉じたシェイプの場合
  if (m_isClosed) {
    // 各ポイントについて
    for (int p = 0; p < m_bezierPointAmount; p++) {
      // ベジエの４つのコントロールポイントを得る
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = (p == m_bezierPointAmount - 1)
                                   ? pointList.at(0)
                                   : pointList.at(p + 1);

      // ２つのハンドル長が０のとき、リニアなので、端点がエッジになる
      if (startPoint.pos == startPoint.secondHandle &&
          endPoint.pos == endPoint.firstHandle) {
        QRectF tmpRect(startPoint.pos, endPoint.pos);
        tmpRect    = tmpRect.normalized();
        resultRect = resultRect.united(tmpRect);
      }
      // ４つのコントロールポイントから、各ベジエのバウンディングボックスを足しこむ
      else {
        resultRect = resultRect.united(
            getBezierBBox(startPoint.pos, startPoint.secondHandle,
                          endPoint.firstHandle, endPoint.pos));
      }
    }
  }
  // 開いたシェイプの場合
  else {
    // 各ポイントについて
    for (int p = 0; p < m_bezierPointAmount - 1; p++) {
      // ベジエの４つのコントロールポイントを得る
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = pointList.at(p + 1);

      // ２つのハンドル長が０のとき、リニアなので、端点がエッジになる
      if (startPoint.pos == startPoint.secondHandle &&
          endPoint.pos == endPoint.firstHandle) {
        QRectF tmpRect(startPoint.pos, endPoint.pos);
        tmpRect    = tmpRect.normalized();
        resultRect = resultRect.united(tmpRect);
      }
      // ４つのコントロールポイントから、各ベジエのバウンディングボックスを足しこむ
      else {
        resultRect = resultRect.united(
            getBezierBBox(startPoint.pos, startPoint.secondHandle,
                          endPoint.firstHandle, endPoint.pos));
      }
    }
  }

  return resultRect;
}

//-----------------------------------------------------------------------------
// 形状キーフレームのセット
//-----------------------------------------------------------------------------
void ShapePair::setFormKey(int frame, int fromTo, BezierPointList bPointList) {
  m_formKeys[fromTo].setKeyData(frame, bPointList);
}

//-----------------------------------------------------------------------------
// 形状キーフレームを消去する
// enableRemoveLastKey(デフォルトfalse)がtrueの場合、最後の1つのキーフレームも消去可にする。
// そのままでは落ちるので、あとで確実に何とかすること！！
//-----------------------------------------------------------------------------
void ShapePair::removeFormKey(int frame, int fromTo, bool enableRemoveLastKey) {
  // frame が キーフレームじゃなければreturn
  if (!isFormKey(frame, fromTo)) return;
  // キーフレームの数が残り１つならreturn
  if (m_formKeys[fromTo].getKeyCount() == 1 && !enableRemoveLastKey) return;

  // 消去
  m_formKeys[fromTo].removeKeyData(frame);
}

//-----------------------------------------------------------------------------
// 対応点キーフレームのセット
//-----------------------------------------------------------------------------

void ShapePair::setCorrKey(int frame, int fromTo, CorrPointList cPointList) {
  m_corrKeys[fromTo].setKeyData(frame, cPointList);
}

//-----------------------------------------------------------------------------
// 対応点キーフレームを消去する
// enableRemoveLastKey(デフォルトfalse)がtrueの場合、最後の1つのキーフレームも消去可にする。
// そのままでは落ちるので、あとで確実に何とかすること！！
//-----------------------------------------------------------------------------
void ShapePair::removeCorrKey(int frame, int fromTo, bool enableRemoveLastKey) {
  // frame が キーフレームじゃなければreturn
  if (!isCorrKey(frame, fromTo)) return;
  // キーフレームの数が残り１つならreturn
  if (m_corrKeys[fromTo].getKeyCount() == 1 && !enableRemoveLastKey) return;

  // 消去
  m_corrKeys[fromTo].removeKeyData(frame);
}

//-----------------------------------------------------------------------------
// 指定位置にコントロールポイントを追加する
// コントロールポイントのインデックスを返す
//-----------------------------------------------------------------------------
int ShapePair::addControlPoint(const QPointF& pos, int frame, int fromTo) {
  // ポイントリストを得る
  BezierPointList pointList = getBezierPointList(frame, fromTo);
  // 各セグメントについて、クリックされた位置とセグメントとの最近傍距離を求めて、
  // どこのセグメントにポイントを追加するのか調べる

  // マウス位置に最近傍のベジエ座標を得る
  double nearestBezierPos = getNearestBezierPos(pos, frame, fromTo);

  int nearestSegmentIndex = (int)nearestBezierPos;
  double nearestRatio     = nearestBezierPos - (double)nearestSegmentIndex;

  for (int ft = 0; ft < 2; ft++) {
    QMap<int, BezierPointList> formData = m_formKeys[ft].getData();
    // 形状データの各キーフレームについて
    QMap<int, BezierPointList>::const_iterator i = formData.constBegin();
    while (i != formData.constEnd()) {
      int frame              = i.key();
      BezierPointList bPList = i.value();

      // 前後の点を得る
      BezierPoint Point0 = bPList[nearestSegmentIndex];
      BezierPoint Point1 =
          (m_isClosed && nearestSegmentIndex == m_bezierPointAmount - 1)
              ? bPList[0]
              : bPList[nearestSegmentIndex + 1];

      BezierPoint newPoint;

      // 各点を更新していく
      newPoint.pos = calcBezier(nearestRatio, Point0.pos, Point0.secondHandle,
                                Point1.firstHandle, Point1.pos);
      // 挿入されたハンドルの座標を得る
      newPoint.firstHandle =
          calcAddedHandlePos(1.0 - nearestRatio, Point0.pos,
                             Point0.secondHandle, Point1.firstHandle);
      newPoint.secondHandle =
          calcAddedHandlePos(1.0 - nearestRatio, Point0.secondHandle,
                             Point1.firstHandle, Point1.pos);

      // 前のコントロールポイントのsecondHandleを編集
      QPointF tmpVec      = Point0.secondHandle - Point0.pos;
      Point0.secondHandle = Point0.pos + tmpVec * nearestRatio;

      tmpVec             = Point1.firstHandle - Point1.pos;
      Point1.firstHandle = Point1.pos + tmpVec * (1.0 - nearestRatio);

      // 前後点の更新
      bPList.replace(nearestSegmentIndex, Point0);
      if (m_isClosed && nearestSegmentIndex == m_bezierPointAmount - 1)
        bPList.replace(0, Point1);
      else
        bPList.replace(nearestSegmentIndex + 1, Point1);

      // 新しい点の挿入
      bPList.insert(nearestSegmentIndex + 1, newPoint);

      // キーフレーム情報を更新
      m_formKeys[ft].setKeyData(frame, bPList);

      ++i;
    }

    // 続いて、対応点を追加する処理をする
    QMap<int, CorrPointList> corrData = m_corrKeys[ft].getData();
    // 対応点データの各キーフレームについて
    QMap<int, CorrPointList>::const_iterator c = corrData.constBegin();
    while (c != corrData.constEnd()) {
      int frame = c.key();

      // 各Corrデータの更新
      CorrPointList cPList = c.value();

      for (int p = 0; p < cPList.size(); p++) {
        // nearestSegmentIndexより小さかったらムシ
        if (cPList.at(p) < (double)nearestSegmentIndex) continue;
        // nearestSegmentIndex+1より大きかったら単に値を＋１する
        if (cPList.at(p) > (double)nearestSegmentIndex + 1.0) {
          cPList[p] += 1.0;
          continue;
        }

        // それ以外の場合
        double oldCorr = cPList.at(p);

        double oldCorrRatio = oldCorr - (double)nearestSegmentIndex;

        // 新しいコントロールポイントより前のCorr点の場合
        if (oldCorrRatio < nearestRatio)
          cPList[p] = oldCorrRatio / nearestRatio + (double)nearestSegmentIndex;
        // 新しいコントロールポイントより後のCorr点の場合
        else
          cPList[p] = 1.0 + (double)nearestSegmentIndex +
                      (oldCorrRatio - nearestRatio) / (1.0 - nearestRatio);
      }

      // キーフレーム情報を更新
      m_corrKeys[ft].setKeyData(frame, cPList);

      ++c;
    }
  }

  // ベジエポイントの追加
  m_bezierPointAmount++;

  return nearestSegmentIndex + 1;
}

//-----------------------------------------------------------------------------
// 指定位置に対応点を追加する うまくいったらtrueを返す
//-----------------------------------------------------------------------------

bool ShapePair::addCorrPoint(const QPointF& pos, int frame, int fromTo) {
  // 元の対応点
  CorrPointList orgCorrs = getCorrPointList(frame, fromTo);

  // マウス位置に最近傍のベジエ座標を得る
  double nearestBezierPos = getNearestBezierPos(pos, frame, fromTo);

  // どこの対応点の間に挿入されるか調べる
  int neighbourIndex;
  double minDiff = 100.0;
  for (int c = 0; c < m_corrPointAmount; c++) {
    double tmpDiff = nearestBezierPos - orgCorrs.at(c);
    if (abs(minDiff) > abs(tmpDiff)) {
      minDiff        = tmpDiff;
      neighbourIndex = c;
    }
  }
  // 前後の対応点インデックス
  int beforeIndex, afterIndex;
  if (minDiff > 0)  // 近傍点が左側にある場合
  {
    beforeIndex = neighbourIndex;
    afterIndex =
        (neighbourIndex == m_corrPointAmount - 1) ? 0 : neighbourIndex + 1;
  } else  // 近傍点が右側にある場合
  {
    beforeIndex =
        (neighbourIndex == 0) ? m_corrPointAmount - 1 : neighbourIndex - 1;
    afterIndex = neighbourIndex;
  }
  // 位置の調整 前後の点から0.05ずつ離す
  double beforeBPos = orgCorrs.at(beforeIndex);
  if (m_isClosed && beforeBPos > nearestBezierPos)
    beforeBPos -= (double)m_bezierPointAmount;
  double afterBPos = orgCorrs.at(afterIndex);
  if (m_isClosed && afterBPos < nearestBezierPos)
    afterBPos += (double)m_bezierPointAmount;
  std::cout << "insert corr point between " << beforeIndex << "(" << beforeBPos
            << ") and " << afterIndex << "(" << afterBPos << ")." << std::endl;
  // 前後の点の間隔が0.1未満の場合、アラートを出してreturn
  if (afterBPos - beforeBPos < 0.1) {
    QMessageBox::warning(
        0, "IwaMorph",
        "Cannot add a correspondence point too close to existing ones.",
        QMessageBox::Ok, QMessageBox::Ok);
    return false;
  }
  // Corr点を追加する ただし、このフレームがキーフレームになるわけではない。
  double ratio = (nearestBezierPos - beforeBPos) / (afterBPos - beforeBPos);

  // 対応点の挿入
  //  Shape２つに対して行う
  for (int ft = 0; ft < 2; ft++) {
    QMap<int, CorrPointList> corrKeysMap = getCorrData(ft);
    // 各キーフレームデータに対応点を挿入する
    QMap<int, CorrPointList>::iterator i = corrKeysMap.begin();
    while (i != corrKeysMap.end()) {
      CorrPointList cpList = i.value();

      double before = cpList.at(beforeIndex);
      double after  = cpList.at(afterIndex);
      if (isClosed() && after < before) after += (double)getBezierPointAmount();

      double insertedPos = before + (after - before) * ratio;
      if (isClosed() && insertedPos > (double)getBezierPointAmount())
        insertedPos -= (double)getBezierPointAmount();

      // リストに点を挿入
      cpList.insert(beforeIndex + 1, insertedPos);
      // insertコマンドは、既に同じKeyにデータが有った場合、置き換えられる
      corrKeysMap.insert(i.key(), cpList);
      ++i;
    }
    // データを戻す 対応点カウントは中で更新している
    setCorrData(corrKeysMap, ft);
  }

  return true;
}

//--------------------------------------------------------
// 指定インデックスの対応点を消去する うまくいったらtrueを返す
//--------------------------------------------------------

bool ShapePair::deleteCorrPoint(int index) {
  if (index < 0 || index >= getCorrPointAmount()) return false;
  // Shape２つに対して行う
  for (int ft = 0; ft < 2; ft++) {
    QMap<int, CorrPointList> corrKeysMap = getCorrData(ft);
    // 各キーフレームデータから対応点を消去する
    QMap<int, CorrPointList>::iterator i = corrKeysMap.begin();
    while (i != corrKeysMap.end()) {
      CorrPointList cpList = i.value();
      cpList.removeAt(index);
      // insertコマンドは、既に同じKeyにデータが有った場合、置き換えられる
      corrKeysMap.insert(i.key(), cpList);
      ++i;
    }
    // データを戻す 対応点カウントは中で更新している
    setCorrData(corrKeysMap, ft);
  }

  return true;
}

//--------------------------------------------------------
// マウス位置に最近傍のベジエ座標を得る
//--------------------------------------------------------

double ShapePair::getNearestBezierPos(const QPointF& pos, int frame, int fromTo,
                                      double& dist) {
  // ポイントリストを得る
  BezierPointList pointList = getBezierPointList(frame, fromTo);

  int nearestSegmentIndex;
  double nearestRatio;

  double tmpDist = 10000.0;

  // 閉じたシェイプの場合
  if (m_isClosed) {
    // 各ポイントについて
    for (int p = 0; p < m_bezierPointAmount; p++) {
      // ベジエの４つのコントロールポイントを得る
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = (p == m_bezierPointAmount - 1)
                                   ? pointList.at(0)
                                   : pointList.at(p + 1);

      // 最近傍距離とその t の値を得る
      double t;
      double dist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                                  endPoint.firstHandle, endPoint.pos, pos, t);
      // これまでで一番近かったら、値を更新
      if (dist < tmpDist) {
        tmpDist             = dist;
        nearestSegmentIndex = p;
        nearestRatio        = t;
      }
    }
  }
  // 開いたシェイプの場合
  else {
    // 各ポイントについて
    for (int p = 0; p < m_bezierPointAmount - 1; p++) {
      // ベジエの４つのコントロールポイントを得る
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = pointList.at(p + 1);

      // 最近傍距離とその t の値を得る
      double t;
      double dist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                                  endPoint.firstHandle, endPoint.pos, pos, t);
      // これまでで一番近かったら、値を更新
      if (dist < tmpDist) {
        tmpDist             = dist;
        nearestSegmentIndex = p;
        nearestRatio        = t;
      }
    }
  }
  dist = tmpDist;

  return (double)nearestSegmentIndex + nearestRatio;
}

double ShapePair::getNearestBezierPos(const QPointF& pos, int frame,
                                      int fromTo) {
  double dist;
  return getNearestBezierPos(pos, frame, fromTo, dist);
}

//--------------------------------------------------------
// マウス位置に最近傍のベジエ座標を得る(範囲つき版)
//--------------------------------------------------------
double ShapePair::getNearestBezierPos(const QPointF& pos, int frame, int fromTo,
                                      const double rangeBefore,
                                      const double rangeAfter,
                                      double& nearestDist) {
  // ポイントリストを得る
  BezierPointList pointList = getBezierPointList(frame, fromTo);

  int nearestSegmentIndex;
  double nearestRatio;

  double tmpDist = 10000.0;

  int beforeIndex    = (int)rangeBefore;
  double beforeRatio = rangeBefore - (double)beforeIndex;
  int afterIndex     = (int)rangeAfter;
  double afterRatio  = rangeAfter - (double)afterIndex;

  // 閉じたシェイプ かつ ０をまたいでいる 場合
  if (m_isClosed && (rangeBefore > rangeAfter)) {
    // ２つに分ける
    double beforeDist;
    double nearestPosBefore =
        getNearestBezierPos(pos, frame, fromTo, rangeBefore,
                            (double)m_bezierPointAmount - 0.0001, beforeDist);

    double afterDist;
    double nearestPosAfter =
        getNearestBezierPos(pos, frame, fromTo, 0.0, rangeAfter, afterDist);

    // 近い方をチョイス
    if (beforeDist < afterDist) {
      tmpDist             = beforeDist;
      nearestSegmentIndex = (int)nearestPosBefore;
      nearestRatio        = nearestPosBefore - (double)nearestSegmentIndex;
    } else {
      tmpDist             = afterDist;
      nearestSegmentIndex = (int)nearestPosAfter;
      nearestRatio        = nearestPosAfter - (double)nearestSegmentIndex;
    }
  }
  // 順当に並んでいる場合
  else {
    // 同じセグメントにある場合
    if (beforeIndex == afterIndex) {
      // ベジエの４つのコントロールポイントを得る
      BezierPoint startPoint = pointList.at(beforeIndex);
      BezierPoint endPoint =
          (m_isClosed && beforeIndex == m_bezierPointAmount - 1)
              ? pointList.at(0)
              : pointList.at(beforeIndex + 1);
      double t;
      // 範囲つきで近傍点を探す
      double dist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                                  endPoint.firstHandle, endPoint.pos, pos, t,
                                  beforeRatio, afterRatio);
      tmpDist     = dist;
      nearestSegmentIndex = beforeIndex;
      nearestRatio        = t;
    } else {
      // 各ポイントについて
      for (int p = beforeIndex; p <= afterIndex; p++) {
        // ベジエの４つのコントロールポイントを得る
        BezierPoint startPoint = pointList.at(p);
        BezierPoint endPoint   = (m_isClosed && p == m_bezierPointAmount - 1)
                                     ? pointList.at(0)
                                     : pointList.at(p + 1);

        // 最近傍距離とその t の値を得る
        double t;
        double dist;
        // 最初の点
        if (p == beforeIndex) {
          // 範囲つきで近傍点を探す
          dist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                               endPoint.firstHandle, endPoint.pos, pos, t,
                               beforeRatio, 1.0);
        }
        // 後の点
        else if (p == afterIndex) {
          // 範囲つきで近傍点を探す
          dist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                               endPoint.firstHandle, endPoint.pos, pos, t, 0.0,
                               afterRatio);
        } else {
          dist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                               endPoint.firstHandle, endPoint.pos, pos, t);
        }
        // これまでで一番近かったら、値を更新
        if (dist < tmpDist) {
          tmpDist             = dist;
          nearestSegmentIndex = p;
          nearestRatio        = t;
        }
      }
    }
  }

  nearestDist = tmpDist;

  return (double)nearestSegmentIndex + nearestRatio;
}

//--------------------------------------------------------
// ワークエリアのリサイズに合わせ逆数でスケールしてシェイプのサイズを維持する
//--------------------------------------------------------
void ShapePair::counterResize(QSizeF workAreaScale) {
  auto c_scale = [&](QPointF pos) {
    return QPointF(0.5 + (pos.x() - 0.5) / workAreaScale.width(),
                   0.5 + (pos.y() - 0.5) / workAreaScale.height());
  };

  for (int ft = 0; ft < 2; ft++) {
    QMap<int, BezierPointList> formData = m_formKeys[ft].getData();
    // 形状データの各キーフレームについて
    QMap<int, BezierPointList>::const_iterator i = formData.constBegin();
    while (i != formData.constEnd()) {
      int frame              = i.key();
      BezierPointList bPList = i.value();

      for (BezierPoint& bp : bPList) {
        bp.firstHandle  = c_scale(bp.firstHandle);
        bp.pos          = c_scale(bp.pos);
        bp.secondHandle = c_scale(bp.secondHandle);
      }

      // キーフレーム情報を更新
      m_formKeys[ft].setKeyData(frame, bPList);

      ++i;
    }
  }
}

//--------------------------------------------------------
// 形状データを返す
//--------------------------------------------------------
QMap<int, BezierPointList> ShapePair::getFormData(int fromTo) {
  return m_formKeys[fromTo].getData();
}
QMap<int, double> ShapePair::getFormInterpolation(int fromTo) {
  return m_formKeys[fromTo].getInterpolation();
}

//--------------------------------------------------------
// 形状データをセット
//--------------------------------------------------------
void ShapePair::setFormData(QMap<int, BezierPointList> data, int fromTo) {
  m_formKeys[fromTo].setData(data);
  // 形状点の数を更新
  m_bezierPointAmount = data.values().first().size();
}

//--------------------------------------------------------
// 対応点データを返す
//--------------------------------------------------------
QMap<int, CorrPointList> ShapePair::getCorrData(int fromTo) {
  return m_corrKeys[fromTo].getData();
}
QMap<int, double> ShapePair::getCorrInterpolation(int fromTo) {
  return m_corrKeys[fromTo].getInterpolation();
}

//--------------------------------------------------------
// 対応点データをセット
//--------------------------------------------------------
void ShapePair::setCorrData(QMap<int, CorrPointList> data, int fromTo) {
  m_corrKeys[fromTo].setData(data);
  // 対応点の数を更新
  m_corrPointAmount = data.values().first().size();
}

//--------------------------------------------------------
// 対応点の座標リストを得る
//--------------------------------------------------------
QList<QPointF> ShapePair::getCorrPointPositions(int frame, int fromTo) {
  // 現在の形状データを得る
  BezierPointList bPList = getBezierPointList(frame, fromTo);
  // 現在の対応点データを得る
  CorrPointList cPList = getCorrPointList(frame, fromTo);

  QList<QPointF> points;

  for (int c = 0; c < cPList.size(); c++) {
    double cPValue = cPList.at(c);

    int segmentIndex = (int)cPValue;
    double ratio     = cPValue - (double)segmentIndex;

    BezierPoint Point1 = bPList.at(segmentIndex);
    BezierPoint Point2 = bPList.at(
        (segmentIndex == m_bezierPointAmount - 1) ? 0 : segmentIndex + 1);

    points.push_back(calcBezier(ratio, Point1.pos, Point1.secondHandle,
                                Point2.firstHandle, Point2.pos));
  }

  return points;
}

//--------------------------------------------------------
// 現在のフレームのCorrenspondence(対応点)間を分割した点の分割値を得る
// 方針：できるだけ等間隔に分割したい。
//--------------------------------------------------------
QList<double> ShapePair::getDiscreteCorrValues(const QPointF& onePix, int frame,
                                               int fromTo) {
  QMutexLocker lock(&mutex_);
  BezierPointList bPList = getBezierPointList(frame, fromTo);
  // 結果を収めるやつ
  QList<double> discreteCorrValues;

  // TODO: 対応点の順番が反転しているばあいも考慮?

  CorrPointList cPList = getCorrPointList(frame, fromTo);

  int corrSegAmount = (m_isClosed) ? m_corrPointAmount : m_corrPointAmount - 1;
  for (int c = 0; c < corrSegAmount; c++) {
    double Corr1 = cPList.at(c);
    double Corr2 =
        cPList.at((m_isClosed && c == m_corrPointAmount - 1) ? 0 : c + 1);

    // 対応点間のピクセル長を計算する
    double corrSegLength =
        getBezierLengthFromValueRange(onePix, frame, fromTo, Corr1, Corr2);

    // 対応点を分割するピクセル距離
    double divideLength = corrSegLength / (double)m_precision;

    double currentCorrPos = Corr1;

    // 分割点毎にベジエ座標点を追加していく
    for (int p = 0; p < m_precision; p++) {
      // 格納
      discreteCorrValues.push_back(currentCorrPos);

      // 最後の点を格納したらおしまい
      if (p == m_precision - 1) break;

      double remainLength = divideLength;

      while (1) {
        int currentIndex    = (int)currentCorrPos;
        double currentRatio = currentCorrPos - (double)currentIndex;

        // 現在のベジエを入れていく
        BezierPoint Point1 = bPList.at(currentIndex);
        BezierPoint Point2 =
            bPList.at((m_isClosed && currentIndex == m_bezierPointAmount - 1)
                          ? 0
                          : currentIndex + 1);

        // ピクセル長からベジエ座標を計算。あふれた場合は1以上を返す
        //  remaimLengthが減っていく
        double newRatio = getBezierRatioByLength(
            onePix, currentRatio, remainLength, Point1.pos, Point1.secondHandle,
            Point2.firstHandle, Point2.pos);

        // このセグメントでは分割点が収まらなかった場合
        if (newRatio == 1.0) {
          currentCorrPos = (double)(currentIndex + 1);
          if (m_isClosed && currentCorrPos >= m_bezierPointAmount)
            currentCorrPos -= (double)m_bezierPointAmount;
          continue;
        } else {
          // 値を格納
          currentCorrPos = (double)currentIndex + newRatio;
          break;
        }
      }
    }
  }

  // Openなシェイプの場合は、最後の端点を追加
  if (!m_isClosed) discreteCorrValues.push_back(cPList.last());

  return discreteCorrValues;
}

//--------------------------------------------------------
// ベジエ値から座標値を計算する
//--------------------------------------------------------
QPointF ShapePair::getBezierPosFromValue(int frame, int fromTo,
                                         double bezierValue) {
  BezierPointList bPList = getBezierPointList(frame, fromTo);
  int segmentIndex       = (int)bezierValue;
  double ratio           = bezierValue - (double)segmentIndex;

  BezierPoint Point1 = bPList.at(segmentIndex);

  // ちょうどBezierPointの位置なら、その座標を返す
  if (ratio == 0.0) return Point1.pos;

  BezierPoint Point2 =
      bPList.at((m_isClosed && segmentIndex == m_bezierPointAmount - 1)
                    ? 0
                    : segmentIndex + 1);

  return calcBezier(ratio, Point1.pos, Point1.secondHandle, Point2.firstHandle,
                    Point2.pos);
}

//--------------------------------------------------------
// 指定ベジエ値での１ベジエ値あたりの座標値の変化量を返す
//--------------------------------------------------------
double ShapePair::getBezierSpeedAt(int frame, int fromTo, double bezierValue,
                                   double distance) {
  auto modifyBezierPos = [&](double val) {
    if (isClosed()) {
      if (val >= (double)getBezierPointAmount())
        return val - (double)getBezierPointAmount();
      else if (val < 0.0)
        return val + (double)getBezierPointAmount();
    } else {
      if (val > (double)getBezierPointAmount())
        return (double)getBezierPointAmount();
      else if (val < 0.0)
        return 0.0;
    }
    return val;
  };

  double val1 = modifyBezierPos(bezierValue);
  double val2 = modifyBezierPos(bezierValue + distance);

  double d = std::abs((isClosed()) ? distance * 2.0 : val2 - val1);

  QVector2D p1(getBezierPosFromValue(frame, fromTo, val1));
  QVector2D p2(getBezierPosFromValue(frame, fromTo, val2));
  return (p1 - p2).length() / d;
}
//--------------------------------------------------------
// 指定したベジエ値間のピクセル距離を返す
//--------------------------------------------------------
double ShapePair::getBezierLengthFromValueRange(const QPointF& onePix,
                                                int frame, int fromTo,
                                                double startVal,
                                                double endVal) {
  if (startVal == endVal) return 0.0;

  BezierPointList bPList = getBezierPointList(frame, fromTo);

  // それぞれが所属しているセグメントID
  int segId1 = (int)startVal;
  int segId2 = (int)endVal;
  // 対応点のセグメント内の小数位置
  double ratio1 = startVal - (double)segId1;
  double ratio2 = endVal - (double)segId2;

  // 対応点間距離を積算する変数
  double segLength = 0.0;

  // 対応点が一周している場合(closedなシェイプの場合にあり得る)
  if (endVal < startVal) {
    // ２つに分けて計算する
    segLength =
        getBezierLengthFromValueRange(onePix, frame, fromTo, startVal,
                                      (double)m_bezierPointAmount - 0.000001) +
        getBezierLengthFromValueRange(onePix, frame, fromTo, 0.0, endVal);
  }
  // 普通の順番に対応点が並んでいるとき
  else {
    // 同じセグメント上の距離
    if (segId1 == segId2) {
      BezierPoint Point1 = bPList.at(segId1);
      BezierPoint Point2 = bPList.at(
          (m_isClosed && segId1 == m_bezierPointAmount - 1) ? 0 : segId1 + 1);

      segLength =
          getBezierLength(ratio1, ratio2, onePix, Point1.pos,
                          Point1.secondHandle, Point2.firstHandle, Point2.pos);
    } else {
      for (int seg = segId1; seg <= segId2; seg++) {
        // ratio2が０なら、そのベジエは計算する必要なし
        if (seg == segId2 && ratio2 == 0.0) continue;

        BezierPoint Point1 = bPList.at(seg);
        BezierPoint Point2 = bPList.at(
            (m_isClosed && seg == m_bezierPointAmount - 1) ? 0 : seg + 1);
        // Startを含むベジエの距離
        if (seg == segId1)
          segLength += getBezierLength(ratio1, 1.0, onePix, Point1.pos,
                                       Point1.secondHandle, Point2.firstHandle,
                                       Point2.pos);
        // Endを含むベジエの距離
        else if (seg == segId2)
          segLength += getBezierLength(0.0, ratio2, onePix, Point1.pos,
                                       Point1.secondHandle, Point2.firstHandle,
                                       Point2.pos);
        // 全部ベジエが含まれている部分の距離
        else
          segLength += getBezierLength(onePix, Point1.pos, Point1.secondHandle,
                                       Point2.firstHandle, Point2.pos);
      }
    }
  }

  return segLength;
}

//--------------------------------------------------------
// Correnspondence(対応点)間を何分割するか、の変更
//--------------------------------------------------------

void ShapePair::setEdgeDensity(int prec) {
  if (m_precision == prec) return;
  m_precision = prec;
}

//--------------------------------------------------------
// シェイプのキー間の補間の度合い、の変更
//--------------------------------------------------------

void ShapePair::setAnimationSmoothness(double smoothness) {
  if (m_animationSmoothness == smoothness) return;
  m_animationSmoothness = smoothness;
}

//--------------------------------------------------------
// 形状キーフレーム数を返す
//--------------------------------------------------------

int ShapePair::getFormKeyFrameAmount(int fromTo) {
  return m_formKeys[fromTo].getKeyCount();
}

//--------------------------------------------------------
// 対応点キーフレーム数を返す
//--------------------------------------------------------

int ShapePair::getCorrKeyFrameAmount(int fromTo) {
  return m_corrKeys[fromTo].getKeyCount();
}

//--------------------------------------------------------
// ソートされた形状キーフレームのリストを返す
//--------------------------------------------------------

QList<int> ShapePair::getFormKeyFrameList(int fromTo) {
  QList<int> frameList = m_formKeys[fromTo].getData().keys();
  std::sort(frameList.begin(), frameList.end());
  return frameList;
}

//--------------------------------------------------------
// ソートされた対応点キーフレームのリストを返す
//--------------------------------------------------------

QList<int> ShapePair::getCorrKeyFrameList(int fromTo) {
  QList<int> frameList = m_corrKeys[fromTo].getData().keys();
  std::sort(frameList.begin(), frameList.end());
  return frameList;
}

// frame 前後のキーフレーム範囲（前後のキーは含まない）を返す
void ShapePair::getFormKeyRange(int& start, int& end, int frame, int fromTo) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // 前方向
  start = frame;
  while (start - 1 >= 0 && !m_formKeys[fromTo].isKey(start - 1)) {
    --start;
  }
  // 後方向
  end = frame;
  while (end + 1 < project->getProjectFrameLength() &&
         !m_formKeys[fromTo].isKey(end + 1)) {
    ++end;
  }
}

void ShapePair::getCorrKeyRange(int& start, int& end, int frame, int fromTo) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // 前方向
  start = frame;
  while (start - 1 >= 0 && !m_corrKeys[fromTo].isKey(start - 1)) {
    --start;
  }
  // 後方向
  end = frame;
  while (end + 1 < project->getProjectFrameLength() &&
         !m_corrKeys[fromTo].isKey(end + 1)) {
    ++end;
  }
}

//----------------------------------
// 以下、タイムラインでの表示用
//--------------------------------------------------------

// 展開しているかどうかを踏まえて表示行数を返す
int ShapePair::getRowCount() {
  // 「シェイプを全て表示」モードの時
  if (!Preferences::instance()->isShowSelectedShapesOnly()) return 5;

  int count = 1;
  for (const auto& isExp : m_isExpanded)
    if (isExp) count += 2;
  return count;
}

//--------------------------------------------------------
// 　描画(ヘッダ部分)
//--------------------------------------------------------

void ShapePair::drawTimeLineHead(QPainter& p, int& vpos, int width,
                                 int rowHeight, int& currentRow,
                                 int mouseOverRow, int mouseHPos,
                                 ShapePair* parentShape, ShapePair* nextShape,
                                 bool layerIsLocked) {
  int indent          = LayerIndent;
  int childIndent     = (m_isParent) ? 0 : indent * 2;
  bool isHighlightRow = (mouseOverRow == currentRow) && !layerIsLocked;
  bool isSelected =
      IwShapePairSelection::instance()->isSelected(OneShape(this, 0)) ||
      IwShapePairSelection::instance()->isSelected(OneShape(this, 1));
  // まずは自分自身
  QRect rowRect(indent + childIndent, vpos, width - indent - childIndent,
                rowHeight);
  rowRect.adjust(0, 0, -1, 0);

  QColor rowColor = (m_isParent) ? QColor(50, 81, 51) : QColor(69, 93, 84);
  p.setPen(Qt::black);
  p.setBrush(rowColor);
  p.drawRect(rowRect);
  if (layerIsLocked) {
    p.fillRect(rowRect, QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
  }

  auto marginFor = [&](int height) { return (rowHeight - height) / 2; };

  // シェイプ上にマウスがあったらハイライト
  if (isHighlightRow) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 220, 50));

    // 補間コントロールをハイライト
    if (mouseHPos > rowRect.right() - 20) {
      p.drawRect(rowRect.right() - 20, rowRect.center().y() - 8, 18, 18);
    } else if (mouseHPos >= rowRect.left() + 23 &&
               mouseHPos < rowRect.left() + 43) {
      p.drawRect(rowRect.left() + 23, rowRect.center().y() - 8, 18, 18);
    } else {
      p.drawRect(rowRect);
    }
  }

  p.drawPixmap(rowRect.left() + 3, rowRect.center().y() - 8, 18, 18,
               m_isClosed ? QPixmap(":Resources/shape_close.svg")
                          : QPixmap(":Resources/shape_open.svg"));

  p.drawPixmap(rowRect.left() + 24, rowRect.center().y() - 7, 15, 15,
               m_isVisible ? QPixmap(":Resources/visible_on.svg")
                           : QPixmap(":Resources/visible_off.svg"));

  p.setPen((isSelected) ? Qt::yellow : Qt::white);
  p.drawText(rowRect.adjusted(45, 0, -20, 0), Qt::AlignVCenter, m_name);

  p.drawPixmap(
      rowRect.right() - 20, rowRect.center().y() - 8, 18, 18,
      (m_animationSmoothness == 0.)   ? QPixmap(":Resources/interp_linear.svg")
      : (m_animationSmoothness == 1.) ? QPixmap(":Resources/interp_smooth.svg")
                                      : QPixmap(":Resources/interp_mixed.svg"));

  // 子シェイプの先頭と親シェイプを結ぶ線を描く
  if (!m_isParent && parentShape) {
    QVector<QPoint> points = {
        QPoint(indent + childIndent / 2, vpos + rowHeight / 2),
        QPoint(indent + childIndent, vpos + rowHeight / 2),
    };
    // 縦線
    points.append(QPoint(indent + childIndent / 2, vpos));
    if (nextShape && !nextShape->isParent())
      points.append(QPoint(indent + childIndent / 2, vpos + rowHeight));
    else
      points.append(QPoint(indent + childIndent / 2, vpos + rowHeight / 2));

    p.setPen(Qt::gray);
    p.drawLines(points);
  }

  vpos += rowHeight;
  currentRow++;

  // キーフレームの行ヘッダを描く
  if (Preferences::instance()->isShowSelectedShapesOnly() && !m_isExpanded[0] &&
      !m_isExpanded[1])
    return;

  QList<QString> formCorrList;
  formCorrList << tr("Form") << tr("Corr");
  QList<QString> fromToList;
  fromToList << tr("From ") << tr("To ");

  // 先頭ずらす
  rowRect.setLeft(CorrIndent);
  // rowRect.setLeft(indent * 4);

  bool doDrawConnectionLine =
      parentShape && nextShape && !nextShape->isParent();

  for (int fromTo = 0; fromTo < 2; fromTo++) {
    if (Preferences::instance()->isShowSelectedShapesOnly() &&
        !m_isExpanded[fromTo])
      continue;

    bool isOneShapeSelected =
        IwShapePairSelection::instance()->isSelected(OneShape(this, fromTo));
    bool isMouseOnLockButton =
        ((mouseOverRow == currentRow) || (mouseOverRow == currentRow + 1)) &&
        (mouseHPos >= LockPos && mouseHPos < PinPos) && !layerIsLocked;
    bool isMouseOnPinButton =
        ((mouseOverRow == currentRow) || (mouseOverRow == currentRow + 1)) &&
        (mouseHPos >= PinPos && mouseHPos < CorrIndent) && !layerIsLocked;

    QRect fcRowRect(rowRect);
    fcRowRect.translate(0, rowHeight);
    fcRowRect.setLeft(LockPos);
    fcRowRect.setBottom(fcRowRect.bottom() + rowHeight);
    QColor rowColor = (fromTo == 0) ? QColor(100, 60, 50) : QColor(50, 60, 110);
    p.setPen(Qt::black);
    p.setBrush(rowColor);
    p.drawRect(fcRowRect);
    if (layerIsLocked) {
      p.fillRect(fcRowRect,
                 QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
    }
    if (m_isLocked[fromTo]) {
      QRect tmpRect(fcRowRect);
      tmpRect.setLeft(CorrIndent);
      p.fillRect(tmpRect, QBrush(QColor(128, 128, 128, 128), Qt::FDiagPattern));
    }

    // ロックエリアの描画
    QRect lockRect(fcRowRect);
    lockRect.setRight(PinPos);
    if (isMouseOnLockButton) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(255, 255, 220, 50));
      p.drawRect(lockRect);
    }
    p.drawPixmap(lockRect.center().x() - 9, lockRect.center().y() - 9, 18, 18,
                 (m_isLocked[fromTo]) ? QPixmap(":Resources/lock_on.svg")
                                      : QPixmap(":Resources/lock.svg"));

    // ピンエリアの描画
    QRect pinRect(fcRowRect);
    pinRect.setLeft(PinPos);
    pinRect.setRight(CorrIndent);
    if (isMouseOnPinButton) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(255, 255, 220, 50));
      p.drawRect(pinRect);
    }
    p.drawPixmap(pinRect.center().x() - 9, pinRect.center().y() - 9, 18, 18,
                 (m_isPinned[fromTo]) ? QPixmap(":Resources/pin_on.svg")
                                      : QPixmap(":Resources/pin.svg"));

    for (int formCorr = 0; formCorr < 2; formCorr++) {
      isHighlightRow =
          (mouseOverRow == currentRow) && !layerIsLocked && !m_isLocked[fromTo];
      rowRect.translate(0, rowHeight);

      if (formCorr == 1) {
        QColor rowColor = QColor(69, 68, 48);

        p.setPen(Qt::black);
        p.setBrush(rowColor);
        p.drawRect(rowRect);
        if (layerIsLocked) {
          p.fillRect(rowRect,
                     QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
        }
        if (m_isLocked[fromTo]) {
          p.fillRect(rowRect,
                     QBrush(QColor(128, 128, 128, 128), Qt::FDiagPattern));
        }
      }

      if (isOneShapeSelected) {
        qreal borderWidth = 4.;  // even number
        p.setPen(QPen(selectedColor, borderWidth, Qt::SolidLine, Qt::SquareCap,
                      Qt::MiterJoin));
        p.setBrush(Qt::NoBrush);
        int halfW = (int)(borderWidth / 2);
        p.drawRect(rowRect.adjusted(1 + halfW, 1 + halfW, -halfW, -halfW));
      }

      if (isHighlightRow && !isMouseOnLockButton && !isMouseOnPinButton) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 220, 50));
        p.drawRect(rowRect);
      }
      p.setPen(Qt::white);
      p.drawText(rowRect.adjusted(indent, 0, 0, 0), Qt::AlignVCenter,
                 fromToList.at(fromTo) + formCorrList.at(formCorr));

      currentRow++;
    }

    if (doDrawConnectionLine) {
      p.setPen(Qt::gray);
      p.drawLine(QPoint(indent + indent, vpos),
                 QPoint(indent + indent, vpos + rowHeight * 2));
    }

    vpos += rowHeight * 2;
  }
}

void ShapePair::drawTimeLine(QPainter& p, int& vpos, int width, int fromFrame,
                             int toFrame, int frameWidth, int rowHeight,
                             int& currentRow, int mouseOverRow,
                             double mouseOverFrameD, bool layerIsLocked,
                             bool layerIsVisibleInRender) {
  bool isHighlightRow = (mouseOverRow == currentRow) && !layerIsLocked;

  // まずは自分自身
  QRect frameRect(0, vpos, frameWidth, rowHeight);

  QColor rowColor = (m_isParent) ? QColor(50, 81, 51) : QColor(69, 93, 84);

  IwProject* prj  = IwApp::instance()->getCurrentProject()->getProject();
  int frameLength = prj->getProjectFrameLength();

  // レイヤがレンダリング対象かどうか
  int targetShapeTag = prj->getOutputSettings()->getShapeTagId();
  bool showCacheState =
      m_isParent && layerIsVisibleInRender && isRenderTarget(targetShapeTag);

  // 各フレームで、中身のあるものだけ描画
  for (int f = fromFrame; f <= toFrame; f++) {
    QRect tmpRect = frameRect.translated(frameWidth * f, 0);
    p.setPen(Qt::black);
    if (f < frameLength)
      p.setBrush(rowColor);
    else
      p.setBrush(QColor(255, 255, 255, 5));
    p.drawRect(tmpRect);

    if (isHighlightRow && (int)mouseOverFrameD == f)
      p.fillRect(tmpRect, QColor(255, 255, 220, 100));
    else if (layerIsLocked) {
      p.fillRect(tmpRect, QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
    }

    // show render cache state if the shape is render target
    if (showCacheState && f < frameLength) {
      bool isCached = IwTriangleCache::instance()->isValid(f, this);
      bool isRunning =
          IwApp::instance()->getCurrentProject()->isRunningPreview(f);
      QRect barRect = tmpRect;
      barRect.setTop(barRect.bottom() - 4);
      barRect.setBottom(barRect.bottom() - 1);
      p.fillRect(barRect, (isCached)    ? Qt::darkGreen
                          : (isRunning) ? Qt::darkYellow
                                        : Qt::darkRed);
    }
  }

  currentRow++;
  vpos += rowHeight;

  static const QPointF handlePoints[3] = {
      QPointF(0.0, 0.0),
      QPointF(-2.0, rowHeight / 2),
      QPointF(2.0, rowHeight / 2),
  };

  for (int fromTo = 0; fromTo < 2; fromTo++) {
    if (Preferences::instance()->isShowSelectedShapesOnly() &&
        !m_isExpanded[fromTo])
      continue;

    bool isSelected = false;
    int selFromTo;
    bool isForm;
    if (IwApp::instance()->getCurrentSelection()->getSelection() ==
        IwTimeLineKeySelection::instance()) {
      OneShape selectedShape = IwTimeLineKeySelection::instance()->getShape();
      if (selectedShape.shapePairP == this) {
        isSelected = true;
        selFromTo  = selectedShape.fromTo;
        isForm     = IwTimeLineKeySelection::instance()->isFormKeySelected();
      }
    }

    // キーフレームの◆
    QPolygon polygon;
    polygon << QPoint(5, 0) << QPoint(0, 5) << QPoint(-5, 0) << QPoint(0, -5);

    QPolygon selectionFrame;
    selectionFrame << QPoint(6, 0) << QPoint(0, 6) << QPoint(-6, 0)
                   << QPoint(0, -6);

    polygon.translate(frameWidth / 2, rowHeight / 2 + vpos);
    selectionFrame.translate(frameWidth / 2, rowHeight / 2 + vpos);

    for (int formCorr = 0; formCorr < 2; formCorr++) {
      isHighlightRow =
          (mouseOverRow == currentRow) && !layerIsLocked && !m_isLocked[fromTo];
      frameRect.translate(0, rowHeight);

      // キーフレーム選択されているかどうか
      bool rowSelected = false;
      if (isSelected && selFromTo == fromTo && (formCorr == 0) == isForm)
        rowSelected = true;

      QColor rowColor    = (formCorr == 1) ? QColor(69, 68, 48)
                           : (fromTo == 0) ? QColor(100, 60, 50)
                                           : QColor(50, 60, 110);
      QRect tmpFrameRect = frameRect.translated(fromFrame * frameWidth, 0);
      for (int f = fromFrame; f <= toFrame; f++) {
        if (f >= frameLength) break;

        p.setBrush(rowColor);
        p.setPen(Qt::black);
        p.drawRect(tmpFrameRect);
        if (layerIsLocked) {
          p.fillRect(tmpFrameRect,
                     QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
        }
        if (m_isLocked[fromTo]) {
          p.fillRect(tmpFrameRect,
                     QBrush(QColor(128, 128, 128, 128), Qt::FDiagPattern));
        }

        bool isSelectedCell = false;
        //----- キーフレーム選択のとき青い背景にする
        if (rowSelected &&
            IwTimeLineKeySelection::instance()->isFrameSelected(f)) {
          p.setBrush(QColor(100, 153, 176, 128));
          p.setPen(Qt::NoPen);
          p.drawRect(tmpFrameRect);

          isSelectedCell = true;
        }
        //-----

        bool isKey;
        if (formCorr == 0)
          isKey = m_formKeys[fromTo].isKey(f);
        else
          isKey = m_corrKeys[fromTo].isKey(f);
        if (isKey) {
          if (isHighlightRow && (int)mouseOverFrameD == f)
            p.setBrush(Qt::yellow);
          else if (layerIsLocked || m_isLocked[fromTo])
            p.setBrush(Qt::lightGray);
          else
            p.setBrush(QColor(255, 128, 0, 255));

          p.setPen(Qt::black);
          p.drawPolygon(polygon.translated(f * frameWidth, 0));
        }

        if (KeyDragTool::instance()->isCurrentRow(OneShape(this, fromTo),
                                                  (formCorr == 0))) {
          if ((isSelectedCell && isKey &&
               !KeyDragTool::instance()->isDragging()) ||
              KeyDragTool::instance()->isDragDestination(f)) {
            p.setPen(Qt::white);
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(selectionFrame.translated(f * frameWidth, 0));
          }
        }

        tmpFrameRect.translate(frameWidth, 0);
      }

      // 割りを描く
      QList<int> keyframes;
      if (formCorr == 0)
        keyframes = getFormKeyFrameList(fromTo);
      else
        keyframes = getCorrKeyFrameList(fromTo);
      // 最後のキーはスキップしてよい
      for (int kId = 0; kId < keyframes.size() - 1; kId++) {
        int curKey  = keyframes.at(kId);
        int nextKey = keyframes.at(kId + 1);
        // 範囲内かどうか
        if (nextKey <= fromFrame || toFrame <= curKey || nextKey == curKey + 1)
          continue;
        // マウスオーバーしているか、割り方が真ん中でないときにハンドルを描く
        double interp;
        if (formCorr == 0)
          interp = getBezierInterpolation(curKey, fromTo);
        else
          interp = getCorrInterpolation(curKey, fromTo);
        if (interp == 0.5 &&
            (!isHighlightRow || (int)mouseOverFrameD < curKey ||
             nextKey < (int)mouseOverFrameD))
          continue;

        // 中割フレームにスライダを描く
        p.setPen(Qt::white);
        int left     = (curKey + 1);
        int right    = nextKey;
        int lineVPos = frameRect.center().y();
        // 横線
        p.drawLine(left * frameWidth, lineVPos, right * frameWidth, lineVPos);
        double handleFramePos = (double)left + interp * (double)(right - left);
        // マウスがハンドル上にあるかどうか
        if (isHighlightRow && std::abs(mouseOverFrameD - handleFramePos) < 0.1)
          p.setPen(Qt::cyan);
        else
          p.setPen(Qt::white);
        p.setBrush(Qt::NoBrush);
        // ハンドル
        p.save();
        p.translate(handleFramePos * frameWidth, lineVPos);
        p.drawPolygon(handlePoints, 3);
        p.restore();
      }

      polygon.translate(0, rowHeight);
      selectionFrame.translate(0, rowHeight);
      currentRow++;
    }
    vpos += rowHeight * 2;
  }
}

//--------------------------------------------------------
// タイムラインクリック時
//--------------------------------------------------------
bool ShapePair::onTimeLineClick(int rowInShape, double frameD, bool ctrlPressed,
                                bool shiftPressed, Qt::MouseButton button) {
  // 最初の行はシェイプの行で、これはとりあえずムシ
  if (rowInShape == 0) return false;
  bool fromShapeIsShown =
      (!Preferences::instance()->isShowSelectedShapesOnly() || m_isExpanded[0]);
  int fromTo  = (fromShapeIsShown && rowInShape <= 2) ? 0 : 1;
  bool isForm = (rowInShape % 2 == 1) ? true : false;
  int frame   = (double)frameD;

  if (m_isLocked[fromTo]) return false;

  IwTimeLineKeySelection::instance()->setShape(OneShape(this, fromTo), isForm);
  // シェイプも単独選択にする
  IwShapePairSelection::instance()->selectOneShape(OneShape(this, fromTo));

  // 補間ハンドルをつかんでいるかどうかの判定
  QList<int> keys;
  if (isForm)
    keys = getFormKeyFrameList(fromTo);
  else
    keys = getCorrKeyFrameList(fromTo);
  if (keys.count() >= 2 && !keys.contains(frame) && keys.first() < frame &&
      frame < keys.last()) {
    // キーを探す
    int prevKey;
    for (auto key : keys) {
      if (key > frame) {
        if (key == prevKey + 1) break;
        double interp;
        if (isForm)
          interp = getBezierInterpolation(prevKey, fromTo);
        else
          interp = getCorrInterpolation(prevKey, fromTo);
        // ハンドルのフレーム位置
        double handleFramePos =
            (double)(prevKey + 1) + interp * (double)(key - prevKey - 1);
        if (std::abs(frameD - handleFramePos) < 0.1) {
          IwTimeLineKeySelection::instance()->makeCurrent();
          InterpHandleDragTool::instance()->onClick(
              OneShape(this, fromTo), isForm, frameD - handleFramePos, prevKey,
              key);
          IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
          return true;
        }
        break;
      } else
        prevKey = key;
    }
  }

  if (!shiftPressed) {
    // 通常クリック
    if (!ctrlPressed) {
      // セルの単独選択　未選択のセル：Press時　選択済のセル：Release時
      if (!IwTimeLineKeySelection::instance()->isFrameSelected(frame)) {
        IwTimeLineKeySelection::instance()->selectNone();
        IwTimeLineKeySelection::instance()->selectFrame(frame);
      }
    }
    // ＋Ctrl
    else {
      // そのセルが未選択なら追加 選択済なら選択からはずす
      if (IwTimeLineKeySelection::instance()->isFrameSelected(frame))
        IwTimeLineKeySelection::instance()->unselectFrame(frame);
      else
        IwTimeLineKeySelection::instance()->selectFrame(frame);
    }
    IwTimeLineKeySelection::instance()->setRangeSelectionStartPos(frame);
  } else {
    IwTimeLineKeySelection::instance()->doRangeSelect(frame, ctrlPressed);
  }

  IwTimeLineKeySelection::instance()->makeCurrent();

  KeyDragTool::instance()->onClick(frame);

  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
  return true;
}

//--------------------------------------------------------
// 左クリック時の処理 rowはこのレイヤ内で上から０で始まる行数。
// 再描画が必要ならtrueを返す
//--------------------------------------------------------

bool ShapePair::onLeftClick(int row, int mouseHPos, QMouseEvent* e) {
  if (row >= getRowCount()) return false;

  int indent      = LayerIndent;
  int childIndent = (m_isParent) ? 0 : indent * 2;

  ViewSettings* vs =
      IwApp::instance()->getCurrentProject()->getProject()->getViewSettings();
  bool showAll = (!Preferences::instance()->isShowSelectedShapesOnly());

  if (row == 0) {
    // 補間コントロールの操作
    if (mouseHPos >= 180) {
      ProjectUtils::openInterpolationPopup(this, e->globalPos());
      return false;
    } else if (mouseHPos >= indent + childIndent + 23 &&
               mouseHPos < indent + childIndent + 43) {
      ProjectUtils::switchShapeVisibility(this);
      return false;
    }

    bool selectionChanged = false;

    // 選択されていない場合は選択に追加
    // Selected: ピンされている場合のみ All:常にfromTo両方
    // fromToが非表示の場合も選択されないようにする。
    if (showAll && !(e->modifiers() & Qt::ShiftModifier))
      IwShapePairSelection::instance()->selectNone();

    if ((showAll || isPinned(0)) && vs->isFromToVisible(0) &&
        !IwShapePairSelection::instance()->isSelected(OneShape(this, 0))) {
      IwShapePairSelection::instance()->addShape(OneShape(this, 0));
      selectionChanged = true;
    }
    if ((showAll || isPinned(1)) && vs->isFromToVisible(1) &&
        !IwShapePairSelection::instance()->isSelected(OneShape(this, 1))) {
      IwShapePairSelection::instance()->addShape(OneShape(this, 1));
      selectionChanged = true;
    }
    if (selectionChanged) {
      IwShapePairSelection::instance()->makeCurrent();
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
    }
    // }
    return selectionChanged;
  }
  // キーフレーム部分をクリックの場合は、FromTo片方のシェイプのみ選択
  else if (row < 5) {
    bool fromShapeIsShown = (showAll || m_isExpanded[0]);
    int fromTo            = (fromShapeIsShown && row <= 2) ? 0 : 1;

    if (mouseHPos >= LockPos && mouseHPos < PinPos) {
      ProjectUtils::switchLock(OneShape(this, fromTo));
      IwApp::instance()->getCurrentProject()->notifyProjectChanged();
      return true;
    }
    if (mouseHPos >= PinPos && mouseHPos < CorrIndent) {
      ProjectUtils::switchPin(OneShape(this, fromTo));
      IwApp::instance()->getCurrentProject()->notifyProjectChanged();
      return true;
    }

    if (!vs->isFromToVisible(fromTo) || m_isLocked[fromTo]) return false;
    IwShapePairSelection::instance()->makeCurrent();
    if (showAll && !(e->modifiers() & Qt::ShiftModifier))
      IwShapePairSelection::instance()->selectNone();

    // 上２つをクリックしたらFROM、下２つならTO
    IwShapePairSelection::instance()->addShape(OneShape(this, fromTo));

    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
    return true;
  }
  // 保険
  return false;
}

//--------------------------------------------------------
// FromシェイプをToシェイプにコピー
//--------------------------------------------------------

void ShapePair::copyFromShapeToToShape(double offset) {
  if (offset == 0.0) {
    m_formKeys[1] = m_formKeys[0];
    m_corrKeys[1] = m_corrKeys[0];
  } else {
    QMap<int, BezierPointList> formData = m_formKeys[0].getData();
    QMap<int, BezierPointList> newFormData;
    // 形状データの各キーフレームについて
    QMap<int, BezierPointList>::const_iterator i = formData.constBegin();
    while (i != formData.constEnd()) {
      int frame              = i.key();
      BezierPointList bpList = i.value();
      for (int bp = 0; bp < bpList.size(); bp++) {
        bpList[bp].pos += QPointF(offset, offset);
        bpList[bp].firstHandle += QPointF(offset, offset);
        bpList[bp].secondHandle += QPointF(offset, offset);
      }
      newFormData[frame] = bpList;
      ++i;
    }

    m_formKeys[1].setData(newFormData);
    m_corrKeys[1] = m_corrKeys[0];
  }
}

bool ShapePair::isAtLeastOneShapeExpanded() {
  return !Preferences::instance()->isShowSelectedShapesOnly() ||
         m_isExpanded[0] | m_isExpanded[1];
}

//--------------------------------------------------------
// セーブ/ロード
//--------------------------------------------------------
void ShapePair::saveData(QXmlStreamWriter& writer) {
  // シェイプ名
  writer.writeTextElement("name", m_name);
  // 対応点間を何分割するか
  writer.writeTextElement("precision", QString::number(m_precision));
  // シェイプが閉じているかどうか
  writer.writeTextElement("closed", (m_isClosed) ? "True" : "False");
  // シェイプが親かどうか
  writer.writeTextElement("parent", (m_isParent) ? "True" : "False");
  // tween （保留）
  // ベジエのポイント数
  writer.writeTextElement("bPoints", QString::number(m_bezierPointAmount));
  // 対応点の数
  writer.writeTextElement("cPoints", QString::number(m_corrPointAmount));

  // 形状データ
  writer.writeStartElement("FormKeysFrom");
  m_formKeys[0].saveData(writer);
  writer.writeEndElement();
  writer.writeStartElement("FormKeysTo");
  m_formKeys[1].saveData(writer);
  writer.writeEndElement();

  // 対応点データ
  writer.writeStartElement("CorrKeysFrom");
  m_corrKeys[0].saveData(writer);
  writer.writeEndElement();
  writer.writeStartElement("CorrKeysTo");
  m_corrKeys[1].saveData(writer);
  writer.writeEndElement();

  // ロック
  writer.writeTextElement("LockFrom", (m_isLocked[0]) ? "True" : "False");
  writer.writeTextElement("LockTo", (m_isLocked[1]) ? "True" : "False");

  // ピン
  writer.writeTextElement("PinnedFrom", (m_isPinned[0]) ? "True" : "False");
  writer.writeTextElement("PinnedTo", (m_isPinned[1]) ? "True" : "False");

  // シェイプタグ
  auto listToString = [](const QList<int> list) {
    QString ret;
    for (auto val : list) {
      ret += QString::number(val);
      ret += ",";
    }
    if (!list.isEmpty()) ret.chop(1);
    return ret;
  };
  if (!m_shapeTag[0].isEmpty())
    writer.writeTextElement("ShapeTagFrom", listToString(m_shapeTag[0]));
  if (!m_shapeTag[1].isEmpty())
    writer.writeTextElement("ShapeTagTo", listToString(m_shapeTag[1]));

  // シェイプが表示されているかどうか
  writer.writeTextElement("visible", (m_isVisible) ? "True" : "False");
  // シェイプのキー間の補間の度合い
  writer.writeTextElement("AnimationSmoothness",
                          QString::number(m_animationSmoothness));
}

//--------------------------------------------------------

void ShapePair::loadData(QXmlStreamReader& reader) {
  while (reader.readNextStartElement()) {
    // シェイプ名
    if (reader.name() == "name") m_name = reader.readElementText();
    // 対応点間を何分割するか
    else if (reader.name() == "precision")
      m_precision = reader.readElementText().toInt();

    // シェイプが閉じているかどうか
    else if (reader.name() == "closed")
      m_isClosed = (reader.readElementText() == "True") ? true : false;

    // シェイプが親かどうか
    else if (reader.name() == "parent")
      m_isParent = (reader.readElementText() == "True") ? true : false;

    // tween （保留）

    // ベジエのポイント数
    else if (reader.name() == "bPoints")
      m_bezierPointAmount = reader.readElementText().toInt();

    // 対応点の数
    else if (reader.name() == "cPoints")
      m_corrPointAmount = reader.readElementText().toInt();

    // 形状データ
    else if (reader.name() == "FormKeysFrom")
      m_formKeys[0].loadData(reader);
    else if (reader.name() == "FormKeysTo")
      m_formKeys[1].loadData(reader);

    // 対応点データ
    else if (reader.name() == "CorrKeysFrom")
      m_corrKeys[0].loadData(reader);
    else if (reader.name() == "CorrKeysTo")
      m_corrKeys[1].loadData(reader);

    // ロック
    else if (reader.name() == "LockFrom")
      m_isLocked[0] = (reader.readElementText() == "True") ? true : false;
    else if (reader.name() == "LockTo")
      m_isLocked[1] = (reader.readElementText() == "True") ? true : false;

    // ピン
    else if (reader.name() == "PinnedFrom")
      m_isPinned[0] = (reader.readElementText() == "True") ? true : false;
    else if (reader.name() == "PinnedTo")
      m_isPinned[1] = (reader.readElementText() == "True") ? true : false;

    else if (reader.name() == "ShapeTagFrom") {
      QStringList list =
          reader.readElementText().split(QLatin1Char(','), Qt::SkipEmptyParts);
      m_shapeTag[0].clear();
      for (auto valStr : list) m_shapeTag[0].append(valStr.toInt());
    } else if (reader.name() == "ShapeTagTo") {
      QStringList list =
          reader.readElementText().split(QLatin1Char(','), Qt::SkipEmptyParts);
      m_shapeTag[1].clear();
      for (auto valStr : list) m_shapeTag[1].append(valStr.toInt());
    } else if (reader.name() == "visible")
      m_isVisible = (reader.readElementText() == "True") ? true : false;
    else if (reader.name() == "AnimationSmoothness")
      m_animationSmoothness = reader.readElementText().toDouble();

    else
      reader.skipCurrentElement();
  }
}