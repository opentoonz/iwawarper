//---------------------------------------
// ProjectUtils
// �V�[������̃R�}���h�̂������{Undo
//---------------------------------------

#include "projectutils.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwlayerhandle.h"
#include "iwproject.h"
#include "iwlayer.h"
#include "shapepair.h"
#include "iwshapepairselection.h"
#include "iwtimelinekeyselection.h"
#include "iwtimelineselection.h"
#include "iwselectionhandle.h"
#include "shapeinterpolationpopup.h"
#include "matteinfodialog.h"
#include "menubarcommand.h"
#include "menubarcommandids.h"

#include "iwundomanager.h"
#include "iwtrianglecache.h"
#include <QMessageBox>
#include <QXmlStreamReader>

//-----
// IwLayer�ɑ΂��鑀��
//-----
// ���C�������̓���ւ�
void ProjectUtils::SwapLayers(int index1, int index2) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  if (index1 < 0 || index1 >= prj->getLayerCount() || index2 < 0 ||
      index2 >= prj->getLayerCount() || index1 == index2)
    return;

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new SwapLayersUndo(prj, index1, index2));
}

bool ProjectUtils::ReorderLayers(QList<IwLayer*> newOrderedLayers) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return false;

  if (prj->getLayerCount() != newOrderedLayers.count()) return false;

  for (int lay = 0; lay < prj->getLayerCount(); lay++) {
    IwLayer* layer = prj->getLayer(lay);
    if (newOrderedLayers.count(layer) != 1) return false;
  }

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new ReorderLayersUndo(prj, newOrderedLayers));
  return true;
}

// ���l�[��
void ProjectUtils::RenameLayer(IwLayer* layer, QString& newName) {
  if (newName.isEmpty()) return;
  QString oldName = layer->getName();
  if (newName == oldName) return;

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(
      new RenameLayerUndo(IwApp::instance()->getCurrentProject()->getProject(),
                          layer, oldName, newName));
}

// ���C���폜
void ProjectUtils::DeleteLayer(int index) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  if (index < 0 || index >= prj->getLayerCount()) return;

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new DeleteLayerUndo(prj, index));
}

// ���C������
void ProjectUtils::CloneLayer(int index) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  if (index < 0 || index >= prj->getLayerCount()) return;

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new CloneLayerUndo(prj, index));
}

// ���C�����̕ύX
void ProjectUtils::toggleLayerProperty(IwLayer* layer,
                                       ProjectUtils::LayerPropertyType type) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new ChangeLayerProperyUndo(prj, layer, type));
}

//-----
// ShapePair�ɑ΂��鑀��
//-----

// �V�F�C�v�����̓���ւ�
void ProjectUtils::SwapShapes(IwLayer* layer, int index1, int index2) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  if (!prj->contains(layer)) return;
  if (index1 < 0 || index1 >= layer->getShapePairCount() || index2 < 0 ||
      index2 >= layer->getShapePairCount() || index1 == index2)
    return;

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(
      new SwapShapesUndo(prj, layer, index1, index2));
}

// �V�F�C�v�̈ړ� ���Ɉړ�����ꍇ�AdstIndex��index�̃A�C�e���𔲂�����̒l
void ProjectUtils::MoveShape(IwLayer* layer, int index, int dstIndex) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  if (!prj->contains(layer)) return;
  if (index < 0 || index >= layer->getShapePairCount() || dstIndex < 0 ||
      dstIndex >= layer->getShapePairCount() || index == dstIndex)
    return;

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(
      new MoveShapeUndo(prj, layer, index, dstIndex));
}

// ���C���[���܂����Ńh���b�O�ړ��B�Ō��bool��isParent�̏��
void ProjectUtils::ReorderShapes(
    QMap<IwLayer*, QList<QPair<ShapePair*, bool>>> layerShapeInfo) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new ReorderShapesUndo(prj, layerShapeInfo));
}

// ���l�[��
void ProjectUtils::RenameShapePair(ShapePair* shape, QString& newName) {
  if (newName.isEmpty()) return;
  QString oldName = shape->getName();
  if (newName == oldName) return;

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new RenameShapePairUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shape, oldName,
      newName));
}

