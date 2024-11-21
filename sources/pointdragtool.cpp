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
        // �o�E���f�B���O�{�b�N�X�ɓ����Ă��Ȃ�������X�L�b�v
        QRectF bBox = shapePair->getBBox(frame, fromTo)
                          .adjusted(-thres_dist.x(), -thres_dist.y(),
                                    thres_dist.x(), thres_dist.y());
        if (!bBox.contains(pos)) continue;

        double dist = 10000.0, w;
        // �h���b�O���̃V�F�C�v�̏ꍇ�A�����Ȃ��|�C���g�ɂ̓X�i�b�v�ł���
        if (m_shapes.contains(OneShape(shapePair, fromTo))) {
          QList<int> activePoints =
              m_pointsInShapes[m_shapes.indexOf(OneShape(shapePair, fromTo))];
          // �V�F�C�v�̑S�Ẵ|�C���g��I�����Ă���ꍇ�̓X�L�b�v
          if (shapePair->getBezierPointAmount() == activePoints.count())
            continue;
          int tmpRangeBefore = -1;
          // �e�|�C���g�����[�v
          for (int bp = 0; bp < shapePair->getBezierPointAmount(); bp++) {
            // ��I���̃|�C���g�Ȃ�
            if (!activePoints.contains(bp)) {
              // �J�n�|�C���g�C���f�b�N�X��-1�Ȃ�J�n�|�C���g�C���f�b�N�X�Ɏw�肷��
              if (tmpRangeBefore == -1) tmpRangeBefore = bp;
            }
            // �I���|�C���g�Ȃ�
            else {
              // �J�n�|�C���g�C���f�b�N�X���w�肳��Ă�����A
              if (tmpRangeBefore >= 0) {
                // �J�n�|�C���g�C���f�b�N�X���獡�̃C���f�b�N�X-1�܂ł͈̔͂ŋߖT�_���X�V
                double tmp_dist;
                double tmp_w = shapePair->getNearestBezierPos(
                    pos, frame, fromTo, (double)tmpRangeBefore,
                    (double)(bp - 1), tmp_dist);
                if (tmp_dist < dist) {
                  dist = tmp_dist;
                  w    = tmp_w;
                }
                // �C���f�b�N�X��-1�ɖ߂�
                tmpRangeBefore = -1;
              }
            }
          }
          // �Ō�̃Z�O�����g
          if (tmpRangeBefore >= 0) {
            double tmp_dist, tmp_w;
            if (shapePair->isClosed() && !activePoints.contains(0))
              tmp_w = shapePair->getNearestBezierPos(
                  pos, frame, fromTo, (double)tmpRangeBefore, 0.0, tmp_dist);
            else
              tmp_w = shapePair->getNearestBezierPos(
                  pos, frame, fromTo, (double)tmpRangeBefore,
                  (double)(shapePair->getBezierPointAmount() - 1), tmp_dist);
            if (tmp_dist < dist) {
              dist = tmp_dist;
              w    = tmp_w;
            }
          }
        }
        // �h���b�O���łȂ����ʂ̃V�F�C�v�̏ꍇ
        else
          w = shapePair->getNearestBezierPos(pos, frame, fromTo, dist);

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
          } else {  // �Ή��_�ւ̃X�i�b�v
            CorrPointList corrs   = shapePair->getCorrPointList(frame, fromTo);
            double minDiff        = 100.0;
            double nearestCorrPos = 0.;
            for (auto corrPos : corrs) {
              double tmpDiff = w - corrPos.value;
              if (abs(minDiff) > abs(tmpDiff)) {
                minDiff        = tmpDiff;
                nearestCorrPos = corrPos.value;
              }
            }
            QPointF posCorr =
                shapePair->getBezierPosFromValue(frame, fromTo, nearestCorrPos);
            if (std::abs(posCorr.x() - pos.x()) < thres_dist.x() &&
                std::abs(posCorr.y() - pos.y()) < thres_dist.y()) {
              minimumW = nearestCorrPos;
            } else  // ����ւ̃X�i�b�v
              minimumW = w;
          }
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

