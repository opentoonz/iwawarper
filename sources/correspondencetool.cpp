//--------------------------------------------------------
// Correspondence Tool
// �Ή��_�ҏW�c�[��
//--------------------------------------------------------

#include "correspondencetool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwselectionhandle.h"
#include "iwproject.h"
#include "sceneviewer.h"
#include "iwundomanager.h"
#include "viewsettings.h"

#include "iwshapepairselection.h"
#include "iwlayerhandle.h"
#include "iwlayer.h"
#include "reshapetool.h"
#include "iwtrianglecache.h"

#include <QList>
#include <QPointF>
#include <QMouseEvent>

#include <math.h>

//--------------------------------------------------------
// �Ή��_�h���b�O�c�[��
//--------------------------------------------------------
CorrDragTool::CorrDragTool(OneShape shape, int corrPointId,
                           const QPointF& onePix)
    // CorrDragTool::CorrDragTool(IwShape* shape, int corrPointId)
    : m_shape(shape), m_corrPointId(corrPointId), m_onePixLength(onePix) {
  m_project = IwApp::instance()->getCurrentProject()->getProject();
  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  // ���̑Ή��_���ƁA�L�[�t���[�����������ǂ������擾����
  m_orgCorrs    = shape.shapePairP->getCorrPointList(frame, shape.fromTo);
  m_wasKeyFrame = shape.shapePairP->isCorrKey(frame, shape.fromTo);
}
//--------------------------------------------------------
void CorrDragTool::onClick(const QPointF& pos, const QMouseEvent*) {
  m_startPos = pos;
}
//--------------------------------------------------------
void CorrDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // �l���i�[���郊�X�g
  CorrPointList newCorrs;

  // ���݂̃t���[���𓾂�
  int frame         = m_project->getViewFrame();
  m_snappedCpId     = -1;
  m_snapTargetShape = OneShape();

  // Shift��������Ă���ꍇ�A���̑Ή��_���S���X���C�h������
  if (e->modifiers() & Qt::ShiftModifier) {
    // �}�E�X�ʒu�ɍŋߖT�̃x�W�G���W�𓾂�
    double nearestBezierPos =
        m_shape.shapePairP->getNearestBezierPos(pos, frame, m_shape.fromTo);

    if (e->modifiers() & Qt::ControlModifier) {
      doSnap(nearestBezierPos, frame);
    }

    // �����𓾂�
    double dc = nearestBezierPos - m_orgCorrs.at(m_corrPointId).value;

    // �����V�F�C�v�̏ꍇ�A�S����
    if (m_shape.shapePairP->isClosed()) {
      for (int c = 0; c < m_orgCorrs.size(); c++) {
        double tmp = m_orgCorrs.at(c).value + dc;
        // �l���N�����v
        if (tmp >= (double)m_shape.shapePairP->getBezierPointAmount())
          tmp -= (double)m_shape.shapePairP->getBezierPointAmount();
        if (tmp < 0.0)
          tmp += (double)m_shape.shapePairP->getBezierPointAmount();

        newCorrs.push_back({tmp, m_orgCorrs.at(c).weight});
      }
    }
    // �J�����V�F�C�v�̏ꍇ�A�[�_�ȊO���X���C�h�B�[�ɂ����炻���ŃX�g�b�v
    else {
      int cpAmount = m_shape.shapePairP->getCorrPointAmount();
      // �������Ɉړ�
      if (dc > 0)
        dc = std::min(dc, m_orgCorrs.at(cpAmount - 1).value -
                              m_orgCorrs.at(cpAmount - 2).value - 0.05);
      // �������Ɉړ�
      else
        dc = std::max(dc,
                      m_orgCorrs.at(0).value - m_orgCorrs.at(1).value + 0.05);

      newCorrs.push_back(m_orgCorrs.at(0));
      for (int c = 1; c < m_orgCorrs.size() - 1; c++) {
        double tmp = m_orgCorrs.at(c).value + dc;
        newCorrs.push_back({tmp, m_orgCorrs.at(c).weight});
      }
      newCorrs.push_back(m_orgCorrs.last());
    }
  }
  // Shift��������Ă��Ȃ���΁A�I��_�݂̂��X���C�h������
  else {
    int beforeIndex = (m_shape.shapePairP->isClosed() && m_corrPointId - 1 < 0)
                          ? m_shape.shapePairP->getCorrPointAmount() - 1
                          : m_corrPointId - 1;
    int afterIndex =
        (m_shape.shapePairP->isClosed() &&
         m_corrPointId + 1 >= m_shape.shapePairP->getCorrPointAmount())
            ? 0
            : m_corrPointId + 1;
    double rangeBefore = m_orgCorrs.at(beforeIndex).value;
    double beforeSpeed = m_shape.shapePairP->getBezierSpeedAt(
        frame, m_shape.fromTo, rangeBefore, 0.001);
    rangeBefore += std::min(0.01, 0.001 / beforeSpeed);
    // �l���N�����v
    if (rangeBefore >= (double)m_shape.shapePairP->getBezierPointAmount())
      rangeBefore -= (double)m_shape.shapePairP->getBezierPointAmount();

    double rangeAfter = m_orgCorrs.at(afterIndex).value;
    double afterSpeed = m_shape.shapePairP->getBezierSpeedAt(
        frame, m_shape.fromTo, rangeAfter, -0.001);
    rangeAfter -= std::min(0.01, 0.001 / afterSpeed);
    // �l���N�����v
    if (rangeAfter < 0.0)
      rangeAfter += (double)m_shape.shapePairP->getBezierPointAmount();

    // �}�E�X�ʒu�ɍŋߖT�̃x�W�G���W�𓾂�(�͈͂���)
    double dummy;
    double nearestBezierPos = m_shape.shapePairP->getNearestBezierPos(
        pos, frame, m_shape.fromTo, rangeBefore, rangeAfter, dummy);

    if (e->modifiers() & Qt::ControlModifier) {
      doSnap(nearestBezierPos, frame, rangeBefore, rangeAfter);
    }
    // �l��1�����ύX
    newCorrs                      = m_orgCorrs;
    newCorrs[m_corrPointId].value = nearestBezierPos;
  }
  //----- �Ή��_�L�[�t���[�����i�[
  m_shape.shapePairP->setCorrKey(frame, m_shape.fromTo, newCorrs);
}