// �|�C���g����
void ProjectUtils::deleteFormPoints(QMap<ShapePair*, QList<int>>& pointList) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // Undo���
  DeleteFormPointsUndo* undo =
      new DeleteFormPointsUndo(project, pointList.keys());

  // �����̏���
  QMapIterator<ShapePair*, QList<int>> itr(pointList);
  while (itr.hasNext()) {
    itr.next();
    ShapePair* shape             = itr.key();
    QList<int> deletingPointList = itr.value();
    // deletingPointList�������ɂ���
    std::sort(deletingPointList.begin(), deletingPointList.end());

    // �����O��Bezier�|�C���g��
    int oldPointAmount = shape->getBezierPointAmount();
    // ��������̎c��Bezier�|�C���g��
    int remainingPointAmount =
        shape->getBezierPointAmount() - deletingPointList.size();

    // �� �V�F�C�v��Open�Ȃ�ACorr�͑S�̂��k��
    if (!shape->isClosed()) {
      // �ΏۃV�F�C�v��FromTo�ɂ���
      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // �@�`��f�[�^�̏���
        // �`��f�[�^�𓾂�
        QMap<int, BezierPointList> formData = shape->getFormData(fromTo);
        QMapIterator<int, BezierPointList> formItr(formData);
        while (formItr.hasNext()) {
          formItr.next();
          BezierPointList bpList = formItr.value();
          // �|�C���g����납������Ă���
          for (int dp = deletingPointList.size() - 1; dp >= 0; dp--)
            bpList.removeAt(deletingPointList[dp]);
          // �l���㏑���Ŗ߂�
          formData.insert(formItr.key(), bpList);
        }
        // �`��f�[�^���㏑���Ŗ߂�(�`��_�̐��͎����ōX�V�����)
        shape->setFormData(formData, fromTo);

        // �A�Ή��_�f�[�^���u�߂�v
        // �߂銄��
        double corrShrinkRatio =
            (double)(remainingPointAmount - 1) / (double)(oldPointAmount - 1);
        // �Ή��_�f�[�^�𓾂�
        QMap<int, CorrPointList> corrData = shape->getCorrData(fromTo);
        QMapIterator<int, CorrPointList> corrItr(corrData);
        while (corrItr.hasNext()) {
          corrItr.next();
          CorrPointList cpList = corrItr.value();
          // �|�C���g�S�ĂɁA�u�߂銄���v���|���Ă���
          for (int cp = 0; cp < cpList.size(); cp++)
            cpList[cp].value *= corrShrinkRatio;
          // �l���㏑���Ŗ߂�
          corrData.insert(corrItr.key(), cpList);
        }
        // �Ή��_�f�[�^���㏑���Ŗ߂�
        shape->setCorrData(corrData, fromTo);
      }
    }

    // �� �V�F�C�v��Closed�Ȃ�
    else {
      // �eBezier���_���A������̐V����Corr�z�u��
      //  Corr�̒l�����ɂȂ�̂��́A�e�[�u�������

      // �ׂ荇�����|�C���g�Ōł܂�����
      QList<QList<int>> deletingPointClusters;
      // �V����Corr�̃e�[�u��
      QList<double> newCorrTable;
      for (int c = 0; c < oldPointAmount; c++) newCorrTable.push_back(c);

      // �����Ȃ��|�C���g��T��
      int startIndex = 0;
      // �|�C���g�O�������Ȃ�A�O�ɐi��ŏ����Ȃ��|�C���g�̎n�܂��������
      if (deletingPointList.contains(startIndex)) {
        while (deletingPointList.contains(startIndex)) startIndex++;
      }
      // �n�߂�1����Ċi�[
      deletingPointClusters.push_back(QList<int>());
      // �ł܂�����
      for (int i = startIndex; i < startIndex + oldPointAmount; i++) {
        int index = (i >= oldPointAmount) ? i - oldPointAmount : i;
        if (deletingPointList.contains(index)) {
          // ���K�̃��X�g�ɒǉ�����
          deletingPointClusters.last().push_back(index);
        } else {
          // ���K�̃��X�g���󂶂�Ȃ���΁A���X�g��V���ɍ���Ēǉ�����
          if (!deletingPointClusters.last().isEmpty())
            deletingPointClusters.push_back(QList<int>());
        }
      }
      // �Ō�̃��X�g����Ȃ�A����
      if (deletingPointClusters.last().isEmpty())
        deletingPointClusters.removeLast();

      int currentPivot        = startIndex - 1;
      int currentClusterIndex = 0;

      for (int i = startIndex; i < startIndex + oldPointAmount;) {
        int index = (i >= oldPointAmount) ? i - oldPointAmount : i;
        if (!deletingPointList.contains(index)) {
          currentPivot++;
          newCorrTable[index] = (double)(currentPivot);
          i++;
        } else {
          int delSize = deletingPointClusters[currentClusterIndex].size();
          // �N���X�^�ɓ˓�
          for (int c = 0; c < delSize; c++) {
            int tmpIndex = (index + c >= oldPointAmount)
                               ? index + c - oldPointAmount
                               : index + c;
            newCorrTable[tmpIndex] =
                (double)currentPivot + (double)(c + 1) / (double)(delSize + 1);
          }
          i += delSize;
          currentClusterIndex++;
        }
      }

      // �e�[�u�������ɁA�Ή��_���ăv���b�g���Ă���

      // �ΏۃV�F�C�v��FromTo�ɂ���
      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // �@�`��f�[�^�̏���
        // �`��f�[�^�𓾂�
        QMap<int, BezierPointList> formData = shape->getFormData(fromTo);
        QMapIterator<int, BezierPointList> formItr(formData);
        while (formItr.hasNext()) {
          formItr.next();
          BezierPointList bpList = formItr.value();
          // �|�C���g����납������Ă���
          for (int dp = deletingPointList.size() - 1; dp >= 0; dp--)
            bpList.removeAt(deletingPointList[dp]);
          // �l���㏑���Ŗ߂�
          formData.insert(formItr.key(), bpList);
        }
        // �`��f�[�^���㏑���Ŗ߂�(�`��_�̐��͎����ōX�V�����)
        shape->setFormData(formData, fromTo);

        // �Ή��_�f�[�^�𓾂�
        QMap<int, CorrPointList> corrData = shape->getCorrData(fromTo);
        QMapIterator<int, CorrPointList> corrItr(corrData);
        while (corrItr.hasNext()) {
          corrItr.next();
          CorrPointList cpList = corrItr.value();
          // �|�C���g�S�ĂɁA�u�߂銄���v���|���Ă���
          for (int cp = 0; cp < cpList.size(); cp++) {
            double tmpCorr = cpList.at(cp).value;
            int k1         = (int)tmpCorr;
            double ratio   = tmpCorr - (double)k1;
            int k2         = (k1 + 1 >= oldPointAmount) ? 0 : k1 + 1;
            double c1      = newCorrTable[k1];
            double c2      = newCorrTable[k2];
            // �Ȑ��̌p���ڂŁA������ăC���f�b�N�X�����O�ɖ߂�ꍇ
            if (c2 < c1) c2 += remainingPointAmount;
            double newCorr = c1 * (1.0 - ratio) + c2 * ratio;
            // ��Ԍ�ACorr�l���O�𒴂����ꍇ�N�����v����
            if (newCorr >= remainingPointAmount)
              newCorr -= remainingPointAmount;
            cpList.replace(cp, {newCorr, cpList.at(cp).weight});
          }
          // �l���㏑���Ŗ߂�
          corrData.insert(corrItr.key(), cpList);
        }
        // �Ή��_�f�[�^���㏑���Ŗ߂�
        shape->setCorrData(corrData, fromTo);
      }
    }

    // invalidate cache
    IwTriangleCache::instance()->invalidateShapeCache(
        project->getParentShape(shape));
  }

  undo->storeAfterData();

  // Undo�ɓo�^
  IwUndoManager::instance()->push(undo);
}

