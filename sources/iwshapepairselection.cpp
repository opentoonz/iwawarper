//---------------------------------------------------
// IwShapePairSelection
// シェイプペアの選択のクラス
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
// 対応点消去のUndo
//---------------------------------------------------

class DeleteCorrPointUndo : public QUndoCommand {
  ShapePair* m_shape;
  IwProject* m_project;
  QMap<int, CorrPoint> m_deletedCorrPos[2];
  int m_deletedIndex;

public:
  DeleteCorrPointUndo(ShapePair* shape, int index)
      : m_shape(shape), m_deletedIndex(index) {
    m_project = IwApp::instance()->getCurrentProject()->getProject();
    // 消される対応点位置を保存
    for (int ft = 0; ft < 2; ft++) {
      QMap<int, CorrPointList> corrKeysMap = m_shape->getCorrData(ft);
      // 各キーフレームデータに対応点を挿入する
      QMap<int, CorrPointList>::iterator i = corrKeysMap.begin();
      while (i != corrKeysMap.end()) {
        CorrPointList cpList = i.value();
        m_deletedCorrPos[ft].insert(i.key(), cpList.at(index));
        ++i;
      }
    }
  }

  void undo() {
    // Shape２つに対して行う
    for (int ft = 0; ft < 2; ft++) {
      QMap<int, CorrPointList> corrKeysMap = m_shape->getCorrData(ft);
      // 各キーフレームデータに対応点を挿入する
      QMap<int, CorrPointList>::iterator i = corrKeysMap.begin();
      while (i != corrKeysMap.end()) {
        CorrPointList cpList = i.value();
        assert(m_deletedCorrPos[ft].contains(i.key()));
        cpList.insert(m_deletedIndex, m_deletedCorrPos[ft].value(i.key()));
        // insertコマンドは、既に同じKeyにデータが有った場合、置き換えられる
        corrKeysMap.insert(i.key(), cpList);
        ++i;
      }
      // データを戻す 対応点カウントは中で更新している
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
    // シグナルをエミット
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
// 選択の解除
//---------------------------------------------------
void IwShapePairSelection::selectNone() {
  m_shapes.clear();
  m_activePoints.clear();
  m_activeCorrPoint = QPair<OneShape, int>(OneShape(), -1);
}

//---------------------------------------------------
// これまでの選択を解除して、シェイプを１つ選択
//---------------------------------------------------
void IwShapePairSelection::selectOneShape(OneShape shape) {
  m_shapes.clear();
  m_activePoints.clear();
  m_shapes.push_back(shape);
  // 選択シェイプとアクティブ対応点シェイプが異なるとき、解除
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.first != shape)
    m_activeCorrPoint = QPair<OneShape, int>(OneShape(), -1);
}

//---------------------------------------------------
// 選択シェイプを追加する
//---------------------------------------------------
void IwShapePairSelection::addShape(OneShape shape) {
  if (!m_shapes.contains(shape)) m_shapes.push_back(shape);
}

//---------------------------------------------------
// 選択シェイプを解除する
//---------------------------------------------------
int IwShapePairSelection::removeShape(OneShape shape) {
  // 解除シェイプとアクティブ対応点シェイプが一致するとき、解除
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.first == shape)
    m_activeCorrPoint = QPair<OneShape, int>(OneShape(), -1);
  return m_shapes.removeAll(shape);
}

//---------------------------------------------------
// FromTo指定で選択を解除
//---------------------------------------------------
bool IwShapePairSelection::removeFromToShapes(int fromTo) {
  bool changed = false;
  for (int s = m_shapes.size() - 1; s >= 0; s--) {
    if (m_shapes.at(s).fromTo == fromTo) {
      m_shapes.removeAt(s);
      changed = true;
    }
  }
  // 消すべきシェイプが見つかったら、アクティブポイントも消す
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
  // 解除fromToとアクティブ対応点シェイプのfromToが一致するとき、解除
  if (m_activeCorrPoint.first.shapePairP &&
      m_activeCorrPoint.first.fromTo == fromTo)
    m_activeCorrPoint = QPair<OneShape, int>(OneShape(), -1);
  return changed;
}

//---------------------------------------------------
// シェイプを取得する 範囲外は０を返す
//---------------------------------------------------
OneShape IwShapePairSelection::getShape(int index) {
  if (index >= m_shapes.size() || index < 0) return 0;

  return m_shapes[index];
}

//---------------------------------------------------
// 引数シェイプが選択範囲に含まれていたらtrue
//---------------------------------------------------
bool IwShapePairSelection::isSelected(OneShape shape) const {
  return m_shapes.contains(shape);
}

bool IwShapePairSelection::isSelected(ShapePair* shapePair) const {
  return isSelected(OneShape(shapePair, 0)) ||
         isSelected(OneShape(shapePair, 1));
}

//---------------------------------------------------
//---- Reshapeツールで使用
//---------------------------------------------------
// ポイントが選択されているかどうか
//---------------------------------------------------
bool IwShapePairSelection::isPointActive(OneShape shape, int pointIndex) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return false;
  // シェイプがアクティブでなければfalse
  if (!isSelected(shape)) return false;

