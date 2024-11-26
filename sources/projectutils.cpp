//---------------------------------------
// ProjectUtils
// シーン操作のコマンドのもろもろ＋Undo
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
// IwLayerに対する操作
//-----
// レイヤ順序の入れ替え
void ProjectUtils::SwapLayers(int index1, int index2) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  if (index1 < 0 || index1 >= prj->getLayerCount() || index2 < 0 ||
      index2 >= prj->getLayerCount() || index1 == index2)
    return;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
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

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new ReorderLayersUndo(prj, newOrderedLayers));
  return true;
}

// リネーム
void ProjectUtils::RenameLayer(IwLayer* layer, QString& newName) {
  if (newName.isEmpty()) return;
  QString oldName = layer->getName();
  if (newName == oldName) return;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new RenameLayerUndo(IwApp::instance()->getCurrentProject()->getProject(),
                          layer, oldName, newName));
}

// レイヤ削除
void ProjectUtils::DeleteLayer(int index) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  if (index < 0 || index >= prj->getLayerCount()) return;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new DeleteLayerUndo(prj, index));
}

// レイヤ複製
void ProjectUtils::CloneLayer(int index) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  if (index < 0 || index >= prj->getLayerCount()) return;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new CloneLayerUndo(prj, index));
}

// レイヤ情報の変更
void ProjectUtils::toggleLayerProperty(IwLayer* layer,
                                       ProjectUtils::LayerPropertyType type) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new ChangeLayerProperyUndo(prj, layer, type));
}

//-----
// ShapePairに対する操作
//-----

// シェイプ順序の入れ替え
void ProjectUtils::SwapShapes(IwLayer* layer, int index1, int index2) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  if (!prj->contains(layer)) return;
  if (index1 < 0 || index1 >= layer->getShapePairCount() || index2 < 0 ||
      index2 >= layer->getShapePairCount() || index1 == index2)
    return;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new SwapShapesUndo(prj, layer, index1, index2));
}

// シェイプの移動 下に移動する場合、dstIndexはindexのアイテムを抜いた後の値
void ProjectUtils::MoveShape(IwLayer* layer, int index, int dstIndex) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  if (!prj->contains(layer)) return;
  if (index < 0 || index >= layer->getShapePairCount() || dstIndex < 0 ||
      dstIndex >= layer->getShapePairCount() || index == dstIndex)
    return;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new MoveShapeUndo(prj, layer, index, dstIndex));
}

// レイヤーをまたいでドラッグ移動。最後のboolはisParentの情報
void ProjectUtils::ReorderShapes(
    QMap<IwLayer*, QList<QPair<ShapePair*, bool>>> layerShapeInfo) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new ReorderShapesUndo(prj, layerShapeInfo));
}

// リネーム
void ProjectUtils::RenameShapePair(ShapePair* shape, QString& newName) {
  if (newName.isEmpty()) return;
  QString oldName = shape->getName();
  if (newName == oldName) return;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new RenameShapePairUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shape, oldName,
      newName));
}