// �e�q��؂�ւ���
void ProjectUtils::switchParentChild(ShapePair* shape) {
  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new SwitchParentChildUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shape));
}

// ���b�N��؂�ւ���
void ProjectUtils::switchLock(OneShape shape) {
  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new SwitchLockUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shape));
}

// �s����؂�ւ���
void ProjectUtils::switchPin(OneShape shape) {
  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new SwitchPinUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shape));
}

// �\����\����؂�ւ���
void ProjectUtils::switchShapeVisibility(ShapePair* shapePair) {
  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new SwitchShapeVisibilityUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shapePair));
}

// �v���W�F�N�g���Ƀ��b�N���ꂽ�V�F�C�v�����邩�ǂ���
bool ProjectUtils::hasLockedShape() {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return false;
  for (int i = 0; i < prj->getLayerCount(); i++) {
    IwLayer* layer = prj->getLayer(i);
    if (!layer) continue;
    for (int s = 0; s < layer->getShapePairCount(); s++) {
      ShapePair* sp = layer->getShapePair(s);
      if (!sp) continue;
      if (sp->isLocked(0) || sp->isLocked(1)) return true;
    }
  }
  return false;
}

// �S�Ẵ��b�N���ꂽ�V�F�C�v���A�����b�N
void ProjectUtils::unlockAllShapes() {
  QList<OneShape> lockedShapes;
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  for (int i = 0; i < prj->getLayerCount(); i++) {
    IwLayer* layer = prj->getLayer(i);
    if (!layer) continue;
    for (int s = 0; s < layer->getShapePairCount(); s++) {
      ShapePair* sp = layer->getShapePair(s);
      if (!sp) continue;
      for (int fromTo = 0; fromTo < 2; fromTo++) {
        if (sp->isLocked(fromTo)) lockedShapes.append(OneShape(sp, fromTo));
      }
    }
  }
  if (lockedShapes.isEmpty()) return;

  // �o�^��redo���s
  IwUndoManager::instance()->push(new LockShapesUndo(lockedShapes, prj, false));
}

//-----
// ���[�N�G���A�̃T�C�Y�ύX
//-----
bool ProjectUtils::resizeWorkArea(QSize& newSize) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return false;
  QSize oldSize = prj->getWorkAreaSize();
  if (newSize == oldSize) return false;

  // �S�ẴV�F�C�v�̃��X�g�����
  QList<ShapePair*> shapePairs;
  for (int i = 0; i < prj->getLayerCount(); i++) {
    IwLayer* layer = prj->getLayer(i);
    for (int s = 0; s < layer->getShapePairCount(); s++)
      shapePairs.append(layer->getShapePair(s));
  }

  bool keepShapeSize = false;
  // �V�F�C�v���Ȃ���΃T�C�Y��ύX���邾��
  // �V�F�C�v������ꍇ�A�V�F�C�v�̃T�C�Y���ێ����邩�A�V�����T�C�Y�ɏ]���ĕό`���邩��I��
  if (!shapePairs.isEmpty()) {
    QMessageBox messageBox(
        QMessageBox::Question, QObject::tr("Resizing work area : Question"),
        QObject::tr(
            "Do you want to shrink the shapes fitting with the work area?"));
    messageBox.addButton(QObject::tr("Shrink the shapes"),
                         QMessageBox::YesRole);
    QPushButton* keep = messageBox.addButton(
        QObject::tr("Keep the shapes unchanged"), QMessageBox::NoRole);
    QPushButton* cancel =
        messageBox.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
    messageBox.exec();
    if (messageBox.clickedButton() == (QAbstractButton*)keep)
      keepShapeSize = true;
    else if (messageBox.clickedButton() == (QAbstractButton*)cancel)
      return false;
    // �_�C�A���O��P�ɕ����ꍇ�͕ό`��I�����ď����p��
  }

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(new resizeWorkAreaUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shapePairs, oldSize,
      newSize, keepShapeSize));

  return true;
}

