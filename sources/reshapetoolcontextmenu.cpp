//------------------------------------------
// ReshapeToolContextMenu
// Reshape Tool �̉E�N���b�N���j���[�p�N���X
//------------------------------------------

#include "reshapetoolcontextmenu.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"

#include "iwshapepairselection.h"
#include "iwundomanager.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"

#include "preferences.h"
#include "iwtrianglecache.h"

// #include <QWidget>
#include <QMenu>
#include <QAction>
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

ReshapeToolContextMenu::ReshapeToolContextMenu() : m_mode(CurrentPointMode) {
  // �A�N�V�����̐錾
  m_lockPointsAct    = new QAction(tr("Lock Points"), this);
  m_cuspAct          = new QAction(tr("Cusp"), this);
  m_linearAct        = new QAction(tr("Linear"), this);
  m_smoothAct        = new QAction(tr("Smooth"), this);
  m_centerAct        = new QAction(tr("Center"), this);
  m_tweenPointAct    = new QAction(tr("Tween Point"), this);
  m_reverseShapesAct = new QAction(tr("Reverse Shapes"), this);

  // �V�O�i���^�X���b�g�ڑ�
  connect(m_lockPointsAct, SIGNAL(triggered()), this, SLOT(doLockPoints()));
  connect(m_cuspAct, SIGNAL(triggered()), this, SLOT(doCusp()));
  connect(m_linearAct, SIGNAL(triggered()), this, SLOT(doLinear()));
  connect(m_smoothAct, SIGNAL(triggered()), this, SLOT(doSmooth()));
  connect(m_centerAct, SIGNAL(triggered()), this, SLOT(doCenter()));
  connect(m_tweenPointAct, SIGNAL(triggered()), this, SLOT(doTween()));
  connect(m_reverseShapesAct, SIGNAL(triggered()), this, SLOT(doReverse()));
}
//------------------------------------------
void ReshapeToolContextMenu::init(IwShapePairSelection* selection,
                                  QPointF onePixelLength) {
  m_selection      = selection;
  m_onePixelLength = onePixelLength;
}

//------------------------------------------
ReshapeToolContextMenu* ReshapeToolContextMenu::instance() {
  static ReshapeToolContextMenu _instance;
  return &_instance;
}

//------------------------------------------
// ���̃��j���[�̑���ŉe����^����|�C���g���X�V
//------------------------------------------
void ReshapeToolContextMenu::updateEffectedPoints() {
  m_effectedPoints.clear();
  m_activePointShapes.clear();

  QList<QPair<OneShape, int>> activePointList =
      m_selection->getActivePointList();

  // �e�_�ɂ���
  for (int i = 0; i < activePointList.size(); i++) {
    QPair<OneShape, int> activePoint = activePointList.at(i);

    OneShape shape = activePoint.first;
    int pointIndex = activePoint.second;

    // ActivePoint���܂ރV�F�C�v�̃��X�g��ǉ�
    if (!m_activePointShapes.contains(shape)) m_activePointShapes.append(shape);

    // ���[�h�ɂ���āA���̃��j���[�̑���ŉe����^����|�C���g��ǉ�
    if (m_mode == SelectedShapeMode ||
        (m_mode == CurrentShapeMode && m_shape == shape))
      m_effectedPoints.append(qMakePair(shape, pointIndex));
  }

  // CurrentPointMode�̂Ƃ��́ASelection�Ɋ֌W�Ȃ��N���b�N�����|�C���g�Ɍ���
  if (m_mode == CurrentPointMode)
    m_effectedPoints.append(qMakePair(m_shape, m_pointIndex));
}

//------------------------------------------
// �A�N�V�����̗L���^������؂�ւ���
//------------------------------------------
void ReshapeToolContextMenu::enableActions() {
  m_lockPointsAct->setEnabled(canLockPoints());

  bool enable = canModifyHandles();
  m_cuspAct->setEnabled(enable);
  m_linearAct->setEnabled(enable);
  m_smoothAct->setEnabled(enable);
  m_centerAct->setEnabled(canCenter());

  // �Ȃɂ�CP���I������Ă����OK
  m_tweenPointAct->setEnabled(!m_effectedPoints.isEmpty());
  // �����V�F�C�v���I������Ă����OK
  m_reverseShapesAct->setEnabled(!m_selection->isEmpty());
}

bool ReshapeToolContextMenu::canLockPoints() {
  // SelectedShapeMode�ł͂��߁i�A���J�[�ƂȂ�_���ǂꂩ�w��ł��Ȃ��̂Łj
  if (m_mode == SelectedShapeMode) return false;
  // CurrentShapeMode�̂Ƃ��AActive�ȓ_���A
  else if (m_mode == CurrentShapeMode) {
    // �����̃V�F�C�v���܂ނQ�ȏ�ɂ܂������Ă���K�v
    if (m_activePointShapes.size() >= 2 &&
        m_activePointShapes.contains(m_shape))
      return true;
    else
      return false;
  }
  // CurrentPointMode�̂Ƃ��AActive�ȓ_���A
  else {
    // �����̃V�F�C�v�ȊO�ɂ����P�K�v�i�{�ƂƂ͎d�l��ς���j
    for (int s = 0; s < m_activePointShapes.size(); s++) {
      if (m_activePointShapes.at(s) != m_shape) return true;
    }
    return false;
  }
}

