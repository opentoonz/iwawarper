//--------------------------------------------------------
// Reshape Tool�p�� �h���b�O�c�[��
// �R���g���[���|�C���g�ҏW�c�[��
//--------------------------------------------------------

#include "pointdragtool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwlayerhandle.h"
#include "iwlayer.h"
#include "iwundomanager.h"
#include "transformdragtool.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"
#include "viewsettings.h"
#include "reshapetool.h"
#include "sceneviewer.h"
#include "iwtrianglecache.h"

#include <QTransform>
#include <QMouseEvent>

namespace {
// Currently the multi frame mode is not available
// MultiFrame���[�h��ON���ǂ�����Ԃ�
bool isMultiFrameActivated() {
  return false;
  // return
  // CommandManager::instance()->getAction(VMI_MultiFrameMode)->isChecked();
}
};  // namespace

//--------------------------------------------------------

PointDragTool::PointDragTool() : m_undoEnabled(true) {
  m_project = IwApp::instance()->getCurrentProject()->getProject();
  m_layer   = IwApp::instance()->getCurrentLayer()->getLayer();
}

//--------------------------------------------------------
//--------------------------------------------------------
// ���s�ړ��c�[��
//--------------------------------------------------------

TranslatePointDragTool::TranslatePointDragTool(const QSet<int>& points,
                                               QPointF& grabbedPointOrgPos,
                                               const QPointF& onePix,
                                               OneShape clickedShape,
                                               int clickedPointIndex)
    : m_grabbedPointOrgPos(grabbedPointOrgPos)
    , m_onePixLength(onePix)
    , m_snapHGrid(-1)
    , m_snapVGrid(-1)
    , m_shape(clickedShape)
    , m_pointIndex(clickedPointIndex)
    , m_handleSnapped(false) {
  // �ւ��V�F�C�v�����X�g�Ɋi�[����
  QSet<int>::const_iterator i = points.constBegin();
  while (i != points.constEnd()) {
    int name       = *i;
    OneShape shape = m_layer->getShapePairFromName(name);

    int pointIndex = (int)((name % 10000) / 10);

    // �V���ȃV�F�C�v�̏ꍇ�A�ǉ�
    if (!m_shapes.contains(shape)) {
      m_shapes.append(shape);
      QList<int> list;
      list << pointIndex;
      m_pointsInShapes.append(list);
    } else {
      m_pointsInShapes[m_shapes.indexOf(shape)].append(pointIndex);
    }
    ++i;
  }

  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();
  // MultiFrame�̏��𓾂�
  QList<int> multiFrameList = m_project->getMultiFrames();

  // ���̌`��ƁA�L�[�t���[�����������ǂ������擾����
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);
    // �L�[�t���[�����������ǂ���
    m_wasKeyFrame.push_back(shape.shapePairP->isFormKey(frame, shape.fromTo));
    // m_wasKeyFrame.push_back(m_shapes.at(s)->isFormKey(frame));

    QMap<int, BezierPointList> formKeyMap;
    // �܂��A�ҏW���̃t���[��
    formKeyMap[frame] =
        shape.shapePairP->getBezierPointList(frame, shape.fromTo);
    // �����āAMultiFrame��ON�̂Ƃ��AMultiFrame�ŁA���L�[�t���[�����i�[
    if (isMultiFrameActivated()) {
      for (int mf = 0; mf < multiFrameList.size(); mf++) {
        int mframe = multiFrameList.at(mf);
        // �J�����g�t���[���͔�΂�
        if (mframe == frame) continue;
        // �L�[�t���[���Ȃ�A�o�^
        if (shape.shapePairP->isFormKey(mframe, shape.fromTo))
          formKeyMap[mframe] =
              shape.shapePairP->getBezierPointList(mframe, shape.fromTo);
      }
    }

    // ���̌`��̓o�^
    m_orgPoints.append(formKeyMap);
  }

  // �V�O�i����Emit
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//--------------------------------------------------------
// PenTool�p
TranslatePointDragTool::TranslatePointDragTool(OneShape shape,
                                               QList<int> points,
                                               const QPointF& onePix)
    : m_onePixLength(onePix) {
  m_shapes.push_back(shape);

  QList<int> list;
  for (auto pId : points) {
    list.push_back((int)(pId / 10));
  }
  m_pointsInShapes.push_back(list);
  m_grabbedPointOrgPos =
      shape.getBezierPointList(m_project->getViewFrame()).at(list.last()).pos;

  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  QMap<int, BezierPointList> formKeyMap;
  formKeyMap[frame] = m_shapes.at(0).shapePairP->getBezierPointList(
      frame, m_shapes.at(0).fromTo);
  m_orgPoints.push_back(formKeyMap);

  m_undoEnabled = false;
}