// ポイント消去
void ProjectUtils::deleteFormPoints(QMap<ShapePair*, QList<int>>& pointList) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // Undo作る
  DeleteFormPointsUndo* undo =
      new DeleteFormPointsUndo(project, pointList.keys());

  // 消去の処理
  QMapIterator<ShapePair*, QList<int>> itr(pointList);
  while (itr.hasNext()) {
    itr.next();
    ShapePair* shape             = itr.key();
    QList<int> deletingPointList = itr.value();
    // deletingPointListを昇順にする
    std::sort(deletingPointList.begin(), deletingPointList.end());

    // 消す前のBezierポイント数
    int oldPointAmount = shape->getBezierPointAmount();
    // 消した後の残りBezierポイント数
    int remainingPointAmount =
        shape->getBezierPointAmount() - deletingPointList.size();

    // ● シェイプがOpenなら、Corrは全体を縮小
    if (!shape->isClosed()) {
      // 対象シェイプのFromToについて
      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // ①形状データの消去
        // 形状データを得る
        QMap<int, BezierPointList> formData = shape->getFormData(fromTo);
        QMapIterator<int, BezierPointList> formItr(formData);
        while (formItr.hasNext()) {
          formItr.next();
          BezierPointList bpList = formItr.value();
          // ポイントを後ろから消していく
          for (int dp = deletingPointList.size() - 1; dp >= 0; dp--)
            bpList.removeAt(deletingPointList[dp]);
          // 値を上書きで戻す
          formData.insert(formItr.key(), bpList);
        }
        // 形状データを上書きで戻す(形状点の数は自動で更新される)
        shape->setFormData(formData, fromTo);

        // ②対応点データを「つめる」
        // つめる割合
        double corrShrinkRatio =
            (double)(remainingPointAmount - 1) / (double)(oldPointAmount - 1);
        // 対応点データを得る
        QMap<int, CorrPointList> corrData = shape->getCorrData(fromTo);
        QMapIterator<int, CorrPointList> corrItr(corrData);
        while (corrItr.hasNext()) {
          corrItr.next();
          CorrPointList cpList = corrItr.value();
          // ポイント全てに、「つめる割合」を掛けていく
          for (int cp = 0; cp < cpList.size(); cp++)
            cpList[cp].value *= corrShrinkRatio;
          // 値を上書きで戻す
          corrData.insert(corrItr.key(), cpList);
        }
        // 対応点データを上書きで戻す
        shape->setCorrData(corrData, fromTo);
      }
    }

    // ● シェイプがClosedなら
    else {
      // 各Bezier頂点が、消去後の新たなCorr配置で
      //  Corrの値が何になるのかの、テーブルを作る

      // 隣り合ったポイントで固まりを作る
      QList<QList<int>> deletingPointClusters;
      // 新しいCorrのテーブル
      QList<double> newCorrTable;
      for (int c = 0; c < oldPointAmount; c++) newCorrTable.push_back(c);

      // 消さないポイントを探す
      int startIndex = 0;
      // ポイント０も消すなら、前に進んで消さないポイントの始まりを見つける
      if (deletingPointList.contains(startIndex)) {
        while (deletingPointList.contains(startIndex)) startIndex++;
      }
      // 始めに1つ作って格納
      deletingPointClusters.push_back(QList<int>());
      // 固まりを作る
      for (int i = startIndex; i < startIndex + oldPointAmount; i++) {
        int index = (i >= oldPointAmount) ? i - oldPointAmount : i;
        if (deletingPointList.contains(index)) {
          // お尻のリストに追加する
          deletingPointClusters.last().push_back(index);
        } else {
          // お尻のリストが空じゃなければ、リストを新たに作って追加する
          if (!deletingPointClusters.last().isEmpty())
            deletingPointClusters.push_back(QList<int>());
        }
      }
      // 最後のリストが空なら、消す
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
          // クラスタに突入
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

      // テーブルを元に、対応点を再プロットしていく

      // 対象シェイプのFromToについて
      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // ①形状データの消去
        // 形状データを得る
        QMap<int, BezierPointList> formData = shape->getFormData(fromTo);
        QMapIterator<int, BezierPointList> formItr(formData);
        while (formItr.hasNext()) {
          formItr.next();
          BezierPointList bpList = formItr.value();
          // ポイントを後ろから消していく
          for (int dp = deletingPointList.size() - 1; dp >= 0; dp--)
            bpList.removeAt(deletingPointList[dp]);
          // 値を上書きで戻す
          formData.insert(formItr.key(), bpList);
        }
        // 形状データを上書きで戻す(形状点の数は自動で更新される)
        shape->setFormData(formData, fromTo);

        // 対応点データを得る
        QMap<int, CorrPointList> corrData = shape->getCorrData(fromTo);
        QMapIterator<int, CorrPointList> corrItr(corrData);
        while (corrItr.hasNext()) {
          corrItr.next();
          CorrPointList cpList = corrItr.value();
          // ポイント全てに、「つめる割合」を掛けていく
          for (int cp = 0; cp < cpList.size(); cp++) {
            double tmpCorr = cpList.at(cp).value;
            int k1         = (int)tmpCorr;
            double ratio   = tmpCorr - (double)k1;
            int k2         = (k1 + 1 >= oldPointAmount) ? 0 : k1 + 1;
            double c1      = newCorrTable[k1];
            double c2      = newCorrTable[k2];
            // 閉曲線の継ぎ目で、一周してインデックスがが０に戻る場合
            if (c2 < c1) c2 += remainingPointAmount;
            double newCorr = c1 * (1.0 - ratio) + c2 * ratio;
            // 補間後、Corr値が０を超えた場合クランプする
            if (newCorr >= remainingPointAmount)
              newCorr -= remainingPointAmount;
            cpList.replace(cp, {newCorr, cpList.at(cp).weight});
          }
          // 値を上書きで戻す
          corrData.insert(corrItr.key(), cpList);
        }
        // 対応点データを上書きで戻す
        shape->setCorrData(corrData, fromTo);
      }
    }

    // invalidate cache
    IwTriangleCache::instance()->invalidateShapeCache(
        project->getParentShape(shape));
  }

  undo->storeAfterData();

  // Undoに登録
  IwUndoManager::instance()->push(undo);
}

