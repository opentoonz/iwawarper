//---------------------------------------------------
// IwShapePairSelection
// �V�F�C�v�y�A�̑I���̃N���X
//---------------------------------------------------

#include "iwshapepairselection.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwlayerhandle.h"
#include "iwproject.h"
#include "iwlayer.h"
#include "iwundomanager.h"
#include "mainwindow.h"
#include "iocommand.h"
#include "viewsettings.h"

#include "shapepairdata.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"

#include "iwselectionhandle.h"

#include "projectutils.h"
#include "iwtrianglecache.h"
#include "outputsettings.h"

#include <QClipboard>
#include <QApplication>
#include <QMessageBox>

//---------------------------------------------------
// �Ή��_������Undo
//---------------------------------------------------

class DeleteCorrPointUndo : public QUndoCommand {
  ShapePair* m_shape;
  IwProject* m_project;
  QMap<int, double> m_deletedCorrPos[2];
  int m_deletedIndex;

public:
  DeleteCorrPointUndo(ShapePair* shape, int index)
      : m_shape(shape), m_deletedIndex(index) {
    m_project = IwApp::instance()->getCurrentProject()->getProject();
    // �������Ή��_�ʒu��ۑ�
    for (int ft = 0; ft < 2; ft++) {
      QMap<int, CorrPointList> corrKeysMap = m_shape->getCorrData(ft);
      // �e�L�[�t���[���f�[�^�ɑΉ��_��}������
      QMap<int, CorrPointList>::iterator i = corrKeysMap.begin();
      while (i != corrKeysMap.end()) {
        CorrPointList cpList = i.value();
        m_deletedCorrPos[ft].insert(i.key(), cpList.at(index));
        ++i;
      }
    }
  }

  void undo() {
    // Shape�Q�ɑ΂��čs��
    for (int ft = 0; ft < 2; ft++) {
      QMap<int, CorrPointList> corrKeysMap = m_shape->getCorrData(ft);
      // �e�L�[�t���[���f�[�^�ɑΉ��_��}������
      QMap<int, CorrPointList>::iterator i = corrKeysMap.begin();
      while (i != corrKeysMap.end()) {
        CorrPointList cpList = i.value();
        assert(m_deletedCorrPos[ft].contains(i.key()));
        cpList.insert(m_deletedIndex, m_deletedCorrPos[ft].value(i.key()));
        // insert�R�}���h�́A���ɓ���Key�Ƀf�[�^���L�����ꍇ�A�u����������
        corrKeysMap.insert(i.key(), cpList);
        ++i;
      }
      // �f�[�^��߂� �Ή��_�J�E���g�͒��ōX�V���Ă���
      m_shape->setCorrData(corrKeysMap, ft);
    }
    if (m_project->isCurrent()) {
      IwApp::instance()->getCurrentProject()->notifyProjectChanged();
      // invalidate cache
      IwTriangleCache::instance()->invalidateShapeCache(
          m_project->getParentShape(m_shape));
    }
  }

  void redo() {
    bool ret = m_shape->deleteCorrPoint(m_deletedIndex);
    // �V�O�i�����G�~�b�g
    if (ret && m_project->isCurrent()) {
      IwApp::instance()->getCurrentProject()->notifyProjectChanged();
      // invalidate cache
      IwTriangleCache::instance()->invalidateShapeCache(
          m_project->getParentShape(m_shape));
    }
  }
};

//---------------------------------------------------
//---------------------------------------------------
IwShapePairSelection::IwShapePairSelection() {
  m_activeCorrPoint = QPair<OneShape, int>(OneShape(), -1);
}

//-----------------------------------------------------------------------------

IwShapePairSelection* IwShapePairSelection::instance() {
  static IwShapePairSelection _instance;
  return &_instance;
}

IwShapePairSelection::~IwShapePairSelection() {}

void IwShapePairSelection::enableCommands() {
  enableCommand(this, MI_Copy, &IwShapePairSelection::copyShapes);
  enableCommand(this, MI_Paste, &IwShapePairSelection::pasteShapes);
  enableCommand(this, MI_Cut, &IwShapePairSelection::cutShapes);
  enableCommand(this, MI_Delete, &IwShapePairSelection::deleteShapes);
  enableCommand(this, MI_ExportSelectedShapes,
                &IwShapePairSelection::exportShapes);
  enableCommand(this, MI_SelectAll, &IwShapePairSelection::selectAllShapes);
  enableCommand(this, MI_ToggleShapeLock, &IwShapePairSelection::toggleLocks);
  enableCommand(this, MI_ToggleVisibility,
                &IwShapePairSelection::toggleVisibility);
}

