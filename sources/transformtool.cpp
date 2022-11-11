//--------------------------------------------------------
// Transform Tool
// シェイプ変形ツール
//--------------------------------------------------------

#include "transformtool.h"
#include "sceneviewer.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwselectionhandle.h"
#include "iwproject.h"
#include "cursors.h"
#include "viewsettings.h"
#include "iwundomanager.h"

#include "iwlayerhandle.h"
#include "iwlayer.h"
#include "iwshapepairselection.h"
#include "iwtrianglecache.h"

#include <QMouseEvent>
#include <iostream>
#include <QPointF>

TransformTool::TransformTool()
    : IwTool("T_Transform")
    , m_dragTool(0)
    , m_isRubberBanding(false)
    // 以下、TransformOptionの内容
    , m_scaleAroundCenter(false)
    , m_rotateAroundCenter(true)
    , m_shapeIndependentTransforms(false)
    , m_frameIndependentTransforms(true)
    , m_shapePairSelection(IwShapePairSelection::instance())
    , m_ctrlPressed(false)
    , m_translateOnly(false) {}

//--------------------------------------------------------

TransformTool::~TransformTool() {
  // TransformDragToolを解放
  if (m_dragTool) delete m_dragTool;
}

//--------------------------------------------------------

int TransformTool::getCursorId(const QMouseEvent* e) {
  if (!m_viewer) return ToolCursor::ArrowCursor;

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) {
    IwApp::instance()->showMessageOnStatusBar(
        tr("Current layer is locked or not exist."));
    return ToolCursor::ArrowCursor;
  }

  IwApp::instance()->showMessageOnStatusBar(
      tr("[Click] to select shape. [Drag handles] to transform selected shape. "
         "[Ctrl+Arrow keys] to translate slightly."));

  if (m_shapePairSelection->isEmpty()) return ToolCursor::ArrowCursor;

  m_ctrlPressed = (e->modifiers() & Qt::ControlModifier);

  int name = m_viewer->pick(e->pos());
  // どのハンドルがクリックされたかチェックする（nameの下一桁）
  TransformHandleId handleId = (TransformHandleId)(name % 100);

  OneShape shape = layer->getShapePairFromName(name);
  // 選択シェイプじゃなかったら普通のカーソル
  if (!shape.shapePairP) {
    return ToolCursor::ArrowCursor;
  }
  if (!m_shapePairSelection->isSelected(shape)) return ToolCursor::ArrowCursor;

  if (m_translateOnly) {  // 移動モード
    IwApp::instance()->showMessageOnStatusBar(
        tr("[Drag] to translate. [+Shift] to move either horizontal or "
           "vertical."));
    return ToolCursor::SizeAllCursor;
  }

  QString statusStr;
  switch (handleId) {
  case Handle_None:
    statusStr =
        tr("[Drag] to translate. [+Shift] to move either horizontal or "
           "vertical. [Ctrl+Shift+Drag] to rotate every 45 degrees.");
    break;
  case Handle_BottomRight:
  case Handle_TopRight:
  case Handle_TopLeft:
  case Handle_BottomLeft:
    statusStr =
        tr("[Drag] to scale. [+Shift] with fixing aspect ratio. [Ctrl+Drag] to "
           "rotate. [+Shift] every 45 degrees. [Alt+Drag] to reshape "
           "trapezoidally. [+Shift] with symmetrically.");
    break;
  case Handle_Right:
  case Handle_Top:
  case Handle_Left:
  case Handle_Bottom:
    statusStr =
        tr("[Drag] to scale. [Ctrl+Drag] to rotate. [+Shift] every 45 degrees. "
           "[Alt+Drag] to shear. [+Shift] with a parallel manner.");
    break;
  case Handle_RightEdge:
  case Handle_TopEdge:
  case Handle_LeftEdge:
  case Handle_BottomEdge:
    statusStr =
        tr("[Drag] to scale. [Ctrl+Shift+Drag] to rotate every 45 degrees.");
    break;
  }
  IwApp::instance()->showMessageOnStatusBar(statusStr);

  // 押されている修飾キー／ハンドルに合わせ、カーソルIDを返す
  // 回転モード
  if (m_ctrlPressed) {
    switch (handleId) {
    case Handle_None:
      return ToolCursor::RotationCursor;
      break;
    case Handle_RightEdge:
    case Handle_LeftEdge:
      return ToolCursor::SizeHorCursor;
      break;
    case Handle_TopEdge:
    case Handle_BottomEdge:
      return ToolCursor::SizeVerCursor;
      break;
    default:
      return ToolCursor::RotationCursor;
      break;
    }
  }
  // Shear変形モード
  else if (e->modifiers() & Qt::AltModifier) {
    switch (handleId) {
    case Handle_BottomRight:
    case Handle_TopRight:
    case Handle_TopLeft:
    case Handle_BottomLeft:
      return ToolCursor::TrapezoidCursor;
      break;
    case Handle_Right:
    case Handle_Top:
    case Handle_Left:
    case Handle_Bottom:
      return ToolCursor::TrapezoidCursor;
      break;
    default:
      return ToolCursor::ArrowCursor;
      break;
    }
  }
  // 拡大縮小モード 又は 移動モード
  else {
    switch (handleId) {
    case Handle_None:  // 移動モード
      return ToolCursor::SizeAllCursor;
      break;
    case Handle_TopRight:
    case Handle_BottomLeft:
      return ToolCursor::SizeFDiagCursor;
      break;
    case Handle_BottomRight:
    case Handle_TopLeft:
      return ToolCursor::SizeBDiagCursor;
      break;
    case Handle_Right:
    case Handle_Left:
      return ToolCursor::SizeHorCursor;
      break;
    case Handle_Top:
    case Handle_Bottom:
      return ToolCursor::SizeVerCursor;
      break;
    default:
      return ToolCursor::ArrowCursor;
      break;
    }
  }
}