//--------------------------------------------------------

void TranslatePointDragTool::onClick(const QPointF& pos,
                                     const QMouseEvent* /*e*/) {
  m_startPos = pos;
}

//--------------------------------------------------------
// �\�����̃V�F�C�v�̃|�C���g�ɃX�i�b�v����
// ���ɂ��X�i�b�v�ł���悤�ɂ�������
// �R���g���[���|�C���g�ɃX�i�b�v�����ꍇ��index��Ԃ�

int TranslatePointDragTool::calculateSnap(QPointF& pos) {
  int ret = -1;
  // ���݂̃t���[��
  int frame             = m_project->getViewFrame();
  IwLayer* currentLayer = IwApp::instance()->getCurrentLayer()->getLayer();

  double thresholdPixels = 10.0;

  QPointF thres_dist = thresholdPixels * m_onePixLength;

  // ���b�N���𓾂�
  bool fromToVisible[2];
  for (int fromTo = 0; fromTo < 2; fromTo++)
    fromToVisible[fromTo] =
        m_project->getViewSettings()->isFromToVisible(fromTo);

  double minimumW    = 0.0;
  double minimumDist = (thres_dist.x() + thres_dist.y()) * 0.5;

  // �e���C���ɂ���
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    // ���C�����擾
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;

    bool isCurrent = (layer == currentLayer);
    // ���݂̃��C������\���Ȃ�continue
    // �������J�����g���C���̏ꍇ�͕\������
    if (!isCurrent && !layer->isVisibleInViewer()) continue;

    for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
      ShapePair* shapePair = layer->getShapePair(sp);
      if (!shapePair) continue;
      if (!shapePair->isVisible()) continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // ���b�N����Ă��Ĕ�\���Ȃ�X�L�b�v
        if (!fromToVisible[fromTo]) continue;
        // �h���b�O���̃V�F�C�v�Ȃ�X�L�b�v
        if (m_shapes.contains(OneShape(shapePair, fromTo))) continue;
        // �o�E���f�B���O�{�b�N�X�ɓ����Ă��Ȃ�������X�L�b�v
        QRectF bBox = shapePair->getBBox(frame, fromTo)
                          .adjusted(-thres_dist.x(), -thres_dist.y(),
                                    thres_dist.x(), thres_dist.y());
        if (!bBox.contains(pos)) continue;
        double dist;
        double w = shapePair->getNearestBezierPos(pos, frame, fromTo, dist);
        if (dist < minimumDist) {
          double pointBefore = std::floor(w);
          QPointF posBefore =
              shapePair->getBezierPosFromValue(frame, fromTo, pointBefore);

          double pointAfter = std::ceil(w);
          if (shapePair->isClosed() &&
              (int)pointAfter == shapePair->getBezierPointAmount())
            pointAfter = 0.0;
          QPointF posAfter =
              shapePair->getBezierPosFromValue(frame, fromTo, pointAfter);

          if (std::abs(posBefore.x() - pos.x()) < thres_dist.x() &&
              std::abs(posBefore.y() - pos.y()) < thres_dist.y()) {
            minimumW = pointBefore;
            ret      = (int)pointBefore;
          } else if (std::abs(posAfter.x() - pos.x()) < thres_dist.x() &&
                     std::abs(posAfter.y() - pos.y()) < thres_dist.y()) {
            minimumW = pointAfter;
            ret      = (int)minimumW;
          } else
            minimumW = w;
          minimumDist  = dist;
          m_snapTarget = OneShape(shapePair, fromTo);
        }
      }
    }
  }

  // �V�F�C�v�ɃX�i�b�v�����Ƃ�
  if (m_snapTarget.shapePairP != nullptr) {
    pos = m_snapTarget.shapePairP->getBezierPosFromValue(
        frame, m_snapTarget.fromTo, minimumW);
    return ret;
  }

  // ���ɁA�K�C�h�ւ̃X�i�b�v����������
  IwProject::Guides hGuide = m_project->getHGuides();
  double minDist           = thres_dist.x();
  double snappedPos        = 0.;
  for (int gId = 0; gId < hGuide.size(); gId++) {
    double g    = hGuide[gId];
    double dist = std::abs(pos.x() - g);
    if (dist <= minDist) {
      minDist     = dist;
      m_snapHGrid = gId;
      snappedPos  = g;
    }
  }
  if (m_snapHGrid >= 0) pos.setX(snappedPos);

  IwProject::Guides vGuide = m_project->getVGuides();
  minDist                  = thres_dist.y();
  for (int gId = 0; gId < vGuide.size(); gId++) {
    double g    = vGuide[gId];
    double dist = std::abs(pos.y() - g);
    if (dist <= minDist) {
      minDist     = dist;
      m_snapVGrid = gId;
      snappedPos  = g;
    }
  }
  if (m_snapVGrid >= 0) pos.setY(snappedPos);
  return ret;
}