// Cusp, Linear, Smooth�p
bool ReshapeToolContextMenu::canModifyHandles() {
  // CurrentPointMode�Ȃ�OK
  if (m_mode == CurrentPointMode) return true;
  // CurrentShapeMode�̏ꍇ�AActive�ȃ|�C���g������Shape�Ɋ܂܂�Ă���K�v
  else if (m_mode == CurrentShapeMode)
    return m_activePointShapes.contains(m_shape);
  // SelectedShapeMode�̏ꍇ�AActive�ȃ|�C���g���P�ł������OK
  else
    return !m_effectedPoints.isEmpty();
}

bool ReshapeToolContextMenu::canCenter() {
  // �������|�C���g�ɁA1�ł��A
  // �@ OpenShape�̒[�_�ȊO�̓_�������OK�B�܂��́A
  // �A CloseShape�ŁA�O�オ�I������Ă��Ȃ��_��
  // �͂��܂�Ă���_�������OK
  if (m_effectedPoints.isEmpty()) return false;

  for (int i = 0; i < m_effectedPoints.size(); i++) {
    OneShape shape = m_effectedPoints.at(i).first;
    int pId        = m_effectedPoints.at(i).second;

    // �@ OpenShape�̒[�_�ȊO�̓_�������OK�B
    if (!shape.shapePairP->isClosed()) {
      if (pId != 0 && pId != shape.shapePairP->getBezierPointAmount() - 1)
        return true;
    }
    // �A CloseShape�ŁA�O�オ�I������Ă��Ȃ��_��
    // �͂��܂�Ă���_�������OK
    else {
      // �O�̓_��T��
      int beforePId = pId - 1;
      while (beforePId != pId) {
        // �C���f�b�N�X�����������
        if (beforePId == -1)
          beforePId = shape.shapePairP->getBezierPointAmount() - 1;

        // ������Ă��܂����ꍇ�͑ʖڂȂ̂Ŏ��̑���_��
        if (beforePId == pId) break;
        // ���삳���|�C���g�łȂ���Δ���
        if (!m_effectedPoints.contains(qMakePair(shape, beforePId))) break;
        // ���̃|�C���g��
        --beforePId;
      }
      // ������Ă��܂����ꍇ�͑ʖڂȂ̂Ŏ��̑���_��
      if (beforePId == pId) continue;

      // ��̓_��T��
      int afterPId = pId + 1;
      while (afterPId != pId) {
        // �C���f�b�N�X�����������
        if (afterPId == shape.shapePairP->getBezierPointAmount()) afterPId = 0;
        // ������Ă��܂����ꍇ�͑ʖڂȂ̂Ŏ��̑���_��
        if (afterPId == pId) break;
        // ���삳���|�C���g�łȂ���Δ���
        if (!m_effectedPoints.contains(qMakePair(shape, afterPId))) break;
        // ���̃|�C���g��
        ++afterPId;
      }
      // ������Ă��܂����ꍇ�͑ʖڂȂ̂Ŏ��̑���_��
      if (afterPId == pId) continue;

      // �O��̓_���Ⴄ�ꍇ�́ACenter���삪�\�B
      // �����ꍇ�͎��̑���_��
      if (beforePId != afterPId) return true;
    }
  }
  // �S�����Ă��ʖڂ�������false
  return false;
}

//------------------------------------------
// ���j���[���J��
//------------------------------------------
void ReshapeToolContextMenu::openMenu(const QMouseEvent* /*e*/, OneShape shape,
                                      int pointIndex, int handleId,
                                      QMenu& menu) {
  m_shape      = shape;
  m_pointIndex = pointIndex;
  m_handleId   = handleId;

  // �E�N���b�N�ʒu�ɂ��킹�A���[�h��ς���
  //  �I���V�F�C�v���N���b�N�����ꍇ
  //  �|�C���g�^�n���h�����N���b�N���Ă����� CurrentPointMode
  //  ����ȊO�Ȃ�ACurrentShapeMode
  if (shape.shapePairP && m_selection->isSelected(shape))
    m_mode = (handleId > 0) ? CurrentPointMode : CurrentShapeMode;
  // �I���V�F�C�v�ȊO���N���b�N�����ꍇ
  else
    m_mode = SelectedShapeMode;

  // ���̃��j���[�̑���ŉe����^����|�C���g���X�V
  updateEffectedPoints();
  // �����ŁA�A�N�V�����̗L���^������؂�ւ���
  enableActions();

  // ���[�h�̕\��
  QString str;
  switch (m_mode) {
  case CurrentPointMode:
    str = tr("Current Point Mode");
    break;
  case CurrentShapeMode:
    str = tr("Current Shape Mode");
    break;
  case SelectedShapeMode:
    str = tr("Selected Shapes Mode");
    break;
  default:
    str = tr("Unknown Mode");
    break;
  }
  menu.addAction(new QAction(str, 0));

  menu.addSeparator();

  // menu.addAction(m_breakShapeAct);
  menu.addAction(m_lockPointsAct);

  menu.addSeparator();

  menu.addAction(m_cuspAct);
  menu.addAction(m_linearAct);
  menu.addAction(m_smoothAct);
  menu.addAction(m_centerAct);

  menu.addSeparator();

  menu.addAction(m_tweenPointAct);
  menu.addAction(m_reverseShapesAct);
}