bool IwShapePairSelection::isEmpty() const { return m_shapes.isEmpty(); }

//---------------------------------------------------
// �I���̉���
//---------------------------------------------------
void IwShapePairSelection::selectNone() {
  m_shapes.clear();
  m_activePoints.clear();
  m_activeCorrPoint = QPair<OneShape, int>(OneShape(), -1);
}

//---------------------------------------------------
// ����܂ł̑I�����������āA�V�F�C�v���P�I��
//---------------------------------------------------
void IwShapePairSelection::selectOneShape(OneShape shape) {
  m_shapes.clear();
  m_activePoints.clear();
  m_shapes.push_back(shape);
  // �I���V�F�C�v�ƃA�N�e�B�u�Ή��_�V�F�C�v���قȂ�Ƃ��A����
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.first != shape)
    m_activeCorrPoint = QPair<OneShape, int>(OneShape(), -1);
}

//---------------------------------------------------
// �I���V�F�C�v��ǉ�����
//---------------------------------------------------
void IwShapePairSelection::addShape(OneShape shape) {
  if (!m_shapes.contains(shape)) m_shapes.push_back(shape);
}

//---------------------------------------------------
// �I���V�F�C�v����������
//---------------------------------------------------
int IwShapePairSelection::removeShape(OneShape shape) {
  // �����V�F�C�v�ƃA�N�e�B�u�Ή��_�V�F�C�v����v����Ƃ��A����
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.first == shape)
    m_activeCorrPoint = QPair<OneShape, int>(OneShape(), -1);
  return m_shapes.removeAll(shape);
}

//---------------------------------------------------
// FromTo�w��őI��������
//---------------------------------------------------
bool IwShapePairSelection::removeFromToShapes(int fromTo) {
  bool changed = false;
  for (int s = m_shapes.size() - 1; s >= 0; s--) {
    if (m_shapes.at(s).fromTo == fromTo) {
      m_shapes.removeAt(s);
      changed = true;
    }
  }
  // �����ׂ��V�F�C�v������������A�A�N�e�B�u�|�C���g������
  if (changed) {
    QList<int> pointsToBeRemoved;
    QSet<int>::const_iterator i = m_activePoints.constBegin();
    while (i != m_activePoints.constEnd()) {
      if (((*i) / 10000) % 2 == fromTo) pointsToBeRemoved.append(*i);
      ++i;
    }
    for (int p = 0; p < pointsToBeRemoved.size(); p++)
      m_activePoints.remove(pointsToBeRemoved.at(p));
  }
  // ����fromTo�ƃA�N�e�B�u�Ή��_�V�F�C�v��fromTo����v����Ƃ��A����
  if (m_activeCorrPoint.first.shapePairP &&
      m_activeCorrPoint.first.fromTo == fromTo)
    m_activeCorrPoint = QPair<OneShape, int>(OneShape(), -1);
  return changed;
}

//---------------------------------------------------
// �V�F�C�v���擾���� �͈͊O�͂O��Ԃ�
//---------------------------------------------------
OneShape IwShapePairSelection::getShape(int index) {
  if (index >= m_shapes.size() || index < 0) return 0;

  return m_shapes[index];
}

//---------------------------------------------------
// �����V�F�C�v���I��͈͂Ɋ܂܂�Ă�����true
//---------------------------------------------------
bool IwShapePairSelection::isSelected(OneShape shape) const {
  return m_shapes.contains(shape);
}

bool IwShapePairSelection::isSelected(ShapePair* shapePair) const {
  return isSelected(OneShape(shapePair, 0)) ||
         isSelected(OneShape(shapePair, 1));
}