void TranslatePointDragTool::draw(SceneViewer* viewer,
                                  const QPointF& onePixelLength) {
  if (!m_snapTarget.shapePairP) return;
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // �\���t���[���𓾂�
  int frame = project->getViewFrame();

  if (m_handleSnapped) {
    BezierPointList bPList =
        m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);

    viewer->setColor(QColor::fromRgbF(1.0, 0.5, 1.0));
    // glColor3d(1.0, 0.5, 1.0);
    ReshapeTool::drawControlPoint(viewer, m_shape, bPList, m_pointIndex, true,
                                  onePixelLength);
  } else {
    BezierPointList bPList =
        m_snapTarget.shapePairP->getBezierPointList(frame, m_snapTarget.fromTo);

    // �Ή��_�̕`��
    viewer->setColor(QColor::fromRgbF(1.0, 1.0, 0.0));
    // glColor3d(1.0, 1.0, 0.0);
    QList<QPointF> corrPoints = m_snapTarget.shapePairP->getCorrPointPositions(
        frame, m_snapTarget.fromTo);
    for (auto corrP : corrPoints) {
      viewer->pushMatrix();
      viewer->translate(corrP.x(), corrP.y(), 0.0);
      viewer->scale(onePixelLength.x(), onePixelLength.y(), 1.0);

      static QVector3D vert[4] = {
          QVector3D(2.0, -2.0, 0.0), QVector3D(2.0, 2.0, 0.0),
          QVector3D(-2.0, 2.0, 0.0), QVector3D(-2.0, -2.0, 0.0)};

      viewer->doDrawLine(GL_LINE_LOOP, vert, 4);
      viewer->popMatrix();
    }

    // �R���g���[���|�C���g�̕`��
    viewer->setColor(QColor::fromRgbF(1.0, 0.0, 1.0));
    // glColor3d(1.0, 0.0, 1.0);
    for (int p = 0; p < bPList.size(); p++) {
      ReshapeTool::drawControlPoint(viewer, m_snapTarget, bPList, p, false,
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

void TranslateHandleDragTool::draw(SceneViewer* viewer,
                                   const QPointF& onePixelLength) {
  if (m_snapCandidates.isEmpty()) return;
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // �\���t���[���𓾂�
  int frame = project->getViewFrame();

  viewer->setColor(QColor::fromRgbF(1.0, 0.0, 1.0));

  for (auto snapCandidate : m_snapCandidates) {
    BezierPointList bPList = snapCandidate.shape.shapePairP->getBezierPointList(
        frame, snapCandidate.shape.fromTo);
    ReshapeTool::drawControlPoint(viewer, snapCandidate.shape, bPList,
                                  snapCandidate.pointIndex, true,
                                  onePixelLength);
  }

  if (m_isHandleSnapped) {
    BezierPointList bPList =
        m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);

    viewer->setColor(QColor::fromRgbF(1.0, 0.5, 1.0));

    glLineWidth(1.5);
    ReshapeTool::drawControlPoint(viewer, m_shape, bPList, m_pointIndex, true,
                                  onePixelLength);
    glLineWidth(1);
  }
}

//--------------------------------------------------------
//--------------------------------------------------------

ActivePointsDragTool::ActivePointsDragTool(TransformHandleId handleId,
                                           const QSet<int>& points,
                                           const QRectF& handleRect)
    : m_handleId(handleId), m_handleRect(handleRect) {
  m_project = IwApp::instance()->getCurrentProject()->getProject();
  m_layer   = IwApp::instance()->getCurrentLayer()->getLayer();
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

  // ���̌`��ƁA�L�[�t���[�����������ǂ������擾����
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);
    // �L�[�t���[�����������ǂ���
    m_wasKeyFrame.push_back(shape.shapePairP->isFormKey(frame, shape.fromTo));
    // m_wasKeyFrame.push_back(m_shapes.at(s)->isFormKey(frame));

    // ���̌`��̓o�^
    m_orgPoints.append(
        shape.shapePairP->getBezierPointList(frame, shape.fromTo));
  }
}

//--------------------------------------------------------
// ���񂾃n���h���̔��Α��̃n���h���ʒu��Ԃ�
//--------------------------------------------------------
QPointF ActivePointsDragTool::getOppositeHandlePos() {
  QRectF box = m_handleRect;

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
  default:
    break;
  }
  // �ی�
  return box.center();
}

//--------------------------------------------------------

bool ActivePointsDragTool::onRelease(const QPointF&, const QMouseEvent*) {
  QList<BezierPointList> afterPoints;
  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  // �ό`��̃V�F�C�v���擾
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // �V�F�C�v���擾
    OneShape shape = m_shapes.at(s);
    BezierPointList afterFormKey =
        shape.shapePairP->getBezierPointList(frame, shape.fromTo);

    afterPoints.push_back(afterFormKey);
  }

  // Undo�ɓo�^ ������redo���Ă΂�邪�A�ŏ��̓t���O�ŉ������
  IwUndoManager::instance()->push(new ActivePointsDragToolUndo(
      m_shapes, m_orgPoints, afterPoints, m_wasKeyFrame, m_project, frame));

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  // �ύX�����t���[����invalidate
  for (auto shape : m_shapes) {
    int start, end;
    shape.shapePairP->getFormKeyRange(start, end, frame, shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, m_project->getParentShape(shape.shapePairP));
  }
  return true;
}