//---------------------------------------------------
// �N���b�N�����V�F�C�v����Active�ȓ_���A
// ���̃V�F�C�v��Active�ȓ_�ōŊ��̂��̂Ƀt�B�b�g������B
// �t�B�b�g�����邩�ǂ����̃|�C���g�Ԃ̋�����臒l��Preference�Ō��߂�
// �J�[�u�̒[�_�ȊO�́A�n���h���������ʒu�ɂ���
// CurrentPointMode�̂Ƃ��́A�I��point��������B�B
// MultiFrame���[�h�ɑΉ��B
//---------------------------------------------------
void ReshapeToolContextMenu::doLockPoints() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // ���݂̃t���[���𓾂�
  int frame = project->getViewFrame();

  // Lock�\�ȃs�N�Z�������𓾂�
  int lockThres = Preferences::instance()->getLockThreshold();

  // �ΏۂƂȂ�t���[���̃��X�g�����
  QList<int> moveFrames;
  if (isMultiFrameActivated()) {
    // MultiFrame�̏��𓾂�
    QList<int> multiFrameList = project->getMultiFrames();
    // �J�����g�t���[�����܂܂�Ă��Ȃ�������ǉ�
    if (!multiFrameList.contains(frame)) multiFrameList.append(frame);

    for (int f = 0; f < multiFrameList.size(); f++) {
      if (m_shape.shapePairP->isFormKey(multiFrameList.at(f), m_shape.fromTo))
        moveFrames.append(multiFrameList.at(f));
    }
    // �Ώۃt���[����1������������return
    if (moveFrames.isEmpty()) return;
  }
  // �}���`��OFF�̂Ƃ��́A�J�����g�t���[�����N���b�N�����V�F�C�v��
  // �`��L�[�t���[���łȂ��ƁA�������Ȃ���return�B
  else {
    if (m_shape.shapePairP->isFormKey(frame, m_shape.fromTo))
      moveFrames.append(frame);
    else
      return;
  }

  // �ړ��ΏۂƂȂ�|�C���g�ƁA�A���J�[�ƂȂ�|�C���g��
  // �C���f�b�N�X�̃��X�g�����
  QList<int> movePoints;
  QList<QPair<OneShape, int>> anchorPoints;

  QList<QPair<OneShape, int>> activePointList =
      m_selection->getActivePointList();

  // �e�A�N�e�B�u�_�ɂ���
  for (int ep = 0; ep < activePointList.size(); ep++) {
    QPair<OneShape, int> curPoint = activePointList.at(ep);

    // �N���b�N�����V�F�C�v�Ɋ܂܂��Active�_�Ȃ�A�ړ�����Ώ�
    // �������ACurrentPointMode�̏ꍇ�́A�N���b�N���Ă���_�݂̂��ړ�����
    if (curPoint.first == m_shape) {
      if (m_mode == CurrentPointMode && curPoint.second != m_pointIndex)
        continue;
      movePoints.append(curPoint.second);
    }
    // ����ȊO�Ȃ�A�A���J�[�ƂȂ�Ώ�
    else
      anchorPoints.append(curPoint);
  }

  //----- �������瑀��

  // Undo�p �`��f�[�^���Ƃ��Ă���
  QMap<int, BezierPointList> beforePoints =
      m_shape.shapePairP->getFormData(m_shape.fromTo);

  // ����ΏۂƂȂ�e�t���[���ɂ���
  for (int f = 0; f < moveFrames.size(); f++) {
    int curFrame = moveFrames.at(f);

    // ���̃|�C���g���ړ������Ă���
    BezierPointList bpList =
        m_shape.shapePairP->getBezierPointList(curFrame, m_shape.fromTo);

    // ���ɂق��̓_�ɏꏊ������Ă���A���J�[�_�̃��X�g
    QList<QPair<OneShape, int>> occupiedAnchors;

    // �ړ��ΏۂƂȂ�e�|�C���g�ɂ���
    for (int p = 0; p < movePoints.size(); p++) {
      int curIndex = movePoints.at(p);
      // ���ꂩ�瓮�����|�C���g
      BezierPoint curPoint = bpList.at(curIndex);

      // ��ԋ߂��A���J�[�_
      QPair<OneShape, int> nearestAnchor;
      BezierPoint nearestBPoint;
      double nearestDistance2 = 100000000.0;

      // �A���J�[�ƂȂ�e�_�ɂ���
      for (int a = 0; a < anchorPoints.size(); a++) {
        QPair<OneShape, int> curAnchor =
            anchorPoints.at(a);  // �ΏۂƂȂ�|�C���g�𓾂�

        // ���łɕʂ̃|�C���g���ꏊ������Ă�����X�L�b�v
        if (occupiedAnchors.contains(curAnchor)) continue;

        BezierPoint anchorBP =
            curAnchor.first.shapePairP
                ->getBezierPointList(curFrame, curAnchor.first.fromTo)
                .at(curAnchor.second);

        // �ړ�����_�ƃA���J�[�_�Ƃ́u�\����́v�s�N�Z�����������߂�
        QPointF vec  = curPoint.pos - anchorBP.pos;
        vec          = QPointF(vec.x() / m_onePixelLength.x(),
                               vec.y() / m_onePixelLength.y());
        double dist2 = vec.x() * vec.x() + vec.y() * vec.y();

        // ����܂ł̈�ԋ߂����������߂�������X�V
        if (dist2 < nearestDistance2) {
          nearestAnchor    = curAnchor;
          nearestBPoint    = anchorBP;
          nearestDistance2 = dist2;
        }
      }
      // �ŋߖT�̃A���J�[�_�Ƃ̋������APreference�Ō��߂�臒l���
      // ����������A���̓_�͓������Ȃ���continue
      // 臒l �b��50
      if (nearestDistance2 > lockThres * lockThres) continue;

      // 臒l���߂���΁A�ʒu���t�B�b�g������
      //  Open�V�F�C�v�̒[�_�ȊO�̏ꍇ�́A�n���h���ʒu����v������B
      //  �Ȃ񂩋������Ⴄ�݂����H�H�[�_�֌W�Ȃ�����
      else {
        bool isEndPoint =
            (!m_shape.shapePairP->isClosed()) &&
            (curIndex == 0 ||
             curIndex == m_shape.shapePairP->getBezierPointAmount() - 1);
        if (isEndPoint)  // ���s�ړ�
        {
          QPointF moveVec = nearestBPoint.pos - bpList[curIndex].pos;
          bpList[curIndex].pos += moveVec;
          bpList[curIndex].firstHandle += moveVec;
          bpList[curIndex].secondHandle += moveVec;
        } else
          bpList.replace(curIndex, nearestBPoint);
        // ��L�_�ɒǉ�
        occupiedAnchors.append(nearestAnchor);
      }
    }

    // �L�[���i�[
    m_shape.shapePairP->setFormKey(curFrame, m_shape.fromTo, bpList);
    // �ύX�����t���[����invalidate
    int start, end;
    m_shape.shapePairP->getFormKeyRange(start, end, curFrame, m_shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, project->getParentShape(m_shape.shapePairP));
  }

  // Undo�p �����̌`��f�[�^
  QMap<int, BezierPointList> afterPoints =
      m_shape.shapePairP->getFormData(m_shape.fromTo);

  // Undo
  QList<OneShape> shapeList;
  QList<QMap<int, BezierPointList>> beforeList;
  QList<QMap<int, BezierPointList>> afterList;
  QList<bool> wasKey;
  shapeList.append(m_shape);
  beforeList.append(beforePoints);
  afterList.append(afterPoints);
  wasKey.append(true);
  IwUndoManager::instance()->push(new ReshapeContextMenuUndo(
      shapeList, beforeList, afterList, wasKey, frame, project));

  // �X�V�V�O�i�����G�~�b�g����
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
// Active�_�̃n���h�������O�ɂ��āA�s�p�ɂ���
// �J�����g�t���[�����L�[�t���[���łȂ������ꍇ�A�L�[�ɂ���
//---------------------------------------------------
void ReshapeToolContextMenu::doCusp() { doModifyHandle(Cusp); }