// 親子を切り替える
void ProjectUtils::switchParentChild(ShapePair* shape) {
  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new SwitchParentChildUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shape));
}

// ロックを切り替える
void ProjectUtils::switchLock(OneShape shape) {
  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new SwitchLockUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shape));
}

// ピンを切り替える
void ProjectUtils::switchPin(OneShape shape) {
  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new SwitchPinUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shape));
}

// 表示非表示を切り替える
void ProjectUtils::switchShapeVisibility(ShapePair* shapePair) {
  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new SwitchShapeVisibilityUndo(
      IwApp::instance()->getCurrentProject()->getProject(), shapePair));
}

// プロジェクト内にロックされたシェイプがあるかどうか
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

// 全てのロックされたシェイプをアンロック
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

  // 登録時redo実行
  IwUndoManager::instance()->push(new LockShapesUndo(lockedShapes, prj, false));
}

//-----
// ワークエリアのサイズ変更
//-----
bool ProjectUtils::resizeWorkArea(QSize& newSize) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return false;
  QSize oldSize = prj->getWorkAreaSize();
  if (newSize == oldSize) return false;

  // 全てのシェイプのリストを作る
  QList<ShapePair*> shapePairs;
  for (int i = 0; i < prj->getLayerCount(); i++) {
    IwLayer* layer = prj->getLayer(i);
    for (int s = 0; s < layer->getShapePairCount(); s++)
      shapePairs.append(layer->getShapePair(s));
  }

  bool keepShapeSize = false;
  // シェイプがなければサイズを変更するだけ
  // シェイプがある場合、シェイプのサイズを維持するか、新しいサイズに従って変形するかを選ぶ
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
    // ダイアログを単に閉じた場合は変形を選択して処理継続
  }

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
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
    // ImageSize (ワークエリアのサイズ)
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
      // ロードして、いままでのロールと差し替える
      while (reader.readNextStartElement()) {
        if (reader.name() == "Layer") {
          // ロードするところ
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

      // プロジェクトに追加
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

  // 全てのシェイプのリストを作る
  QList<ShapePair*> shapePairs;
  for (auto layer : importedLayers) {
    for (int s = 0; s < layer->getShapePairCount(); s++)
      shapePairs.append(layer->getShapePair(s));
  }
  if (shapePairs.isEmpty()) return;
  bool keepShapeSize = false;
  // シェイプがなければサイズを変更するだけ
  // シェイプがある場合、シェイプのサイズを維持するか、新しいサイズに従って変形するかを選ぶ
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
  // ダイアログを単に閉じた場合は変形を選択して処理継続

  if (!keepShapeSize) return;

  // シェイプの変形
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
    // もう一度startを指定することで現状のTo以降への指定を可能にする
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
// 以下、Undoを登録
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

    // 選択を解除する
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
  // サイズ変形
  m_project->setWorkAreaSize(m_oldSize);
  // シェイプの変形
  if (m_keepShapeSize && !m_newSize.isEmpty() && !m_oldSize.isEmpty()) {
    QSizeF scale((qreal)m_oldSize.width() / (qreal)m_newSize.width(),
                 (qreal)m_oldSize.height() / (qreal)m_newSize.height());
    for (ShapePair* shapePair : m_shapePairs) shapePair->counterResize(scale);
  }
  // ここにシグナルエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    for (ShapePair* shapePair : m_shapePairs)
      IwTriangleCache::instance()->invalidateShapeCache(
          m_project->getParentShape(shapePair));
  }
}

