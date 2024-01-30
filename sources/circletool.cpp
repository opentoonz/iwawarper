//--------------------------------------------------------
// Circle Tool
// 円形描画ツール
// 作成後はそのシェイプがカレントになる
// カレントシェイプの変形もできる（TransformToolと同じ機能）
//--------------------------------------------------------

#include "circletool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwselectionhandle.h"
#include "viewsettings.h"
#include "iwundomanager.h"
#include "preferences.h"

#include "transformtool.h"
#include "sceneviewer.h"

#include "iwlayerhandle.h"
#include "shapepair.h"
#include "iwlayer.h"

#include "iwshapepairselection.h"

#include <QMouseEvent>
#include <QMessageBox>

CircleTool::CircleTool() : IwTool("T_Circle"), m_startDefined(false) {}

//--------------------------------------------------------
bool CircleTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return false;
  if (layer->isLocked()) {
    QMessageBox::critical(m_viewer, tr("Critical"),
                          tr("The current layer is locked."));
    return false;
  }

  // Transformモードに入る条件
  // 何かシェイプが選択されている
  if (!IwShapePairSelection::instance()->isEmpty()) {
    // 選択シェイプのどこかをクリックしていたらTrasformToolを実行する
    IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
    if (!project) return false;

    int name = m_viewer->pick(e->pos());

    OneShape shape = layer->getShapePairFromName(name);
    if (shape.shapePairP &&
        IwShapePairSelection::instance()->isSelected(shape)) {
      return getTool("T_Transform")->leftButtonDown(pos, e);
    }
  }

  // それ以外の場合、CircleToolを実行
  m_startPos     = pos;
  m_startDefined = true;

  // シェイプの選択を解除する
  IwShapePairSelection::instance()->selectNone();

  return false;
}
//--------------------------------------------------------
bool CircleTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  // Transformモードかもしれない
  if (!m_startDefined) {
    return getTool("T_Transform")->leftButtonDrag(pos, e);
  }

  m_endPos = pos;

  return true;
}
//--------------------------------------------------------
bool CircleTool::leftButtonUp(const QPointF& pos, const QMouseEvent* e) {
  // Transformモードかもしれない
  if (!m_startDefined) {
    return getTool("T_Transform")->leftButtonUp(pos, e);
  }

  // まず、2点で作るRectを作り、正規化する
  QRectF rect(m_startPos, pos);
  rect = rect.normalized();

  // 横幅、高さが極端に小さい場合はreturn
  if (rect.width() < 0.001 || rect.height() < 0.001) {
    m_startDefined = false;
    return true;
  }

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  IwLayer* layer     = IwApp::instance()->getCurrentLayer()->getLayer();

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new CreateCircleShapeUndo(rect, project, layer));

  m_startDefined = false;

  return true;
}
//--------------------------------------------------------
void CircleTool::leftButtonDoubleClick(const QPointF&, const QMouseEvent*) {}

//--------------------------------------------------------
void CircleTool::draw() {
  if (!m_startDefined) {
    getTool("T_Transform")->draw();
    return;
  }

  // まず、2点で作るRectを作り、正規化する
  QRectF rect(m_startPos, m_endPos);
  rect = rect.normalized();

  // 横幅、高さが極端に小さい場合はreturn
  if (rect.width() < 0.001 || rect.height() < 0.001) {
    return;
  }

  GLint src, dst;
  bool isEnabled = glIsEnabled(GL_BLEND);
  glGetIntegerv(GL_BLEND_SRC, &src);
  glGetIntegerv(GL_BLEND_DST, &dst);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

  glPushMatrix();
  glColor3d(0.8, 1.0, 0.8);

  // 描いたRECTに収まるように移動
  glTranslated(rect.center().x(), rect.center().y(), 0.0);
  glScaled(rect.width() / 2.0, rect.height() / 2.0, 1.0);

  // ベジエの分割数を求める
  // 1周4セグメントなので、1ステップ当たりπ／(2*bezierPrec)
  int bezierPrec = Preferences::instance()->getBezierActivePrecision();
  // 中心(0,0), 半径1の円を描く
  glBegin(GL_LINE_LOOP);
  // 角度の変分
  double dTheta = M_PI / (double)(bezierPrec * 2);
  double theta  = 0.0;
  for (int t = 0; t < 4 * bezierPrec; t++) {
    glVertex3d(cosf(theta), sinf(theta), 0.0);
    theta += dTheta;
  }
  glEnd();

  if (!isEnabled) glDisable(GL_BLEND);
  glBlendFunc(src, dst);

  glPopMatrix();
}
//--------------------------------------------------------