//--------------------------------------------------------

void CorrDragTool::doSnap(double& bezierPos, int frame, double rangeBefore,
                          double rangeAfter) {
  double snapCandidateBezierPos = std::round(bezierPos);
  if (rangeAfter > 0. && (snapCandidateBezierPos <= rangeBefore &&
                          snapCandidateBezierPos >= rangeAfter))
    return;

  if (m_shape.shapePairP->isClosed() &&
      (int)snapCandidateBezierPos == m_shape.shapePairP->getBezierPointAmount())
    snapCandidateBezierPos = 0.0;
  QPointF snapPoint = m_shape.shapePairP->getBezierPosFromValue(
      frame, m_shape.fromTo, snapCandidateBezierPos);
  QPointF currentPoint = m_shape.shapePairP->getBezierPosFromValue(
      frame, m_shape.fromTo, bezierPos);

  double thresholdPixels = 10.0;
  QPointF thres_dist     = thresholdPixels * m_onePixLength;
  double minimumDist     = (thres_dist.x() + thres_dist.y()) * 0.5;
  QPointF distVec        = currentPoint - snapPoint;
  if (distVec.x() * distVec.x() + distVec.y() * distVec.y() <
      minimumDist * minimumDist) {
    bezierPos     = snapCandidateBezierPos;
    m_snappedCpId = (int)snapCandidateBezierPos;
    return;
  }

  // ���ɁA���̃V�F�C�v�Ƃ̌�_�ւ̃X�i�b�v�����݂�
  IwLayer* currentLayer = IwApp::instance()->getCurrentLayer()->getLayer();
  // ���b�N���𓾂�
  bool fromToVisible[2];
  for (int fromTo = 0; fromTo < 2; fromTo++)
    fromToVisible[fromTo] =
        m_project->getViewSettings()->isFromToVisible(fromTo);
  OneShape tmpSnapTarget;
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
        // �������g�̃V�F�C�v�̓X�L�b�v
        if (m_shape.shapePairP == shapePair && m_shape.fromTo == fromTo)
          continue;
        // �o�E���f�B���O�{�b�N�X�ɓ����Ă��Ȃ�������X�L�b�v
        QRectF bBox = shapePair->getBBox(frame, fromTo)
                          .adjusted(-thres_dist.x(), -thres_dist.y(),
                                    thres_dist.x(), thres_dist.y());
        if (!bBox.contains(currentPoint)) continue;
        double dist;
        double w =
            shapePair->getNearestBezierPos(currentPoint, frame, fromTo, dist);
        if (dist < minimumDist) {
          minimumDist   = dist;
          tmpSnapTarget = OneShape(shapePair, fromTo);
        }
      }
    }
  }
  if (tmpSnapTarget.shapePairP) {
    // �ߖT�_�̎擾���J��Ԃ��Č�_���o��
    double dist;
    double snappedBezierPos = bezierPos;
    snapPoint               = currentPoint;
    int itr                 = 0;
    while (itr < 10) {
      double w = tmpSnapTarget.shapePairP->getNearestBezierPos(
          snapPoint, frame, tmpSnapTarget.fromTo, dist);
      snapPoint = tmpSnapTarget.shapePairP->getBezierPosFromValue(
          frame, tmpSnapTarget.fromTo, w);
      snappedBezierPos = m_shape.shapePairP->getNearestBezierPos(
          snapPoint, frame, m_shape.fromTo, dist);
      snapPoint = m_shape.shapePairP->getBezierPosFromValue(
          frame, m_shape.fromTo, snappedBezierPos);

      // ��_�Ƃ���
      if (dist < 0.001) {
        QPointF distVec = currentPoint - snapPoint;
        if (distVec.x() * distVec.x() + distVec.y() * distVec.y() <
            minimumDist * minimumDist) {
          bezierPos         = snappedBezierPos;
          m_snapTargetShape = tmpSnapTarget;
        }
        return;
      }

      itr++;
    }
  }
}

