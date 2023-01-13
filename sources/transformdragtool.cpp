//--------------------------------------------------------
// Transform Tool�p �� �h���b�O�c�[��
// �V�F�C�v�ό`�c�[��
//--------------------------------------------------------
#include "transformdragtool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwundomanager.h"

#include "transformtool.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"

#include "iwlayerhandle.h"
#include "iwlayer.h"
#include "iwtrianglecache.h"

#include <QTransform>
#include <QMouseEvent>

namespace {
// MultiFrame���[�h��ON���ǂ�����Ԃ�
bool isMultiFrameActivated() {
  return false;
  // return
  // CommandManager::instance()->getAction(VMI_MultiFrameMode)->isChecked();
}
};  // namespace

//--------------------------------------------------------

TransformDragTool::TransformDragTool(OneShape shape, TransformHandleId handleId,
                                     const QList<OneShape>& selectedShapes)
    : m_grabbingShape(shape), m_handleId(handleId), m_shapes(selectedShapes) {
  m_project = IwApp::instance()->getCurrentProject()->getProject();
  m_layer   = IwApp::instance()->getCurrentLayer()->getLayer();

  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  // MultiFrame�̏��𓾂�
  QList<int> multiFrameList = m_project->getMultiFrames();

  // ���̌`��ƁA�L�[�t���[�����������ǂ������擾����
  for (int s = 0; s < m_shapes.count(); s++) {
    // �L�[�t���[�����������ǂ���
    m_wasKeyFrame.push_back(
        m_shapes.at(s).shapePairP->isFormKey(frame, m_shapes.at(s).fromTo));

    QMap<int, BezierPointList> formKeyMap;
    // �܂��A�ҏW���̃t���[��
    formKeyMap[frame] = m_shapes.at(s).shapePairP->getBezierPointList(
        frame, m_shapes.at(s).fromTo);
    // �����āAMultiFrame��ON�̂Ƃ��AMultiFrame�ŁA���L�[�t���[�����i�[
    if (isMultiFrameActivated()) {
      for (int mf = 0; mf < multiFrameList.size(); mf++) {
        int mframe = multiFrameList.at(mf);
        // �J�����g�t���[���͔�΂�
        if (mframe == frame) continue;
        // �L�[�t���[���Ȃ�A�o�^
        if (m_shapes.at(s).shapePairP->isFormKey(mframe, m_shapes.at(s).fromTo))
          formKeyMap[mframe] = m_shapes.at(s).shapePairP->getBezierPointList(
              mframe, m_shapes.at(s).fromTo);
      }
    }

    // ���̌`��̓o�^
    m_orgPoints.append(formKeyMap);
  }

  // TransformOption�̏��𓾂邽�߂ɁATransformTool�ւ̃|�C���^���T���Ă���
  m_tool = dynamic_cast<TransformTool*>(IwTool::getTool("T_Transform"));
}

//--------------------------------------------------------
// ���񂾃n���h���̔��Α��̃n���h���ʒu��Ԃ�
//--------------------------------------------------------
QPointF TransformDragTool::getOppositeHandlePos(OneShape shape, int frame) {
  // �V�F�C�v�������ꍇ�͍�����ł���V�F�C�v
  if (!shape.shapePairP) shape = m_grabbingShape;
  // �t���[����-1�̂Ƃ��̓J�����g�t���[��
  if (frame == -1) frame = m_project->getViewFrame();

  QRectF box = shape.shapePairP->getBBox(frame, shape.fromTo);

  // ��̓_��Ԃ�
  switch (m_handleId) {
  case Handle_BottomRight:
    return box.topLeft();
    break;
  case Handle_Right:
  case Handle_RightEdge:
    return QPointF(box.left(), box.center().y());
    break;
  case Handle_TopRight:
    return box.bottomLeft();
    break;
  case Handle_Top:
  case Handle_TopEdge:
    return QPointF(box.center().x(), box.bottom());
    break;
  case Handle_TopLeft:
    return box.bottomRight();
    break;
  case Handle_Left:
  case Handle_LeftEdge:
    return QPointF(box.right(), box.center().y());
    break;
  case Handle_BottomLeft:
    return box.topRight();
    break;
  case Handle_Bottom:
  case Handle_BottomEdge:
    return QPointF(box.center().x(), box.top());
    break;
  }
  // �ی�
  return box.center();
}