//---------------------------------------------------
// Active�_�̃n���h�����A�ׂ̃|�C���g�ւ̐�����1/4�ʒu�ɂ���
// �J�����g�t���[�����L�[�t���[���łȂ������ꍇ�A�L�[�ɂ���
//---------------------------------------------------
void ReshapeToolContextMenu::doLinear() { doModifyHandle(Linear); }

//---------------------------------------------------
// Active�_�̃n���h�����A�O���CP�����Ԑ����ɕ��s�Œ����͂��ꂼ��1�^4�ƂȂ�悤�ɂ���
// �J�����g�t���[�����L�[�t���[���łȂ������ꍇ�A�L�[�ɂ���
//---------------------------------------------------
void ReshapeToolContextMenu::doSmooth() { doModifyHandle(Smooth); }

//---------------------------------------------------
// Active�_�̃n���h�����A�O��̔�Active�_�𓙕��Ɋ���ʒu�Ɉړ�����B
// �n���h���̓_�́A�_�̊Ԃ̋�����1�^3�̈ʒu�ɂ���
// �J�����g�t���[�����L�[�t���[���łȂ������ꍇ�A�L�[�ɂ���
//---------------------------------------------------
void ReshapeToolContextMenu::doCenter() { doModifyHandle(Center); }

void ReshapeToolContextMenu::doModifyHandle(ModifyHandleActId actId) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // ���݂̃t���[���𓾂�
  int frame = project->getViewFrame();

  // �ΏۂƂȂ�_���A�V�F�C�v���Ƃɕ��ёւ���
  QList<OneShape> shapes;
  QList<QList<int>> indicesList;

  // ���̃��j���[�̑���ŉe����^����|�C���g
  for (int ep = 0; ep < m_effectedPoints.size(); ep++) {
    QPair<OneShape, int> pointPair = m_effectedPoints.at(ep);

    OneShape sh = pointPair.first;
    // �V�F�C�v��������Γo�^
    if (!shapes.contains(sh)) {
      shapes.append(sh);
      indicesList.append(QList<int>());
    }
    // �|�C���g��o�^
    indicesList[shapes.indexOf(sh)].append(pointPair.second);
  }

  // �ΏۂƂȂ�t���[���̃��X�g�����
  QList<int> moveFrames;
  if (isMultiFrameActivated()) moveFrames.append(project->getMultiFrames());
  if (!moveFrames.contains(frame)) moveFrames.append(frame);

  // �e�V�F�C�v���A�J�����g�t���[���Ō`��L�[���������ǂ���
  QList<bool> wasKeyFrame;
  // �V�F�C�v���Ƃ́A�ҏW���ꂽ�t���[�����Ƃ̌��̌`��̃��X�g
  QList<QMap<int, BezierPointList>> beforePoints;
  // �V�F�C�v���Ƃ́A�ҏW���ꂽ�t���[�����Ƃ̑����̌`��̃��X�g
  QList<QMap<int, BezierPointList>> afterPoints;

  // �e�ΏۃV�F�C�v�ɂ���
  for (int s = 0; s < shapes.size(); s++) {
    OneShape curShape = shapes.at(s);

    // ���̌`����i�[
    beforePoints.append(curShape.shapePairP->getFormData(curShape.fromTo));

    // ����O�ɃJ�����g�t���[���Ō`��L�[���������ǂ���
    wasKeyFrame.append(curShape.shapePairP->isFormKey(frame, curShape.fromTo));

    // ���̃V�F�C�v�̃A�N�e�B�u�_�̃��X�g
    QList<int> cpIndices = indicesList.at(s);

    // �e�Ώۃt���[������
    for (int f = 0; f < moveFrames.size(); f++) {
      int curFrame = moveFrames.at(f);
      // �J�����g�t���[���łȂ��A���L�[�t���[���łȂ����continue
      if (curFrame != frame &&
          !curShape.shapePairP->isFormKey(curFrame, curShape.fromTo))
        continue;

      // ���̃t���[���ł̃x�W�G�`��f�[�^���擾
      BezierPointList bpList =
          curShape.shapePairP->getBezierPointList(curFrame, curShape.fromTo);

      // �e�A�N�e�B�u�|�C���g�ɂ���
      for (int i = 0; i < cpIndices.size(); i++) {
        int curIndex = cpIndices.at(i);
        // �A�N�V�����ɉ����ď����𕪂���
        switch (actId) {
          // �s�p��
        case Cusp: {
          // �n���h�������O�ɂ���
          QPointF cpPos                 = bpList.at(curIndex).pos;
          bpList[curIndex].firstHandle  = cpPos;
          bpList[curIndex].secondHandle = cpPos;
        } break;

          // �n���h����ׂ�CP�ւ̃x�N�g����1/4�ʒu��
        case Linear: {
          QPointF cpPos = bpList.at(curIndex).pos;
          // �O�̃n���h�� : Open�ȃV�F�C�v�Ŏn�_�̏ꍇ�̓X�L�b�v
          if (curShape.shapePairP->isClosed() || curIndex != 0) {
            int beforeIndex =
                (curIndex == 0)
                    ? curShape.shapePairP->getBezierPointAmount() - 1
                    : curIndex - 1;
            // �ׂւ̃x�N�g��
            QPointF vec = bpList.at(beforeIndex).pos - cpPos;
            vec *= 0.25f;
            bpList[curIndex].firstHandle = cpPos + vec;
          }
          // ���̃n���h�� : Open�ȃV�F�C�v�ŏI�_�̏ꍇ�̓X�L�b�v
          if (curShape.shapePairP->isClosed() ||
              curIndex != curShape.shapePairP->getBezierPointAmount() - 1) {
            int afterIndex =
                (curIndex == curShape.shapePairP->getBezierPointAmount() - 1)
                    ? 0
                    : curIndex + 1;
            // �ׂւ̃x�N�g��
            QPointF vec = bpList.at(afterIndex).pos - cpPos;
            vec *= 0.25f;
            bpList[curIndex].secondHandle = cpPos + vec;
          }
        } break;

          // �n���h����O��̓_�����Ԑ����ƕ��s�ɁA�����͂��ꂼ��1/4��
        case Smooth: {
          QPointF cpPos = bpList.at(curIndex).pos;
          // OpenShape�Ŏn�_�̏ꍇ ���̓_�Ƃ̒��_�܂ł̒����ɂ���
          if (!curShape.shapePairP->isClosed() && curIndex == 0) {
            QPointF vec = bpList.at(1).pos - cpPos;
            vec *= 0.5f;
            bpList[curIndex].firstHandle  = cpPos - vec;
            bpList[curIndex].secondHandle = cpPos + vec;
          }
          // OpenShape�ŏI�_�̏ꍇ
          else if (!curShape.shapePairP->isClosed() &&
                   curIndex ==
                       curShape.shapePairP->getBezierPointAmount() - 1) {
            QPointF vec = bpList.at(curIndex - 1).pos - cpPos;
            vec *= 0.5f;
            bpList[curIndex].firstHandle  = cpPos + vec;
            bpList[curIndex].secondHandle = cpPos - vec;
          }
          // �O��ɓ_������ꍇ
          else {
            int beforeIndex =
                (curIndex == 0)
                    ? curShape.shapePairP->getBezierPointAmount() - 1
                    : curIndex - 1;
            int afterIndex =
                (curIndex == curShape.shapePairP->getBezierPointAmount() - 1)
                    ? 0
                    : curIndex + 1;
            QPointF vec =
                bpList.at(afterIndex).pos - bpList.at(beforeIndex).pos;
            vec *= 0.25f;
            bpList[curIndex].firstHandle  = cpPos - vec;
            bpList[curIndex].secondHandle = cpPos + vec;
          }
        } break;

          // �O��̔�Active�_�̊Ԃ𓙕�����
        case Center:
          // �����Ȃ肻���Ȃ̂œ�����
          doCenter_Imp(curShape, curIndex, bpList);
          break;
        }
      }

      // �`��f�[�^��߂�
      curShape.shapePairP->setFormKey(curFrame, curShape.fromTo, bpList);
      // �ύX�����t���[����invalidate
      int start, end;
      curShape.shapePairP->getFormKeyRange(start, end, curFrame,
                                           curShape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, project->getParentShape(curShape.shapePairP));
    }

    // �����̌`����i�[
    afterPoints.append(curShape.shapePairP->getFormData(curShape.fromTo));
  }

  // Undo��o�^
  IwUndoManager::instance()->push(new ReshapeContextMenuUndo(
      shapes, beforePoints, afterPoints, wasKeyFrame, frame, project));

  // �V�O�i�����G�~�b�g
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
// doCenter�̎���
//---------------------------------------------------
void ReshapeToolContextMenu::doCenter_Imp(OneShape shape, int index,
                                          BezierPointList& bpList) {
  int beforeIndex, afterIndex;
  int beforeSegCount = 0;
  int afterSegCount  = 0;
  // �@ OpenShape�̏ꍇ
  if (!shape.shapePairP->isClosed()) {
    // �O�̓_��T��
    beforeIndex = index;
    while (1) {
      if (beforeIndex == 0 || !m_selection->isPointActive(shape, beforeIndex))
        break;

      beforeIndex--;
      beforeSegCount += 3;
    }
    // ��̓_��T��
    afterIndex = index;
    while (1) {
      if (afterIndex == shape.shapePairP->getBezierPointAmount() - 1 ||
          !m_selection->isPointActive(shape, afterIndex))
        break;

      afterIndex++;
      afterSegCount += 3;
    }
  }
  // �A CloseShape�̏ꍇ
  else {
    // �O�̓_��T��
    beforeIndex = index;
    while (1) {
      beforeIndex--;
      // �C���f�b�N�X�̃��[�v
      if (beforeIndex == -1)
        beforeIndex = shape.shapePairP->getBezierPointAmount() - 1;
      beforeSegCount += 3;
      // �[�_������������break
      if (!m_selection->isPointActive(shape, beforeIndex)) break;
      // ��������������return
      if (beforeIndex == index) return;
    }
    // ��̓_��T��
    afterIndex = index;
    while (1) {
      afterIndex++;
      // �C���f�b�N�X�̃��[�v
      if (afterIndex == shape.shapePairP->getBezierPointAmount())
        afterIndex = 0;
      afterSegCount += 3;
      // �[�_������������break
      if (!m_selection->isPointActive(shape, afterIndex)) break;
      // ��������������return
      if (afterIndex == index) return;
    }
    // �O�̓_�ƌ�̓_�������_�������ꍇ�Areturn
    if (beforeIndex == afterIndex) return;
  }
  // �ی�
  if (beforeSegCount + afterSegCount == 0) return;
  // �O��̃A���J�[�_�𓙕�����
  QPointF beforePos = bpList.at(beforeIndex).pos;
  QPointF afterPos  = bpList.at(afterIndex).pos;
  QPointF vec       = afterPos - beforePos;
  vec /= (beforeSegCount + afterSegCount);
  bpList[index].pos          = beforePos + vec * beforeSegCount;
  bpList[index].firstHandle  = bpList[index].pos - vec;
  bpList[index].secondHandle = bpList[index].pos + vec;
}