//--------------------------------------------------------
void CorrDragTool::onRelease(const QPointF& /*pos*/, const QMouseEvent*) {
  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();
  CorrPointList afterCorrs =
      m_shape.shapePairP->getCorrPointList(frame, m_shape.fromTo);

  bool somethingChanged = false;
  for (int c = 0; c < m_orgCorrs.size(); c++) {
    if (m_orgCorrs[c] == afterCorrs[c])
      continue;
    else {
      somethingChanged = true;
      break;
    }
  }

  if (!somethingChanged) return;

  // Undo�o�^ ������redo���Ă΂�邪�A�ŏ��̓t���O�ŉ������
  IwUndoManager::instance()->push(new CorrDragToolUndo(
      m_shape, m_orgCorrs, afterCorrs, m_wasKeyFrame, m_project, frame));

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();

  // �ύX�����t���[����invalidate
  int start, end;
  m_shape.shapePairP->getCorrKeyRange(start, end, frame, m_shape.fromTo);
  IwTriangleCache::instance()->invalidateCache(
      start, end, m_project->getParentShape(m_shape.shapePairP));
}

//--------------------------------------------------------

void CorrDragTool::draw() {
  int frame = m_project->getViewFrame();
  // ���g�̃V�F�C�v�̃R���g���[���|�C���g�ɃX�i�b�v
  if (m_snappedCpId >= 0) {
    glColor3d(1.0, 0.0, 1.0);
    BezierPointList bPList =
        m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);
    for (int p = 0; p < bPList.size(); p++) {
      ReshapeTool::drawControlPoint(m_shape, bPList, p, false, m_onePixLength,
                                    0, true);
      if (p == m_snappedCpId)
        ReshapeTool::drawControlPoint(m_shape, bPList, p, false,
                                      QPointF(3.0 * m_onePixLength));
    }
  }
  // ���V�F�C�v�Ƃ̌�_�ɃX�i�b�v�̏ꍇ�́AsetSpecialShapeColor�ŃX�i�b�v�Ώۂ̐F��ς���
  // else if (m_snapTargetShape.shapePairP) {}
}

//--------------------------------------------------------
// �Ή��_�ړ���Undo
//--------------------------------------------------------
CorrDragToolUndo::CorrDragToolUndo(OneShape shape,
                                   // IwShape* & shape,
                                   CorrPointList& beforeCorrs,
                                   CorrPointList& afterCorrs, bool& wasKeyFrame,
                                   IwProject* project, int frame)
    : m_project(project)
    , m_shape(shape)
    , m_beforeCorrs(beforeCorrs)
    , m_afterCorrs(afterCorrs)
    , m_wasKeyFrame(wasKeyFrame)
    , m_firstFlag(true)
    , m_frame(frame) {}