int CircleTool::getCursorId(const QMouseEvent* e) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer || layer->isLocked()) {
    IwApp::instance()->showMessageOnStatusBar(
        tr("Current layer is locked or not exist."));
    return ToolCursor::ForbiddenCursor;
  }

  QString statusStr;

  if (!IwShapePairSelection::instance()->isEmpty()) {
    // TransformのカーソルIDをまず取得
    int cursorID = getTool("T_Transform")->getCursorId(e);
    // 何かハンドルをつかんでいる場合は、そのカーソルにする
    if (cursorID != ToolCursor::ArrowCursor) return cursorID;
    statusStr += tr("[Drag handles] to transform selected shape. ");
  }

  statusStr += tr("[Drag] to create a new circle shape.");
  IwApp::instance()->showMessageOnStatusBar(statusStr);
  return ToolCursor::CircleCursor;
}
//--------------------------------------------------------

CircleTool circleTool;

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

CreateCircleShapeUndo::CreateCircleShapeUndo(QRectF& rect, IwProject* prj,
                                             IwLayer* layer)
    : m_project(prj), m_layer(layer) {
  // 円に近似したシェイプを作るためのハンドル長
  double handleRatio = 0.5522847;

  // 各コントロールポイント位置
  QPointF p0(rect.left(), rect.center().y());
  QPointF p1(rect.center().x(), rect.top());
  QPointF p2(rect.right(), rect.center().y());
  QPointF p3(rect.center().x(), rect.bottom());

  // ハンドル（たて）
  QPointF vertHandle(0.0, rect.height() * 0.5 * handleRatio);
  // ハンドル（よこ）
  QPointF horizHandle(rect.width() * 0.5 * handleRatio, 0.0);

  // rectに合わせてシェイプを作る
  BezierPointList bpList;
  BezierPoint p[4] = {{p0, p0 + vertHandle, p0 - vertHandle},
                      {p1, p1 - horizHandle, p1 + horizHandle},
                      {p2, p2 - vertHandle, p2 + vertHandle},
                      {p3, p3 + horizHandle, p3 - horizHandle}};

  bpList << p[0] << p[1] << p[2] << p[3];

  CorrPointList cpList;
  CorrPoint c[4] = {{1.5, 1.0}, {2.5, 1.0}, {3.5, 1.0}, {0.5, 1.0}};
  cpList << c[0] << c[1] << c[2] << c[3];

  // 現在のフレーム
  int frame = m_project->getViewFrame();

  m_shapePair = new ShapePair(frame, true, bpList, cpList, 0.01);
  m_shapePair->setName(QObject::tr("Circle"));

  // シェイプを挿入するインデックスは、シェイプリストの最後
  m_index = m_layer->getShapePairCount();
}

CreateCircleShapeUndo::~CreateCircleShapeUndo() {
  // シェイプをdelete
  delete m_shapePair;
}

void CreateCircleShapeUndo::undo() {
  // シェイプを消去する
  m_layer->deleteShapePair(m_index);

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void CreateCircleShapeUndo::redo() {
  // シェイプをプロジェクトに追加
  m_layer->addShapePair(m_shapePair, m_index);

  // もしこれがカレントなら、
  if (m_project->isCurrent()) {
    // 作成したシェイプを選択状態にする
    IwShapePairSelection::instance()->selectOneShape(OneShape(m_shapePair, 0));
    IwShapePairSelection::instance()->addShape(OneShape(m_shapePair, 1));
    // シグナルをエミット
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}