//---------------------------------------------------

void ProjectUtils::importProjectData(QXmlStreamReader& reader) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  QSize childSize;
  QList<IwLayer*> importedLayers;
  while (reader.readNextStartElement()) {
    // ImageSize (���[�N�G���A�̃T�C�Y)
    if (reader.name() == "ImageSize") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "width") {
          int w = reader.readElementText().toInt();
          childSize.setWidth(w);
        } else if (reader.name() == "height") {
          int h = reader.readElementText().toInt();
          childSize.setHeight(h);
        }
      }
    }
    // Layers
    else if (reader.name() == "Layers") {
      // ���[�h���āA���܂܂ł̃��[���ƍ����ւ���
      while (reader.readNextStartElement()) {
        if (reader.name() == "Layer") {
          // ���[�h����Ƃ���
          IwLayer* layer = prj->appendLayer();
          layer->loadData(reader);
          importedLayers.append(layer);
        } else
          reader.skipCurrentElement();
      }
    } else
      reader.skipCurrentElement();
  }

  ProjectUtils::resizeImportedShapes(importedLayers, prj->getWorkAreaSize(),
                                     childSize);

  IwUndoManager::instance()->push(new ImportProjectUndo(prj, importedLayers));
}

void ProjectUtils::importShapesData(QXmlStreamReader& reader) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  QList<ShapePair*> importedShapes;
  while (reader.readNextStartElement()) {
    if (reader.name() == "ShapePair") {
      ShapePair* shapePair = new ShapePair();

      shapePair->loadData(reader);

      // �v���W�F�N�g�ɒǉ�
      layer->addShapePair(shapePair);
      importedShapes.append(shapePair);
    } else
      reader.skipCurrentElement();
  }

  IwUndoManager::instance()->push(
      new ImportShapesUndo(prj, layer, importedShapes));
}

//---------------------------------------------------

void ProjectUtils::resizeImportedShapes(QList<IwLayer*> importedLayers,
                                        QSize workAreaSize, QSize childSize) {
  if (workAreaSize == childSize) return;

  // �S�ẴV�F�C�v�̃��X�g�����
  QList<ShapePair*> shapePairs;
  for (auto layer : importedLayers) {
    for (int s = 0; s < layer->getShapePairCount(); s++)
      shapePairs.append(layer->getShapePair(s));
  }
  if (shapePairs.isEmpty()) return;
  bool keepShapeSize = false;
  // �V�F�C�v���Ȃ���΃T�C�Y��ύX���邾��
  // �V�F�C�v������ꍇ�A�V�F�C�v�̃T�C�Y���ێ����邩�A�V�����T�C�Y�ɏ]���ĕό`���邩��I��
  QMessageBox messageBox(
      QMessageBox::Question, QObject::tr("Resizing work area : Question"),
      QObject::tr(
          "Do you want to shrink the shapes fitting with the work area?"));
  messageBox.addButton(QObject::tr("Shrink the shapes"), QMessageBox::YesRole);
  QPushButton* keep = messageBox.addButton(
      QObject::tr("Keep the shapes unchanged"), QMessageBox::NoRole);
  QPushButton* cancel =
      messageBox.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
  messageBox.exec();
  if (messageBox.clickedButton() == (QAbstractButton*)keep)
    keepShapeSize = true;
  else if (messageBox.clickedButton() == (QAbstractButton*)cancel)
    return;
  // �_�C�A���O��P�ɕ����ꍇ�͕ό`��I�����ď����p��

  if (!keepShapeSize) return;

  // �V�F�C�v�̕ό`
  QSizeF scale((qreal)workAreaSize.width() / (qreal)childSize.width(),
               (qreal)workAreaSize.height() / (qreal)childSize.height());
  for (ShapePair* shapePair : shapePairs) shapePair->counterResize(scale);

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  // invalidate
  for (auto layer : importedLayers)
    IwTriangleCache::instance()->invalidateLayerCache(layer);
}

//---------------------------------------------------

void ProjectUtils::setPreviewRange(int start, int end) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  PreviewRangeUndo* undo = new PreviewRangeUndo(prj);
  if (start < 0 && end < 0) {
    prj->clearPreviewRange();
  } else {
    if (start >= 0) prj->setPreviewFrom(start);
    if (end >= 0) prj->setPreviewTo(end);
    // ������xstart���w�肷�邱�ƂŌ����To�ȍ~�ւ̎w����\�ɂ���
    if (start >= 0) prj->setPreviewFrom(start);
  }

  bool changed = undo->registerAfter();
  if (changed)
    IwUndoManager::instance()->push(undo);
  else
    delete undo;
}