//---------------------------------------------------
//---- Reshape�c�[���Ŏg�p
//---------------------------------------------------
// �|�C���g���I������Ă��邩�ǂ���
//---------------------------------------------------
bool IwShapePairSelection::isPointActive(OneShape shape, int pointIndex) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return false;
  // �V�F�C�v���A�N�e�B�u�łȂ����false
  if (!isSelected(shape)) return false;

  int shapeName = layer->getNameFromShapePair(shape);
  shapeName += pointIndex * 10;  // ���P���̓[��

  return m_activePoints.contains(shapeName);
}
//---------------------------------------------------
// �|�C���g��I������
//---------------------------------------------------
void IwShapePairSelection::activatePoint(int name) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;
  // �V�F�C�v���A�N�e�B�u�ɂ���
  OneShape shape = layer->getShapePairFromName(name);
  addShape(shape);

  // �I���|�C���g�ɒǉ�����
  m_activePoints.insert(name);
}
void IwShapePairSelection::activatePoint(OneShape shape, int index) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // �V�F�C�v��I������
  addShape(shape);

  int name = layer->getNameFromShapePair(shape);
  name += index * 10;  // ���P���̓[��

  // �A�N�e�B�u�|�C���g��ǉ�����
  m_activePoints.insert(name);
}
//---------------------------------------------------
// �|�C���g�I������������
//---------------------------------------------------
void IwShapePairSelection::deactivatePoint(int name) {
  // �V�F�C�v�̃A�N�e�B�u�^��A�N�e�B�u�͂��̂܂�
  // �I���|�C���g���������邾��
  m_activePoints.remove(name);
}
void IwShapePairSelection::deactivatePoint(OneShape shape, int index) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  int name = layer->getNameFromShapePair(shape);
  name += index * 10;

  // �V�F�C�v�̑I���^��I���͂��̂܂�
  // �I���|�C���g���������邾��
  m_activePoints.remove(name);
}

//---------------------------------------------------
// �I���|�C���g�̃��X�g��Ԃ�
//---------------------------------------------------
QList<QPair<OneShape, int>> IwShapePairSelection::getActivePointList() {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  QList<QPair<OneShape, int>> list;
  if (!layer) return list;
  QSet<int>::iterator i = m_activePoints.begin();
  // �e�A�N�e�B�u�ȃ|�C���g�ɂ���
  while (i != m_activePoints.end()) {
    int name = *i;
    // �V�F�C�v���A�N�e�B�u�ɂ���
    OneShape shape = layer->getShapePairFromName(name);
    int pId        = (int)((name % 10000) / 10);

    list.append(qMakePair(shape, pId));
    ++i;
  }
  return list;
}

//---------------------------------------------------
// �I���|�C���g��S�ĉ�������
//---------------------------------------------------
void IwShapePairSelection::deactivateAllPoints() { m_activePoints.clear(); }

//---------------------------------------------------
//-- �ȉ��A���̑I���Ŏg����R�}���h�Q
//---------------------------------------------------
// �R�s�[
//---------------------------------------------------

void IwShapePairSelection::copyShapes() {
  std::cout << "copyShapes" << std::endl;

  // �v���W�F�N�g�������return
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // ���g�̖����V�F�C�v��I������͂���
  QList<OneShape>::iterator i = m_shapes.begin();
  QList<ShapePair*> shapePairs;
  while (i != m_shapes.end()) {
    if ((*i).shapePairP && project->contains((*i).shapePairP) &&
        !shapePairs.contains((*i).shapePairP)) {
      shapePairs.append((*i).shapePairP);
    }
    i++;
  }

  if (shapePairs.isEmpty()) return;

  // sort shapes in stacking order
  std::sort(shapePairs.begin(), shapePairs.end(),
            [&](ShapePair* s1, ShapePair* s2) {
              int lay1, sha1, lay2, sha2;
              bool ret1 = project->getShapeIndex(s1, lay1, sha1);
              bool ret2 = project->getShapeIndex(s2, lay2, sha2);
              if (!ret2) return true;
              if (!ret1) return false;

              return (lay1 < lay2) ? true : (lay1 > lay2) ? false : sha1 < sha2;
            });

  // ShapeData�f�[�^�����A�I���V�F�C�v�f�[�^���i�[
  ShapePairData* shapePairData = new ShapePairData();

  shapePairData->setShapePairData(shapePairs);

  // �N���b�v�{�[�h�Ƀf�[�^������
  QClipboard* clipboard = QApplication::clipboard();
  clipboard->setMimeData(shapePairData, QClipboard::Clipboard);
}

//---------------------------------------------------
// �y�[�X�g
//---------------------------------------------------

