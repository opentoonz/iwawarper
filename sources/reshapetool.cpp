//--------------------------------------------------------
// Reshape Tool
// コントロールポイント編集ツール
//--------------------------------------------------------

#include "reshapetool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwselectionhandle.h"
#include "sceneviewer.h"
#include "pointdragtool.h"
#include "cursors.h"
#include "iwundomanager.h"
#include "iwproject.h"

#include "transformtool.h"
#include "reshapetoolcontextmenu.h"
#include "iwtrianglecache.h"

#include <QMouseEvent>

#include "iwshapepairselection.h"
#include "iwlayer.h"
#include "iwlayerhandle.h"

ReshapeTool::ReshapeTool()
    : IwTool("T_Reshape")
    , m_selection(IwShapePairSelection::instance())
    , m_dragTool(0)
    , m_isRubberBanding(false) {}

//--------------------------------------------------------

ReshapeTool::~ReshapeTool() {}

//--------------------------------------------------------

bool ReshapeTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
  if (!m_viewer) return false;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return false;

  // クリック位置から、操作対象となるシェイプ／ハンドルを得る
  int pointIndex, handleId;
  OneShape shape = getClickedShapeAndIndex(pointIndex, handleId, e);

  // シェイプをクリックした場合
  if (shape.shapePairP) {
    // ポイントをクリックしたらポイントを選択する
    if (handleId == 1) {
      m_selection->makeCurrent();
      if (!m_selection->isPointActive(shape, pointIndex) &&
          !(e->modifiers() & Qt::ShiftModifier))
        m_selection->selectNone();
      m_selection->activatePoint(shape, pointIndex);
      // m_selection->activatePoint(selectingName);
      QPointF grabbedPointOrgPos =
          shape.getBezierPointList(project->getViewFrame()).at(pointIndex).pos;
      m_dragTool = new TranslatePointDragTool(
          m_selection->getActivePointSet(), grabbedPointOrgPos,
          m_viewer->getOnePixelLength(), shape, pointIndex);
      m_dragTool->onClick(pos, e);

      // シグナルをエミット
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
    // ハンドルをクリックしたらそのポイントだけを選択にする
    else if (handleId == 2 || handleId == 3) {
      m_selection->makeCurrent();
      m_selection->selectNone();
      m_selection->activatePoint(shape, pointIndex);

      m_dragTool = new TranslateHandleDragTool(shape, pointIndex, handleId,
                                               m_viewer->getOnePixelLength());
      m_dragTool->onClick(pos, e);

      // シグナルをエミット
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
    // シェイプをクリックしたらアクティブに
    else {
      m_selection->makeCurrent();

      // アクティブなシェイプをAltクリックした場合、コントロールポイントを追加する
      if ((e->modifiers() & Qt::AltModifier) &&
          m_selection->isSelected(shape)) {
        addControlPoint(shape, pos);
        IwTriangleCache::instance()->invalidateShapeCache(
            project->getParentShape(shape.shapePairP));
        return true;
      }

      else if (e->modifiers() & Qt::ShiftModifier) {
        m_selection->addShape(shape);
        // シグナルをエミット
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
        return true;
      }
      m_selection->selectOneShape(shape);
      // シグナルをエミット
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
  }
  // 何も無いところをクリックした場合
  else {
    if (m_selection->isEmpty())
      return false;
    else {
      // ラバーバンド選択を始める
      m_isRubberBanding = true;
      m_rubberStartPos  = pos;
      m_currentMousePos = pos;
      // ラバーバンドは、選択中のシェイプのポイントだけアクティブにできる

      //+shiftで追加選択
      if (!(e->modifiers() & Qt::ShiftModifier))
        m_selection->deactivateAllPoints();

      // シグナルをエミット
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
  }
}

//--------------------------------------------------------

bool ReshapeTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  if (m_dragTool) {
    m_dragTool->onDrag(pos, e);
    return true;
  }

  if (m_isRubberBanding) {
    m_currentMousePos = pos;
    return true;
  }

  return false;
}

//--------------------------------------------------------

bool ReshapeTool::leftButtonUp(const QPointF& pos, const QMouseEvent* e) {
  if (m_dragTool) {
    bool dragMoved = m_dragTool->onRelease(pos, e);
    delete m_dragTool;
    m_dragTool = 0;

    // その場でクリック→リリースした場合、そのポイントの単独選択
    if (!dragMoved) {
      // クリック位置から、操作対象となるシェイプ／ハンドルを得る
      int pointIndex, handleId;
      OneShape shape = getClickedShapeAndIndex(pointIndex, handleId, e);
      if (shape.shapePairP && handleId == 1 &&
          !(e->modifiers() & Qt::ShiftModifier)) {
        m_selection->selectNone();
        m_selection->activatePoint(shape, pointIndex);
        // シグナルをエミット
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      }
    }

    return true;
  }

  if (!m_selection->isEmpty()) {
    IwProject* project = IwApp::instance()->getCurrentProject()->getProject();

    IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
    if (!layer) return false;

    // ラバーバンド選択の場合、Rectに囲まれたポイントを追加選択
    if (isRubberBandValid()) {
      // 選択中のシェイプで、ラバーバンドに含まれているCPを追加していく
      QRectF rubberBand =
          QRectF(m_rubberStartPos, m_currentMousePos).normalized();
      int frame = project->getViewFrame();

      bool somethingSelected = false;

      QList<OneShape> shapes = m_selection->getShapes();
      // QList<IwShape*> shapes = m_selection->getShapes();
      for (int s = 0; s < shapes.size(); s++) {
        OneShape shape = shapes.at(s);
        if (!shape.shapePairP) continue;

        BezierPointList bPList = shape.getBezierPointList(frame);
        for (int p = 0; p < bPList.size(); p++) {
          BezierPoint bp = bPList.at(p);
          // バウンディングに含まれる条件
          if (rubberBand.contains(bp.pos)) {
            m_selection->activatePoint(shape, p);
            somethingSelected = true;
          }
        }
      }

      if (somethingSelected)
        // シグナルをエミット
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      // ラバーバンドを解除
      m_isRubberBanding = false;

      return true;
    }
    // ラバーバンド選択でなく、シェイプが選択されている場合、
    // かつ、マウス下になにもシェイプが無い場合、
    // マウスリリース時にシェイプ選択を解除する
    else {
      // ラバーバンドを解除
      m_isRubberBanding = false;

      int name       = m_viewer->pick(e->pos());
      OneShape shape = layer->getShapePairFromName(name);
      if (!shape.shapePairP) {
        m_selection->selectNone();
        // シグナルをエミット
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
        return true;
      }
    }
  }

  return false;
}

//--------------------------------------------------------

void ReshapeTool::leftButtonDoubleClick(const QPointF&, const QMouseEvent*) {}

//--------------------------------------------------------
// 右クリックメニュー
//--------------------------------------------------------
bool ReshapeTool::rightButtonDown(const QPointF&, const QMouseEvent* e,
                                  bool& canOpenMenu, QMenu& menu) {
  // クリック位置から、操作対象となるシェイプ／ハンドルを得る
  int pointIndex, handleId;
  OneShape shape = getClickedShapeAndIndex(pointIndex, handleId, e);

  ReshapeToolContextMenu::instance()->init(m_selection,
                                           m_viewer->getOnePixelLength());
  ReshapeToolContextMenu::instance()->openMenu(e, shape, pointIndex, handleId,
                                               menu);
  return true;
}

//--------------------------------------------------------
// 矢印キーで1ピクセル分ずつ移動。+Shiftで10、+Ctrlで1/10
//--------------------------------------------------------
bool ReshapeTool::keyDown(int key, bool ctrl, bool shift, bool alt) {
  // 選択シェイプが無ければ無効
  if (m_selection->isEmpty()) return false;

  if (key == Qt::Key_Down || key == Qt::Key_Up || key == Qt::Key_Left ||
      key == Qt::Key_Right) {
    QPointF delta(0, 0);
    double distance = (shift) ? 10.0 : (ctrl) ? 0.1 : 1.0;
    switch (key) {
    case Qt::Key_Down:
      delta.setY(-distance);
      break;
    case Qt::Key_Up:
      delta.setY(distance);
      break;
    case Qt::Key_Left:
      delta.setX(-distance);
      break;
    case Qt::Key_Right:
      delta.setX(distance);
      break;
    }
    // 矢印キーでの移動
    arrowKeyMove(delta);
    return true;
  }

  return false;
}

//--------------------------------------------------------

bool ReshapeTool::setSpecialShapeColor(OneShape shape) {
  if (m_dragTool) return m_dragTool->setSpecialShapeColor(shape);
  return false;
}

bool ReshapeTool::setSpecialGridColor(int gId, bool isVertical) {
  if (m_dragTool) return m_dragTool->setSpecialGridColor(gId, isVertical);
  return false;
}

//--------------------------------------------------------
// アクティブなシェイプにコントロールポイントを描く
// 選択ポイントは色を変え、ハンドルも描く
//--------------------------------------------------------
void ReshapeTool::draw() {
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // 選択シェイプが無かったらreturn
  if (m_selection->isEmpty()) return;

  // 表示フレームを得る
  int frame = project->getViewFrame();

  // コントロールポイントの色を得ておく
  double cpColor[3], cpSelected[3], cpInbetween[3];
  ColorSettings::instance()->getColor(cpColor, Color_CtrlPoint);
  ColorSettings::instance()->getColor(cpSelected, Color_ActiveCtrl);
  ColorSettings::instance()->getColor(cpInbetween, Color_InbetweenCtrl);

  QList<OneShape> shapes = m_selection->getShapes();

  // 各アクティブなシェイプについて
  for (int s = 0; s < shapes.size(); s++) {
    OneShape shape = shapes.at(s);

    // それぞれのポイントについて、選択されていたらハンドル付きで描画
    // 選択されていなければ普通に四角を描く
    BezierPointList bPList =
        shape.shapePairP->getBezierPointList(frame, shape.fromTo);
    for (int p = 0; p < bPList.size(); p++) {
      // 選択されている場合
      if (m_selection->isPointActive(shape, p)) {
        // ハンドル付きでコントロールポイントを描画する
        glColor3dv(cpSelected);
        ReshapeTool::drawControlPoint(shape, bPList, p, true,
                                      m_viewer->getOnePixelLength());
      }
      // 選択されていない場合
      else {
        // 単にコントロールポイントを描画する
        // カレントフレームがキーフレームかどうかで表示を分ける
        glColor3dv(shape.shapePairP->isFormKey(frame, shape.fromTo)
                       ? cpColor
                       : cpInbetween);
        ReshapeTool::drawControlPoint(shape, bPList, p, false,
                                      m_viewer->getOnePixelLength());
      }
    }
    //	++i;
  }

  if (m_dragTool) m_dragTool->draw(m_viewer->getOnePixelLength());

  // 次に、ラバーバンドを描画
  if (isRubberBandValid()) {
    // Rectを得る
    QRectF rubberBand =
        QRectF(m_rubberStartPos, m_currentMousePos).normalized();
    // 点線で描画
    glColor3d(1.0, 1.0, 1.0);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(3, 0xAAAA);

    glBegin(GL_LINE_LOOP);

    glVertex3d(rubberBand.bottomRight().x(), rubberBand.bottomRight().y(), 0.0);
    glVertex3d(rubberBand.topRight().x(), rubberBand.topRight().y(), 0.0);
    glVertex3d(rubberBand.topLeft().x(), rubberBand.topLeft().y(), 0.0);
    glVertex3d(rubberBand.bottomLeft().x(), rubberBand.bottomLeft().y(), 0.0);

    glEnd();

    // 点線を解除
    glDisable(GL_LINE_STIPPLE);
  }
}

//--------------------------------------------------------

int ReshapeTool::getCursorId(const QMouseEvent* e) {
  if (!m_viewer) return ToolCursor::ForbiddenCursor;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return ToolCursor::ForbiddenCursor;

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) {
    IwApp::instance()->showMessageOnStatusBar(
        tr("Current layer is locked or not exist."));
    return ToolCursor::ForbiddenCursor;
  }

  int name = m_viewer->pick(e->pos());

  // ハンドルがクリックされたかチェックする（nameの下一桁）
  int handleId = name % 10;
  if (handleId) {
    if (handleId == 1)  // over point
      IwApp::instance()->showMessageOnStatusBar(
          tr("[Drag points] to move. [+Shift] to move either horizontal or "
             "vertical. [+Ctrl] with snapping. [+Ctrl+Alt] with snapping "
             "points and handles."));
    else  // over handles
      IwApp::instance()->showMessageOnStatusBar(
          tr("[Drag handles] to move both handles. [+Shift] to extend with "
             "keeping angle. [+Ctrl] to move the other handle symmetrically. "
             "[+Alt] move one handle. [+Ctrl+Alt] with snapping."));

    return ToolCursor::BlackArrowCursor;
  }

  IwApp::instance()->showMessageOnStatusBar(
      tr("[Click points / Drag area] to select. [+Shift] to add to the "
         "selection. [Alt + Click shape] to insert a new point. [Arrow keys] "
         "to move. [+Shift] with large step. [+Ctrl] with small step."));

  // コントロールポイント追加モードの条件
  OneShape shape = layer->getShapePairFromName(name);
  if (shape.shapePairP && (e->modifiers() & Qt::AltModifier) &&
      m_selection->isSelected(shape))
    return ToolCursor::BlackArrowAddCursor;

  return ToolCursor::ArrowCursor;
}

//--------------------------------------------------------
// コントロールポイントを描画する。ハンドルも付けるかどうかも引数で決める
//--------------------------------------------------------

void ReshapeTool::drawControlPoint(OneShape shape, BezierPointList& pointList,
                                   int cpIndex, bool drawHandles,
                                   QPointF& onePix, int specialShapeIndex,
                                   bool fillPoint) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;
  BezierPoint bPoint = pointList.at(cpIndex);

  // ハンドルを描く場合、CPとハンドルを繋ぐ直線を描く
  if (drawHandles) {
    glBegin(GL_LINE_STRIP);
    glVertex3d(bPoint.firstHandle.x(), bPoint.firstHandle.y(), 0.0);
    glVertex3d(bPoint.pos.x(), bPoint.pos.y(), 0.0);
    glVertex3d(bPoint.secondHandle.x(), bPoint.secondHandle.y(), 0.0);
    glEnd();
  }

  // 名前を付ける
  int name;
  if (specialShapeIndex == 0)
    name = layer->getNameFromShapePair(shape) + cpIndex * 10 +
           1;  // 1はコントロールポイントの印
  else
    name = specialShapeIndex * 10000 + cpIndex * 10 +
           1;  // 1はコントロールポイントの印

  // コントロールポイントを描く
  glPushMatrix();
  glPushName(name);

  glTranslated(bPoint.pos.x(), bPoint.pos.y(), 0.0);
  glScaled(onePix.x(), onePix.y(), 1.0);
  glBegin((fillPoint) ? GL_POLYGON : GL_LINE_LOOP);
  glVertex3d(2.0, -2.0, 0.0);
  glVertex3d(2.0, 2.0, 0.0);
  glVertex3d(-2.0, 2.0, 0.0);
  glVertex3d(-2.0, -2.0, 0.0);
  glEnd();

  glPopName();
  glPopMatrix();

  if (!drawHandles) return;

  //--- ここからハンドルを書く

  // firstHandle
  name += 1;
  glPushMatrix();
  glPushName(name);

  glTranslated(bPoint.firstHandle.x(), bPoint.firstHandle.y(), 0.0);
  glScaled(onePix.x(), onePix.y(), 1.0);

  ReshapeTool::drawCircle();

  glPopName();
  glPopMatrix();

  // secondHandle
  name += 1;
  glPushMatrix();
  glPushName(name);

  glTranslated(bPoint.secondHandle.x(), bPoint.secondHandle.y(), 0.0);
  glScaled(onePix.x(), onePix.y(), 1.0);

  ReshapeTool::drawCircle();

  glPopName();
  glPopMatrix();
}