void ProjectUtils::toggleOnionSkinFrame(int frame) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  IwUndoManager::instance()->push(new ToggleOnionUndo(prj, frame));
}

void ProjectUtils::openInterpolationPopup(ShapePair* shape, QPoint pos) {
  static ShapeInterpolationPopup* popup = new ShapeInterpolationPopup(nullptr);

  popup->setShape(shape);
  popup->move(pos);
  popup->show();
}

void ProjectUtils::openMaskInfoPopup() {
  CommandManager::instance()->execute(MI_MatteInfo);
}

//=========================================
// �ȉ��AUndo��o�^
//---------------------------------------------------

ChangeLayerProperyUndo::ChangeLayerProperyUndo(
    IwProject* prj, IwLayer* layer, ProjectUtils::LayerPropertyType type)
    : m_project(prj), m_layer(layer), m_type(type) {}

void ChangeLayerProperyUndo::doChange(bool isUndo) {
  switch (m_type) {
  case ProjectUtils::Visibility: {
    IwLayer::LayerVisibility visibility = m_layer->isVisibleInViewer();
    if (isUndo)
      visibility = (visibility == IwLayer::Full)
                       ? IwLayer::Invisible
                       : (IwLayer::LayerVisibility)(visibility + 1);
    else
      visibility = (visibility == IwLayer::Invisible)
                       ? IwLayer::Full
                       : (IwLayer::LayerVisibility)(visibility - 1);
    m_layer->setVisibilityInViewer(visibility);
    IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
    break;
  }
  case ProjectUtils::Render:
    m_layer->setVisibleInRender(!m_layer->isVisibleInRender());
    break;
  case ProjectUtils::Lock:
    m_layer->setLocked(!m_layer->isLocked());

    // �I������������
    if (m_layer->isLocked()) {
      IwSelection* selection =
          IwApp::instance()->getCurrentSelection()->getSelection();
      IwTimeLineKeySelection* keySelection =
          dynamic_cast<IwTimeLineKeySelection*>(selection);
      IwTimeLineSelection* frameSelection =
          dynamic_cast<IwTimeLineSelection*>(selection);
      IwShapePairSelection* shapeSelection =
          dynamic_cast<IwShapePairSelection*>(selection);
      if (keySelection || frameSelection) {
        if (IwApp::instance()->getCurrentLayer()->getLayer() == m_layer)
          selection->selectNone();
      } else if (shapeSelection) {
        for (auto shape : shapeSelection->getShapes()) {
          if (m_layer->contains(shape.shapePairP))
            shapeSelection->removeShape(shape);
        }
      }
    }

    IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
    break;
  default:
    return;
    break;
  }
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
}

void ChangeLayerProperyUndo::undo() { doChange(true); }
void ChangeLayerProperyUndo::redo() { doChange(false); }

//---------------------------------------------------

resizeWorkAreaUndo::resizeWorkAreaUndo(IwProject* prj,
                                       QList<ShapePair*> shapePairs,
                                       QSize oldSize, QSize newSize,
                                       bool keepShapeSize)
    : m_project(prj)
    , m_shapePairs(shapePairs)
    , m_oldSize(oldSize)
    , m_newSize(newSize)
    , m_keepShapeSize(keepShapeSize) {}

void resizeWorkAreaUndo::undo() {
  // �T�C�Y�ό`
  m_project->setWorkAreaSize(m_oldSize);
  // �V�F�C�v�̕ό`
  if (m_keepShapeSize && !m_newSize.isEmpty() && !m_oldSize.isEmpty()) {
    QSizeF scale((qreal)m_oldSize.width() / (qreal)m_newSize.width(),
                 (qreal)m_oldSize.height() / (qreal)m_newSize.height());
    for (ShapePair* shapePair : m_shapePairs) shapePair->counterResize(scale);
  }
  // �����ɃV�O�i���G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    for (ShapePair* shapePair : m_shapePairs)
      IwTriangleCache::instance()->invalidateShapeCache(
          m_project->getParentShape(shapePair));
  }
}

void resizeWorkAreaUndo::redo() {
  // �T�C�Y�ό`
  m_project->setWorkAreaSize(m_newSize);
  // �V�F�C�v�̕ό`
  if (m_keepShapeSize && !m_newSize.isEmpty() && !m_oldSize.isEmpty()) {
    QSizeF scale((qreal)m_newSize.width() / (qreal)m_oldSize.width(),
                 (qreal)m_newSize.height() / (qreal)m_oldSize.height());
    for (ShapePair* shapePair : m_shapePairs) shapePair->counterResize(scale);
  }
  // �����ɃV�O�i���G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    for (ShapePair* shapePair : m_shapePairs)
      IwTriangleCache::instance()->invalidateShapeCache(
          m_project->getParentShape(shapePair));
  }
}

//---------------------------------------------------

