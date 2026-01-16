//--------------------------------------------------------
// Square Tool
// 矩形描画ツール
// 作成後はそのシェイプがカレントになる
// カレントシェイプの変形もできる（TransformToolと同じ機能）
//--------------------------------------------------------

#include "squaretool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwselectionhandle.h"
#include "viewsettings.h"
#include "iwundomanager.h"

#include "transformtool.h"
#include "sceneviewer.h"

#include "iwlayerhandle.h"
#include "shapepair.h"
#include "iwlayer.h"

#include "iwshapepairselection.h"

#include <QMouseEvent>
#include <QMessageBox>

SquareTool::SquareTool() : IwTool("T_Square"), m_startDefined(false) {}

//--------------------------------------------------------
bool SquareTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
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

  // それ以外の場合、SquareToolを実行
  m_startPos     = pos;
  m_startDefined = true;

  // シェイプの選択を解除する
  IwShapePairSelection::instance()->selectNone();

  return false;
}
//--------------------------------------------------------
bool SquareTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  // Transformモードかもしれない
  if (!m_startDefined) {
    return getTool("T_Transform")->leftButtonDrag(pos, e);
  }

  m_endPos = pos;

  return true;
}
//--------------------------------------------------------
bool SquareTool::leftButtonUp(const QPointF& pos, const QMouseEvent* e) {
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
      new CreateSquareShapeUndo(rect, project, layer));

  m_startDefined = false;

  return true;
}
//--------------------------------------------------------
void SquareTool::leftButtonDoubleClick(const QPointF&, const QMouseEvent*) {}
//--------------------------------------------------------
void SquareTool::draw() {
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

  m_viewer->pushMatrix();
  m_viewer->setColor(QColor::fromRgbF(0.8, 1.0, 0.8));

  QVector3D vert[4] = {QVector3D(rect.bottomRight()),
                       QVector3D(rect.topRight()), QVector3D(rect.topLeft()),
                       QVector3D(rect.bottomLeft())};

  m_viewer->doDrawLine(GL_LINE_LOOP, vert, 4);

  m_viewer->popMatrix();

  if (!isEnabled) glDisable(GL_BLEND);
  glBlendFunc(src, dst);
}
//--------------------------------------------------------

int SquareTool::getCursorId(const QMouseEvent* e) {
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

  statusStr += tr("[Drag] to create a new square shape.");
  IwApp::instance()->showMessageOnStatusBar(statusStr);
  return ToolCursor::SquareCursor;
}
//--------------------------------------------------------

SquareTool squareTool;

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

CreateSquareShapeUndo::CreateSquareShapeUndo(QRectF& rect, IwProject* prj,
                                             IwLayer* layer)
    : m_project(prj), m_layer(layer) {
  // rectに合わせてシェイプを作る
  BezierPointList bpList;
  BezierPoint p[4] = {
      {rect.topRight(), rect.topRight(), rect.topRight()},
      {rect.bottomRight(), rect.bottomRight(), rect.bottomRight()},
      {rect.bottomLeft(), rect.bottomLeft(), rect.bottomLeft()},
      {rect.topLeft(), rect.topLeft(), rect.topLeft()}};

  bpList << p[0] << p[1] << p[2] << p[3];

  CorrPointList cpList;
  CorrPoint c[4] = {{0., 1., 1.}, {1., 1., 1.}, {2., 1., 1.}, {3., 1., 1.}};
  cpList << c[0] << c[1] << c[2] << c[3];

  // 現在のフレーム
  int frame = m_project->getViewFrame();

  m_shapePair = new ShapePair(frame, true, bpList, cpList, 0.01);
  m_shapePair->setName(QObject::tr("Rectangle"));

  // シェイプを挿入するインデックスは、シェイプリストの最後
  m_index = m_layer->getShapePairCount();
}

CreateSquareShapeUndo::~CreateSquareShapeUndo() {
  // シェイプをdelete
  delete m_shapePair;
}

void CreateSquareShapeUndo::undo() {
  // シェイプを消去する
  m_layer->deleteShapePair(m_index);

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void CreateSquareShapeUndo::redo() {
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