//---------------------------------------------------
// �L�[�t���[����ɂ���Active�_���A�O��̃L�[�t���[���̒����ʒu�Ɉړ�������
// �}���`�t���[������
//---------------------------------------------------
void ReshapeToolContextMenu::doTween() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // ���݂̃t���[���𓾂�
  int frame = project->getViewFrame();

  // �ΏۂƂȂ�_���A�V�F�C�v���Ƃɕ��ёւ���
  QList<OneShape> shapes;
  QList<QList<int>> indicesList;

  // ���̃��j���[�̑���ŉe����^����|�C���g
  for (int ep = 0; ep < m_effectedPoints.size(); ep++) {
    QPair<OneShape, int> pointPair = m_effectedPoints.at(ep);
    // QPair<IwShape*, int> pointPair = m_effectedPoints.at(ep);

    OneShape sh = pointPair.first;
    // IwShape* sh = pointPair.first;
    // �V�F�C�v��������Γo�^
    if (!shapes.contains(sh)) {
      shapes.append(sh);
      indicesList.append(QList<int>());
    }
    // �|�C���g��o�^
    indicesList[shapes.indexOf(sh)].append(pointPair.second);
  }

  // �V�F�C�v���Ƃ́A�ҏW���ꂽ�t���[�����Ƃ̌��̌`��̃��X�g
  QList<QMap<int, BezierPointList>> beforePoints;
  // �V�F�C�v���Ƃ́A�ҏW���ꂽ�t���[�����Ƃ̑����̌`��̃��X�g
  QList<QMap<int, BezierPointList>> afterPoints;
  // �L�[�t���[�����������ǂ����B���ׂ�true
  QList<bool> wasKeyFrame;

  // �e�ΏۃV�F�C�v�ɂ���
  for (int s = 0; s < shapes.size(); s++) {
    OneShape curShape = shapes.at(s);

    // �L�[�t���[���łȂ����continue
    if (!curShape.shapePairP->isFormKey(frame, curShape.fromTo)) continue;
    // ���̃V�F�C�v�̌`��L�[�t���[���̃��X�g
    QList<int> formKeyFrameList =
        curShape.shapePairP->getFormKeyFrameList(curShape.fromTo);
    // �L�[�t���[�����ŏ��^�Ō�̃L�[�t���[���Ȃ�continue
    int keyPos = formKeyFrameList.indexOf(frame);
    if (keyPos == 0 || keyPos == formKeyFrameList.size() - 1) continue;

    // ���̌`����i�[
    beforePoints.append(curShape.shapePairP->getFormData(curShape.fromTo));
    // �L�[�t���[�����������ǂ����B�S��true
    wasKeyFrame.append(true);

    //---------------------------------
    // �O��̃L�[����
    // �O��L�[�t���[������A��Ԃ̊��������߂�
    double interpRatio = (double)(frame - formKeyFrameList.at(keyPos - 1)) /
                         (double)(formKeyFrameList.at(keyPos + 1) -
                                  formKeyFrameList.at(keyPos - 1));
    BezierPointList beforeBPList = curShape.shapePairP->getBezierPointList(
        formKeyFrameList.at(keyPos - 1), curShape.fromTo);
    BezierPointList afterBPList = curShape.shapePairP->getBezierPointList(
        formKeyFrameList.at(keyPos + 1), curShape.fromTo);
    //---------------------------------
    // ���̃t���[���ł̃x�W�G�`��f�[�^���擾
    BezierPointList bpList =
        curShape.shapePairP->getBezierPointList(frame, curShape.fromTo);

    // ���̃V�F�C�v�̃A�N�e�B�u�_�̃��X�g
    QList<int> cpIndices = indicesList.at(s);
    // �e�A�N�e�B�u�|�C���g�ɂ���
    for (int i = 0; i < cpIndices.size(); i++) {
      int curIndex = cpIndices.at(i);

      bpList[curIndex].pos =
          beforeBPList.at(curIndex).pos * (1.0 - interpRatio) +
          afterBPList.at(curIndex).pos * interpRatio;
      bpList[curIndex].firstHandle =
          beforeBPList.at(curIndex).firstHandle * (1.0 - interpRatio) +
          afterBPList.at(curIndex).firstHandle * interpRatio;
      bpList[curIndex].secondHandle =
          beforeBPList.at(curIndex).secondHandle * (1.0 - interpRatio) +
          afterBPList.at(curIndex).secondHandle * interpRatio;
    }
    // �L�[���i�[
    curShape.shapePairP->setFormKey(frame, curShape.fromTo, bpList);

    // �����̌`����i�[
    afterPoints.append(curShape.shapePairP->getFormData(curShape.fromTo));
  }

  // Undo��o�^
  IwUndoManager::instance()->push(new ReshapeContextMenuUndo(
      shapes, beforePoints, afterPoints, wasKeyFrame, frame, project));

  // �V�O�i�����G�~�b�g
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  for (auto shape : shapes) {
    // �ύX�����t���[����invalidate
    int start, end;
    shape.shapePairP->getFormKeyRange(start, end, frame, shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, project->getParentShape(shape.shapePairP));
  }
}