//--------------------------------------------------------
// ハンドル用の円を描く
//--------------------------------------------------------
void ReshapeTool::drawCircle() {
  double theta;
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < 12; i++) {
    theta = (double)i * M_PI / 6.0;

    glVertex3d(2.0 * cosf(theta), 2.0 * sinf(theta), 0.0);
  }
  glEnd();
}

//--------------------------------------------------------
// アクティブなシェイプをAltクリックした場合、コントロールポイントを追加する
//--------------------------------------------------------
void ReshapeTool::addControlPoint(OneShape shape, const QPointF& pos) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // 表示フレームを得る
  int frame = project->getViewFrame();

  // Undoを作る
  AddControlPointUndo* undo = new AddControlPointUndo(
      shape.shapePairP->getFormData(0), shape.shapePairP->getCorrData(0),
      shape.shapePairP->getFormData(1), shape.shapePairP->getCorrData(1), shape,
      project, layer);

  // 指定位置にコントロールポイントを追加する
  int pointIndex = shape.shapePairP->addControlPoint(pos, frame, shape.fromTo);
  // shape->addControlPoint(pos, frame);を選択する

  undo->setAfterData(
      shape.shapePairP->getFormData(0), shape.shapePairP->getCorrData(0),
      shape.shapePairP->getFormData(1), shape.shapePairP->getCorrData(1));

  // Undoに登録 同時にredoが呼ばれるが、最初はフラグで回避する
  IwUndoManager::instance()->push(undo);

  // 作成したポイントを選択
  m_selection->makeCurrent();
  m_selection->selectNone();
  m_selection->activatePoint(shape, pointIndex);
  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();

  IwTriangleCache::instance()->invalidateShapeCache(
      project->getParentShape(shape.shapePairP));
}