void IwShapePairSelection::pasteShapes() {
  // �v���W�F�N�g�������return
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // �ǂ��̃��C���ɏ��������邩
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // �N���b�v�{�[�h����f�[�^�����AShapePairData���ǂ����m�F
  QClipboard* clipboard     = QApplication::clipboard();
  const QMimeData* mimeData = clipboard->mimeData();
  const ShapePairData* shapePairData =
      dynamic_cast<const ShapePairData*>(mimeData);
  // �f�[�^������Ă�����return
  if (!shapePairData) return;

  // �y�[�X�g���ꂽ�V�F�C�v���T���Ă����������
  QList<ShapePair*> pastedShapes;

  shapePairData->getShapePairData(layer, pastedShapes);

  // �y�[�X�g�����V�F�C�v��I������
  selectNone();

  for (int s = 0; s < pastedShapes.size(); s++) {
    addShape(OneShape(pastedShapes.at(s), 0));
    addShape(OneShape(pastedShapes.at(s), 1));
  }

  // Undo�ɓo�^ ������redo���Ă΂�邪�A�ŏ��̓t���O�ŉ������
  IwUndoManager::instance()->push(
      new PasteShapePairUndo(pastedShapes, project, layer));

  IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

//---------------------------------------------------
// �J�b�g
//---------------------------------------------------

void IwShapePairSelection::cutShapes() {
  copyShapes();
  deleteShapes();
}

//---------------------------------------------------
// ����
//---------------------------------------------------

void IwShapePairSelection::deleteShapes() {
  // �v���W�F�N�g�������return
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // ���g�̖����V�F�C�v��I������͂���
  QList<OneShape>::iterator i = m_shapes.begin();
  while (i != m_shapes.end()) {
    if ((*i).shapePairP == 0 || !project->contains((*i).shapePairP))
      m_shapes.erase(i);
    else
      i++;
  }

  if (m_shapes.isEmpty()) return;

  // CorrespondenceTool �g�p���A�Ή��_���I������Ă���ꍇ�͑Ή��_����
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.second >= 0) {
    onDeleteCorrPoint();
    return;
  }

  // activePoint���I������Ă���ꍇ�́A�|�C���g����
  QList<QPair<OneShape, int>> activePointList = getActivePointList();
  // ���g�̖����V�F�C�v��I������͂���
  QList<QPair<OneShape, int>>::iterator ap = activePointList.begin();
  while (ap != activePointList.end()) {
    if ((*ap).first.shapePairP == 0 ||
        !project->contains((*ap).first.shapePairP))
      activePointList.erase(ap);
    else
      ap++;
  }
  // �����|�C���g���I������Ă���ꍇ,�|�C���g�������[�h
  if (!activePointList.isEmpty()) {
    //	�悸�A�|�C���g�������\���ǂ������`�F�b�N����B
    //  �����F������̃V�F�C�v�̃|�C���g�����A
    //  Closed�F2�ȉ��AOpen�F�P�ȉ��Ȃ�����Ȃ�
    QMap<ShapePair*, QList<int>> deletingPointsMap;
    for (int p = 0; p < activePointList.size(); p++) {
      QPair<OneShape, int> pair = activePointList.at(p);
      // �����̃��X�g�ɒǉ�
      if (deletingPointsMap.contains(pair.first.shapePairP)) {
        QList<int>& list = deletingPointsMap[pair.first.shapePairP];
        if (!list.contains(pair.second)) list.push_back(pair.second);
      }
      // �V���Ƀ��X�g�����
      else {
        QList<int> list;
        list.push_back(pair.second);
        deletingPointsMap[pair.first.shapePairP] = list;
      }
    }

    QMapIterator<ShapePair*, QList<int>> j(deletingPointsMap);
    bool canDelete = true;
    while (j.hasNext()) {
      j.next();
      if (j.key()->isClosed() &&
          (j.key()->getBezierPointAmount() - j.value().size()) < 3)
        canDelete = false;
      else if (!j.key()->isClosed() &&
               (j.key()->getBezierPointAmount() - j.value().size()) < 2)
        canDelete = false;

      if (!canDelete) break;
    }

    // �������b�Z�[�W�o�������H
    if (!canDelete) return;

    // �|�C���g�����R�}���h
    ProjectUtils::deleteFormPoints(deletingPointsMap);
  }

  //-------------------

  // �|�C���g�I�����Ȃ��ꍇ�A�V�F�C�v����
  else {
    QList<ShapeInfo> deletedShapes;
    for (int s = 0; s < m_shapes.size(); s++) {
      ShapePair* shapePair = m_shapes.at(s).shapePairP;
      if (!shapePair || !project->contains(shapePair)) continue;

      IwLayer* layer = project->getLayerFromShapePair(shapePair);
      int index      = layer->getIndexFromShapePair(shapePair);
      ShapeInfo info = {index, shapePair, layer};
      deletedShapes.append(info);

      layer->deleteShapePair(index);
    }

    // �I��������
    selectNone();

    IwUndoManager::instance()->push(
        new DeleteShapePairUndo(deletedShapes, project));
  }

  IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

//---------------------------------------------------

void IwShapePairSelection::onDeleteCorrPoint() {
  assert(m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.second);
  ShapePair* shapePair     = m_activeCorrPoint.first.shapePairP;
  int corrIndexToBeDeleted = m_activeCorrPoint.second;

  // �o�^��redo���s
  IwUndoManager::instance()->push(
      new DeleteCorrPointUndo(shapePair, corrIndexToBeDeleted));

  // �Ή��_�I�����N���A
  setActiveCorrPoint(OneShape(), -1);
}

//---------------------------------------------------
// �\������Shape��S�I��
//---------------------------------------------------

void IwShapePairSelection::selectAllShapes() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  IwLayer* currentLayer = IwApp::instance()->getCurrentLayer()->getLayer();

  // ��������I������
  selectNone();

  // ���b�N���𓾂�
  bool fromToVisible[2];
  for (int fromTo = 0; fromTo < 2; fromTo++)
    fromToVisible[fromTo] = project->getViewSettings()->isFromToVisible(fromTo);

  // �e���C���ɂ���
  for (int lay = project->getLayerCount() - 1; lay >= 0; lay--) {
    // ���C�����擾
    IwLayer* layer = project->getLayer(lay);
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
        if (shapePair->isLocked(fromTo)) continue;
        addShape(OneShape(shapePair, fromTo));
      }
    }
  }

  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//---------------------------------------------------