//--------------------------------------------------------
void CorrDragToolUndo::undo() {
  // �V�F�C�v�Ɍ��̑Ή��_�����i�[
  // �L�[�t���[���������ꍇ�A�Ή��_���������ւ�
  if (m_wasKeyFrame)
    m_shape.shapePairP->setCorrKey(m_frame, m_shape.fromTo, m_beforeCorrs);
  // �L�[�t���[������Ȃ������ꍇ�A�L�[������
  else
    m_shape.shapePairP->removeCorrKey(m_frame, m_shape.fromTo);

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();

    // �ύX�����t���[����invalidate
    int start, end;
    m_shape.shapePairP->getCorrKeyRange(start, end, m_frame, m_shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, m_project->getParentShape(m_shape.shapePairP));
  }
}
//--------------------------------------------------------
void CorrDragToolUndo::redo() {
  // Undo�o�^���ɂ�redo���s��Ȃ���[�ɂ���
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }
  // �V�F�C�v�ɑΉ��_���Z�b�g
  m_shape.shapePairP->setCorrKey(m_frame, m_shape.fromTo, m_afterCorrs);

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // �ύX�����t���[����invalidate
    int start, end;
    m_shape.shapePairP->getCorrKeyRange(start, end, m_frame, m_shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, m_project->getParentShape(m_shape.shapePairP));
  }
}
//--------------------------------------------------------

//--------------------------------------------------------
// +Alt�őΉ��_�̒ǉ�
//--------------------------------------------------------

void CorrespondenceTool::addCorrPoint(const QPointF& pos, OneShape shape)
// void CorrespondenceTool::addCorrPoint(const QPointF & pos, IwShape * shape)
{
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // ���݂̃t���[���𓾂�
  int frame = project->getViewFrame();

  AddCorrPointUndo* undo = new AddCorrPointUndo(shape, project);

  bool ok = shape.shapePairP->addCorrPoint(pos, frame, shape.fromTo);

  if (!ok) {
    delete undo;
    return;
  }

  // OK�Ȃ�UNDO�i�[
  undo->setAfterData();

  // Undo�o�^ ������redo���Ă΂�邪�A�ŏ��̓t���O�ŉ������
  IwUndoManager::instance()->push(undo);

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  // invalidate cache
  IwTriangleCache::instance()->invalidateShapeCache(
      project->getParentShape(shape.shapePairP));
}

//--------------------------------------------------------
// �Ή��_�ǉ���Undo
//--------------------------------------------------------

AddCorrPointUndo::AddCorrPointUndo(OneShape shape, IwProject* project)
    : m_project(project), m_shape(shape), m_partnerShape(0), m_firstFlag(true) {
  m_beforeData = shape.shapePairP->getCorrData(shape.fromTo);
  // if(shape->getPartner())
  {
    m_partnerShape = OneShape(shape.shapePairP, (shape.fromTo + 1) % 2);
    m_partnerBeforeData =
        m_partnerShape.shapePairP->getCorrData(m_partnerShape.fromTo);
  }
}

void AddCorrPointUndo::setAfterData() {
  m_afterData = m_shape.shapePairP->getCorrData(m_shape.fromTo);
  if (m_partnerShape.shapePairP)
    m_partnerAfterData =
        m_partnerShape.shapePairP->getCorrData(m_partnerShape.fromTo);
}