SwapLayersUndo::SwapLayersUndo(IwProject* project, int index1, int index2)
    : m_project(project), m_index1(index1), m_index2(index2) {}

void SwapLayersUndo::undo() {
  m_project->swapLayers(m_index1, m_index2);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    // �Ƃ肠�����S��invalidate
    // IwTriangleCache::instance()->invalidateAllCache();
  }
}

void SwapLayersUndo::redo() {
  m_project->swapLayers(m_index1, m_index2);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

//---------------------------------------------------

ReorderLayersUndo::ReorderLayersUndo(IwProject* project, QList<IwLayer*> after)
    : m_project(project), m_before(), m_after(after) {
  for (int lay = 0; lay < m_project->getLayerCount(); lay++) {
    m_before.append(m_project->getLayer(lay));
  }
}

void ReorderLayersUndo::undo() {
  m_project->reorderLayers(m_before);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

void ReorderLayersUndo::redo() {
  m_project->reorderLayers(m_after);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

//---------------------------------------------------
RenameLayerUndo::RenameLayerUndo(IwProject* project, IwLayer* layer,
                                 QString& oldName, QString& newName)
    : m_project(project)
    , m_layer(layer)
    , m_oldName(oldName)
    , m_newName(newName) {}
void RenameLayerUndo::undo() {
  m_layer->setName(m_oldName);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}
void RenameLayerUndo::redo() {
  m_layer->setName(m_newName);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}
//---------------------------------------------------

DeleteLayerUndo::DeleteLayerUndo(IwProject* project, int index)
    : m_project(project), m_index(index) {
  m_layer = m_project->getLayer(index);
}

DeleteLayerUndo::~DeleteLayerUndo() { delete m_layer; }

void DeleteLayerUndo::undo() {
  m_project->insertLayer(m_layer, m_index);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

void DeleteLayerUndo::redo() {
  m_project->deleteLayer(m_index);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    // ���C���[�ɑ�����Shape��S��invalidate
    for (int s = 0; s < m_layer->getShapePairCount(); s++) {
      ShapePair* shape = m_layer->getShapePair(s);
      if (!shape) continue;
      IwTriangleCache::instance()->invalidateShapeCache(shape);
    }
  }
}

//---------------------------------------------------

CloneLayerUndo::CloneLayerUndo(IwProject* project, int index)
    : m_project(project), m_index(index) {
  m_layer = m_project->getLayer(index)->clone();
}

CloneLayerUndo::~CloneLayerUndo() { delete m_layer; }

void CloneLayerUndo::undo() {
  m_project->deleteLayer(m_index);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    // ���C���[�ɑ�����Shape��S��invalidate
    IwTriangleCache::instance()->invalidateLayerCache(m_layer);
  }
}

void CloneLayerUndo::redo() {
  m_project->insertLayer(m_layer, m_index);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

//---------------------------------------------------

SwapShapesUndo::SwapShapesUndo(IwProject* project, IwLayer* layer, int index1,
                               int index2)
    : m_project(project), m_layer(layer), m_index1(index1), m_index2(index2) {}

void SwapShapesUndo::doSwap() {
  QSet<ShapePair*> changedShapes;
  changedShapes.insert(
      m_layer->getParentShape(m_layer->getShapePair(m_index1)));
  changedShapes.insert(
      m_layer->getParentShape(m_layer->getShapePair(m_index2)));
  m_layer->swapShapes(m_index1, m_index2);
  changedShapes.insert(
      m_layer->getParentShape(m_layer->getShapePair(m_index1)));
  changedShapes.insert(
      m_layer->getParentShape(m_layer->getShapePair(m_index2)));
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    for (ShapePair* shape : changedShapes)
      IwTriangleCache::instance()->invalidateShapeCache(shape);
  }
}

void SwapShapesUndo::undo() { doSwap(); }

void SwapShapesUndo::redo() { doSwap(); }

//---------------------------------------------------

MoveShapeUndo::MoveShapeUndo(IwProject* project, IwLayer* layer, int index,
                             int dstIndex)
    : m_project(project)
    , m_layer(layer)
    , m_index(index)
    , m_dstIndex(dstIndex) {}

void MoveShapeUndo::doMove(int from, int to) {
  QSet<ShapePair*> changedShapes;
  changedShapes.insert(m_layer->getParentShape(m_layer->getShapePair(from)));
  changedShapes.insert(m_layer->getParentShape(m_layer->getShapePair(to)));
  m_layer->moveShape(from, to);
  changedShapes.insert(m_layer->getParentShape(m_layer->getShapePair(from)));
  changedShapes.insert(m_layer->getParentShape(m_layer->getShapePair(to)));
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    for (ShapePair* shape : changedShapes)
      IwTriangleCache::instance()->invalidateShapeCache(shape);
  }
}

void MoveShapeUndo::undo() { doMove(m_dstIndex, m_index); }

void MoveShapeUndo::redo() { doMove(m_index, m_dstIndex); }

//---------------------------------------------------
ReorderShapesUndo::ReorderShapesUndo(
    IwProject* project, QMap<IwLayer*, QList<QPair<ShapePair*, bool>>> after)
    : m_project(project), m_after(after) {
  // before �̃f�[�^������
  for (auto layer : after.keys()) {
    QList<QPair<ShapePair*, bool>> shapeInfo;
    for (int s = 0; s < layer->getShapePairCount(); s++) {
      ShapePair* shapePair = layer->getShapePair(s);
      if (!shapePair) continue;
      shapeInfo.append({shapePair, shapePair->isParent()});
    }
    m_before[layer] = shapeInfo;
  }
}

void ReorderShapesUndo::undo() {
  QMapIterator<IwLayer*, QList<QPair<ShapePair*, bool>>> itr(m_before);
  while (itr.hasNext()) {
    itr.next();
    IwLayer* layer                           = itr.key();
    QList<QPair<ShapePair*, bool>> shapeInfo = itr.value();
    QList<ShapePair*> shapes;
    for (auto s_info : shapeInfo) {
      shapes.append(s_info.first);
      s_info.first->setIsParent(s_info.second);
    }

    layer->replaceShapePairs(shapes);
  }
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    // �Ƃ肠�����S��invalidate
    IwTriangleCache::instance()->invalidateAllCache();
  }
}