//--------------------------------------------------------
// �V�F�C�v�̒��S�ʒu��Ԃ�
//--------------------------------------------------------
QPointF TransformDragTool::getCenterPos(OneShape shape, int frame) {
  // �V�F�C�v�������ꍇ�͍�����ł���V�F�C�v
  if (!shape.shapePairP) shape = m_grabbingShape;
  // �t���[����-1�̂Ƃ��̓J�����g�t���[��
  if (frame == -1) frame = m_project->getViewFrame();

  QRectF box = shape.shapePairP->getBBox(frame, shape.fromTo);

  return box.center();
}

//--------------------------------------------------------

void TransformDragTool::onRelease(const QPointF&, const QMouseEvent*) {
  QList<QMap<int, BezierPointList>> afterPoints;
  // �ό`��̃V�F�C�v���擾
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // �V�F�C�v���擾
    OneShape shape = m_shapes.at(s);
    // IwShape* shape = m_shapes.at(s);
    QList<int> frameList = m_orgPoints.at(s).keys();
    QMap<int, BezierPointList> afterFormKeyMap;
    for (int f = 0; f < frameList.size(); f++) {
      int tmpFrame = frameList.at(f);
      afterFormKeyMap[tmpFrame] =
          shape.shapePairP->getBezierPointList(tmpFrame, shape.fromTo);
    }

    afterPoints.push_back(afterFormKeyMap);
  }

  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  // Undo�ɓo�^ ������redo���Ă΂�邪�A�ŏ��̓t���O�ŉ������
  IwUndoManager::instance()->push(new TransformDragToolUndo(
      m_shapes, m_orgPoints, afterPoints, m_wasKeyFrame, m_project, frame));

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  // �ύX�����t���[����invalidate
  for (auto shape : m_shapes) {
    int start, end;
    shape.shapePairP->getFormKeyRange(start, end, frame, shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, m_project->getParentShape(shape.shapePairP));
  }
}

//--------------------------------------------------------

//--------------------------------------------------------
//--------------------------------------------------------
// �n���h��Ctrl�N���b�N   �� ��]���[�h (�{Shift��45�x����)
//--------------------------------------------------------

RotateDragTool::RotateDragTool(OneShape shape, TransformHandleId handleId,
                               const QList<OneShape>& selectedShapes,
                               const QPointF& onePix)
    : TransformDragTool(shape, handleId, selectedShapes)
    , m_onePixLength(onePix) {}

//--------------------------------------------------------

void RotateDragTool::onClick(const QPointF& pos, const QMouseEvent* e) {
  // �ŏ��̊p�x�𓾂�
  if (m_tool->IsRotateAroundCenter())
    m_anchorPos = getCenterPos();
  else
    m_anchorPos = getOppositeHandlePos();
  QPointF initVec = pos - m_anchorPos;

  m_initialAngle = atan2(initVec.x(), initVec.y());

  // �e�V�F�C�v�^�t���[���ł́A��]���S�̍��W���i�[
  // �e�V�F�C�v�ɂ���
  OneShape tmpShape = m_grabbingShape;

  int tmpFrame = m_project->getViewFrame();
  for (int s = 0; s < m_shapes.size(); s++) {
    // �V�F�C�v���Ƃɉ�]���S�����ꍇ�A�V�F�C�v��؂�ւ�
    if (m_tool->IsShapeIndependentTransforms()) tmpShape = m_shapes.at(s);

    QList<int> frames = m_orgPoints.at(s).keys();
    QMap<int, QPointF> centerPosMap;
    // �e�t���[���ɂ���
    for (int f = 0; f < frames.size(); f++) {
      // �t���[�����Ƃɉ�]���S���قȂ�ꍇ�A�t���[����؂�ւ�
      if (m_tool->IsFrameIndependentTransforms()) tmpFrame = frames.at(f);

      // ��]���S��BBox�̒��S���A�n���h���̔��Α���
      if (m_tool->IsRotateAroundCenter())
        centerPosMap[frames.at(f)] = getCenterPos(tmpShape, tmpFrame);
      else
        centerPosMap[frames.at(f)] = getOppositeHandlePos(tmpShape, tmpFrame);
    }
    // ��]���S���i�[
    m_centerPos.append(centerPosMap);
  }
}

//--------------------------------------------------------

void RotateDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // �V���ȉ�]�p
  QPointF newVec = pos - m_anchorPos;

  double rotAngle = atan2(newVec.x(), newVec.y()) - m_initialAngle;

  double rotDegree = rotAngle * 180.0 / M_PI;

  // Shift��������Ă�����A45�x����]
  if (e->modifiers() & Qt::ShiftModifier) {
    while (rotDegree < 0) rotDegree += 360.0;

    int pizzaIndex = (int)(rotDegree / 45.0);

    double amari = rotDegree - (double)pizzaIndex * 45.0;

    if (amari > 22.5f) pizzaIndex += 1;

    rotDegree = (double)pizzaIndex * 45.0;
  }

  // �e�V�F�C�v�ɕό`��K�p
  for (int s = 0; s < m_orgPoints.size(); s++) {
    // �V�F�C�v�𓾂�
    OneShape shape = m_shapes.at(s);
    // IwShape* shape = m_shapes.at(s);
    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, QPointF> centerPosMap        = m_centerPos.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // ���ꂩ��ό`���s���`��f�[�^
      BezierPointList scaledBPointList = i.value();
      // ��]���S
      QPointF tmpCenter = centerPosMap.value(frame);
      // ��]�ɍ��킹�ĕό`��K�p���Ă���
      QTransform affine;
      affine = affine.translate(tmpCenter.x(), tmpCenter.y());
      affine = affine.scale(m_onePixLength.x(), m_onePixLength.y());
      affine = affine.rotate(-rotDegree);
      affine = affine.scale(1.0 / m_onePixLength.x(), 1.0 / m_onePixLength.y());
      affine = affine.translate(-tmpCenter.x(), -tmpCenter.y());
      // �ό`
      for (int bp = 0; bp < scaledBPointList.count(); bp++) {
        scaledBPointList[bp].pos = affine.map(scaledBPointList.at(bp).pos);
        scaledBPointList[bp].firstHandle =
            affine.map(scaledBPointList.at(bp).firstHandle);
        scaledBPointList[bp].secondHandle =
            affine.map(scaledBPointList.at(bp).secondHandle);
      }

      // �L�[���i�[
      shape.shapePairP->setFormKey(frame, shape.fromTo, scaledBPointList);

      ++i;
    }
  }
}

//--------------------------------------------------------
//--------------------------------------------------------
// �c���n���h��Alt�N���b�N�@  �� Shear�ό`���[�h�i�{Shift�ŕ��s�ɕό`�j
//--------------------------------------------------------

ShearDragTool::ShearDragTool(OneShape shape, TransformHandleId handleId,
                             const QList<OneShape>& selectedShapes)
    : TransformDragTool(shape, handleId, selectedShapes) {}

//--------------------------------------------------------

void ShearDragTool::onClick(const QPointF& pos, const QMouseEvent*) {
  m_startPos = pos;

  // �e�V�F�C�v�^�t���[���ł́A�V�A�[�ό`���S�̍��W���i�[
  // �e�V�F�C�v�ɂ���
  OneShape tmpShape = m_grabbingShape;
  int tmpFrame      = m_project->getViewFrame();
  for (int s = 0; s < m_shapes.size(); s++) {
    // �V�F�C�v���Ƃɉ�]���S�����ꍇ�A�V�F�C�v��؂�ւ�
    if (m_tool->IsShapeIndependentTransforms()) tmpShape = m_shapes.at(s);

    QList<int> frames = m_orgPoints.at(s).keys();
    QMap<int, QPointF> anchorPosMap;
    // �e�t���[���ɂ���
    for (int f = 0; f < frames.size(); f++) {
      // �t���[�����Ƃɉ�]���S���قȂ�ꍇ�A�t���[����؂�ւ�
      if (m_tool->IsFrameIndependentTransforms()) tmpFrame = frames.at(f);
      // �n���h���̔��Α����V�A�[�ό`�̒��S
      anchorPosMap[frames.at(f)] = getOppositeHandlePos(tmpShape, tmpFrame);
    }
    // ��]���S���i�[
    m_anchorPos.append(anchorPosMap);
  }

  // �ό`�̃t���O�𓾂�
  // ���E�̃n���h�����N���b�N�F���Ɋg��k���A�c�ɃV�A�[�ό`
  // �㉺�̃n���h�����N���b�N�F�c�Ɋg��k���A���ɃV�A�[�ό`
  if (m_handleId == Handle_Right || m_handleId == Handle_Left)
    m_isVertical = true;
  else
    m_isVertical = false;

  if (m_handleId == Handle_Left || m_handleId == Handle_Top)
    m_isInv = true;
  else
    m_isInv = false;
}