//--------------------------------------------------------
// ① シェイプをクリック → シェイプが選択済みなら、ドラッグ操作モードになる
//							シェイプが選択されていなければ、そのシェイプ１つだけを選択し直す
// ② シェイプをShiftクリック →
// シェイプが選択されていなければ、シェイプを追加選択 ③ シェイプをCtrlクリック
// → シェイプが選択済みなら、その選択を解除
//								 シェイプが選択されていなければ、シェイプを追加選択
// ④ 何もないところをクリック → シェイプのラバーバンド選択を始める.
//									Shiftか、Ctrlが押されていたら何もしない。
//								  普通の左クリックなら、選択を解除
bool TransformTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
  if (!m_viewer) return false;

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return false;

  int name = m_viewer->pick(e->pos());

  OneShape shape = layer->getShapePairFromName(name);

  // シェイプがクリックされていた場合
  if (shape.shapePairP) {
    // シェイプをShiftクリック →
    // シェイプが選択されていなければ、シェイプを追加選択
    if (e->modifiers() & Qt::ShiftModifier) {
      if (m_shapePairSelection->isSelected(shape)) {
        // どのハンドルがクリックされたかチェックする（nameの下一桁）
        TransformHandleId handleId = (TransformHandleId)(name % 100);
        if (m_translateOnly) handleId = Handle_None;
        // ハンドル以外のとこなら移動モード
        if (handleId == Handle_None)
          m_dragTool = new TranslateDragTool(shape, handleId,
                                             m_shapePairSelection->getShapes());
        // Ctrlクリック   → 回転モード (＋Shiftで45度ずつ)
        else if (e->modifiers() & Qt::ControlModifier)
          m_dragTool = new RotateDragTool(shape, handleId,
                                          m_shapePairSelection->getShapes(),
                                          m_viewer->getOnePixelLength());
        // Altクリック　  → Shear変形モード（＋）
        else if (e->modifiers() & Qt::AltModifier) {
          if (handleId == Handle_Right || handleId == Handle_Top ||
              handleId == Handle_Left || handleId == Handle_Bottom)
            m_dragTool = new ShearDragTool(shape, handleId,
                                           m_shapePairSelection->getShapes());
          else
            m_dragTool = new TrapezoidDragTool(
                shape, handleId, m_shapePairSelection->getShapes());
        }
        // 普通のクリック → 拡大／縮小モード
        else
          m_dragTool = new ScaleDragTool(shape, handleId,
                                         m_shapePairSelection->getShapes());

        m_dragTool->onClick(pos, e);
        return true;
      } else {
        m_shapePairSelection->makeCurrent();
        m_shapePairSelection->addShape(shape);
        // シグナルをエミット
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
        return true;
      }
    }
    // シェイプをCtrlクリック  → シェイプが選択済みなら、その選択を解除
    //							 シェイプが選択されていなければ、シェイプを追加選択
    else if (e->modifiers() & Qt::ControlModifier) {
      if (m_shapePairSelection->isSelected(shape)) {
        // ハンドルを掴んでいるときだけ回転モード
        // どのハンドルがクリックされたかチェックする（nameの下一桁）
        TransformHandleId handleId = (TransformHandleId)(name % 100);
        if (m_translateOnly) handleId = Handle_None;
        // TransformHandleId handleId = (TransformHandleId)(name%10);
        if (handleId == Handle_RightEdge || handleId == Handle_TopEdge ||
            handleId == Handle_LeftEdge || handleId == Handle_BottomEdge) {
          m_dragTool = new ScaleDragTool(shape, handleId,
                                         m_shapePairSelection->getShapes());
        } else if (m_translateOnly)
          m_dragTool = new TranslateDragTool(shape, handleId,
                                             m_shapePairSelection->getShapes());
        else {
          m_dragTool = new RotateDragTool(shape, handleId,
                                          m_shapePairSelection->getShapes(),
                                          m_viewer->getOnePixelLength());
        }
        m_dragTool->onClick(pos, e);
      } else {
        m_shapePairSelection->makeCurrent();
        m_shapePairSelection->addShape(shape);
      }
      // シグナルをエミット
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
    // シェイプをクリック → シェイプが選択済みなら、ドラッグ操作モードになる
    //						 シェイプが選択されていなければ、そのシェイプ１つだけを選択し直す
    else {
      if (m_shapePairSelection->isSelected(shape)) {
        // どのハンドルがクリックされたかチェックする（nameの下一桁）
        TransformHandleId handleId = (TransformHandleId)(name % 100);
        if (m_translateOnly) handleId = Handle_None;
        // ハンドル以外のとこなら移動モード
        if (handleId == Handle_None)
          m_dragTool = new TranslateDragTool(shape, handleId,
                                             m_shapePairSelection->getShapes());
        // Altクリック　  → Shear変形モード（＋）
        else if (e->modifiers() & Qt::AltModifier) {
          if (handleId == Handle_Right || handleId == Handle_Top ||
              handleId == Handle_Left || handleId == Handle_Bottom)
            m_dragTool = new ShearDragTool(shape, handleId,
                                           m_shapePairSelection->getShapes());
          else
            m_dragTool = new TrapezoidDragTool(
                shape, handleId, m_shapePairSelection->getShapes());
        }
        // 普通のクリック → 拡大／縮小モード
        else
          m_dragTool = new ScaleDragTool(shape, handleId,
                                         m_shapePairSelection->getShapes());

        m_dragTool->onClick(pos, e);
      } else {
        m_shapePairSelection->makeCurrent();
        m_shapePairSelection->selectOneShape(shape);
      }
      // シグナルをエミット
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
  }
  // 何も無いところをクリックした場合
  else {
    // ラバーバンド選択を始める
    m_isRubberBanding = true;
    m_rubberStartPos  = pos;
    m_currentMousePos = pos;

    // Shiftか、Ctrlが押されていたら何もしない。
    if (e->modifiers() & Qt::ShiftModifier ||
        e->modifiers() & Qt::ControlModifier)
      return false;
    // 普通の左クリックなら、選択を解除
    else {
      if (m_shapePairSelection->isEmpty())
        return false;
      else {
        m_shapePairSelection->selectNone();
        // シグナルをエミット
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
        return true;
      }
    }
  }
}
//--------------------------------------------------------
bool TransformTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  if (m_dragTool) {
    m_dragTool->onDrag(pos, e);
    return true;
  }

  // ラバーバンド選択の場合、選択Rectを更新
  if (m_isRubberBanding) {
    m_currentMousePos = pos;
    return true;
  }

  return false;
}
//--------------------------------------------------------
bool TransformTool::leftButtonUp(const QPointF& pos, const QMouseEvent* e) {
  if (m_dragTool) {
    m_dragTool->onRelease(pos, e);
    delete m_dragTool;
    m_dragTool = 0;
    return true;
  }

  // ラバーバンド選択の場合、Rectに囲まれたシェイプを追加選択
  if (m_isRubberBanding) {
    QRectF rubberBand =
        QRectF(m_rubberStartPos, m_currentMousePos).normalized();

    IwProject* project = IwApp::instance()->getCurrentProject()->getProject();

    IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();

    m_shapePairSelection->makeCurrent();
    // m_selection->makeCurrent();

    // ロック情報を得る
    bool fromToVisible[2];
    for (int fromTo = 0; fromTo < 2; fromTo++)
      fromToVisible[fromTo] =
          project->getViewSettings()->isFromToVisible(fromTo);

    // プロジェクト内の各シェイプについて、
    //  現在表示中のシェイプ、かつ未選択、かつRectのバウンディングに含まれている場合、選択する
    for (int s = 0; s < layer->getShapePairCount(); s++) {
      ShapePair* shapePair = layer->getShapePair(s);
      if (!shapePair) continue;
      if (!shapePair->isVisible()) continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // ロックされていたら飛ばす
        if (!fromToVisible[fromTo]) continue;
        if (shapePair->isLocked(fromTo)) continue;
        if (layer->isLocked())
          continue;  // レイヤーがロックされていても選択不可

        // 未選択の条件
        if (m_shapePairSelection->isSelected(OneShape(shapePair, fromTo)))
          continue;
        // バウンディングに含まれる条件
        QRectF bBox = shapePair->getBBox(project->getViewFrame(), fromTo);
        if (!rubberBand.contains(bBox)) continue;

        m_shapePairSelection->addShape(OneShape(shapePair, fromTo));
      }
    }

    // シグナルをエミット
    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();

    // ラバーバンドを解除
    m_isRubberBanding = false;

    return true;
  }

  return false;
}
//--------------------------------------------------------
void TransformTool::leftButtonDoubleClick(const QPointF&, const QMouseEvent*) {}