//---------------------------------------------------
// �I���V�F�C�v�̑S�ẴL�[�t���[���́A�x�W�G�_�̏��Ԃ��t�ɂ���
// �}���`�t���[������
//---------------------------------------------------
void ReshapeToolContextMenu::doReverse() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // ���݂̃t���[���𓾂�
  int frame = project->getViewFrame();

  QList<OneShape> shapes = m_selection->getShapes();

  // �V�F�C�v���Ƃ́A�ҏW���ꂽ�t���[�����Ƃ̌��̌`��̃��X�g
  QList<QMap<int, BezierPointList>> beforePoints;
  // �V�F�C�v���Ƃ́A�ҏW���ꂽ�t���[�����Ƃ̑����̌`��̃��X�g
  QList<QMap<int, BezierPointList>> afterPoints;
  // �L�[�t���[�����������ǂ����B���ׂ�true
  QList<bool> wasKeyFrame;

  // �e�ΏۃV�F�C�v�ɂ���
  for (int s = 0; s < shapes.size(); s++) {
    OneShape curShape = shapes.at(s);

    wasKeyFrame.append(true);

    // ���̌`����i�[
    beforePoints.append(curShape.shapePairP->getFormData(curShape.fromTo));

    QMap<int, BezierPointList> formData =
        curShape.shapePairP->getFormData(curShape.fromTo);
    QMap<int, BezierPointList>::iterator i = formData.begin();
    while (i != formData.end()) {
      BezierPointList bpList = i.value();
      // Open�ȃV�F�C�v��Closa�ȃV�F�C�v���ŋ������Ⴄ
      // Close�Ȃ�A�|�C���g#1�i�n�_�̈ʒu�͌Œ�j
      if (curShape.shapePairP->isClosed()) {
        for (int j = 1; j < (int)((bpList.size() - 1) / 2) + 1; j++)
          bpList.swapItemsAt(j, bpList.size() - j);
      }
      // Open�Ȃ�A�[����v�f���������Ă���
      else {
        for (int j = 0; j < (int)(bpList.size() / 2); j++)
          bpList.swapItemsAt(j, bpList.size() - 1 - j);
      }

      // �n���h���ʒu�����ւ���
      for (int p = 0; p < bpList.size(); p++) {
        QPointF first          = bpList[p].firstHandle;
        bpList[p].firstHandle  = bpList[p].secondHandle;
        bpList[p].secondHandle = first;
      }

      formData[i.key()] = bpList;
      ++i;
    }

    afterPoints.append(formData);
    curShape.shapePairP->setFormData(formData, curShape.fromTo);
  }
  // Undo��o�^
  IwUndoManager::instance()->push(new ReshapeContextMenuUndo(
      shapes, beforePoints, afterPoints, wasKeyFrame, frame, project));

  // �V�O�i�����G�~�b�g
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  for (auto shape : shapes) {
    IwTriangleCache::instance()->invalidateShapeCache(
        project->getParentShape(shape.shapePairP));
  }
}