//--------------------------------------------------------

void ShearDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // �e�V�F�C�v�ɕό`��K�p
  for (int s = 0; s < m_orgPoints.size(); s++) {
    // �V�F�C�v�𓾂�
    OneShape shape = m_shapes.at(s);

    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, QPointF> anchorPosMap        = m_anchorPos.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // ���ꂩ��ό`���s���`��f�[�^
      BezierPointList shearedBPointList = i.value();
      // �V�A�[�ό`���S
      QPointF tmpAnchor = anchorPosMap.value(frame);

      QTransform affine;

      QPointF scale(1.0, 1.0);
      if (!(e->modifiers() & Qt::ShiftModifier)) {
        if (m_isVertical)
          scale.setX((pos.x() - tmpAnchor.x()) /
                     (m_startPos.x() - tmpAnchor.x()));
        else
          scale.setY((pos.y() - tmpAnchor.y()) /
                     (m_startPos.y() - tmpAnchor.y()));
      }

      // �V�A�[�ʒu
      QPointF shear(0.0, 0.0);
      if (m_isVertical) {
        shear.setY(pos.y() - m_startPos.y());
        shear.setY(shear.y() / (pos.x() - tmpAnchor.x()));
      } else {
        shear.setX(pos.x() - m_startPos.x());
        shear.setX(shear.x() / (pos.y() - tmpAnchor.y()));
      }

      affine = affine.translate(tmpAnchor.x(), tmpAnchor.y());
      affine = affine.shear(shear.x(), shear.y());
      affine = affine.scale(scale.x(), scale.y());
      affine = affine.translate(-tmpAnchor.x(), -tmpAnchor.y());

      // �ό`
      for (int bp = 0; bp < shearedBPointList.count(); bp++) {
        shearedBPointList[bp].pos = affine.map(shearedBPointList.at(bp).pos);
        shearedBPointList[bp].firstHandle =
            affine.map(shearedBPointList.at(bp).firstHandle);
        shearedBPointList[bp].secondHandle =
            affine.map(shearedBPointList.at(bp).secondHandle);
      }

      // �L�[���i�[
      shape.shapePairP->setFormKey(frame, shape.fromTo, shearedBPointList);

      ++i;
    }
  }
}

//--------------------------------------------------------
//--------------------------------------------------------
// �΂߃n���h��Alt�N���b�N�@  �� ��`�ό`���[�h�i�{Shift�őΏ̕ό`�j
//--------------------------------------------------------

TrapezoidDragTool::TrapezoidDragTool(OneShape shape, TransformHandleId handleId,
                                     const QList<OneShape>& selectedShapes)
    : TransformDragTool(shape, handleId, selectedShapes) {}

//--------------------------------------------------------

void TrapezoidDragTool::onClick(const QPointF& pos, const QMouseEvent*) {
  m_startPos = pos;

  // �e�V�F�C�v�^�t���[���ł́A��`�ό`�̊�ƂȂ�o�E���f�B���O�{�b�N�X���i�[
  // �e�V�F�C�v�ɂ���
  OneShape tmpShape = m_grabbingShape;
  int tmpFrame      = m_project->getViewFrame();
  for (int s = 0; s < m_shapes.size(); s++) {
    // �V�F�C�v���ƂɊ�o�E���f�B���O�{�b�N�X���قȂ�ꍇ�A�V�F�C�v��؂�ւ�
    if (m_tool->IsShapeIndependentTransforms()) tmpShape = m_shapes.at(s);

    QList<int> frames = m_orgPoints.at(s).keys();
    QMap<int, QRectF> bBoxMap;
    // �e�t���[���ɂ���
    for (int f = 0; f < frames.size(); f++) {
      // �t���[�����ƂɊ�o�E���f�B���O�{�b�N�X���قȂ�ꍇ�A�t���[����؂�ւ�
      if (m_tool->IsFrameIndependentTransforms()) tmpFrame = frames.at(f);

      bBoxMap[frames.at(f)] =
          tmpShape.shapePairP->getBBox(tmpFrame, tmpShape.fromTo);
    }
    // ��o�E���f�B���O�{�b�N�X���i�[
    m_initialBox.append(bBoxMap);
  }
}

//--------------------------------------------------------

void TrapezoidDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // �e���_�̕ψڃx�N�g����p��
  QPointF bottomRightVec(0.0, 0.0);
  QPointF topRightVec(0.0, 0.0);
  QPointF topLeftVec(0.0, 0.0);
  QPointF bottomLeftVec(0.0, 0.0);

  bool isShiftPressed = (e->modifiers() & Qt::ShiftModifier);
  // �n���h���ɍ��킹�ĕψڃx�N�g�����ړ�
  switch (m_handleId) {
  case Handle_BottomRight:
    bottomRightVec = pos - m_startPos;
    if (isShiftPressed)
      bottomLeftVec = QPointF(-bottomRightVec.x(), bottomRightVec.y());
    break;
  case Handle_TopRight:
    topRightVec = pos - m_startPos;
    if (isShiftPressed)
      bottomRightVec = QPointF(topRightVec.x(), -topRightVec.y());
    break;
  case Handle_TopLeft:
    topLeftVec = pos - m_startPos;
    if (isShiftPressed) topRightVec = QPointF(-topLeftVec.x(), topLeftVec.y());
    break;
  case Handle_BottomLeft:
    bottomLeftVec = pos - m_startPos;
    if (isShiftPressed)
      topLeftVec = QPointF(bottomLeftVec.x(), -bottomLeftVec.y());
    break;
  }

  // �e�V�F�C�v�̊e�|�C���g�ɂ��āA�S�̕ψڃx�N�g���̐��`��Ԃ𑫂����Ƃňړ�������

  // �e�V�F�C�v�ɕό`��K�p
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // �V�F�C�v�𓾂�
    OneShape shape = m_shapes.at(s);
    // IwShape* shape = m_shapes.at(s);
    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, QRectF> bBoxMap              = m_initialBox.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // ���ꂩ��ό`���s���`��f�[�^
      BezierPointList trapezoidedBPointList = i.value();
      // �ό`�̊�o�E���f�B���O�{�b�N�X
      QRectF tmpBBox = bBoxMap.value(frame);
      // �ό`
      for (int bp = 0; bp < trapezoidedBPointList.count(); bp++) {
        trapezoidedBPointList[bp].pos =
            map(tmpBBox, bottomRightVec, topRightVec, topLeftVec, bottomLeftVec,
                trapezoidedBPointList[bp].pos);
        trapezoidedBPointList[bp].firstHandle =
            map(tmpBBox, bottomRightVec, topRightVec, topLeftVec, bottomLeftVec,
                trapezoidedBPointList[bp].firstHandle);
        trapezoidedBPointList[bp].secondHandle =
            map(tmpBBox, bottomRightVec, topRightVec, topLeftVec, bottomLeftVec,
                trapezoidedBPointList[bp].secondHandle);
      }

      // �L�[���i�[
      shape.shapePairP->setFormKey(frame, shape.fromTo, trapezoidedBPointList);

      ++i;
    }
  }
}

//--------------------------------------------------------
// �S�̕ψڃx�N�g���̐��`��Ԃ𑫂����Ƃňړ�������
//--------------------------------------------------------

QPointF TrapezoidDragTool::map(const QRectF& bBox,
                               const QPointF& bottomRightVec,
                               const QPointF& topRightVec,
                               const QPointF& topLeftVec,
                               const QPointF& bottomLeftVec,
                               QPointF& targetPoint) {
  double hRatio = (targetPoint.x() - bBox.left()) / bBox.width();
  double vRatio = (targetPoint.y() - bBox.top()) / bBox.height();

  QPointF moveVec = (1.0 - hRatio) * (1.0 - vRatio) * topLeftVec +
                    (1.0 - hRatio) * vRatio * bottomLeftVec +
                    hRatio * (1.0 - vRatio) * topRightVec +
                    hRatio * vRatio * bottomRightVec;

  return targetPoint + moveVec;
}

//--------------------------------------------------------
// �n���h�����ʂ̃N���b�N �� �g��^�k�����[�h
//--------------------------------------------------------

ScaleDragTool::ScaleDragTool(OneShape shape, TransformHandleId handleId,
                             const QList<OneShape>& selectedShapes)
    : TransformDragTool(shape, handleId, selectedShapes)
    , m_scaleHorizontal(true)
    , m_scaleVertical(true) {
  if (m_handleId == Handle_Right || m_handleId == Handle_Left ||
      m_handleId == Handle_RightEdge || m_handleId == Handle_LeftEdge)
    m_scaleVertical = false;
  else if (m_handleId == Handle_Top || m_handleId == Handle_Bottom ||
           m_handleId == Handle_TopEdge || m_handleId == Handle_BottomEdge)
    m_scaleHorizontal = false;
}