//--------------------------------------------------------

void TranslatePointDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  m_handleSnapped = false;

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

  // �����ɃX�i�b�v����������
  m_snapTarget       = OneShape();
  m_snapHGrid        = -1;
  m_snapVGrid        = -1;
  int snapPointIndex = -1;
  if (e->modifiers() & Qt::ControlModifier) {
    QPointF newPos = m_startPos + moveVec;
    snapPointIndex = calculateSnap(newPos);
    if (m_snapTarget.shapePairP != nullptr || m_snapHGrid >= 0 ||
        m_snapVGrid >= 0) {
      moveVec = newPos - m_grabbedPointOrgPos;
    }
  }

  bool altPressed = (e->modifiers() & Qt::AltModifier);

  // �e�R���g���[���|�C���g�Ɉړ���K�p
  for (int s = 0; s < m_pointsInShapes.size(); s++) {
    QList<int> points = m_pointsInShapes[s];
    // �V�F�C�v�𓾂�
    OneShape shape = m_shapes[s];

    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // ���ꂩ��ό`���s���`��f�[�^
      BezierPointList pointList = i.value();
      // �ό`
      for (auto pointIndex : points) {
        pointList[pointIndex].pos += moveVec;
        pointList[pointIndex].firstHandle += moveVec;
        pointList[pointIndex].secondHandle += moveVec;
        // �n���h�����ꏏ�ɃX�i�b�v����
        if (snapPointIndex >= 0 && altPressed && shape == m_shape &&
            pointIndex == m_pointIndex) {
          // �ǂ���̃n���h���ɋ߂Â��邩�A�ړ��������߂�����I��
          BezierPoint snapPoint =
              m_snapTarget.shapePairP
                  ->getBezierPointList(frame, m_snapTarget.fromTo)
                  .at(snapPointIndex);
          double len1 =
              (pointList[pointIndex].firstHandle - snapPoint.firstHandle)
                  .manhattanLength() +
              (pointList[pointIndex].secondHandle - snapPoint.secondHandle)
                  .manhattanLength();
          double len2 =
              (pointList[pointIndex].firstHandle - snapPoint.secondHandle)
                  .manhattanLength() +
              (pointList[pointIndex].secondHandle - snapPoint.firstHandle)
                  .manhattanLength();
          if (len1 < len2) {
            pointList[pointIndex].firstHandle  = snapPoint.firstHandle;
            pointList[pointIndex].secondHandle = snapPoint.secondHandle;
          } else {
            pointList[pointIndex].firstHandle  = snapPoint.secondHandle;
            pointList[pointIndex].secondHandle = snapPoint.firstHandle;
          }
          m_handleSnapped = true;
        }
      }

      // �L�[���i�[
      shape.shapePairP->setFormKey(frame, shape.fromTo, pointList);

      ++i;
    }
  }
  // �X�V�V�O�i�����G�~�b�g����
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//--------------------------------------------------------

// Click������}�E�X�ʒu���ς���Ă��Ȃ����false��Ԃ��悤�ɂ���
bool TranslatePointDragTool::onRelease(const QPointF& pos, const QMouseEvent*) {
  // �����Ă��Ȃ���� false
  if (pos == m_startPos) return false;

  // PenTool�̏ꍇ�͂�����return
  if (!m_undoEnabled) return true;

  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  QList<QMap<int, BezierPointList>> afterPoints;
  // �ό`��̃V�F�C�v���擾
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // �V�F�C�v���擾
    OneShape shape = m_shapes.at(s);

    QList<int> frameList = m_orgPoints.at(s).keys();
    QMap<int, BezierPointList> afterFormKeyMap;
    for (int f = 0; f < frameList.size(); f++) {
      int tmpFrame = frameList.at(f);
      afterFormKeyMap[tmpFrame] =
          shape.shapePairP->getBezierPointList(tmpFrame, shape.fromTo);
    }

    afterPoints.push_back(afterFormKeyMap);
  }

  // Transform Tool�p��Undo���g���܂킷
  // Undo�ɓo�^ ������redo���Ă΂�邪�A�ŏ��̓t���O�ŉ������
  IwUndoManager::instance()->push(new TransformDragToolUndo(
      m_shapes, m_orgPoints, afterPoints, m_wasKeyFrame, m_project, frame));

  // �ύX�����t���[����invalidate
  for (auto shape : m_shapes) {
    int start, end;
    shape.shapePairP->getFormKeyRange(start, end, frame, shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, m_project->getParentShape(shape.shapePairP));
  }
  return true;
}