//---------------------------------------------------
// �ȉ��AUndo�R�}���h
//---------------------------------------------------

//---------------------------------------------------
// ReshapeContextMenuUndo ���ʂŎg��
//---------------------------------------------------

ReshapeContextMenuUndo::ReshapeContextMenuUndo(
    QList<OneShape>& shapes, QList<QMap<int, BezierPointList>>& beforePoints,
    QList<QMap<int, BezierPointList>>& afterPoints, QList<bool>& wasKeyFrame,
    int frame, IwProject* project)
    : m_shapes(shapes)
    , m_beforePoints(beforePoints)
    , m_afterPoints(afterPoints)
    , m_wasKeyFrame(wasKeyFrame)
    , m_frame(frame)
    , m_project(project)
    , m_firstFlag(true) {}

void ReshapeContextMenuUndo::undo() {
  // �e�V�F�C�v�Ƀ|�C���g���Z�b�g
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    // ����O�̓L�[�t���[���łȂ������ꍇ�A�L�[������
    if (!m_wasKeyFrame.at(s))
      shape.shapePairP->removeFormKey(m_frame, shape.fromTo);

    QMap<int, BezierPointList> beforeFormKeys = m_beforePoints.at(s);
    QMap<int, BezierPointList>::iterator i    = beforeFormKeys.begin();
    while (i != beforeFormKeys.end()) {
      // �|�C���g�������ւ�
      shape.shapePairP->setFormKey(i.key(), shape.fromTo, i.value());
      ++i;
    }
  }

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    for (auto shape : m_shapes) {
      // �ύX�����t���[����invalidate
      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }
}

void ReshapeContextMenuUndo::redo() {
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
    for (auto shape : m_shapes) {
      // �ύX�����t���[����invalidate
      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }
}