//--------------------------------------------------------
// Ctrl+矢印キーで0.25ピクセル分ずつ移動させる
//--------------------------------------------------------
void ReshapeTool::arrowKeyMove(QPointF& delta) {
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  if (m_selection->isEmpty()) return;
  if (m_selection->getActivePointSet().isEmpty()) return;

  QSize workAreaSize = project->getWorkAreaSize();
  QPointF onePix(1.0 / (double)workAreaSize.width(),
                 1.0 / (double)workAreaSize.height());
  // 移動ベクタ
  QPointF moveVec(delta.x() * onePix.x(), delta.y() * onePix.y());

  int frame = project->getViewFrame();

  QList<BezierPointList> beforePoints;
  QList<BezierPointList> afterPoints;
  QList<bool> wasKeyFrame;

  for (int s = 0; s < m_selection->getShapesCount(); s++) {
    OneShape shape = m_selection->getShape(s);
    if (!shape.shapePairP) continue;

    // キーフレームだったかどうかを格納
    wasKeyFrame.append(shape.shapePairP->isFormKey(frame, shape.fromTo));

    BezierPointList bpList =
        shape.shapePairP->getBezierPointList(frame, shape.fromTo);

    beforePoints.append(bpList);

    for (int bp = 0; bp < bpList.size(); bp++) {
      // Pointがアクティブなら移動
      if (!m_selection->isPointActive(shape, bp)) continue;
      bpList[bp].pos += moveVec;
      bpList[bp].firstHandle += moveVec;
      bpList[bp].secondHandle += moveVec;
    }

    afterPoints.append(bpList);

    // キーを格納
    shape.shapePairP->setFormKey(frame, shape.fromTo, bpList);
  }

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------

bool ReshapeTool::isRubberBandValid() {
  if (!m_isRubberBanding) return false;

  QPointF mouseMov = m_currentMousePos - m_rubberStartPos;
  QPointF onePix   = m_viewer->getOnePixelLength();
  mouseMov.setX(mouseMov.x() / onePix.x());
  mouseMov.setY(mouseMov.y() / onePix.y());
  // ラバーバンド選択がある程度の大きさなら有効
  return (mouseMov.manhattanLength() > 4);
}

//---------------------------------------------------
// クリック位置から、操作対象となるシェイプ／ハンドルを得る
//---------------------------------------------------
OneShape ReshapeTool::getClickedShapeAndIndex(int& pointIndex, int& handleId,
                                              const QMouseEvent* e) {
  if (!m_viewer) return OneShape();
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return OneShape();

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return OneShape();

  pointIndex = 0;
  handleId   = 0;

  QList<int> nameList = m_viewer->pickAll(e->pos());

  // 何もつかんでいなければreturn
  if (nameList.isEmpty()) return OneShape();

  struct ClickedPointInfo {
    OneShape s;
    int pId;
    int hId;
  };

  // つかんだ対象ごとにリストをつくる
  QList<ClickedPointInfo> cpList, shapeList, handleList;

  // つかんだアイテムを仕分けていく
  for (int i = 0; i < nameList.size(); i++) {
    int name          = nameList.at(i);
    OneShape tmpShape = layer->getShapePairFromName(name);
    if (!tmpShape.shapePairP) continue;

    int tmp_pId = (int)((name % 10000) / 10);
    int tmp_hId = name % 10;

    ClickedPointInfo clickedInfo = {tmpShape, tmp_pId, tmp_hId};
    // ハンドルに合わせてリストを分けて格納
    if (tmp_hId == 1)  // ポイントの場合
      cpList.append(clickedInfo);
    else if (tmp_hId == 2 || tmp_hId == 3)
      handleList.append(clickedInfo);
    else
      shapeList.append(clickedInfo);
  }
  // 全てのリストが空ならreturn
  if (cpList.isEmpty() && shapeList.isEmpty() && handleList.isEmpty()) return 0;

  ClickedPointInfo retInfo;  // 最終的に返すクリック情報
  // 修飾キーによって、ポイント／ハンドルどちらを優先してつかむかを判断する
  //  +Ctrl, +Alt の場合はハンドルを優先
  if (e->modifiers() & Qt::ControlModifier ||
      e->modifiers() & Qt::AltModifier) {
    if (!handleList.isEmpty())
      retInfo = handleList.first();
    else if (!cpList.isEmpty())
      retInfo = cpList.first();
    else
      retInfo = shapeList.first();
  }
  // そうでない場合は、コントロールポイントを優先して掴む
  else {
    if (!cpList.isEmpty())
      retInfo = cpList.first();
    else if (!handleList.isEmpty())
      retInfo = handleList.first();
    else
      retInfo = shapeList.first();
  }

  pointIndex = retInfo.pId;
  handleId   = retInfo.hId;

  return retInfo.s;
}

//---------------------------------------------------
//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------
AddControlPointUndo::AddControlPointUndo(
    QMap<int, BezierPointList>& beforeFormDataFrom,
    QMap<int, CorrPointList>& beforeCorrDataFrom,
    QMap<int, BezierPointList>& beforeFormDataTo,
    QMap<int, CorrPointList>& beforeCorrDataTo, OneShape shape,
    IwProject* project, IwLayer* layer)
    : m_project(project), m_shape(shape), m_layer(layer), m_firstFlag(true) {
  m_beforeFormData[0] = beforeFormDataFrom;
  m_beforeFormData[1] = beforeFormDataTo;
  m_beforeCorrData[0] = beforeCorrDataFrom;
  m_beforeCorrData[1] = beforeCorrDataTo;
}

void AddControlPointUndo::setAfterData(
    QMap<int, BezierPointList>& afterFormDataFrom,
    QMap<int, CorrPointList>& afterCorrDataFrom,
    QMap<int, BezierPointList>& afterFormDataTo,
    QMap<int, CorrPointList>& afterCorrDataTo) {
  m_afterFormData[0] = afterFormDataFrom;
  m_afterFormData[1] = afterFormDataTo;
  m_afterCorrData[0] = afterCorrDataFrom;
  m_afterCorrData[1] = afterCorrDataTo;
}

void AddControlPointUndo::undo() {
  for (int fromTo = 0; fromTo < 2; fromTo++) {
    // シェイプに形状データを戻す
    m_shape.shapePairP->setFormData(m_beforeFormData[fromTo], fromTo);
    // シェイプに対応点データを戻す
    m_shape.shapePairP->setCorrData(m_beforeCorrData[fromTo], fromTo);
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

void AddControlPointUndo::redo() {
  // Undo登録時にはredoを行わないよーにする
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  for (int fromTo = 0; fromTo < 2; fromTo++) {
    // シェイプに形状データを戻す
    m_shape.shapePairP->setFormData(m_afterFormData[fromTo], fromTo);
    // シェイプに対応点データを戻す
    m_shape.shapePairP->setCorrData(m_afterCorrData[fromTo], fromTo);
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

ReshapeTool reshapeTool;