//--------------------------------------------------------

void ScaleDragTool::onClick(const QPointF& pos, const QMouseEvent* e) {
  m_startPos = pos;
  // �g��k���̊�ʒu
  if (m_tool->IsScaleAroundCenter())
    m_anchorPos = getCenterPos();
  else
    m_anchorPos = getOppositeHandlePos();

  // �e�V�F�C�v�^�t���[���ł́A�g��k���̒��S�̍��W���i�[
  // �e�V�F�C�v�ɂ���
  OneShape tmpShape = m_grabbingShape;
  int tmpFrame      = m_project->getViewFrame();
  for (int s = 0; s < m_shapes.size(); s++) {
    // �V�F�C�v���ƂɊg��k���̒��S�����ꍇ�A�V�F�C�v��؂�ւ�
    if (m_tool->IsShapeIndependentTransforms()) tmpShape = m_shapes.at(s);

    QList<int> frames = m_orgPoints.at(s).keys();
    QMap<int, QPointF> centerPosMap;
    // �e�t���[���ɂ���
    for (int f = 0; f < frames.size(); f++) {
      // �t���[�����Ƃɉ�]���S���قȂ�ꍇ�A�t���[����؂�ւ�
      if (m_tool->IsFrameIndependentTransforms()) tmpFrame = frames.at(f);

      // ��]���S��BBox�̒��S���A�n���h���̔��Α���
      if (m_tool->IsScaleAroundCenter())
        centerPosMap[frames.at(f)] = getCenterPos(tmpShape, tmpFrame);
      else
        centerPosMap[frames.at(f)] = getOppositeHandlePos(tmpShape, tmpFrame);
    }
    // ��]���S���i�[
    m_scaleCenter.append(centerPosMap);
  }
}

//--------------------------------------------------------

void ScaleDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // �c���̕ό`�{�����v�Z����
  QPointF orgVector = pos - m_anchorPos;
  QPointF dstVector = m_startPos - m_anchorPos;

  QPointF scaleRatio =
      QPointF(orgVector.x() / dstVector.x(), orgVector.y() / dstVector.y());

  // �n���h���ɂ���Ĕ{���𐧌�����
  if (!m_scaleVertical)
    scaleRatio.setY((e->modifiers() & Qt::ShiftModifier) ? scaleRatio.x()
                                                         : 1.0);
  else if (!m_scaleHorizontal)
    scaleRatio.setX((e->modifiers() & Qt::ShiftModifier) ? scaleRatio.y()
                                                         : 1.0);
  else {
    if (e->modifiers() & Qt::ShiftModifier) {
      double largerScale = (abs(scaleRatio.x()) > abs(scaleRatio.y()))
                               ? scaleRatio.x()
                               : scaleRatio.y();
      scaleRatio         = QPointF(largerScale, largerScale);
    }
  }

  // TODO: �{�����O�ɋ߂�����ꍇ��return

  // �e�V�F�C�v�ɕό`��K�p
  for (int s = 0; s < m_orgPoints.size(); s++) {
    // �V�F�C�v���擾
    OneShape shape = m_shapes.at(s);

    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, QPointF> scaleCenterMap      = m_scaleCenter.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // ���ꂩ��ό`���s���`��f�[�^
      BezierPointList scaledBPointList = i.value();
      // �g��k���̒��S
      QPointF tmpCenter = scaleCenterMap.value(frame);

      // �{���ɍ��킹�ĕό`��K�p���Ă���
      QTransform affine;
      affine = affine.translate(tmpCenter.x(), tmpCenter.y());
      affine = affine.scale(scaleRatio.x(), scaleRatio.y());
      affine = affine.translate(-tmpCenter.x(), -tmpCenter.y());
      // �ό`
      for (int bp = 0; bp < scaledBPointList.count(); bp++) {
        scaledBPointList[bp].pos = affine.map(scaledBPointList.at(bp).pos);
        scaledBPointList[bp].firstHandle =
            affine.map(scaledBPointList.at(bp).firstHandle);
        scaledBPointList[bp].secondHandle =
            affine.map(scaledBPointList.at(bp).secondHandle);
      }
      // �L�[���i�[
      shape.shapePairP->setFormKey(frame, shape.fromTo, scaledBPointList);

      ++i;
    }
  }
}