//--------------------------------------------------------
//--------------------------------------------------------
// �n���h��Ctrl�N���b�N   �� ��]���[�h (�{Shift��45�x����)
//--------------------------------------------------------

ActivePointsRotateDragTool::ActivePointsRotateDragTool(
    TransformHandleId handleId, const QSet<int>& points,
    const QRectF& handleRect, const QPointF& onePix)
    : ActivePointsDragTool(handleId, points, handleRect)
    , m_onePixLength(onePix) {}

//--------------------------------------------------------

void ActivePointsRotateDragTool::onClick(const QPointF& pos,
                                         const QMouseEvent* /*e*/) {
  // �ŏ��̊p�x�𓾂�
  // if (m_tool->IsRotateAroundCenter())
  m_centerPos = getCenterPos();
  // else
  //   m_anchorPos = getOppositeHandlePos();
  QPointF initVec = pos - m_centerPos;

  m_initialAngle = atan2(initVec.x(), initVec.y());
}

//--------------------------------------------------------

void ActivePointsRotateDragTool::onDrag(const QPointF& pos,
                                        const QMouseEvent* e) {
  // �V���ȉ�]�p
  QPointF newVec = pos - m_centerPos;

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

  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();
  // �e�V�F�C�v�ɕό`��K�p
  for (int s = 0; s < m_orgPoints.size(); s++) {
    // �V�F�C�v�𓾂�
    OneShape shape = m_shapes.at(s);
    // IwShape* shape = m_shapes.at(s);
    BezierPointList formKey = m_orgPoints.at(s);

    // ��]�ɍ��킹�ĕό`��K�p���Ă���
    QTransform affine;
    affine = affine.translate(m_centerPos.x(), m_centerPos.y());
    affine = affine.scale(m_onePixLength.x(), m_onePixLength.y());
    affine = affine.rotate(-rotDegree);
    affine = affine.scale(1.0 / m_onePixLength.x(), 1.0 / m_onePixLength.y());
    affine = affine.translate(-m_centerPos.x(), -m_centerPos.y());
    // �ό`
    for (auto bp : m_pointsInShapes[s]) {
      // for (int bp = 0; bp < formKey.count(); bp++) {
      formKey[bp].pos          = affine.map(formKey.at(bp).pos);
      formKey[bp].firstHandle  = affine.map(formKey.at(bp).firstHandle);
      formKey[bp].secondHandle = affine.map(formKey.at(bp).secondHandle);
    }

    // �L�[���i�[
    shape.shapePairP->setFormKey(frame, shape.fromTo, formKey);
  }
}

//--------------------------------------------------------
//--------------------------------------------------------
// �c���n���h��Alt�N���b�N�@  �� Shear�ό`���[�h�i�{Shift�ŕ��s�ɕό`�j
//--------------------------------------------------------

ActivePointsShearDragTool::ActivePointsShearDragTool(TransformHandleId handleId,
                                                     const QSet<int>& points,
                                                     const QRectF& handleRect)
    : ActivePointsDragTool(handleId, points, handleRect) {}

//--------------------------------------------------------