bool TranslatePointDragTool::setSpecialShapeColor(OneShape shape) {
  return (shape == m_snapTarget);
}

bool TranslatePointDragTool::setSpecialGridColor(int gId, bool isVertical) {
  return gId == ((isVertical) ? m_snapVGrid : m_snapHGrid);
}

void TranslatePointDragTool::draw(const QPointF& onePixelLength) {
  if (!m_snapTarget.shapePairP) return;
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // �\���t���[���𓾂�
  int frame = project->getViewFrame();

  if (m_handleSnapped) {
    BezierPointList bPList =
        m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);

    glColor3d(1.0, 0.5, 1.0);
    ReshapeTool::drawControlPoint(m_shape, bPList, m_pointIndex, true,
                                  onePixelLength);
  } else {
    BezierPointList bPList =
        m_snapTarget.shapePairP->getBezierPointList(frame, m_snapTarget.fromTo);

    glColor3d(1.0, 0.0, 1.0);
    for (int p = 0; p < bPList.size(); p++) {
      ReshapeTool::drawControlPoint(m_snapTarget, bPList, p, false,
                                    onePixelLength);
    }
  }
}

//--------------------------------------------------------
//--------------------------------------------------------
// �n���h������c�[��
//--------------------------------------------------------

TranslateHandleDragTool::TranslateHandleDragTool(int name,
                                                 const QPointF& onePix)
    : m_onePixLength(onePix), m_isInitial(false) {
  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  m_shape = m_layer->getShapePairFromName(name);

  m_pointIndex  = (int)((name % 10000) / 10);
  m_handleIndex = name % 10;  // 2 : firstHandle, 3 : secondHandle

  m_orgPoints   = m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);
  m_wasKeyFrame = m_shape.shapePairP->isFormKey(frame, m_shape.fromTo);
}

//--------------------------------------------------------

TranslateHandleDragTool::TranslateHandleDragTool(OneShape shape, int pointIndex,
                                                 int handleIndex,
                                                 const QPointF& onePix)
    : m_onePixLength(onePix)
    , m_shape(shape)
    , m_pointIndex(pointIndex)
    , m_handleIndex(handleIndex)
    , m_isInitial(false) {
  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  m_orgPoints   = m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);
  m_wasKeyFrame = m_shape.shapePairP->isFormKey(frame, m_shape.fromTo);
}

//--------------------------------------------------------

// PenTool�p
TranslateHandleDragTool::TranslateHandleDragTool(OneShape shape, int name,
                                                 const QPointF& onePix)
    : m_onePixLength(onePix), m_shape(shape), m_isInitial(false) {
  m_pointIndex  = (int)(name / 10);
  m_handleIndex = name % 10;  // 2 : firstHandle, 3 : secondHandle

  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  m_orgPoints = m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);

  m_undoEnabled = false;
}

//--------------------------------------------------------

void TranslateHandleDragTool::onClick(const QPointF& pos, const QMouseEvent*) {
  m_startPos = pos;
}

//--------------------------------------------------------

void TranslateHandleDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  BezierPoint point = m_orgPoints[m_pointIndex];

  QPointF touchedPoint =
      (m_handleIndex == 2) ? point.firstHandle : point.secondHandle;

  // ���̃x�N�g��
  QPointF orgVec = touchedPoint - point.pos;

  // �V�����ʒu�ƃA���O��
  // �ψ�
  QPointF moveVec = pos - m_startPos;
  // �V�����ʒu
  QPointF newHandlePoint = touchedPoint + moveVec;
  // �V�����x�N�g��
  QPointF newVec = newHandlePoint - point.pos;

  m_snapCandidates.clear();
  m_isHandleSnapped = false;

  // �C���L�[�ɍ��킹�ă|�C���g��ҏW����
  //  Alt�L�[ : ���Α��̃n���h���ɉe�����Ȃ�
  if (e->modifiers() & Qt::AltModifier) {
    // �����ŁA�ʒu����v���Ă��鑼�̃V�F�C�v�̒��_���������ꍇ�A
    // �n���h���̃X�i�b�v���\�ɂ���
    if (e->modifiers() & Qt::ControlModifier)
      calculateHandleSnap(point.pos, newHandlePoint);

    if (m_handleIndex == 2)  // firstHandle����������
      point.firstHandle = newHandlePoint;
    else  // secondHandle����������
      point.secondHandle = newHandlePoint;
  }
  // Shift�L�[�F�p�x�ς����A�L�΂�����
  else if (e->modifiers() & Qt::ShiftModifier && !orgVec.isNull()) {
    double orgLength =
        std::sqrt(orgVec.x() * orgVec.x() + orgVec.y() * orgVec.y());
    double newLength =
        std::sqrt(newVec.x() * newVec.x() + newVec.y() * newVec.y());

    QTransform affine;
    affine = affine.translate(point.pos.x(), point.pos.y());
    affine = affine.scale(newLength / orgLength, newLength / orgLength);
    affine = affine.translate(-point.pos.x(), -point.pos.y());

    if (m_handleIndex == 2)
      point.firstHandle = affine.map(point.firstHandle);
    else
      point.secondHandle = affine.map(point.secondHandle);
  }
  // Ctrl�L�[�F���Α����_�Ώ̕����ɐL�΂�
  // m_isInitial : PenTool�p�B�|�C���g�𑝂₵���Ɠ����ɐL�΂��n���h���́A
  // Ctrl�������ĐL�΂��Ă��邩�̂悤�ɂӂ�܂��B
  else if (e->modifiers() & Qt::ControlModifier || m_isInitial) {
    if (m_handleIndex == 2) {
      point.firstHandle  = newHandlePoint;
      point.secondHandle = point.pos - newVec;
    } else {
      point.secondHandle = newHandlePoint;
      point.firstHandle  = point.pos - newVec;
    }
  }
  // �C���L�[�Ȃ��F���Α��͓������ĉ�]
  else {
    double orgAngle = (orgVec.isNull())
                          ? 0.0
                          : std::atan2(orgVec.y() / m_onePixLength.y(),
                                       orgVec.x() / m_onePixLength.x());
    double newAngle = (newVec.isNull())
                          ? 0.0
                          : std::atan2(newVec.y() / m_onePixLength.y(),
                                       newVec.x() / m_onePixLength.x());

    QTransform affine;
    affine = affine.translate(point.pos.x(), point.pos.y());
    affine = affine.scale(m_onePixLength.x(), m_onePixLength.y());
    affine = affine.rotateRadians(newAngle - orgAngle);
    affine = affine.scale(1.0 / m_onePixLength.x(), 1.0 / m_onePixLength.y());
    affine = affine.translate(-point.pos.x(), -point.pos.y());

    if (m_handleIndex == 2) {
      point.firstHandle  = newHandlePoint;
      point.secondHandle = affine.map(point.secondHandle);
    } else {
      point.secondHandle = newHandlePoint;
      point.firstHandle  = affine.map(point.firstHandle);
    }
  }

  BezierPointList newBPList = m_orgPoints;

  newBPList.replace(m_pointIndex, point);

  // �L�[���i�[
  m_shape.shapePairP->setFormKey(frame, m_shape.fromTo, newBPList);
  // �X�V�V�O�i�����G�~�b�g����
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();

  // �ύX�����t���[����invalidate
  int start, end;
  m_shape.shapePairP->getFormKeyRange(start, end, frame, m_shape.fromTo);
  IwTriangleCache::instance()->invalidateCache(
      start, end, m_project->getParentShape(m_shape.shapePairP));
}

//--------------------------------------------------------

bool TranslateHandleDragTool::onRelease(const QPointF& pos,
                                        const QMouseEvent* /*e*/) {
  if (pos == m_startPos) return false;

  // PenTool�̏ꍇreturn
  if (!m_undoEnabled) return true;

  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  QList<OneShape> shapes;
  // QList<IwShape*> shapes;
  shapes.push_back(m_shape);

  QList<QMap<int, BezierPointList>> beforePoints;
  QMap<int, BezierPointList> bpMap;
  bpMap[frame] = m_orgPoints;
  beforePoints.push_back(bpMap);

  QList<QMap<int, BezierPointList>> afterPoints;
  QMap<int, BezierPointList> afterBpMap;
  afterBpMap[frame] =
      m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);
  // afterBpMap[frame] = m_shape->getBezierPointList(frame);
  afterPoints.push_back(afterBpMap);

  QList<bool> wasKey;
  wasKey.push_back(m_wasKeyFrame);

  // Transform Tool�p��Undo���g���܂킷
  // Undo�ɓo�^ ������redo���Ă΂�邪�A�ŏ��̓t���O�ŉ������
  IwUndoManager::instance()->push(new TransformDragToolUndo(
      shapes, beforePoints, afterPoints, wasKey, m_project, frame));
  return true;
}