void ReorderShapesUndo::redo() {
  QMapIterator<IwLayer*, QList<QPair<ShapePair*, bool>>> itr(m_after);
  while (itr.hasNext()) {
    itr.next();
    IwLayer* layer                           = itr.key();
    QList<QPair<ShapePair*, bool>> shapeInfo = itr.value();
    QList<ShapePair*> shapes;
    for (auto s_info : shapeInfo) {
      shapes.append(s_info.first);
      s_info.first->setIsParent(s_info.second);
    }

    layer->replaceShapePairs(shapes);
  }
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    // �Ƃ肠�����S��invalidate
    IwTriangleCache::instance()->invalidateAllCache();
  }
}

//---------------------------------------------------
RenameShapePairUndo::RenameShapePairUndo(IwProject* project, ShapePair* shape,
                                         QString& oldName, QString& newName)
    : m_project(project)
    , m_shape(shape)
    , m_oldName(oldName)
    , m_newName(newName) {}
void RenameShapePairUndo::undo() {
  m_shape->setName(m_oldName);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}
void RenameShapePairUndo::redo() {
  m_shape->setName(m_newName);
  // �V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
DeleteFormPointsUndo::DeleteFormPointsUndo(IwProject* project,
                                           const QList<ShapePair*>& shapes)
    : m_project(project), m_firstFlag(true) {
  // Before�f�[�^���i�[���Ă���
  for (int s = 0; s < shapes.size(); s++) {
    ShapeUndoInfo info;
    info.shape = shapes.at(s);
    for (int fromTo = 0; fromTo < 2; fromTo++) {
      info.beforeForm[fromTo] = info.shape->getFormData(fromTo);
      info.beforeCorr[fromTo] = info.shape->getCorrData(fromTo);
    }
    m_shapeInfo.push_back(info);
  }
}

void DeleteFormPointsUndo::storeAfterData() {
  // Before�f�[�^���i�[���Ă���
  for (int s = 0; s < m_shapeInfo.size(); s++) {
    for (int fromTo = 0; fromTo < 2; fromTo++) {
      m_shapeInfo[s].afterForm[fromTo] =
          m_shapeInfo[s].shape->getFormData(fromTo);
      m_shapeInfo[s].afterCorr[fromTo] =
          m_shapeInfo[s].shape->getCorrData(fromTo);
    }
  }
}

void DeleteFormPointsUndo::undo() {
  // before�̒l��������
  for (int s = 0; s < m_shapeInfo.size(); s++) {
    for (int fromTo = 0; fromTo < 2; fromTo++) {
      m_shapeInfo[s].shape->setFormData(m_shapeInfo[s].beforeForm[fromTo],
                                        fromTo);
      m_shapeInfo[s].shape->setCorrData(m_shapeInfo[s].beforeCorr[fromTo],
                                        fromTo);
    }
  }
  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // invalidate cache
    for (int s = 0; s < m_shapeInfo.size(); s++)
      IwTriangleCache::instance()->invalidateShapeCache(
          m_project->getParentShape(m_shapeInfo[s].shape));
  }
}

void DeleteFormPointsUndo::redo() {
  // �o�^����redo�̓X�L�b�v����
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }
  // after�̒l��������
  for (int s = 0; s < m_shapeInfo.size(); s++) {
    for (int fromTo = 0; fromTo < 2; fromTo++) {
      m_shapeInfo[s].shape->setFormData(m_shapeInfo[s].afterForm[fromTo],
                                        fromTo);
      m_shapeInfo[s].shape->setCorrData(m_shapeInfo[s].afterCorr[fromTo],
                                        fromTo);
    }
  }
  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // invalidate cache
    for (int s = 0; s < m_shapeInfo.size(); s++)
      IwTriangleCache::instance()->invalidateShapeCache(
          m_project->getParentShape(m_shapeInfo[s].shape));
  }
}

//---------------------------------------------------