void AddCorrPointUndo::undo() {
  // �V�F�C�v�ɑΉ��_�f�[�^��߂�
  m_shape.shapePairP->setCorrData(m_beforeData, m_shape.fromTo);
  if (m_partnerShape.shapePairP)
    m_partnerShape.shapePairP->setCorrData(m_partnerBeforeData,
                                           m_partnerShape.fromTo);

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // invalidate cache
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

void AddCorrPointUndo::redo() {
  // Undo�o�^���ɂ�redo���s��Ȃ���[�ɂ���
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  // �V�F�C�v�ɑΉ��_�f�[�^��߂�
  m_shape.shapePairP->setCorrData(m_afterData, m_shape.fromTo);
  if (m_partnerShape.shapePairP)
    m_partnerShape.shapePairP->setCorrData(m_partnerAfterData,
                                           m_partnerShape.fromTo);

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // invalidate cache
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

//--------------------------------------------------------
// �@�c�[���{��
//--------------------------------------------------------

CorrespondenceTool::CorrespondenceTool()
    : IwTool("T_Correspondence")
    , m_selection(IwShapePairSelection::instance())
    , m_dragTool(nullptr)
    , m_ctrlPressed(false) {}

//--------------------------------------------------------

CorrespondenceTool::~CorrespondenceTool() {}

//--------------------------------------------------------

bool CorrespondenceTool::leftButtonDown(const QPointF& pos,
                                        const QMouseEvent* e) {
  if (!m_viewer) return false;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return false;

  int name = m_viewer->pick(e->pos());

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return false;

  OneShape shape = layer->getShapePairFromName(name);

  // �p�[�g�i�[�̑I�����s�\���ǂ���
  bool partnerIsVisible =
      project->getViewSettings()->isFromToVisible((shape.fromTo == 0) ? 1 : 0);

  // �V�F�C�v���N���b�N����Ă����ꍇ
  // �n���h�����N���b�N����Ă����灨�h���b�O�c�[����
  // �{Shift���A���I���Ȃ�ǉ��I��
  // �{Alt���A�I�����݂Ȃ�Ή��_�ǉ�
  // ����ȊO�̏ꍇ�A�V�F�C�v��P�ƑI��
  if (shape.shapePairP) {
    m_selection->makeCurrent();
    // �n���h�����N���b�N����Ă����灨�h���b�O�c�[����
    // �n���h���̖��O�͉��ꌅ��1�������Ă���
    if ((name % 10) != 0) {
      int corrPointId = (int)(name / 10) % 1000;
      m_selection->setActiveCorrPoint(shape, corrPointId);
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();

      // �I�[�v���ȃV�F�C�v�ŁA�[�_�͓������Ȃ�
      if (!shape.shapePairP->isClosed() &&
          (corrPointId == 0 ||
           corrPointId == shape.shapePairP->getCorrPointAmount() - 1))
        return true;

      // �h���b�O�c�[�������
      m_dragTool =
          new CorrDragTool(shape, corrPointId, m_viewer->getOnePixelLength());
      m_dragTool->onClick(pos, e);
      return true;
    }
    // �{Shift���A���I���Ȃ�ǉ��I��
    else if (e->modifiers() & Qt::ShiftModifier) {
      if (m_selection->isSelected(shape))
        return false;
      else {
        m_selection->addShape(shape);
        // �����A���̃V�F�C�v�Ƀp�[�g�i�[������A���I���Ȃ�ǉ�����
        if (!m_selection->isSelected(shape.getPartner()) && partnerIsVisible)
          m_selection->addShape(shape.getPartner());
      }
    }
    // �{Alt���A�I�����݂Ȃ�Ή��_�ǉ�
    else if (e->modifiers() & Qt::AltModifier &&
             m_selection->isSelected(shape)) {
      // �Ή��_�ǉ��R�}���h
      addCorrPoint(pos, shape);

    }
    // ����ȊO�̏ꍇ�A�V�F�C�v��P�ƑI��
    else {
      m_selection->selectOneShape(shape);
      // �����A���̃V�F�C�v�Ƀp�[�g�i�[������A���I���Ȃ�ǉ�����
      if (!m_selection->isSelected(shape.getPartner()) && partnerIsVisible)
        m_selection->addShape(shape.getPartner());
    }
    // �V�O�i�����G�~�b�g
    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
    return true;
  }
  // �I���̉���
  else {
    m_selection->selectNone();
    // �V�O�i�����G�~�b�g
    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
    return true;
  }
}

//--------------------------------------------------------

bool CorrespondenceTool::leftButtonDrag(const QPointF& pos,
                                        const QMouseEvent* e) {
  if (m_dragTool) {
    m_viewer->setFocus();
    m_dragTool->onDrag(pos, e);
    return true;
  }

  return false;
}

//--------------------------------------------------------

bool CorrespondenceTool::leftButtonUp(const QPointF& pos,
                                      const QMouseEvent* e) {
  if (m_dragTool) {
    m_dragTool->onRelease(pos, e);
    delete m_dragTool;
    m_dragTool = 0;
    return true;
  }

  return false;
}

//--------------------------------------------------------

void CorrespondenceTool::leftButtonDoubleClick(const QPointF&,
                                               const QMouseEvent*) {}

//--------------------------------------------------------

// �Ή��_�ƕ������ꂽ�Ή��V�F�C�v�A�V�F�C�v���m�̘A����\������
void CorrespondenceTool::draw() {
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // �\���t���[���𓾂�
  int frame = project->getViewFrame();

  // �I���V�F�C�v������������return
  if (m_selection->isEmpty()) return;

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  QList<ShapePair*>
      alreadyDrawnLinkList;  // ���ɑΉ�����`�����V�F�C�v���L�^���Ă���

  bool isSnapping = (m_dragTool != nullptr) && m_ctrlPressed;
  // join lines are drawn when from and to shapes are both drawn
  bool isJoinLinesVisible = project->getViewSettings()->isFromToVisible(0) &&
                            project->getViewSettings()->isFromToVisible(1);

  // �e�I���V�F�C�v�̗֊s��`��
  for (int s = 0; s < m_selection->getCount(); s++) {
    // �V�F�C�v���擾
    OneShape shape = m_selection->getShape(s);

    drawCorrLine(shape);

    // �Ή����̕`��
    if (isJoinLinesVisible &&
        !alreadyDrawnLinkList.contains(shape.shapePairP) && !isSnapping) {
      drawJoinLine(shape.shapePairP);

      alreadyDrawnLinkList.push_back(shape.shapePairP);
    }
  }

  // �X�i�b�v���쒆�̓R���g���[���|�C���g��`���B�Ή����͕`���Ȃ�
  if (isSnapping) {
    m_dragTool->draw();
  }
}

//--------------------------------------------------------

int CorrespondenceTool::getCursorId(const QMouseEvent* e) {
  if (!m_viewer) return ToolCursor::ArrowCursor;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return ToolCursor::ArrowCursor;

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return ToolCursor::ArrowCursor;

  int name      = m_viewer->pick(e->pos());
  m_ctrlPressed = (e->modifiers() & Qt::ControlModifier);

  OneShape shape = layer->getShapePairFromName(name);
  if (m_selection->isEmpty()) {
    IwApp::instance()->showMessageOnStatusBar(tr("[Click shape] to select."));
    return ToolCursor::ArrowCursor;
  }

  IwApp::instance()->showMessageOnStatusBar(
      tr("[Drag corr point] to move. [+Shift] to move all points. [+Ctrl] with "
         "snapping. [Alt + Click] to insert a new corr point."));

  // �I���V�F�C�v����Ȃ������畁�ʂ̃J�[�\��
  if (!shape.shapePairP || !m_selection->isSelected(shape))
    return ToolCursor::ArrowCursor;

  // �n���h�����N���b�N����Ă����灨�h���b�O�c�[����
  // �n���h���̖��O�͉��ꌅ��1�������Ă���
  if ((name % 10) != 0) return ToolCursor::BlackArrowCursor;

  // �|�C���g�ȊO�ŁAAlt��������Ă�����A�Ή��_�ǉ��̃J�[�\��
  if (e->modifiers() & Qt::AltModifier) return ToolCursor::BlackArrowAddCursor;

  return ToolCursor::ArrowCursor;
}

//--------------------------------------------------------
// �A�N�e�B�u���AJoin���Ă���V�F�C�v�̕Е��������I������Ă�����A
// �����Е����I������
//--------------------------------------------------------

void CorrespondenceTool::onActivate() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // �������ALock����Ă��Ȃ��ꍇ�B�����ŁALock�̏��𓾂�
  bool fromToVisible[2];
  for (int ft = 0; ft < 2; ft++)
    fromToVisible[ft] = project->getViewSettings()->isFromToVisible(ft);

  // �I���V�F�C�v������������return
  if (m_selection->isEmpty()) return;

  QList<OneShape> shapeToBeSelected;
  // �e�I���V�F�C�v�ɂ���
  for (int s = 0; s < m_selection->getCount(); s++) {
    // �V�F�C�v���擾
    OneShape shape = m_selection->getShape(s);
    if (!shape.shapePairP) continue;
    // �I������Ă��Ȃ���΃��X�g�Ɋi�[
    // ���b�N����Ă���������Ȃ�
    if (!m_selection->isSelected(shape.getPartner()) &&
        fromToVisible[(shape.fromTo == 0) ? 1 : 0])
      shapeToBeSelected.push_back(shape.getPartner());
  }

  // ���X�g����Ȃ�return
  if (shapeToBeSelected.isEmpty()) return;
  // �V�F�C�v��I���ɒǉ�
  for (int p = 0; p < shapeToBeSelected.size(); p++)
    m_selection->addShape(shapeToBeSelected.at(p));
}

//--------------------------------------------------------
// �A�N�e�B�u�ȑΉ��_�����Z�b�g����

void CorrespondenceTool::onDeactivate() {
  m_selection->setActiveCorrPoint(OneShape(), -1);
  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//--------------------------------------------------------

bool CorrespondenceTool::setSpecialShapeColor(OneShape shape) {
  return (m_dragTool && m_dragTool->isSnapTargetShape(shape));
}

//--------------------------------------------------------
CorrespondenceTool correspondenceTool;