  int shapeName = layer->getNameFromShapePair(shape);
  shapeName += pointIndex * 10;  // 下１桁はゼロ

  return m_activePoints.contains(shapeName);
}
//---------------------------------------------------
// ポイントを選択する
//---------------------------------------------------
void IwShapePairSelection::activatePoint(int name) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;
  // シェイプをアクティブにする
  OneShape shape = layer->getShapePairFromName(name);
  addShape(shape);

  // 選択ポイントに追加する
  m_activePoints.insert(name);
}
void IwShapePairSelection::activatePoint(OneShape shape, int index) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // シェイプを選択する
  addShape(shape);

  int name = layer->getNameFromShapePair(shape);
  name += index * 10;  // 下１桁はゼロ

  // アクティブポイントを追加する
  m_activePoints.insert(name);
}
//---------------------------------------------------
// ポイント選択を解除する
//---------------------------------------------------
void IwShapePairSelection::deactivatePoint(int name) {
  // シェイプのアクティブ／非アクティブはそのまま
  // 選択ポイントを解除するだけ
  m_activePoints.remove(name);
}
void IwShapePairSelection::deactivatePoint(OneShape shape, int index) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  int name = layer->getNameFromShapePair(shape);
  name += index * 10;

  // シェイプの選択／非選択はそのまま
  // 選択ポイントを解除するだけ
  m_activePoints.remove(name);
}

//---------------------------------------------------
// 選択ポイントのリストを返す
//---------------------------------------------------
QList<QPair<OneShape, int>> IwShapePairSelection::getActivePointList() {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  QList<QPair<OneShape, int>> list;
  if (!layer) return list;
  QSet<int>::iterator i = m_activePoints.begin();
  // 各アクティブなポイントについて
  while (i != m_activePoints.end()) {
    int name = *i;
    // シェイプをアクティブにする
    OneShape shape = layer->getShapePairFromName(name);
    int pId        = (int)((name % 10000) / 10);

    list.append(qMakePair(shape, pId));
    ++i;
  }
  return list;
}

//---------------------------------------------------
// 選択ポイントを全て解除する
//---------------------------------------------------
void IwShapePairSelection::deactivateAllPoints() { m_activePoints.clear(); }

//---------------------------------------------------
//-- 以下、この選択で使えるコマンド群
//---------------------------------------------------
// コピー
//---------------------------------------------------

void IwShapePairSelection::copyShapes() {
  std::cout << "copyShapes" << std::endl;

  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // 中身の無いシェイプを選択からはずす
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

  // ShapeDataデータを作り、選択シェイプデータを格納
  ShapePairData* shapePairData = new ShapePairData();

  shapePairData->setShapePairData(shapePairs);

  // クリップボードにデータを入れる
  QClipboard* clipboard = QApplication::clipboard();
  clipboard->setMimeData(shapePairData, QClipboard::Clipboard);
}

//---------------------------------------------------
// ペースト
//---------------------------------------------------