//--------------------------------------------------------

void TranslateHandleDragTool::calculateHandleSnap(const QPointF pointPos,
                                                  QPointF& handlePos) {
  // ���݂̃t���[��
  int frame             = m_project->getViewFrame();
  IwLayer* currentLayer = IwApp::instance()->getCurrentLayer()->getLayer();

  double thresholdPixels = 10.0;

  QPointF thres_dist = thresholdPixels * m_onePixLength;

  // ���b�N���𓾂�
  bool fromToVisible[2];
  for (int fromTo = 0; fromTo < 2; fromTo++)
    fromToVisible[fromTo] =
        m_project->getViewSettings()->isFromToVisible(fromTo);

  double minimumDist  = (thres_dist.x() + thres_dist.y()) * 0.5;
  double minimumDist2 = minimumDist * minimumDist;

  QPointF tmpSnappedPos;

  // �e���C���ɂ���
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    // ���C�����擾
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;

    bool isCurrent = (layer == currentLayer);
    // ���݂̃��C������\���Ȃ�continue
    // �������J�����g���C���̏ꍇ�͕\������
    if (!isCurrent && !layer->isVisibleInViewer()) continue;

    for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
      ShapePair* shapePair = layer->getShapePair(sp);
      if (!shapePair) continue;
      if (!shapePair->isVisible()) continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // ���b�N����Ă��Ĕ�\���Ȃ�X�L�b�v
        if (!fromToVisible[fromTo]) continue;
        // �h���b�O���̃V�F�C�v�Ȃ�X�L�b�v
        if (m_shape == OneShape(shapePair, fromTo)) continue;

        BezierPointList bpList = shapePair->getBezierPointList(frame, fromTo);
        for (int index = 0; index < bpList.size(); index++) {
          BezierPoint bp = bpList[index];
          // �|�C���g�ʒu����v���Ă��Ȃ���� continue
          if ((pointPos - bp.pos).manhattanLength() > 0.0001) continue;
          // ���ɓ����
          m_snapCandidates.append({OneShape(shapePair, fromTo), index});

          double dist2;
          // �n���h���P��
          QPointF VecToFirstHandle(bp.firstHandle - handlePos);
          dist2 = QPointF::dotProduct(VecToFirstHandle, VecToFirstHandle);
          if (dist2 < minimumDist2) {
            tmpSnappedPos     = bp.firstHandle;
            minimumDist2      = dist2;
            m_isHandleSnapped = true;
          }
          // �n���h���Q��
          QPointF VecToSecondHandle(bp.secondHandle - handlePos);
          dist2 = QPointF::dotProduct(VecToSecondHandle, VecToSecondHandle);
          if (dist2 < minimumDist2) {
            tmpSnappedPos     = bp.secondHandle;
            minimumDist2      = dist2;
            m_isHandleSnapped = true;
          }
        }
      }
    }
  }

  if (m_isHandleSnapped) handlePos = tmpSnappedPos;
}

//--------------------------------------------------------

void TranslateHandleDragTool::draw(const QPointF& onePixelLength) {
  if (m_snapCandidates.isEmpty()) return;
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // �\���t���[���𓾂�
  int frame = project->getViewFrame();

  glColor3d(1.0, 0.0, 1.0);
  for (auto snapCandidate : m_snapCandidates) {
    BezierPointList bPList = snapCandidate.shape.shapePairP->getBezierPointList(
        frame, snapCandidate.shape.fromTo);
    ReshapeTool::drawControlPoint(snapCandidate.shape, bPList,
                                  snapCandidate.pointIndex, true,
                                  onePixelLength);
  }

  if (m_isHandleSnapped) {
    BezierPointList bPList =
        m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);

    glColor3d(1.0, 0.5, 1.0);
    glLineWidth(1.5);
    ReshapeTool::drawControlPoint(m_shape, bPList, m_pointIndex, true,
                                  onePixelLength);
    glLineWidth(1);
  }
}