//--------------------------------------------------------
// �V�F�C�v�̃n���h���ȊO���N���b�N  �� �ړ����[�h�i�{Shift�ŕ��s�ړ��j
//--------------------------------------------------------

TranslateDragTool::TranslateDragTool(OneShape shape, TransformHandleId handleId,
                                     const QList<OneShape>& selectedShapes)
    : TransformDragTool(shape, handleId, selectedShapes) {}

//--------------------------------------------------------

void TranslateDragTool::onClick(const QPointF& pos, const QMouseEvent*) {
  m_startPos = pos;
}

//--------------------------------------------------------

void TranslateDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // �ړ��x�N�g���𓾂�
  QPointF moveVec = pos - m_startPos;

  // Shift��������Ă�����ړ��ʂ��傫�����̂ݎc��
  if (e->modifiers() & Qt::ShiftModifier) {
    // X�����ɕ��s�ړ�
    if (abs(moveVec.x()) > abs(moveVec.y())) moveVec.setY(0.0);
    // Y�����ɕ��s�ړ�
    else
      moveVec.setX(0.0);
  }

  // �ړ��ɍ��킹�ό`��K�p���Ă���
  QTransform affine;
  affine = affine.translate(moveVec.x(), moveVec.y());

  // �e�V�F�C�v�ɕό`��K�p
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // �V�F�C�v�𓾂�
    OneShape shape = m_shapes.at(s);
    // IwShape* shape = m_shapes.at(s);
    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // ���ꂩ��ό`���s���`��f�[�^
      BezierPointList translatedBPointList = i.value();
      // �ό`
      for (int bp = 0; bp < translatedBPointList.count(); bp++) {
        translatedBPointList[bp].pos =
            affine.map(translatedBPointList.at(bp).pos);
        translatedBPointList[bp].firstHandle =
            affine.map(translatedBPointList.at(bp).firstHandle);
        translatedBPointList[bp].secondHandle =
            affine.map(translatedBPointList.at(bp).secondHandle);
      }

      // �L�[���i�[
      shape.shapePairP->setFormKey(frame, shape.fromTo, translatedBPointList);
      ++i;
    }
  }
}

//---------------------------------------------------
// �ȉ��AUndo�R�}���h
//---------------------------------------------------

TransformDragToolUndo::TransformDragToolUndo(
    QList<OneShape>& shapes, QList<QMap<int, BezierPointList>>& beforePoints,
    QList<QMap<int, BezierPointList>>& afterPoints, QList<bool>& wasKeyFrame,
    IwProject* project, int frame)
    : m_project(project)
    , m_shapes(shapes)
    , m_beforePoints(beforePoints)
    , m_afterPoints(afterPoints)
    , m_wasKeyFrame(wasKeyFrame)
    , m_frame(frame)
    , m_firstFlag(true) {}

//---------------------------------------------------

TransformDragToolUndo::~TransformDragToolUndo() {}

//---------------------------------------------------

void TransformDragToolUndo::undo() {
  // �e�V�F�C�v�Ƀ|�C���g���Z�b�g
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    // ����O�̓L�[�t���[���łȂ������ꍇ�A�L�[������
    if (!m_wasKeyFrame.at(s))
      shape.shapePairP->removeFormKey(m_frame, shape.fromTo);
    else {
      QMap<int, BezierPointList> beforeFormKeys = m_beforePoints.at(s);
      QMap<int, BezierPointList>::iterator i    = beforeFormKeys.begin();
      while (i != beforeFormKeys.end()) {
        shape.shapePairP->setFormKey(i.key(), shape.fromTo, i.value());
        ++i;
      }
    }
  }

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // �ύX�����t���[����invalidate
    for (auto shape : m_shapes) {
      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }
}

//---------------------------------------------------

void TransformDragToolUndo::redo() {
  // Undo�o�^���ɂ�redo���s��Ȃ���[�ɂ���
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  // �e�V�F�C�v�Ƀ|�C���g���Z�b�g
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    QMap<int, BezierPointList> afterFormKeys = m_afterPoints.at(s);
    QMap<int, BezierPointList>::iterator i   = afterFormKeys.begin();
    while (i != afterFormKeys.end()) {
      shape.shapePairP->setFormKey(i.key(), shape.fromTo, i.value());
      ++i;
    }
  }

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();

    // �ύX�����t���[����invalidate
    for (auto shape : m_shapes) {
      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }
}