// �I���V�F�C�v�̃G�N�X�|�[�g
void IwShapePairSelection::exportShapes() {
  if (isEmpty()) {
    QMessageBox::warning(IwApp::instance()->getMainWindow(), tr("Warning"),
                         tr("No shapes are selected."));
    return;
  }
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  QList<ShapePair*> shapePairs;
  for (auto shape : m_shapes) {
    if (!shapePairs.contains(shape.shapePairP))
      shapePairs.append(shape.shapePairP);
  }

  // �d�ˏ��ɕ��ׂ�
  std::sort(shapePairs.begin(), shapePairs.end(),
            [&](ShapePair* s1, ShapePair* s2) {
              int layerId1, shapeId1, layerId2, shapeId2;
              project->getShapeIndex(s1, layerId1, shapeId1);
              project->getShapeIndex(s2, layerId2, shapeId2);
              if (layerId1 != layerId2) return layerId1 < layerId2;
              return shapeId1 < shapeId2;
            });

  if (IoCmd::exportShapes(shapePairs))
    QMessageBox::information(
        IwApp::instance()->getMainWindow(), tr("Info"),
        tr("%1 shapes are exported successfully.").arg(shapePairs.count()));
}

//---------------------------------------------------
// �I���V�F�C�v�̃��b�N�؂�ւ�
void IwShapePairSelection::toggleLocks() {
  if (isEmpty()) return;
  // �v���W�F�N�g�������return
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  bool doLock = false;
  // �I���V�F�C�v�Ɉ�ł������b�N�̂��̂�����΁A�S�ă��b�N
  // �����łȂ���΃��b�N����
  for (auto shape : m_shapes) {
    if (!shape.shapePairP->isLocked(shape.fromTo)) {
      doLock = true;
      break;
    }
  }

  // �o�^��redo���s
  IwUndoManager::instance()->push(
      new LockShapesUndo(m_shapes, project, doLock));
}