void ActivePointsShearDragTool::onClick(const QPointF& pos,
                                        const QMouseEvent*) {
  m_startPos = pos;

  m_anchorPos = getOppositeHandlePos();

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

void ActivePointsShearDragTool::onDrag(const QPointF& pos,
                                       const QMouseEvent* e) {
  int frame = m_project->getViewFrame();
  // �e�V�F�C�v�ɕό`��K�p
  for (int s = 0; s < m_orgPoints.size(); s++) {
    // �V�F�C�v�𓾂�
    OneShape shape = m_shapes.at(s);

    BezierPointList formKey = m_orgPoints.at(s);

    QTransform affine;

    QPointF scale(1.0, 1.0);
    if (!(e->modifiers() & Qt::ShiftModifier)) {
      if (m_isVertical)
        scale.setX((pos.x() - m_anchorPos.x()) /
                   (m_startPos.x() - m_anchorPos.x()));
      else
        scale.setY((pos.y() - m_anchorPos.y()) /
                   (m_startPos.y() - m_anchorPos.y()));
    }

    // �V�A�[�ʒu
    QPointF shear(0.0, 0.0);
    if (m_isVertical) {
      shear.setY(pos.y() - m_startPos.y());
      shear.setY(shear.y() / (pos.x() - m_anchorPos.x()));
    } else {
      shear.setX(pos.x() - m_startPos.x());
      shear.setX(shear.x() / (pos.y() - m_anchorPos.y()));
    }

    affine = affine.translate(m_anchorPos.x(), m_anchorPos.y());
    affine = affine.shear(shear.x(), shear.y());
    affine = affine.scale(scale.x(), scale.y());
    affine = affine.translate(-m_anchorPos.x(), -m_anchorPos.y());

    // �ό`
    for (auto bp : m_pointsInShapes[s]) {
      // for (int bp = 0; bp < formKey.count(); bp++) {
      formKey[bp].pos          = affine.map(formKey.at(bp).pos);
      formKey[bp].firstHandle  = affine.map(formKey.at(bp).firstHandle);
      formKey[bp].secondHandle = affine.map(formKey.at(bp).secondHandle);
    }

    // �L�[���i�[
    shape.shapePairP->setFormKey(frame, shape.fromTo, formKey);
  }
}

//--------------------------------------------------------
//--------------------------------------------------------
// �΂߃n���h��Alt�N���b�N�@  �� ��`�ό`���[�h�i�{Shift�őΏ̕ό`�j
//--------------------------------------------------------

ActivePointsTrapezoidDragTool::ActivePointsTrapezoidDragTool(
    TransformHandleId handleId, const QSet<int>& points,
    const QRectF& handleRect)
    : ActivePointsDragTool(handleId, points, handleRect) {}

//--------------------------------------------------------

void ActivePointsTrapezoidDragTool::onClick(const QPointF& pos,
                                            const QMouseEvent*) {
  m_startPos = pos;

  // ��`�ό`�̊�ƂȂ�o�E���f�B���O�{�b�N�X���i�[
  m_initialBox = m_handleRect;
}

//--------------------------------------------------------

void ActivePointsTrapezoidDragTool::onDrag(const QPointF& pos,
                                           const QMouseEvent* e) {
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
  default:
    break;
  }

  // �e�V�F�C�v�̊e�|�C���g�ɂ��āA�S�̕ψڃx�N�g���̐��`��Ԃ𑫂����Ƃňړ�������

  int frame = m_project->getViewFrame();
  // �e�V�F�C�v�ɕό`��K�p
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // �V�F�C�v�𓾂�
    OneShape shape = m_shapes.at(s);
    // IwShape* shape = m_shapes.at(s);
    BezierPointList formKey = m_orgPoints.at(s);

    // �ό`�̊�o�E���f�B���O�{�b�N�X
    QRectF tmpBBox = m_initialBox;
    // �ό`
    for (auto bp : m_pointsInShapes[s]) {
      // for (int bp = 0; bp < formKey.count(); bp++) {

      formKey[bp].pos = map(tmpBBox, bottomRightVec, topRightVec, topLeftVec,
                            bottomLeftVec, formKey[bp].pos);
      formKey[bp].firstHandle =
          map(tmpBBox, bottomRightVec, topRightVec, topLeftVec, bottomLeftVec,
              formKey[bp].firstHandle);
      formKey[bp].secondHandle =
          map(tmpBBox, bottomRightVec, topRightVec, topLeftVec, bottomLeftVec,
              formKey[bp].secondHandle);
    }

    // �L�[���i�[
    shape.shapePairP->setFormKey(frame, shape.fromTo, formKey);
  }
}

//--------------------------------------------------------
// �S�̕ψڃx�N�g���̐��`��Ԃ𑫂����Ƃňړ�������
//--------------------------------------------------------

