//--------------------------------------------------------
// Transform Tool
// �V�F�C�v�ό`�c�[��
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
    // �ȉ��ATransformOption�̓��e
    , m_scaleAroundCenter(false)
    , m_rotateAroundCenter(true)
    , m_shapeIndependentTransforms(false)
    , m_frameIndependentTransforms(true)
    , m_shapePairSelection(IwShapePairSelection::instance())
    , m_ctrlPressed(false)
    , m_translateOnly(false) {}

//--------------------------------------------------------

TransformTool::~TransformTool() {
  // TransformDragTool�����
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
  // �ǂ̃n���h�����N���b�N���ꂽ���`�F�b�N����iname�̉��ꌅ�j
  TransformHandleId handleId = (TransformHandleId)(name % 100);

  OneShape shape = layer->getShapePairFromName(name);
  // �I���V�F�C�v����Ȃ������畁�ʂ̃J�[�\��
  if (!shape.shapePairP) {
    return ToolCursor::ArrowCursor;
  }
  if (!m_shapePairSelection->isSelected(shape)) return ToolCursor::ArrowCursor;

  if (m_translateOnly) {  // �ړ����[�h
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

  // ������Ă���C���L�[�^�n���h���ɍ��킹�A�J�[�\��ID��Ԃ�
  // ��]���[�h
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
    case Handle_None:  // �ړ����[�h
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
// �@ �V�F�C�v���N���b�N �� �V�F�C�v���I���ς݂Ȃ�A�h���b�O���샂�[�h�ɂȂ�
//							�V�F�C�v���I������Ă��Ȃ���΁A���̃V�F�C�v�P������I��������
// �A �V�F�C�v��Shift�N���b�N ��
// �V�F�C�v���I������Ă��Ȃ���΁A�V�F�C�v��ǉ��I�� �B �V�F�C�v��Ctrl�N���b�N
// �� �V�F�C�v���I���ς݂Ȃ�A���̑I��������
//								 �V�F�C�v���I������Ă��Ȃ���΁A�V�F�C�v��ǉ��I��
// �C �����Ȃ��Ƃ�����N���b�N �� �V�F�C�v�̃��o�[�o���h�I�����n�߂�.
//									Shift���ACtrl��������Ă����牽�����Ȃ��B
//								  ���ʂ̍��N���b�N�Ȃ�A�I��������
bool TransformTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
  if (!m_viewer) return false;

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return false;

  int name = m_viewer->pick(e->pos());

  OneShape shape = layer->getShapePairFromName(name);

  // �V�F�C�v���N���b�N����Ă����ꍇ
  if (shape.shapePairP) {
    // �V�F�C�v��Shift�N���b�N ��
    // �V�F�C�v���I������Ă��Ȃ���΁A�V�F�C�v��ǉ��I��
    if (e->modifiers() & Qt::ShiftModifier) {
      if (m_shapePairSelection->isSelected(shape)) {
        // �ǂ̃n���h�����N���b�N���ꂽ���`�F�b�N����iname�̉��ꌅ�j
        TransformHandleId handleId = (TransformHandleId)(name % 100);
        if (m_translateOnly) handleId = Handle_None;
        // �n���h���ȊO�̂Ƃ��Ȃ�ړ����[�h
        if (handleId == Handle_None)
          m_dragTool = new TranslateDragTool(shape, handleId,
                                             m_shapePairSelection->getShapes());
        // Ctrl�N���b�N   �� ��]���[�h (�{Shift��45�x����)
        else if (e->modifiers() & Qt::ControlModifier)
          m_dragTool = new RotateDragTool(shape, handleId,
                                          m_shapePairSelection->getShapes(),
                                          m_viewer->getOnePixelLength());
        // Alt�N���b�N�@  �� Shear�ό`���[�h�i�{�j
        else if (e->modifiers() & Qt::AltModifier) {
          if (handleId == Handle_Right || handleId == Handle_Top ||
              handleId == Handle_Left || handleId == Handle_Bottom)
            m_dragTool = new ShearDragTool(shape, handleId,
                                           m_shapePairSelection->getShapes());
          else
            m_dragTool = new TrapezoidDragTool(
                shape, handleId, m_shapePairSelection->getShapes());
        }
        // ���ʂ̃N���b�N �� �g��^�k�����[�h
        else
          m_dragTool = new ScaleDragTool(shape, handleId,
                                         m_shapePairSelection->getShapes());

        m_dragTool->onClick(pos, e);
        return true;
      } else {
        m_shapePairSelection->makeCurrent();
        m_shapePairSelection->addShape(shape);
        // �V�O�i�����G�~�b�g
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
        return true;
      }
    }
    // �V�F�C�v��Ctrl�N���b�N  �� �V�F�C�v���I���ς݂Ȃ�A���̑I��������
    //							 �V�F�C�v���I������Ă��Ȃ���΁A�V�F�C�v��ǉ��I��
    else if (e->modifiers() & Qt::ControlModifier) {
      if (m_shapePairSelection->isSelected(shape)) {
        // �n���h����͂�ł���Ƃ�������]���[�h
        // �ǂ̃n���h�����N���b�N���ꂽ���`�F�b�N����iname�̉��ꌅ�j
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
      // �V�O�i�����G�~�b�g
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
    // �V�F�C�v���N���b�N �� �V�F�C�v���I���ς݂Ȃ�A�h���b�O���샂�[�h�ɂȂ�
    //						 �V�F�C�v���I������Ă��Ȃ���΁A���̃V�F�C�v�P������I��������
    else {
      if (m_shapePairSelection->isSelected(shape)) {
        // �ǂ̃n���h�����N���b�N���ꂽ���`�F�b�N����iname�̉��ꌅ�j
        TransformHandleId handleId = (TransformHandleId)(name % 100);
        if (m_translateOnly) handleId = Handle_None;
        // �n���h���ȊO�̂Ƃ��Ȃ�ړ����[�h
        if (handleId == Handle_None)
          m_dragTool = new TranslateDragTool(shape, handleId,
                                             m_shapePairSelection->getShapes());
        // Alt�N���b�N�@  �� Shear�ό`���[�h�i�{�j
        else if (e->modifiers() & Qt::AltModifier) {
          if (handleId == Handle_Right || handleId == Handle_Top ||
              handleId == Handle_Left || handleId == Handle_Bottom)
            m_dragTool = new ShearDragTool(shape, handleId,
                                           m_shapePairSelection->getShapes());
          else
            m_dragTool = new TrapezoidDragTool(
                shape, handleId, m_shapePairSelection->getShapes());
        }
        // ���ʂ̃N���b�N �� �g��^�k�����[�h
        else
          m_dragTool = new ScaleDragTool(shape, handleId,
                                         m_shapePairSelection->getShapes());

        m_dragTool->onClick(pos, e);
      } else {
        m_shapePairSelection->makeCurrent();
        m_shapePairSelection->selectOneShape(shape);
      }
      // �V�O�i�����G�~�b�g
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
  }
  // ���������Ƃ�����N���b�N�����ꍇ
  else {
    // ���o�[�o���h�I�����n�߂�
    m_isRubberBanding = true;
    m_rubberStartPos  = pos;
    m_currentMousePos = pos;

    // Shift���ACtrl��������Ă����牽�����Ȃ��B
    if (e->modifiers() & Qt::ShiftModifier ||
        e->modifiers() & Qt::ControlModifier)
      return false;
    // ���ʂ̍��N���b�N�Ȃ�A�I��������
    else {
      if (m_shapePairSelection->isEmpty())
        return false;
      else {
        m_shapePairSelection->selectNone();
        // �V�O�i�����G�~�b�g
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

  // ���o�[�o���h�I���̏ꍇ�A�I��Rect���X�V
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

  // ���o�[�o���h�I���̏ꍇ�ARect�Ɉ͂܂ꂽ�V�F�C�v��ǉ��I��
  if (m_isRubberBanding) {
    QRectF rubberBand =
        QRectF(m_rubberStartPos, m_currentMousePos).normalized();

    IwProject* project = IwApp::instance()->getCurrentProject()->getProject();

    IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();

    m_shapePairSelection->makeCurrent();
    // m_selection->makeCurrent();

    // ���b�N���𓾂�
    bool fromToVisible[2];
    for (int fromTo = 0; fromTo < 2; fromTo++)
      fromToVisible[fromTo] =
          project->getViewSettings()->isFromToVisible(fromTo);

    // �v���W�F�N�g���̊e�V�F�C�v�ɂ��āA
    //  ���ݕ\�����̃V�F�C�v�A�����I���A����Rect�̃o�E���f�B���O�Ɋ܂܂�Ă���ꍇ�A�I������
    for (int s = 0; s < layer->getShapePairCount(); s++) {
      ShapePair* shapePair = layer->getShapePair(s);
      if (!shapePair) continue;
      if (!shapePair->isVisible()) continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // ���b�N����Ă������΂�
        if (!fromToVisible[fromTo]) continue;
        if (shapePair->isLocked(fromTo)) continue;
        if (layer->isLocked())
          continue;  // ���C���[�����b�N����Ă��Ă��I��s��

        // ���I���̏���
        if (m_shapePairSelection->isSelected(OneShape(shapePair, fromTo)))
          continue;
        // �o�E���f�B���O�Ɋ܂܂�����
        QRectF bBox = shapePair->getBBox(project->getViewFrame(), fromTo);
        if (!rubberBand.contains(bBox)) continue;

        m_shapePairSelection->addShape(OneShape(shapePair, fromTo));
      }
    }

    // �V�O�i�����G�~�b�g
    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();

    // ���o�[�o���h������
    m_isRubberBanding = false;

    return true;
  }

  return false;
}
//--------------------------------------------------------
void TransformTool::leftButtonDoubleClick(const QPointF&, const QMouseEvent*) {}

//--------------------------------------------------------
// Ctrl + ���L�[��1/4�s�N�Z�������ړ�
//--------------------------------------------------------
bool TransformTool::keyDown(int key, bool ctrl, bool shift, bool alt) {
  // �I���V�F�C�v��������Ζ���
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
    // ���L�[�ł̈ړ�
    arrowKeyMove(delta);
    return true;
  }

  return false;
}

//--------------------------------------------------------
// �I������Ă���V�F�C�v�ɘg������ �n���h���ɂ͖��O������
//--------------------------------------------------------
void TransformTool::draw() {
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // �e�I���V�F�C�v�̗֊s��`��
  for (int s = 0; s < m_shapePairSelection->getCount(); s++) {
    // �V�F�C�v���擾
    OneShape shape = m_shapePairSelection->getShape(s);

    // �V�F�C�v��������Ȃ������玟�ցi�ی��j
    if (!shape.shapePairP) continue;

    // �V�F�C�v���v���W�F�N�g�ɑ����Ă��Ȃ������玟�ցi�ی��j
    if (!layer->contains(shape.shapePairP)) continue;

    // �o�E���f�B���O���A�V�F�C�v�Ɠ������O��t����
    int name = layer->getNameFromShapePair(shape);

    QRectF bBox =
        shape.shapePairP->getBBox(project->getViewFrame(), shape.fromTo);

    // �o�E���f�B���O�͂P�s�N�Z���O���ɕ`���̂ŁA
    // �V�F�C�v���W�n�Ō����ڂP�s�N�Z���ɂȂ钷�����擾����
    QPointF onePix = m_viewer->getOnePixelLength();
    bBox.adjust(-onePix.x(), -onePix.y(), onePix.x(), onePix.y());

    if (shape.fromTo == 0)
      m_viewer->setLineStipple(3, 0xFCFC);
    else
      m_viewer->setLineStipple(3, 0xAAAA);

    m_viewer->setColor(QColor(Qt::white));
    // glColor3d(1.0, 1.0, 1.0);

    // �o�E���f�B���O��`��
    if (m_ctrlPressed) {
      drawEdgeForResize(m_viewer, layer, shape, Handle_RightEdge,
                        bBox.bottomRight(), bBox.topRight());
      drawEdgeForResize(m_viewer, layer, shape, Handle_TopEdge, bBox.topRight(),
                        bBox.topLeft());
      drawEdgeForResize(m_viewer, layer, shape, Handle_LeftEdge, bBox.topLeft(),
                        bBox.bottomLeft());
      drawEdgeForResize(m_viewer, layer, shape, Handle_BottomEdge,
                        bBox.bottomLeft(), bBox.bottomRight());
    } else {
      m_viewer->pushName(name);

      QVector3D vert[4] = {
          QVector3D(bBox.bottomRight()), QVector3D(bBox.topRight()),
          QVector3D(bBox.topLeft()), QVector3D(bBox.bottomLeft())};
      m_viewer->doDrawLine(GL_LINE_LOOP, vert, 4);

      m_viewer->popName();
    }

    // �_��������
    m_viewer->setLineStipple(1, 0xFFFF);

    // �n���h����`��
    drawHandle(m_viewer, layer, shape, Handle_BottomRight, onePix,
               bBox.bottomRight());
    drawHandle(m_viewer, layer, shape, Handle_Right, onePix,
               QPointF(bBox.right(), bBox.center().y()));
    drawHandle(m_viewer, layer, shape, Handle_TopRight, onePix,
               bBox.topRight());
    drawHandle(m_viewer, layer, shape, Handle_Top, onePix,
               QPointF(bBox.center().x(), bBox.top()));
    drawHandle(m_viewer, layer, shape, Handle_TopLeft, onePix, bBox.topLeft());
    drawHandle(m_viewer, layer, shape, Handle_Left, onePix,
               QPointF(bBox.left(), bBox.center().y()));
    drawHandle(m_viewer, layer, shape, Handle_BottomLeft, onePix,
               bBox.bottomLeft());
    drawHandle(m_viewer, layer, shape, Handle_Bottom, onePix,
               QPointF(bBox.center().x(), bBox.bottom()));
  }

  // ���ɁA���o�[�o���h��`��
  if (m_isRubberBanding) {
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
// �n���h�����������`��
//--------------------------------------------------------
void TransformTool::drawHandle(SceneViewer* viewer, IwLayer* layer,
                               OneShape shape, TransformHandleId handleId,
                               const QPointF& onePix, const QPointF& pos) {
  // ���O�����
  int handleName = layer->getNameFromShapePair(shape);
  handleName += (int)handleId;

  viewer->pushMatrix();
  viewer->pushName(handleName);

  viewer->translate(pos.x(), pos.y(), 0.0);
  viewer->scale(onePix.x(), onePix.y(), 1.0);

  static QVector3D vert[4] = {
      QVector3D(2.0, -2.0, 0.0), QVector3D(2.0, 2.0, 0.0),
      QVector3D(-2.0, 2.0, 0.0), QVector3D(-2.0, -2.0, 0.0)};
  viewer->doDrawLine(GL_LINE_LOOP, vert, 4);

  viewer->popName();
  viewer->popMatrix();
}

//--------------------------------------------------------
// Ctrl�������Ƃ��Ӄh���b�O�ŃT�C�Y�ύX�̂��߂̕�
//--------------------------------------------------------
void TransformTool::drawEdgeForResize(SceneViewer* viewer, IwLayer* layer,
                                      OneShape shape,
                                      TransformHandleId handleId,
                                      const QPointF& p1, const QPointF& p2) {
  int handleName = layer->getNameFromShapePair(shape);
  handleName += (int)handleId;
  viewer->pushName(handleName);

  QVector3D vert[2] = {QVector3D(p1), QVector3D(p2)};
  viewer->doDrawLine(GL_LINE_STRIP, vert, 2);

  viewer->popName();
}

//--------------------------------------------------------
// Ctrl+���L�[��0.25�s�N�Z�������ړ�������
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
  // �ړ��x�N�^
  QPointF moveVec(delta.x() * onePix.x() * factor,
                  delta.y() * onePix.y() * factor);

  int frame = project->getViewFrame();

  QList<BezierPointList> beforePoints;
  QList<BezierPointList> afterPoints;
  QList<bool> wasKeyFrame;

  for (int s = 0; s < m_shapePairSelection->getShapesCount(); s++) {
    OneShape shape = m_shapePairSelection->getShape(s);
    if (!shape.shapePairP) continue;

    // �L�[�t���[�����������ǂ������i�[
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

    // �L�[���i�[
    shape.shapePairP->setFormKey(frame, shape.fromTo, bpList);
  }

  // Undo��o�^
  IwUndoManager::instance()->push(
      new ArrowKeyMoveUndo(m_shapePairSelection->getShapes(), beforePoints,
                           afterPoints, wasKeyFrame, project, layer, frame));

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//--------------------------------------------------------
// �ȉ��AUndo�R�}���h
//--------------------------------------------------------

//--------------------------------------------------------
// Ctrl+���L�[��0.25�s�N�Z�������ړ������� ��Undo
//--------------------------------------------------------
ArrowKeyMoveUndo::ArrowKeyMoveUndo(const QList<OneShape>& shapes,
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
  // �e�V�F�C�v�Ƀ|�C���g���Z�b�g
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    if (!shape.shapePairP) continue;

    // ����O�̓L�[�t���[���łȂ������ꍇ�A�L�[������
    if (!m_wasKeyFrame.at(s))
      shape.shapePairP->removeFormKey(m_frame, shape.fromTo);
    // ����ȊO�̏ꍇ�̓|�C���g�������ւ�
    else
      shape.shapePairP->setFormKey(m_frame, shape.fromTo, m_beforePoints.at(s));
  }

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
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
  // Undo�o�^���ɂ�redo���s��Ȃ���[�ɂ���
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  // �e�V�F�C�v�Ƀ|�C���g���Z�b�g
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    if (!shape.shapePairP) continue;

    // �|�C���g�������ւ�
    shape.shapePairP->setFormKey(m_frame, shape.fromTo, m_afterPoints.at(s));
  }

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
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