//---------------------------------------------------
// �I���V�F�C�v�̃s���؂�ւ�
void IwShapePairSelection::togglePins() {
  if (isEmpty()) return;
  // �v���W�F�N�g�������return
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  bool doPin = false;
  // �I���V�F�C�v�Ɉ�ł����s���̂��̂�����΁A�S�ăs��
  // �����łȂ���΃s������
  for (auto shape : m_shapes) {
    if (!shape.shapePairP->isPinned(shape.fromTo)) {
      doPin = true;
      break;
    }
  }

  // �o�^��redo���s
  IwUndoManager::instance()->push(new PinShapesUndo(m_shapes, project, doPin));
}

//---------------------------------------------------
// �I���V�F�C�v�̃s���؂�ւ�
void IwShapePairSelection::toggleVisibility() {
  if (isEmpty()) return;
  // �v���W�F�N�g�������return
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  bool setVisible = false;
  QList<ShapePair*> shapePairList;
  // �I���V�F�C�v�Ɉ�ł���\���̂��̂�����΁A�S�ĕ\��
  // �����łȂ���Δ�\����
  for (auto shape : m_shapes) {
    ShapePair* shapePair = shape.shapePairP;
    if (shapePairList.contains(shapePair)) continue;
    shapePairList.append(shapePair);
    if (!setVisible && !shapePair->isVisible()) {
      setVisible = true;
    }
  }

  // �o�^��redo���s
  IwUndoManager::instance()->push(
      new SetVisibleShapesUndo(shapePairList, project, setVisible));
}

//---------------------------------------------------
// �V�F�C�v�^�O�؂�ւ�
//---------------------------------------------------
void IwShapePairSelection::setShapeTag(int tagId, bool on) {
  if (isEmpty()) return;
  // �v���W�F�N�g�������return
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // �o�^��redo���s
  IwUndoManager::instance()->push(
      new SetShapeTagUndo(m_shapes, project, tagId, on));
}

//---------------------------------------------------
// �ȉ��AUndo�R�}���h��o�^
//---------------------------------------------------

//---------------------------------------------------
// �y�[�X�g��Undo
//---------------------------------------------------

PasteShapePairUndo::PasteShapePairUndo(QList<ShapePair*>& shapes,
                                       IwProject* project, IwLayer* layer)
    : m_shapes(shapes), m_project(project), m_layer(layer), m_firstFlag(true) {}

PasteShapePairUndo::~PasteShapePairUndo() {
  // �V�F�C�v��delete
  for (int s = 0; s < m_shapes.size(); s++) {
    delete m_shapes[s];
    m_shapes[s] = 0;
  }
}

void PasteShapePairUndo::undo() {
  for (int s = 0; s < m_shapes.size(); s++)
    m_layer->deleteShapePair(m_shapes.at(s), true);

  // �v���W�F�N�g���J�����g�Ȃ�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void PasteShapePairUndo::redo() {
  // Undo�o�^���ɂ�redo���s��Ȃ���[�ɂ���
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  for (int s = 0; s < m_shapes.size(); s++)
    m_layer->addShapePair(m_shapes.at(s));

  // �v���W�F�N�g���J�����g�Ȃ�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

//---------------------------------------------------
// �J�b�g/������Undo
//---------------------------------------------------

DeleteShapePairUndo::DeleteShapePairUndo(QList<ShapeInfo>& shapes,
                                         IwProject* project)
    : m_shapes(shapes), m_project(project) {}

DeleteShapePairUndo::~DeleteShapePairUndo() {}

void DeleteShapePairUndo::undo() {
  // �V�F�C�v��߂�
  for (int s = 0; s < m_shapes.size(); s++) {
    ShapeInfo info = m_shapes.at(s);
    // trianglecache�̏����͒��ł���Ă���
    info.layer->addShapePair(info.shapePair, info.index);
    //++s;
  }

  // �v���W�F�N�g���J�����g�Ȃ�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void DeleteShapePairUndo::redo() {
  // Undo�o�^���ɂ�redo���s��Ȃ���[�ɂ���
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  // �V�F�C�v������
  for (int s = 0; s < m_shapes.size(); s++) {
    ShapeInfo info = m_shapes.at(s);

    // trianglecache�̏����͒��ł���Ă���
    info.layer->deleteShapePair(info.index);
    //++s;
  }

  // �v���W�F�N�g���J�����g�Ȃ�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

//---------------------------------------------------
// ���b�N�؂�ւ���Undo
//---------------------------------------------------

LockShapesUndo::LockShapesUndo(QList<OneShape>& shapes, IwProject* project,
                               bool doLock)
    : m_project(project), m_doLock(doLock) {
  // ����O�̏�Ԃ�ۑ�
  for (auto shape : shapes)
    m_shape_status.append({shape, shape.shapePairP->isLocked(shape.fromTo)});
}

void LockShapesUndo::lockShapes(bool isUndo) {
  int selectedCount = 0;
  for (auto shape_state : m_shape_status) {
    OneShape shape = shape_state.first;
    if (isUndo)
      shape.shapePairP->setLocked(shape.fromTo, shape_state.second);
    else  // redo
      shape.shapePairP->setLocked(shape.fromTo, m_doLock);
    if (shape.shapePairP->isLocked(shape.fromTo))
      selectedCount += IwShapePairSelection::instance()->removeShape(shape);
  }
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    if (selectedCount > 0)
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
  }
}

void LockShapesUndo::undo() { lockShapes(true); }
void LockShapesUndo::redo() { lockShapes(false); }

//---------------------------------------------------
// �s���؂�ւ���Undo
//---------------------------------------------------

PinShapesUndo::PinShapesUndo(QList<OneShape>& shapes, IwProject* project,
                             bool doPin)
    : m_project(project), m_doPin(doPin) {
  // ����O�̏�Ԃ�ۑ�
  for (auto shape : shapes)
    m_shape_status.append({shape, shape.shapePairP->isPinned(shape.fromTo)});
}

void PinShapesUndo::pinShapes(bool isUndo) {
  for (auto shape_state : m_shape_status) {
    OneShape shape = shape_state.first;
    if (isUndo)
      shape.shapePairP->setPinned(shape.fromTo, shape_state.second);
    else  // redo
      shape.shapePairP->setPinned(shape.fromTo, m_doPin);
  }
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
  }
}

