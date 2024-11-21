//--------------------------------------------------------
// Reshape Tool
// �R���g���[���|�C���g�ҏW�c�[��
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

namespace {

void drawEdgeForResize(SceneViewer* viewer, TransformHandleId handleId,
                       const QPointF& p1, const QPointF& p2) {
  glPushName((int)handleId);

  QVector3D vert[2] = {QVector3D(p1), QVector3D(p2)};
  viewer->doDrawLine(GL_LINE_STRIP, vert, 2);

  glPopName();
}
void drawHandle(SceneViewer* viewer, TransformHandleId handleId,
                const QPointF& onePix, const QPointF& pos) {
  viewer->pushMatrix();
  glPushName((int)handleId);
  viewer->translate(pos.x(), pos.y(), 0.0);
  viewer->scale(onePix.x(), onePix.y(), 1.0);

  static QVector3D vert[4] = {
      QVector3D(2.0, -2.0, 0.0), QVector3D(2.0, 2.0, 0.0),
      QVector3D(-2.0, 2.0, 0.0), QVector3D(-2.0, -2.0, 0.0)};
  viewer->doDrawLine(GL_LINE_LOOP, vert, 4);

  glPopName();
  viewer->popMatrix();
}

}  // namespace

ReshapeTool::ReshapeTool()
    : IwTool("T_Reshape")
    , m_selection(IwShapePairSelection::instance())
    , m_dragTool(0)
    , m_isRubberBanding(false)
    , m_transformHandles(false) {}

//--------------------------------------------------------

ReshapeTool::~ReshapeTool() {}

//--------------------------------------------------------