void IwShapePairSelection::pasteShapes() {
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // どこのレイヤに所属させるか
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // クリップボードからデータを取り、ShapePairDataかどうか確認
  QClipboard* clipboard     = QApplication::clipboard();
  const QMimeData* mimeData = clipboard->mimeData();
  const ShapePairData* shapePairData =
      dynamic_cast<const ShapePairData*>(mimeData);
  // データが違っていたらreturn
  if (!shapePairData) return;

  // ペーストされたシェイプを控えておくいれもの
  QList<ShapePair*> pastedShapes;

  shapePairData->getShapePairData(layer, pastedShapes);

  // ペーストしたシェイプを選択する
  selectNone();

  for (int s = 0; s < pastedShapes.size(); s++) {
    addShape(OneShape(pastedShapes.at(s), 0));
    addShape(OneShape(pastedShapes.at(s), 1));
  }

  // Undoに登録 同時にredoが呼ばれるが、最初はフラグで回避する
  IwUndoManager::instance()->push(
      new PasteShapePairUndo(pastedShapes, project, layer));

  IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

//---------------------------------------------------
// カット
//---------------------------------------------------

void IwShapePairSelection::cutShapes() {
  copyShapes();
  deleteShapes();
}

//---------------------------------------------------
// 消去
//---------------------------------------------------

void IwShapePairSelection::deleteShapes() {
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // 中身の無いシェイプを選択からはずす
  QList<OneShape>::iterator i = m_shapes.begin();
  while (i != m_shapes.end()) {
    if ((*i).shapePairP == 0 || !project->contains((*i).shapePairP))
      m_shapes.erase(i);
    else
      i++;
  }

  if (m_shapes.isEmpty()) return;

  // CorrespondenceTool 使用時、対応点が選択されている場合は対応点消去
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.second >= 0) {
    onDeleteCorrPoint();
    return;
  }

  // activePointが選択されている場合は、ポイント消去
  QList<QPair<OneShape, int>> activePointList = getActivePointList();
  // 中身の無いシェイプを選択からはずす
  QList<QPair<OneShape, int>>::iterator ap = activePointList.begin();
  while (ap != activePointList.end()) {
    if ((*ap).first.shapePairP == 0 ||
        !project->contains((*ap).first.shapePairP))
      activePointList.erase(ap);
    else
      ap++;
  }
  // 何かポイントが選択されている場合,ポイント消去モード
  if (!activePointList.isEmpty()) {
    //	先ず、ポイントが消去可能かどうかをチェックする。
    //  条件：消去後のシェイプのポイント数が、
    //  Closed：2つ以下、Open：１つ以下なら消さない
    QMap<ShapePair*, QList<int>> deletingPointsMap;
    for (int p = 0; p < activePointList.size(); p++) {
      QPair<OneShape, int> pair = activePointList.at(p);
      // 既存のリストに追加
      if (deletingPointsMap.contains(pair.first.shapePairP)) {
        QList<int>& list = deletingPointsMap[pair.first.shapePairP];
        if (!list.contains(pair.second)) list.push_back(pair.second);
      }
      // 新たにリストを作る
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

    // 何かメッセージ出そうか？
    if (!canDelete) return;

    // ポイント消去コマンド
    ProjectUtils::deleteFormPoints(deletingPointsMap);
  }

  //-------------------

  // ポイント選択がない場合、シェイプ消去
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

    // 選択を解除
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

  // 登録時redo実行
  IwUndoManager::instance()->push(
      new DeleteCorrPointUndo(shapePair, corrIndexToBeDeleted));

  // 対応点選択をクリア
  setActiveCorrPoint(OneShape(), -1);
  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//---------------------------------------------------
// 表示中のShapeを全選択
//---------------------------------------------------

void IwShapePairSelection::selectAllShapes() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  int currentFrame      = project->getViewFrame();
  IwLayer* currentLayer = IwApp::instance()->getCurrentLayer()->getLayer();

  // いったん選択解除
  selectNone();

  // ロック情報を得る
  bool fromToVisible[2];
  for (int fromTo = 0; fromTo < 2; fromTo++)
    fromToVisible[fromTo] = project->getViewSettings()->isFromToVisible(fromTo);

  // 各レイヤについて
  for (int lay = project->getLayerCount() - 1; lay >= 0; lay--) {
    // レイヤを取得
    IwLayer* layer = project->getLayer(lay);
    if (!layer) continue;

    bool isCurrent = (layer == currentLayer);
    // 現在のレイヤが非表示ならcontinue
    // ただしカレントレイヤの場合は表示する
    if (!isCurrent && !layer->isVisibleInViewer()) continue;

    for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
      ShapePair* shapePair = layer->getShapePair(sp);
      if (!shapePair) continue;
      if (!shapePair->isVisible() || !shapePair->isEffective(currentFrame))
        continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // ロックされていて非表示ならスキップ
        if (!fromToVisible[fromTo]) continue;
        if (shapePair->isLocked(fromTo)) continue;
        addShape(OneShape(shapePair, fromTo));
      }
    }
  }

  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//---------------------------------------------------
// 選択シェイプのエクスポート
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

  // 重ね順に並べる
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
// 選択シェイプのロック切り替え
void IwShapePairSelection::toggleLocks() {
  if (isEmpty()) return;
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  bool doLock = false;
  // 選択シェイプに一つでも未ロックのものがあれば、全てロック
  // そうでなければロック解除
  for (auto shape : m_shapes) {
    if (!shape.shapePairP->isLocked(shape.fromTo)) {
      doLock = true;
      break;
    }
  }

  // 登録時redo実行
  IwUndoManager::instance()->push(
      new LockShapesUndo(m_shapes, project, doLock));
}

//---------------------------------------------------
// 選択シェイプのピン切り替え
void IwShapePairSelection::togglePins() {
  if (isEmpty()) return;
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  bool doPin = false;
  // 選択シェイプに一つでも未ピンのものがあれば、全てピン
  // そうでなければピン解除
  for (auto shape : m_shapes) {
    if (!shape.shapePairP->isPinned(shape.fromTo)) {
      doPin = true;
      break;
    }
  }

  // 登録時redo実行
  IwUndoManager::instance()->push(new PinShapesUndo(m_shapes, project, doPin));
}