void PinShapesUndo::undo() { pinShapes(true); }
void PinShapesUndo::redo() { pinShapes(false); }

//---------------------------------------------------
// �\���A��\���؂�ւ���Undo
//---------------------------------------------------

SetVisibleShapesUndo::SetVisibleShapesUndo(QList<ShapePair*>& shapePairs,
                                           IwProject* project, bool setVisible)
    : m_project(project), m_setVisible(setVisible) {
  // ����O�̏�Ԃ�ۑ�
  for (auto shapePair : shapePairs)
    m_shapePair_status.append({shapePair, shapePair->isVisible()});
}

void SetVisibleShapesUndo::setVisibleShapes(bool isUndo) {
  for (auto shapePair_state : m_shapePair_status) {
    ShapePair* shapePair = shapePair_state.first;
    if (isUndo)
      shapePair->setVisible(shapePair_state.second);
    else  // redo
      shapePair->setVisible(m_setVisible);
  }
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
  }
}

void SetVisibleShapesUndo::undo() { setVisibleShapes(true); }
void SetVisibleShapesUndo::redo() { setVisibleShapes(false); }

//---------------------------------------------------
// �V�F�C�v�^�O�؂�ւ���Undo
//---------------------------------------------------

SetShapeTagUndo::SetShapeTagUndo(QList<OneShape>& shapes, IwProject* project,
                                 int tagId, bool on)
    : m_project(project), m_tagId(tagId), m_on(on) {
  // ����O�̏�Ԃ�ۑ�
  for (auto shape : shapes)
    m_shape_status.append(
        {shape, shape.shapePairP->containsShapeTag(shape.fromTo, m_tagId)});
}

void SetShapeTagUndo::setTag(bool isUndo) {
  for (auto shape_state : m_shape_status) {
    OneShape shape = shape_state.first;
    bool on        = (isUndo) ? shape_state.second : m_on;
    if (on)
      shape.shapePairP->addShapeTag(shape.fromTo, m_tagId);
    else
      shape.shapePairP->removeShapeTag(shape.fromTo, m_tagId);
  }
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // �����_�����O�Ɋւ��^�O�̏ꍇ�V�O�i�����o��
    if (m_tagId == m_project->getOutputSettings()->getShapeTagId())
      IwApp::instance()->getCurrentProject()->notifyPreviewCacheInvalidated();
  }
}

void SetShapeTagUndo::undo() { setTag(true); }

void SetShapeTagUndo::redo() { setTag(false); }