void resizeWorkAreaUndo::redo() {
  // サイズ変形
  m_project->setWorkAreaSize(m_newSize);
  // シェイプの変形
  if (m_keepShapeSize && !m_newSize.isEmpty() && !m_oldSize.isEmpty()) {
    QSizeF scale((qreal)m_newSize.width() / (qreal)m_oldSize.width(),
                 (qreal)m_newSize.height() / (qreal)m_oldSize.height());
    for (ShapePair* shapePair : m_shapePairs) shapePair->counterResize(scale);
  }
  // ここにシグナルエミット
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
  // シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    // とりあえず全部invalidate
    // IwTriangleCache::instance()->invalidateAllCache();
  }
}

void SwapLayersUndo::redo() {
  m_project->swapLayers(m_index1, m_index2);
  // シグナルをエミット
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
  // シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

void ReorderLayersUndo::redo() {
  m_project->reorderLayers(m_after);
  // シグナルをエミット
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
  // シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}
void RenameLayerUndo::redo() {
  m_layer->setName(m_newName);
  // シグナルをエミット
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
  // シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

void DeleteLayerUndo::redo() {
  m_project->deleteLayer(m_index);
  // シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    // レイヤーに属するShapeを全部invalidate
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
  // シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    // レイヤーに属するShapeを全部invalidate
    IwTriangleCache::instance()->invalidateLayerCache(m_layer);
  }
}

void CloneLayerUndo::redo() {
  m_project->insertLayer(m_layer, m_index);
  // シグナルをエミット
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
  // シグナルをエミット
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
  // シグナルをエミット
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
  // before のデータをつくる
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
  // シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    // とりあえず全部invalidate
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
  // シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    // とりあえず全部invalidate
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
  // シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}
void RenameShapePairUndo::redo() {
  m_shape->setName(m_newName);
  // シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
DeleteFormPointsUndo::DeleteFormPointsUndo(IwProject* project,
                                           const QList<ShapePair*>& shapes)
    : m_project(project), m_firstFlag(true) {
  // Beforeデータを格納していく
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
  // Beforeデータを格納していく
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
  // beforeの値を代入する
  for (int s = 0; s < m_shapeInfo.size(); s++) {
    for (int fromTo = 0; fromTo < 2; fromTo++) {
      m_shapeInfo[s].shape->setFormData(m_shapeInfo[s].beforeForm[fromTo],
                                        fromTo);
      m_shapeInfo[s].shape->setCorrData(m_shapeInfo[s].beforeCorr[fromTo],
                                        fromTo);
    }
  }
  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // invalidate cache
    for (int s = 0; s < m_shapeInfo.size(); s++)
      IwTriangleCache::instance()->invalidateShapeCache(
          m_project->getParentShape(m_shapeInfo[s].shape));
  }
}

void DeleteFormPointsUndo::redo() {
  // 登録時のredoはスキップする
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }
  // afterの値を代入する
  for (int s = 0; s < m_shapeInfo.size(); s++) {
    for (int fromTo = 0; fromTo < 2; fromTo++) {
      m_shapeInfo[s].shape->setFormData(m_shapeInfo[s].afterForm[fromTo],
                                        fromTo);
      m_shapeInfo[s].shape->setCorrData(m_shapeInfo[s].afterCorr[fromTo],
                                        fromTo);
    }
  }
  // もしこれがカレントなら、シグナルをエミット
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

  // もしこれがカレントなら、シグナルをエミット
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
  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------

SwitchPinUndo::SwitchPinUndo(IwProject* prj, OneShape shape)
    : m_project(prj), m_shape(shape) {}

void SwitchPinUndo::undo() {
  m_shape.shapePairP->setPinned(m_shape.fromTo,
                                !m_shape.shapePairP->isPinned(m_shape.fromTo));

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------

SwitchShapeVisibilityUndo::SwitchShapeVisibilityUndo(IwProject* prj,
                                                     ShapePair* shapePair)
    : m_project(prj), m_shapePair(shapePair) {}

void SwitchShapeVisibilityUndo::undo() {
  m_shapePair->setVisible(!m_shapePair->isVisible());

  // もしこれがカレントなら、シグナルをエミット
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

// 何か変わっていたら true を返す
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