//---------------------------------------------------
// 選択シェイプのピン切り替え
void IwShapePairSelection::toggleVisibility() {
  if (isEmpty()) return;
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  bool setVisible = false;
  QList<ShapePair*> shapePairList;
  // 選択シェイプに一つでも非表示のものがあれば、全て表示
  // そうでなければ非表示化
  for (auto shape : m_shapes) {
    ShapePair* shapePair = shape.shapePairP;
    if (shapePairList.contains(shapePair)) continue;
    shapePairList.append(shapePair);
    if (!setVisible && !shapePair->isVisible()) {
      setVisible = true;
    }
  }

  // 登録時redo実行
  IwUndoManager::instance()->push(
      new SetVisibleShapesUndo(shapePairList, project, setVisible));
}

//---------------------------------------------------
// シェイプタグ切り替え
//---------------------------------------------------
void IwShapePairSelection::setShapeTag(int tagId, bool on) {
  if (isEmpty()) return;
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // 登録時redo実行
  IwUndoManager::instance()->push(
      new SetShapeTagUndo(m_shapes, project, tagId, on));
}

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

//---------------------------------------------------
// ペーストのUndo
//---------------------------------------------------

PasteShapePairUndo::PasteShapePairUndo(QList<ShapePair*>& shapes,
                                       IwProject* project, IwLayer* layer)
    : m_shapes(shapes), m_project(project), m_layer(layer), m_firstFlag(true) {}

PasteShapePairUndo::~PasteShapePairUndo() {
  // シェイプをdelete
  for (int s = 0; s < m_shapes.size(); s++) {
    delete m_shapes[s];
    m_shapes[s] = 0;
  }
}

void PasteShapePairUndo::undo() {
  for (int s = 0; s < m_shapes.size(); s++)
    m_layer->deleteShapePair(m_shapes.at(s), true);

  // プロジェクトがカレントならシグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void PasteShapePairUndo::redo() {
  // Undo登録時にはredoを行わないよーにする
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  for (int s = 0; s < m_shapes.size(); s++)
    m_layer->addShapePair(m_shapes.at(s));

  // プロジェクトがカレントならシグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

//---------------------------------------------------
// カット/消去のUndo
//---------------------------------------------------

DeleteShapePairUndo::DeleteShapePairUndo(QList<ShapeInfo>& shapes,
                                         IwProject* project)
    : m_shapes(shapes), m_project(project) {}

DeleteShapePairUndo::~DeleteShapePairUndo() {}

void DeleteShapePairUndo::undo() {
  // シェイプを戻す
  for (int s = 0; s < m_shapes.size(); s++) {
    ShapeInfo info = m_shapes.at(s);
    // trianglecacheの処理は中でやっている
    info.layer->addShapePair(info.shapePair, info.index);
    //++s;
  }

  // プロジェクトがカレントならシグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void DeleteShapePairUndo::redo() {
  // Undo登録時にはredoを行わないよーにする
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  // シェイプを消す
  for (int s = 0; s < m_shapes.size(); s++) {
    ShapeInfo info = m_shapes.at(s);

    // trianglecacheの処理は中でやっている
    info.layer->deleteShapePair(info.index);
    //++s;
  }

  // プロジェクトがカレントならシグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

//---------------------------------------------------
// ロック切り替えのUndo
//---------------------------------------------------

LockShapesUndo::LockShapesUndo(QList<OneShape>& shapes, IwProject* project,
                               bool doLock)
    : m_project(project), m_doLock(doLock) {
  // 操作前の状態を保存
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
// ピン切り替えのUndo
//---------------------------------------------------

PinShapesUndo::PinShapesUndo(QList<OneShape>& shapes, IwProject* project,
                             bool doPin)
    : m_project(project), m_doPin(doPin) {
  // 操作前の状態を保存
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
// 表示、非表示切り替えのUndo
//---------------------------------------------------

SetVisibleShapesUndo::SetVisibleShapesUndo(QList<ShapePair*>& shapePairs,
                                           IwProject* project, bool setVisible)
    : m_project(project), m_setVisible(setVisible) {
  // 操作前の状態を保存
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
// シェイプタグ切り替えのUndo
//---------------------------------------------------

SetShapeTagUndo::SetShapeTagUndo(QList<OneShape>& shapes, IwProject* project,
                                 int tagId, bool on)
    : m_project(project), m_tagId(tagId), m_on(on) {
  // 操作前の状態を保存
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
    // レンダリングに関わるタグの場合シグナルを出す
    if (m_tagId ==
        m_project->getRenderQueue()->currentOutputSettings()->getShapeTagId())
      IwApp::instance()->getCurrentProject()->notifyPreviewCacheInvalidated();
  }
}

void SetShapeTagUndo::undo() { setTag(true); }

void SetShapeTagUndo::redo() { setTag(false); }