QPointF ActivePointsTrapezoidDragTool::map(const QRectF& bBox,
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

ActivePointsScaleDragTool::ActivePointsScaleDragTool(TransformHandleId handleId,
                                                     const QSet<int>& points,
                                                     const QRectF& handleRect)
    : ActivePointsDragTool(handleId, points, handleRect)
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

void ActivePointsScaleDragTool::onClick(const QPointF& pos,
                                        const QMouseEvent* e) {
  m_startPos = pos;
  // �g��k���̊�ʒu
  bool isCtrlPressed = (e->modifiers() & Qt::ControlModifier);
  if (isCtrlPressed)
    m_centerPos = getOppositeHandlePos();
  else
    m_centerPos = getCenterPos();
  // if (m_tool->IsScaleAroundCenter())
  //   m_anchorPos = getCenterPos();
  // else
  //   m_anchorPos = getOppositeHandlePos();
}

//--------------------------------------------------------

void ActivePointsScaleDragTool::onDrag(const QPointF& pos,
                                       const QMouseEvent* e) {
  // �c���̕ό`�{�����v�Z����
  QPointF orgVector = pos - m_centerPos;
  QPointF dstVector = m_startPos - m_centerPos;

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

  int frame = m_project->getViewFrame();
  // TODO: �{�����O�ɋ߂�����ꍇ��return

  // �e�V�F�C�v�ɕό`��K�p
  for (int s = 0; s < m_orgPoints.size(); s++) {
    // �V�F�C�v���擾
    OneShape shape = m_shapes.at(s);

    BezierPointList formKey = m_orgPoints.at(s);

    // �{���ɍ��킹�ĕό`��K�p���Ă���
    QTransform affine;
    affine = affine.translate(m_centerPos.x(), m_centerPos.y());
    affine = affine.scale(scaleRatio.x(), scaleRatio.y());
    affine = affine.translate(-m_centerPos.x(), -m_centerPos.y());
    // �ό`
    for (auto bp : m_pointsInShapes[s]) {
      // for (int bp = 0; bp < scaledBPointList.count(); bp++) {
      formKey[bp].pos          = affine.map(formKey.at(bp).pos);
      formKey[bp].firstHandle  = affine.map(formKey.at(bp).firstHandle);
      formKey[bp].secondHandle = affine.map(formKey.at(bp).secondHandle);
    }
    // �L�[���i�[
    shape.shapePairP->setFormKey(frame, shape.fromTo, formKey);
  }
}

//--------------------------------------------------------
// �V�F�C�v�̃n���h���ȊO���N���b�N  �� �ړ����[�h�i�{Shift�ŕ��s�ړ��j
//--------------------------------------------------------

ActivePointsTranslateDragTool::ActivePointsTranslateDragTool(
    TransformHandleId handleId, const QSet<int>& points,
    const QRectF& handleRect)
    : ActivePointsDragTool(handleId, points, handleRect) {}

//--------------------------------------------------------

void ActivePointsTranslateDragTool::onClick(const QPointF& pos,
                                            const QMouseEvent*) {
  m_startPos = pos;
}

//--------------------------------------------------------

void ActivePointsTranslateDragTool::onDrag(const QPointF& pos,
                                           const QMouseEvent* e) {
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
  affine    = affine.translate(moveVec.x(), moveVec.y());
  int frame = m_project->getViewFrame();

  // �e�V�F�C�v�ɕό`��K�p
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // �V�F�C�v�𓾂�
    OneShape shape = m_shapes.at(s);

    BezierPointList formKey = m_orgPoints.at(s);

    // �ό`
    for (auto bp : m_pointsInShapes[s]) {
      // for (int bp = 0; bp < translatedBPointList.count(); bp++) {
      formKey[bp].pos          = affine.map(formKey.at(bp).pos);
      formKey[bp].firstHandle  = affine.map(formKey.at(bp).firstHandle);
      formKey[bp].secondHandle = affine.map(formKey.at(bp).secondHandle);
    }

    // �L�[���i�[
    shape.shapePairP->setFormKey(frame, shape.fromTo, formKey);
  }
}

//---------------------------------------------------
// �ȉ��AUndo�R�}���h
//---------------------------------------------------

ActivePointsDragToolUndo::ActivePointsDragToolUndo(
    QList<OneShape>& shapes, QList<BezierPointList>& beforePoints,
    QList<BezierPointList>& afterPoints, QList<bool>& wasKeyFrame,
    IwProject* project, int frame)
    : m_project(project)
    , m_shapes(shapes)
    , m_beforePoints(beforePoints)
    , m_afterPoints(afterPoints)
    , m_wasKeyFrame(wasKeyFrame)
    , m_frame(frame)
    , m_firstFlag(true) {}

//---------------------------------------------------

ActivePointsDragToolUndo::~ActivePointsDragToolUndo() {}

//---------------------------------------------------

void ActivePointsDragToolUndo::undo() {
  // �e�V�F�C�v�Ƀ|�C���g���Z�b�g
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    // ����O�̓L�[�t���[���łȂ������ꍇ�A�L�[������
    if (!m_wasKeyFrame.at(s))
      shape.shapePairP->removeFormKey(m_frame, shape.fromTo);
    else {
      BezierPointList beforeFormKey = m_beforePoints.at(s);
      shape.shapePairP->setFormKey(m_frame, shape.fromTo, beforeFormKey);
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

void ActivePointsDragToolUndo::redo() {
  // Undo�o�^���ɂ�redo���s��Ȃ���[�ɂ���
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  // �e�V�F�C�v�Ƀ|�C���g���Z�b�g
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape               = m_shapes.at(s);
    BezierPointList afterFormKey = m_afterPoints.at(s);
    shape.shapePairP->setFormKey(m_frame, shape.fromTo, afterFormKey);
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