bool ReshapeTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
  if (!m_viewer) return false;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return false;

  // �N���b�N�ʒu����A����ΏۂƂȂ�V�F�C�v�^�n���h���𓾂�
  int pointIndex, handleId;
  OneShape shape = getClickedShapeAndIndex(pointIndex, handleId, e);

  // �V�F�C�v���N���b�N�����ꍇ
  if (shape.shapePairP) {
    // �|�C���g���N���b�N������|�C���g��I������
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

      // �V�O�i�����G�~�b�g
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
    // �n���h�����N���b�N�����炻�̃|�C���g������I���ɂ���
    else if (handleId == 2 || handleId == 3) {
      m_selection->makeCurrent();
      m_selection->selectNone();
      m_selection->activatePoint(shape, pointIndex);

      m_dragTool = new TranslateHandleDragTool(shape, pointIndex, handleId,
                                               m_viewer->getOnePixelLength());
      m_dragTool->onClick(pos, e);

      // �V�O�i�����G�~�b�g
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
    // �V�F�C�v���N���b�N������A�N�e�B�u��
    else {
      m_selection->makeCurrent();

      // �A�N�e�B�u�ȃV�F�C�v��Alt�N���b�N�����ꍇ�A�R���g���[���|�C���g��ǉ�����
      if ((e->modifiers() & Qt::AltModifier) &&
          m_selection->isSelected(shape)) {
        addControlPoint(shape, pos);
        IwTriangleCache::instance()->invalidateShapeCache(
            project->getParentShape(shape.shapePairP));
        return true;
      }

      else if (e->modifiers() & Qt::ShiftModifier) {
        m_selection->addShape(shape);
        // �V�O�i�����G�~�b�g
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
        return true;
      }
      m_selection->selectOneShape(shape);
      // �V�O�i�����G�~�b�g
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
  } else if (m_transformHandles && handleId > 0) {
    TransformHandleId tHandleId = (TransformHandleId)handleId;
    if (e->modifiers() & Qt::ControlModifier) {
      if (tHandleId == Handle_RightEdge || tHandleId == Handle_TopEdge ||
          tHandleId == Handle_LeftEdge || tHandleId == Handle_BottomEdge) {
        m_dragTool = new ActivePointsRotateDragTool(
            tHandleId, m_selection->getActivePointSet(), m_handleRect,
            m_viewer->getOnePixelLength());
      } else {
        m_dragTool = new ActivePointsScaleDragTool(
            tHandleId, m_selection->getActivePointSet(), m_handleRect);
      }
    } else {
      if (tHandleId == Handle_RightEdge || tHandleId == Handle_TopEdge ||
          tHandleId == Handle_LeftEdge || tHandleId == Handle_BottomEdge)
        m_dragTool = new ActivePointsTranslateDragTool(
            tHandleId, m_selection->getActivePointSet(), m_handleRect);
      // Alt�N���b�N�@  �� Shear�ό`���[�h�i�{�j
      else if (e->modifiers() & Qt::AltModifier) {
        if (tHandleId == Handle_Right || tHandleId == Handle_Top ||
            tHandleId == Handle_Left || tHandleId == Handle_Bottom)
          m_dragTool = new ActivePointsShearDragTool(
              tHandleId, m_selection->getActivePointSet(), m_handleRect);
        else
          m_dragTool = new ActivePointsTrapezoidDragTool(
              tHandleId, m_selection->getActivePointSet(), m_handleRect);
      }
      // ���ʂ̃N���b�N �� �g��^�k�����[�h
      else
        m_dragTool = new ActivePointsScaleDragTool(
            tHandleId, m_selection->getActivePointSet(), m_handleRect);
    }
    m_dragTool->onClick(pos, e);
    return true;
  }
  // ���������Ƃ�����N���b�N�����ꍇ
  else {
    if (m_selection->isEmpty())
      return false;
    else {
      // ���o�[�o���h�I�����n�߂�
      m_isRubberBanding = true;
      m_rubberStartPos  = pos;
      m_currentMousePos = pos;
      // ���o�[�o���h�́A�I�𒆂̃V�F�C�v�̃|�C���g�����A�N�e�B�u�ɂł���

      //+shift�Œǉ��I��
      if (!(e->modifiers() & Qt::ShiftModifier))
        m_selection->deactivateAllPoints();

      // �V�O�i�����G�~�b�g
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

    // ���̏�ŃN���b�N�������[�X�����ꍇ�A���̃|�C���g�̒P�ƑI��
    if (!dragMoved) {
      // �N���b�N�ʒu����A����ΏۂƂȂ�V�F�C�v�^�n���h���𓾂�
      int pointIndex, handleId;
      OneShape shape = getClickedShapeAndIndex(pointIndex, handleId, e);
      if (shape.shapePairP && handleId == 1 &&
          !(e->modifiers() & Qt::ShiftModifier)) {
        m_selection->selectNone();
        m_selection->activatePoint(shape, pointIndex);
        // �V�O�i�����G�~�b�g
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      }
    }

    return true;
  }

  if (!m_selection->isEmpty()) {
    IwProject* project = IwApp::instance()->getCurrentProject()->getProject();

    IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
    if (!layer) return false;

    // ���o�[�o���h�I���̏ꍇ�ARect�Ɉ͂܂ꂽ�|�C���g��ǉ��I��
    if (isRubberBandValid()) {
      // �I�𒆂̃V�F�C�v�ŁA���o�[�o���h�Ɋ܂܂�Ă���CP��ǉ����Ă���
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
          // �o�E���f�B���O�Ɋ܂܂�����
          if (rubberBand.contains(bp.pos)) {
            m_selection->activatePoint(shape, p);
            somethingSelected = true;
          }
        }
      }

      if (somethingSelected)
        // �V�O�i�����G�~�b�g
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      // ���o�[�o���h������
      m_isRubberBanding = false;

      return true;
    }
    // ���o�[�o���h�I���łȂ��A�V�F�C�v���I������Ă���ꍇ�A
    // ���A�}�E�X���ɂȂɂ��V�F�C�v�������ꍇ�A
    // �}�E�X�����[�X���ɃV�F�C�v�I������������
    else {
      // ���o�[�o���h������
      m_isRubberBanding = false;

      int name       = m_viewer->pick(e->pos());
      OneShape shape = layer->getShapePairFromName(name);
      if (!shape.shapePairP) {
        m_selection->selectNone();
        // �V�O�i�����G�~�b�g
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
// �E�N���b�N���j���[
//--------------------------------------------------------
bool ReshapeTool::rightButtonDown(const QPointF&, const QMouseEvent* e,
                                  bool& /*canOpenMenu*/, QMenu& menu) {
  // �N���b�N�ʒu����A����ΏۂƂȂ�V�F�C�v�^�n���h���𓾂�
  int pointIndex, handleId;
  OneShape shape = getClickedShapeAndIndex(pointIndex, handleId, e);

  ReshapeToolContextMenu::instance()->init(m_selection,
                                           m_viewer->getOnePixelLength());
  ReshapeToolContextMenu::instance()->openMenu(e, shape, pointIndex, handleId,
                                               menu);
  return true;
}

//--------------------------------------------------------
// ���L�[��1�s�N�Z�������ړ��B+Shift��10�A+Ctrl��1/10
//--------------------------------------------------------
bool ReshapeTool::keyDown(int key, bool ctrl, bool shift, bool /*alt*/) {
  // �I���V�F�C�v��������Ζ���
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
    // ���L�[�ł̈ړ�
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
// �A�N�e�B�u�ȃV�F�C�v�ɃR���g���[���|�C���g��`��
// �I���|�C���g�͐F��ς��A�n���h�����`��
//--------------------------------------------------------
void ReshapeTool::draw() {
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // �I���V�F�C�v������������return
  if (m_selection->isEmpty()) return;

  // �\���t���[���𓾂�
  int frame = project->getViewFrame();

  // �R���g���[���|�C���g�̐F�𓾂Ă���
  QColor cpColor    = ColorSettings::instance()->getQColor(Color_CtrlPoint);
  QColor cpSelected = ColorSettings::instance()->getQColor(Color_ActiveCtrl);
  QColor cpInbetween =
      ColorSettings::instance()->getQColor(Color_InbetweenCtrl);

  QList<OneShape> shapes = m_selection->getShapes();

  QPointF topLeft(10000, 10000);
  QPointF bottomRight(-1000, -1000);
  auto updateBBox = [&](const QPointF& pos) {
    topLeft =
        QPointF(std::min(pos.x(), topLeft.x()), std::min(pos.y(), topLeft.y()));
    bottomRight = QPointF(std::max(pos.x(), bottomRight.x()),
                          std::max(pos.y(), bottomRight.y()));
  };

  // �e�A�N�e�B�u�ȃV�F�C�v�ɂ���
  for (int s = 0; s < shapes.size(); s++) {
    OneShape shape = shapes.at(s);

    // ���ꂼ��̃|�C���g�ɂ��āA�I������Ă�����n���h���t���ŕ`��
    // �I������Ă��Ȃ���Ε��ʂɎl�p��`��
    BezierPointList bPList =
        shape.shapePairP->getBezierPointList(frame, shape.fromTo);
    for (int p = 0; p < bPList.size(); p++) {
      // �I������Ă���ꍇ
      if (m_selection->isPointActive(shape, p)) {
        // �n���h���t���ŃR���g���[���|�C���g��`�悷��
        m_viewer->setColor(cpSelected);
        ReshapeTool::drawControlPoint(m_viewer, shape, bPList, p, true,
                                      m_viewer->getOnePixelLength());

        // get bounding box for transform handle
        if (m_transformHandles) {
          BezierPoint bp = bPList.at(p);
          updateBBox(bp.firstHandle);
          updateBBox(bp.pos);
          updateBBox(bp.secondHandle);
        }

      }
      // �I������Ă��Ȃ��ꍇ
      else {
        // �P�ɃR���g���[���|�C���g��`�悷��
        // �J�����g�t���[�����L�[�t���[�����ǂ����ŕ\���𕪂���
        m_viewer->setColor(shape.shapePairP->isFormKey(frame, shape.fromTo)
                               ? cpColor
                               : cpInbetween);
        ReshapeTool::drawControlPoint(m_viewer, shape, bPList, p, false,
                                      m_viewer->getOnePixelLength());
      }
    }
    //	++i;
  }

  if (m_dragTool) m_dragTool->draw(m_viewer, m_viewer->getOnePixelLength());

  if (m_transformHandles) {
    QRectF handleRect(topLeft, bottomRight);
    if (!handleRect.isEmpty()) {
      QPointF onePix = m_viewer->getOnePixelLength();
      handleRect.adjust(-onePix.x(), -onePix.y(), onePix.x(), onePix.y());
      m_viewer->setColor(cpSelected);

      m_viewer->setLineStipple(3, 0xAAAA);

      drawEdgeForResize(m_viewer, Handle_RightEdge, handleRect.bottomRight(),
                        handleRect.topRight());
      drawEdgeForResize(m_viewer, Handle_TopEdge, handleRect.topRight(),
                        handleRect.topLeft());
      drawEdgeForResize(m_viewer, Handle_LeftEdge, handleRect.topLeft(),
                        handleRect.bottomLeft());
      drawEdgeForResize(m_viewer, Handle_BottomEdge, handleRect.bottomLeft(),
                        handleRect.bottomRight());

      m_viewer->setLineStipple(1, 0xFFFF);

      // �n���h����`��
      drawHandle(m_viewer, Handle_BottomRight, onePix,
                 handleRect.bottomRight());
      drawHandle(m_viewer, Handle_Right, onePix,
                 QPointF(handleRect.right(), handleRect.center().y()));
      drawHandle(m_viewer, Handle_TopRight, onePix, handleRect.topRight());
      drawHandle(m_viewer, Handle_Top, onePix,
                 QPointF(handleRect.center().x(), handleRect.top()));
      drawHandle(m_viewer, Handle_TopLeft, onePix, handleRect.topLeft());
      drawHandle(m_viewer, Handle_Left, onePix,
                 QPointF(handleRect.left(), handleRect.center().y()));
      drawHandle(m_viewer, Handle_BottomLeft, onePix, handleRect.bottomLeft());
      drawHandle(m_viewer, Handle_Bottom, onePix,
                 QPointF(handleRect.center().x(), handleRect.bottom()));
      m_handleRect = handleRect;
    }
  }

  // ���ɁA���o�[�o���h��`��
  if (isRubberBandValid()) {
    // Rect�𓾂�
    QRectF rubberBand =
        QRectF(m_rubberStartPos, m_currentMousePos).normalized();
    // �_���ŕ`��
    m_viewer->setColor(QColor(Qt::white));
    m_viewer->setLineStipple(3, 0xAAAA);

    QVector3D vert[4] = {
        QVector3D(rubberBand.bottomRight()), QVector3D(rubberBand.topRight()),
        QVector3D(rubberBand.topLeft()), QVector3D(rubberBand.bottomLeft())};
    m_viewer->doDrawLine(GL_LINE_LOOP, vert, 4);

    // �_��������
    m_viewer->setLineStipple(1, 0xFFFF);
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

  // �N���b�N�ʒu����A����ΏۂƂȂ�V�F�C�v�^�n���h���𓾂�
  int pointIndex, handleId;
  OneShape shape = getClickedShapeAndIndex(pointIndex, handleId, e);
  // int name = m_viewer->pick(e->pos());

  m_ctrlPressed = (e->modifiers() & Qt::ControlModifier);

  // �n���h�����N���b�N���ꂽ���`�F�b�N����iname�̉��ꌅ�j
  if (shape.shapePairP && handleId >= 1) {
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

  QString statusStr =
      tr("[Click points / Drag area] to select. [+Shift] to add to the "
         "selection. [Alt + Click shape] to insert a new point. [Arrow keys] "
         "to move. [+Shift] with large step. [+Ctrl] with small step.");

  if (m_transformHandles) {
    if (m_ctrlPressed) {
      switch (handleId) {
      case Handle_RightEdge:
      case Handle_TopEdge:
      case Handle_LeftEdge:
      case Handle_BottomEdge:
        statusStr = tr("[Ctrl+Drag] to rotate. [+Shift] every 45 degrees.");
        break;
      default:
        statusStr =
            tr("[Ctrl+Drag] to scale. [+Shift] with fixing aspect ratio.");
        break;
      }
    } else {
      switch (handleId) {
      case Handle_BottomRight:
      case Handle_TopRight:
      case Handle_TopLeft:
      case Handle_BottomLeft:
        statusStr =
            tr("[Drag] to scale. [+Shift] with fixing aspect ratio. [Alt+Drag] "
               "to reshape "
               "trapezoidally. [+Shift] with symmetrically.");
        break;
      case Handle_Right:
      case Handle_Top:
      case Handle_Left:
      case Handle_Bottom:
        statusStr =
            tr("[Drag] to scale. [Alt+Drag] to shear. [+Shift] with a parallel "
               "manner.");
        break;
      case Handle_RightEdge:
      case Handle_TopEdge:
      case Handle_LeftEdge:
      case Handle_BottomEdge:
        statusStr =
            tr("[Drag] to move. [+Shift] to either vertical or horizontal "
               "direction. [Ctrl+Drag] to rotate.");
        break;
      default:
        break;
      }
    }
  }
  IwApp::instance()->showMessageOnStatusBar(statusStr);

  // �R���g���[���|�C���g�ǉ����[�h�̏���
  if (shape.shapePairP && (e->modifiers() & Qt::AltModifier) &&
      m_selection->isSelected(shape))
    return ToolCursor::BlackArrowAddCursor;

  if (!m_transformHandles || handleId == Handle_None)
    return ToolCursor::ArrowCursor;

  // ������Ă���C���L�[�^�n���h���ɍ��킹�A�J�[�\��ID��Ԃ�
  // ��]���[�h
  if (m_ctrlPressed) {
    switch (handleId) {
    case Handle_RightEdge:
    case Handle_TopEdge:
    case Handle_LeftEdge:
    case Handle_BottomEdge:
      return ToolCursor::RotationCursor;
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
  // Shear�ό`���[�h
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
  // �g��k�����[�h ���� �ړ����[�h
  else {
    switch (handleId) {
    case Handle_RightEdge:
    case Handle_TopEdge:
    case Handle_LeftEdge:
    case Handle_BottomEdge:
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
  return ToolCursor::ArrowCursor;
}

//--------------------------------------------------------
// �R���g���[���|�C���g��`�悷��B�n���h�����t���邩�ǂ����������Ō��߂�
//--------------------------------------------------------

void ReshapeTool::drawControlPoint(SceneViewer* viewer, OneShape shape,
                                   BezierPointList& pointList, int cpIndex,
                                   bool drawHandles, const QPointF& onePix,
                                   int specialShapeIndex, bool fillPoint,
                                   QColor fillColor) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;
  BezierPoint bPoint = pointList.at(cpIndex);

  // �n���h����`���ꍇ�ACP�ƃn���h�����q��������`��
  if (drawHandles) {
    QVector3D vert[3] = {
        QVector3D(bPoint.firstHandle.x(), bPoint.firstHandle.y(), 0.0),
        QVector3D(bPoint.pos.x(), bPoint.pos.y(), 0.0),
        QVector3D(bPoint.secondHandle.x(), bPoint.secondHandle.y(), 0.0)};
    viewer->doDrawLine(GL_LINE_STRIP, vert, 3);
  }

  // ���O��t����
  int name;
  if (specialShapeIndex == 0)
    name = layer->getNameFromShapePair(shape) + cpIndex * 10 +
           1;  // 1�̓R���g���[���|�C���g�̈�
  else
    name = specialShapeIndex * 10000 + cpIndex * 10 +
           1;  // 1�̓R���g���[���|�C���g�̈�

  // �R���g���[���|�C���g��`��
  viewer->pushMatrix();
  glPushName(name);

  viewer->translate(bPoint.pos.x(), bPoint.pos.y(), 0.0);
  viewer->scale(onePix.x(), onePix.y(), 1.0);

  QVector3D vert[4] = {QVector3D(2.0, -2.0, 0.0), QVector3D(2.0, 2.0, 0.0),
                       QVector3D(-2.0, 2.0, 0.0), QVector3D(-2.0, -2.0, 0.0)};
  if (fillPoint)
    viewer->doDrawFill(GL_POLYGON, vert, 4, fillColor);
  else
    viewer->doDrawLine(GL_LINE_LOOP, vert, 4);

  glPopName();
  viewer->popMatrix();

  if (!drawHandles) return;

  //--- ��������n���h��������

  // firstHandle
  name += 1;
  viewer->pushMatrix();
  glPushName(name);

  viewer->translate(bPoint.firstHandle.x(), bPoint.firstHandle.y(), 0.0);
  viewer->scale(onePix.x(), onePix.y(), 1.0);

  ReshapeTool::drawCircle(viewer);

  glPopName();
  viewer->popMatrix();

  // secondHandle
  name += 1;
  viewer->pushMatrix();
  glPushName(name);

  viewer->translate(bPoint.secondHandle.x(), bPoint.secondHandle.y(), 0.0);
  viewer->scale(onePix.x(), onePix.y(), 1.0);

  ReshapeTool::drawCircle(viewer);

  glPopName();
  viewer->popMatrix();
  // glPopMatrix();
}

//--------------------------------------------------------
// �n���h���p�̉~��`��
//--------------------------------------------------------
void ReshapeTool::drawCircle(SceneViewer* viewer) {
  static QVector3D vert[12];
  static bool init = true;
  if (init) {
    double theta;
    for (int i = 0; i < 12; i++) {
      theta   = (double)i * M_PI / 6.0;
      vert[i] = QVector3D(2.0 * cosf(theta), 2.0 * sinf(theta), 0.0);
    }

    init = false;
  }

  viewer->doDrawLine(GL_LINE_LOOP, vert, 12);
}

//--------------------------------------------------------
// �A�N�e�B�u�ȃV�F�C�v��Alt�N���b�N�����ꍇ�A�R���g���[���|�C���g��ǉ�����
//--------------------------------------------------------
void ReshapeTool::addControlPoint(OneShape shape, const QPointF& pos) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // �\���t���[���𓾂�
  int frame = project->getViewFrame();

  // Undo�����
  AddControlPointUndo* undo = new AddControlPointUndo(
      shape.shapePairP->getFormData(0), shape.shapePairP->getCorrData(0),
      shape.shapePairP->getFormData(1), shape.shapePairP->getCorrData(1), shape,
      project, layer);

  // �w��ʒu�ɃR���g���[���|�C���g��ǉ�����
  int pointIndex = shape.shapePairP->addControlPoint(pos, frame, shape.fromTo);
  // shape->addControlPoint(pos, frame);��I������

  undo->setAfterData(
      shape.shapePairP->getFormData(0), shape.shapePairP->getCorrData(0),
      shape.shapePairP->getFormData(1), shape.shapePairP->getCorrData(1));

  // Undo�ɓo�^ ������redo���Ă΂�邪�A�ŏ��̓t���O�ŉ������
  IwUndoManager::instance()->push(undo);

  // �쐬�����|�C���g��I��
  m_selection->makeCurrent();
  m_selection->selectNone();
  m_selection->activatePoint(shape, pointIndex);
  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();

  IwTriangleCache::instance()->invalidateShapeCache(
      project->getParentShape(shape.shapePairP));
}

//--------------------------------------------------------
// Ctrl+���L�[��0.25�s�N�Z�������ړ�������
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
  // �ړ��x�N�^
  QPointF moveVec(delta.x() * onePix.x(), delta.y() * onePix.y());

  int frame = project->getViewFrame();

  QList<BezierPointList> beforePoints;
  QList<BezierPointList> afterPoints;
  QList<bool> wasKeyFrame;

  for (int s = 0; s < m_selection->getShapesCount(); s++) {
    OneShape shape = m_selection->getShape(s);
    if (!shape.shapePairP) continue;

    // �L�[�t���[�����������ǂ������i�[
    wasKeyFrame.append(shape.shapePairP->isFormKey(frame, shape.fromTo));

    BezierPointList bpList =
        shape.shapePairP->getBezierPointList(frame, shape.fromTo);

    beforePoints.append(bpList);

    for (int bp = 0; bp < bpList.size(); bp++) {
      // Point���A�N�e�B�u�Ȃ�ړ�
      if (!m_selection->isPointActive(shape, bp)) continue;
      bpList[bp].pos += moveVec;
      bpList[bp].firstHandle += moveVec;
      bpList[bp].secondHandle += moveVec;
    }

    afterPoints.append(bpList);

    // �L�[���i�[
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
  // ���o�[�o���h�I����������x�̑傫���Ȃ�L��
  return (mouseMov.manhattanLength() > 4);
}

//---------------------------------------------------
// �N���b�N�ʒu����A����ΏۂƂȂ�V�F�C�v�^�n���h���𓾂�
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

  // ��������ł��Ȃ����return
  if (nameList.isEmpty()) return OneShape();

  struct ClickedPointInfo {
    OneShape s;
    int pId;
    int hId;
  };

  // ���񂾑Ώۂ��ƂɃ��X�g������
  QList<ClickedPointInfo> cpList, shapeList, handleList, transformHandleList;

  // ���񂾃A�C�e�����d�����Ă���
  for (int i = 0; i < nameList.size(); i++) {
    int name = nameList.at(i);

    if (m_transformHandles && name < 10000) {
      transformHandleList.append({OneShape(), -1, name % 100});
      continue;
    }

    OneShape tmpShape = layer->getShapePairFromName(name);
    if (!tmpShape.shapePairP) continue;

    int tmp_pId = (int)((name % 10000) / 10);
    int tmp_hId = name % 10;

    ClickedPointInfo clickedInfo = {tmpShape, tmp_pId, tmp_hId};
    // �n���h���ɍ��킹�ă��X�g�𕪂��Ċi�[
    if (tmp_hId == 1)  // �|�C���g�̏ꍇ
      cpList.append(clickedInfo);
    else if (tmp_hId == 2 || tmp_hId == 3)
      handleList.append(clickedInfo);
    else
      shapeList.append(clickedInfo);
  }
  // �S�Ẵ��X�g����Ȃ�return
  if (cpList.isEmpty() && shapeList.isEmpty() && handleList.isEmpty() &&
      transformHandleList.isEmpty())
    return 0;

  ClickedPointInfo retInfo;  // �ŏI�I�ɕԂ��N���b�N���

  if (!transformHandleList.isEmpty()) {
    std::sort(transformHandleList.begin(), transformHandleList.end(),
              [](ClickedPointInfo lhs, ClickedPointInfo rhs) {
                return lhs.hId < rhs.hId;
              });
    retInfo = transformHandleList.first();
  }
  // �C���L�[�ɂ���āA�|�C���g�^�n���h���ǂ����D�悵�Ă��ނ��𔻒f����
  //  +Ctrl, +Alt �̏ꍇ�̓n���h����D��
  else if (e->modifiers() & Qt::ControlModifier ||
           e->modifiers() & Qt::AltModifier) {
    if (!handleList.isEmpty())
      retInfo = handleList.first();
    else if (!cpList.isEmpty())
      retInfo = cpList.first();
    else
      retInfo = shapeList.first();
  }
  // �����łȂ��ꍇ�́A�R���g���[���|�C���g��D�悵�Ē͂�
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
// �ȉ��AUndo�R�}���h��o�^
//---------------------------------------------------
AddControlPointUndo::AddControlPointUndo(
    const QMap<int, BezierPointList>& beforeFormDataFrom,
    const QMap<int, CorrPointList>& beforeCorrDataFrom,
    const QMap<int, BezierPointList>& beforeFormDataTo,
    const QMap<int, CorrPointList>& beforeCorrDataTo, OneShape shape,
    IwProject* project, IwLayer* layer)
    : m_project(project), m_shape(shape), m_layer(layer), m_firstFlag(true) {
  m_beforeFormData[0] = beforeFormDataFrom;
  m_beforeFormData[1] = beforeFormDataTo;
  m_beforeCorrData[0] = beforeCorrDataFrom;
  m_beforeCorrData[1] = beforeCorrDataTo;
}

void AddControlPointUndo::setAfterData(
    const QMap<int, BezierPointList>& afterFormDataFrom,
    const QMap<int, CorrPointList>& afterCorrDataFrom,
    const QMap<int, BezierPointList>& afterFormDataTo,
    const QMap<int, CorrPointList>& afterCorrDataTo) {
  m_afterFormData[0] = afterFormDataFrom;
  m_afterFormData[1] = afterFormDataTo;
  m_afterCorrData[0] = afterCorrDataFrom;
  m_afterCorrData[1] = afterCorrDataTo;
}

void AddControlPointUndo::undo() {
  for (int fromTo = 0; fromTo < 2; fromTo++) {
    // �V�F�C�v�Ɍ`��f�[�^��߂�
    m_shape.shapePairP->setFormData(m_beforeFormData[fromTo], fromTo);
    // �V�F�C�v�ɑΉ��_�f�[�^��߂�
    m_shape.shapePairP->setCorrData(m_beforeCorrData[fromTo], fromTo);
  }

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

void AddControlPointUndo::redo() {
  // Undo�o�^���ɂ�redo���s��Ȃ���[�ɂ���
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  for (int fromTo = 0; fromTo < 2; fromTo++) {
    // �V�F�C�v�Ɍ`��f�[�^��߂�
    m_shape.shapePairP->setFormData(m_afterFormData[fromTo], fromTo);
    // �V�F�C�v�ɑΉ��_�f�[�^��߂�
    m_shape.shapePairP->setCorrData(m_afterCorrData[fromTo], fromTo);
  }

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

ReshapeTool reshapeTool;