//--------------------------------------------------------
// Ctrl + 矢印キーで1/4ピクセル分ずつ移動
//--------------------------------------------------------
bool TransformTool::keyDown(int key, bool ctrl, bool shift, bool alt) {
  // 選択シェイプが無ければ無効
  if (m_shapePairSelection->isEmpty()) return false;

  if (ctrl && !shift && !alt &&
      (key == Qt::Key_Down || key == Qt::Key_Up || key == Qt::Key_Left ||
       key == Qt::Key_Right)) {
    QPointF delta(0, 0);
    switch (key) {
    case Qt::Key_Down:
      delta.setY(-1.0);
      break;
    case Qt::Key_Up:
      delta.setY(1.0);
      break;
    case Qt::Key_Left:
      delta.setX(-1.0);
      break;
    case Qt::Key_Right:
      delta.setX(1.0);
      break;
    }
    // 矢印キーでの移動
    arrowKeyMove(delta);
    return true;
  }

  return false;
}

//--------------------------------------------------------
// 選択されているシェイプに枠を書く ハンドルには名前をつける
//--------------------------------------------------------
void TransformTool::draw() {
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // 各選択シェイプの輪郭を描く
  for (int s = 0; s < m_shapePairSelection->getCount(); s++) {
    // シェイプを取得
    OneShape shape = m_shapePairSelection->getShape(s);

    // シェイプが見つからなかったら次へ（保険）
    if (!shape.shapePairP) continue;

    // シェイプがプロジェクトに属していなかったら次へ（保険）
    if (!layer->contains(shape.shapePairP)) continue;

    // バウンディングも、シェイプと同じ名前を付ける
    int name = layer->getNameFromShapePair(shape);

    QRectF bBox =
        shape.shapePairP->getBBox(project->getViewFrame(), shape.fromTo);

    // バウンディングは１ピクセル外側に描くので、
    // シェイプ座標系で見た目１ピクセルになる長さを取得する
    QPointF onePix = m_viewer->getOnePixelLength();
    bBox.adjust(-onePix.x(), -onePix.y(), onePix.x(), onePix.y());

    {
      glEnable(GL_LINE_STIPPLE);
      if (shape.fromTo == 0)
        glLineStipple(3, 0xFCFC);
      else
        glLineStipple(3, 0xAAAA);
    }

    glColor3d(1.0, 1.0, 1.0);

    // バウンディングを描く
    if (m_ctrlPressed) {
      drawEdgeForResize(layer, shape, Handle_RightEdge, bBox.bottomRight(),
                        bBox.topRight());
      drawEdgeForResize(layer, shape, Handle_TopEdge, bBox.topRight(),
                        bBox.topLeft());
      drawEdgeForResize(layer, shape, Handle_LeftEdge, bBox.topLeft(),
                        bBox.bottomLeft());
      drawEdgeForResize(layer, shape, Handle_BottomEdge, bBox.bottomLeft(),
                        bBox.bottomRight());
    } else {
      glPushName(name);
      glBegin(GL_LINE_LOOP);
      glVertex3d(bBox.bottomRight().x(), bBox.bottomRight().y(), 0.0);
      glVertex3d(bBox.topRight().x(), bBox.topRight().y(), 0.0);
      glVertex3d(bBox.topLeft().x(), bBox.topLeft().y(), 0.0);
      glVertex3d(bBox.bottomLeft().x(), bBox.bottomLeft().y(), 0.0);
      glEnd();
      glPopName();
    }

    // 点線を解除
    glDisable(GL_LINE_STIPPLE);

    // ハンドルを描く
    drawHandle(layer, shape, Handle_BottomRight, onePix, bBox.bottomRight());
    drawHandle(layer, shape, Handle_Right, onePix,
               QPointF(bBox.right(), bBox.center().y()));
    drawHandle(layer, shape, Handle_TopRight, onePix, bBox.topRight());
    drawHandle(layer, shape, Handle_Top, onePix,
               QPointF(bBox.center().x(), bBox.top()));
    drawHandle(layer, shape, Handle_TopLeft, onePix, bBox.topLeft());
    drawHandle(layer, shape, Handle_Left, onePix,
               QPointF(bBox.left(), bBox.center().y()));
    drawHandle(layer, shape, Handle_BottomLeft, onePix, bBox.bottomLeft());
    drawHandle(layer, shape, Handle_Bottom, onePix,
               QPointF(bBox.center().x(), bBox.bottom()));
  }

  // 次に、ラバーバンドを描画
  if (m_isRubberBanding) {
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
// ハンドルをいっこ描く
//--------------------------------------------------------
void TransformTool::drawHandle(IwLayer* layer, OneShape shape,
                               TransformHandleId handleId, QPointF& onePix,
                               QPointF& pos) {
  // 名前を作る
  int handleName = layer->getNameFromShapePair(shape);
  handleName += (int)handleId;

  glPushMatrix();
  glPushName(handleName);
  glTranslated(pos.x(), pos.y(), 0.0);
  glScaled(onePix.x(), onePix.y(), 1.0);
  glBegin(GL_LINE_LOOP);
  glVertex3d(2.0, -2.0, 0.0);
  glVertex3d(2.0, 2.0, 0.0);
  glVertex3d(-2.0, 2.0, 0.0);
  glVertex3d(-2.0, -2.0, 0.0);
  glEnd();
  glPopName();
  glPopMatrix();
}

//--------------------------------------------------------
// Ctrl押したとき辺ドラッグでサイズ変更のための辺
//--------------------------------------------------------
void TransformTool::drawEdgeForResize(IwLayer* layer, OneShape shape,
                                      TransformHandleId handleId, QPointF& p1,
                                      QPointF& p2) {
  int handleName = layer->getNameFromShapePair(shape);
  handleName += (int)handleId;
  glPushName(handleName);
  glBegin(GL_LINE_STRIP);
  glVertex3d(p1.x(), p1.y(), 0.0);
  glVertex3d(p2.x(), p2.y(), 0.0);
  glEnd();
  glPopName();
}

//--------------------------------------------------------
// Ctrl+矢印キーで0.25ピクセル分ずつ移動させる
//--------------------------------------------------------
void TransformTool::arrowKeyMove(QPointF& delta) {
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  if (m_shapePairSelection->isEmpty()) return;

  QSize workAreaSize = project->getWorkAreaSize();
  QPointF onePix(1.0 / (double)workAreaSize.width(),
                 1.0 / (double)workAreaSize.height());
  double factor = 0.25f;
  // 移動ベクタ
  QPointF moveVec(delta.x() * onePix.x() * factor,
                  delta.y() * onePix.y() * factor);

  int frame = project->getViewFrame();

  QList<BezierPointList> beforePoints;
  QList<BezierPointList> afterPoints;
  QList<bool> wasKeyFrame;

  for (int s = 0; s < m_shapePairSelection->getShapesCount(); s++) {
    OneShape shape = m_shapePairSelection->getShape(s);
    if (!shape.shapePairP) continue;

    // キーフレームだったかどうかを格納
    wasKeyFrame.append(shape.shapePairP->isFormKey(frame, shape.fromTo));

    BezierPointList bpList =
        shape.shapePairP->getBezierPointList(frame, shape.fromTo);

    beforePoints.append(bpList);

    for (int bp = 0; bp < bpList.size(); bp++) {
      bpList[bp].pos += moveVec;
      bpList[bp].firstHandle += moveVec;
      bpList[bp].secondHandle += moveVec;
    }

    afterPoints.append(bpList);

    // キーを格納
    shape.shapePairP->setFormKey(frame, shape.fromTo, bpList);
  }

  // Undoを登録
  IwUndoManager::instance()->push(
      new ArrowKeyMoveUndo(m_shapePairSelection->getShapes(), beforePoints,
                           afterPoints, wasKeyFrame, project, layer, frame));

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//--------------------------------------------------------
// 以下、Undoコマンド
//--------------------------------------------------------

//--------------------------------------------------------
// Ctrl+矢印キーで0.25ピクセル分ずつ移動させる のUndo
//--------------------------------------------------------
ArrowKeyMoveUndo::ArrowKeyMoveUndo(QList<OneShape>& shapes,
                                   QList<BezierPointList>& beforePoints,
                                   QList<BezierPointList>& afterPoints,
                                   QList<bool>& wasKeyFrame, IwProject* project,
                                   IwLayer* layer, int frame)
    : m_shapes(shapes)
    , m_beforePoints(beforePoints)
    , m_afterPoints(afterPoints)
    , m_wasKeyFrame(wasKeyFrame)
    , m_project(project)
    , m_layer(layer)
    , m_frame(frame)
    , m_firstFlag(true) {}

void ArrowKeyMoveUndo::undo() {
  // 各シェイプにポイントをセット
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    if (!shape.shapePairP) continue;

    // 操作前はキーフレームでなかった場合、キーを消去
    if (!m_wasKeyFrame.at(s))
      shape.shapePairP->removeFormKey(m_frame, shape.fromTo);
    // それ以外の場合はポイントを差し替え
    else
      shape.shapePairP->setFormKey(m_frame, shape.fromTo, m_beforePoints.at(s));
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();

    for (auto shape : m_shapes) {
      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }
}

void ArrowKeyMoveUndo::redo() {
  // Undo登録時にはredoを行わないよーにする
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  // 各シェイプにポイントをセット
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    if (!shape.shapePairP) continue;

    // ポイントを差し替え
    shape.shapePairP->setFormKey(m_frame, shape.fromTo, m_afterPoints.at(s));
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();

    for (auto shape : m_shapes) {
      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }
}

TransformTool transformTool;