SwitchParentChildUndo::SwitchParentChildUndo(IwProject* prj, ShapePair* shape)
    : m_project(prj), m_shape(shape) {}

void SwitchParentChildUndo::undo() {
  QSet<ShapePair*> changedShapes;
  IwLayer* layer = m_project->getLayerFromShapePair(m_shape);
  changedShapes.insert(layer->getParentShape(m_shape));

  m_shape->setIsParent(!m_shape->isParent());

  changedShapes.insert(layer->getParentShape(m_shape));

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);

    for (ShapePair* shape : changedShapes)
      IwTriangleCache::instance()->invalidateShapeCache(shape);
  }
}

//---------------------------------------------------

SwitchLockUndo::SwitchLockUndo(IwProject* prj, OneShape shape)
    : m_project(prj), m_shape(shape) {}

void SwitchLockUndo::undo() {
  m_shape.shapePairP->setLocked(m_shape.fromTo,
                                !m_shape.shapePairP->isLocked(m_shape.fromTo));

  if (m_shape.shapePairP->isLocked(m_shape.fromTo)) {
    if (IwShapePairSelection::instance()->isSelected(m_shape)) {
      IwShapePairSelection::instance()->removeShape(m_shape);
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
    }
  }
  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------

SwitchPinUndo::SwitchPinUndo(IwProject* prj, OneShape shape)
    : m_project(prj), m_shape(shape) {}

void SwitchPinUndo::undo() {
  m_shape.shapePairP->setPinned(m_shape.fromTo,
                                !m_shape.shapePairP->isPinned(m_shape.fromTo));

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------

SwitchShapeVisibilityUndo::SwitchShapeVisibilityUndo(IwProject* prj,
                                                     ShapePair* shapePair)
    : m_project(prj), m_shapePair(shapePair) {}

void SwitchShapeVisibilityUndo::undo() {
  m_shapePair->setVisible(!m_shapePair->isVisible());

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------

ImportProjectUndo::ImportProjectUndo(IwProject* prj,
                                     const QList<IwLayer*>& layers)
    : m_project(prj), m_layers(layers), m_onPush(true) {}

ImportProjectUndo::~ImportProjectUndo() {
  for (auto layer : m_layers) delete layer;
}

void ImportProjectUndo::undo() {
  for (auto layer : m_layers) m_project->deleteLayer(layer);
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    for (auto layer : m_layers)
      IwTriangleCache::instance()->invalidateLayerCache(layer);
  }
}

void ImportProjectUndo::redo() {
  if (m_onPush) {
    m_onPush = false;
    return;
  }
  for (auto layer : m_layers)
    m_project->insertLayer(layer, m_project->getLayerCount());
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

//---------------------------------------------------

ImportShapesUndo::ImportShapesUndo(IwProject* prj, IwLayer* layer,
                                   const QList<ShapePair*>& shapes)
    : m_project(prj), m_layer(layer), m_shapes(shapes), m_onPush(true) {
  for (auto shape : m_shapes) {
    ShapePair* parent = layer->getParentShape(shape);
    if (parent) m_parentShapes.insert(parent);
  }
}

ImportShapesUndo::~ImportShapesUndo() {
  for (auto shape : m_shapes) delete shape;
}

void ImportShapesUndo::undo() {
  for (auto shape : m_shapes) m_layer->deleteShapePair(shape);
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    QSet<ShapePair*>::iterator itr = m_parentShapes.begin();
    while (itr != m_parentShapes.end()) {
      IwTriangleCache::instance()->invalidateShapeCache(*itr);
      ++itr;
    }
  }
}

void ImportShapesUndo::redo() {
  if (m_onPush) {
    m_onPush = false;
    return;
  }
  for (auto shape : m_shapes) m_layer->addShapePair(shape);
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

//---------------------------------------------------

PreviewRangeUndo::PreviewRangeUndo(IwProject* project)
    : m_project(project), m_onPush(true) {
  m_before.isSpecified = project->getPreviewRange(m_before.start, m_before.end);
}

// �����ς���Ă����� true ��Ԃ�
bool PreviewRangeUndo::registerAfter() {
  m_after.isSpecified = m_project->getPreviewRange(m_after.start, m_after.end);
  return (m_before.isSpecified != m_after.isSpecified ||
          m_before.start != m_after.start || m_before.end != m_after.end);
}

void PreviewRangeUndo::undo() {
  if (m_before.isSpecified) {
    m_project->setPreviewFrom(m_before.start);
    m_project->setPreviewTo(m_before.end);
  } else
    m_project->clearPreviewRange();
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void PreviewRangeUndo::redo() {
  if (m_onPush) {
    m_onPush = false;
    return;
  }
  if (m_after.isSpecified) {
    m_project->setPreviewFrom(m_after.start);
    m_project->setPreviewTo(m_after.end);
  } else
    m_project->clearPreviewRange();
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}
//---------------------------------------------------

ToggleOnionUndo::ToggleOnionUndo(IwProject* project, int frame)
    : m_project(project), m_frame(frame) {}
void ToggleOnionUndo::undo() {
  m_project->toggleOnionFrame(m_frame);
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}
void ToggleOnionUndo::redo() { undo(); }
