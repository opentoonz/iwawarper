//---------------------------------------------------
// IwTimeLineSelection
// タイムライン内の素材フレームの選択範囲のクラス
//---------------------------------------------------

#include "iwtimelineselection.h"

#include <iostream>

#include <QClipboard>
#include <QApplication>

#include "iwapp.h"
#include "iwlayerhandle.h"
#include "iwprojecthandle.h"
#include "timelinedata.h"
#include "iwundomanager.h"
#include "iwimagecache.h"
#include "iwproject.h"
#include "viewsettings.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"
#include "iocommand.h"

#include "shapepairdata.h"
#include "iwshapepairselection.h"
#include "iwtrianglecache.h"

//===================================================

IwTimeLineSelection::IwTimeLineSelection()
    : m_start(0), m_end(-1), m_rightFrameIncluded(false) {}

//-----------------------------------------------------------------------------

IwTimeLineSelection* IwTimeLineSelection::instance() {
  static IwTimeLineSelection _instance;
  return &_instance;
}

//---------------------------------------------------
IwTimeLineSelection::~IwTimeLineSelection() {}
//---------------------------------------------------
void IwTimeLineSelection::enableCommands() {
  enableCommand(this, MI_Copy, &IwTimeLineSelection::copyFrames);
  enableCommand(this, MI_Paste, &IwTimeLineSelection::pasteFrames);
  enableCommand(this, MI_Delete, &IwTimeLineSelection::deleteFrames);
  enableCommand(this, MI_Cut, &IwTimeLineSelection::cutFrames);

  enableCommand(this, MI_InsertEmpty, &IwTimeLineSelection::insertEmptyFrame);

  // ファイルパスの差し替え
  enableCommand(this, MI_ReplaceImages, &IwTimeLineSelection::replaceImages);
  enableCommand(this, MI_ReloadImages, &IwTimeLineSelection::reloadImages);
}
//---------------------------------------------------
bool IwTimeLineSelection::isEmpty() const {
  return (m_start > m_end) ? true : false;
}
//---------------------------------------------------
// 選択の解除
//---------------------------------------------------
void IwTimeLineSelection::selectNone() {
  m_start              = 0;
  m_end                = -1;
  m_rightFrameIncluded = false;
}
//---------------------------------------------------
// 複数フレームの選択 選択が変わったらtrue、変わらなければfalseを返す
//---------------------------------------------------
bool IwTimeLineSelection::selectFrames(int start, int end,
                                       bool includeRightFrame) {
  if (m_start == start && m_end == end &&
      m_rightFrameIncluded == includeRightFrame)
    return false;

  m_start              = start;
  m_end                = end;
  m_rightFrameIncluded = includeRightFrame;
  return true;
}
//---------------------------------------------------
// 単体フレームの選択
//---------------------------------------------------
void IwTimeLineSelection::selectFrame(int frame) {
  m_start              = frame;
  m_end                = frame;
  m_rightFrameIncluded = true;
}
//---------------------------------------------------
// 境界ひとつの選択
//---------------------------------------------------
void IwTimeLineSelection::selectBorder(int frame) {
  m_start              = frame;
  m_end                = frame;
  m_rightFrameIncluded = false;
}
//---------------------------------------------------
// フレームが選択されているか
//---------------------------------------------------
bool IwTimeLineSelection::isFrameSelected(int frame) const {
  // 選択範囲が空ならfalse
  if (isEmpty()) return false;
  // start以上かつend未満ならＯＫ
  if (m_start <= frame && m_end > frame) return true;
  // frame==endの場合、含まれているかどうかはフラグで決まる
  if (m_end == frame) return m_rightFrameIncluded;
  // それ意外の場合は選択範囲外
  return false;
}
//---------------------------------------------------
// 境界が選択されているか
//---------------------------------------------------
bool IwTimeLineSelection::isBorderSelected(int frame) const {
  // 選択範囲が空ならfalse
  if (isEmpty()) return false;
  // start以上かつend以下ならＯＫ
  if (m_start <= frame && m_end >= frame) return true;
  // それ意外の場合は選択範囲外
  return false;
}
//---------------------------------------------------
// 選択範囲を得る
//---------------------------------------------------
void IwTimeLineSelection::getRange(int& start, int& end,
                                   bool& rightFrameIncluded) {
  start              = m_start;
  end                = m_end;
  rightFrameIncluded = m_rightFrameIncluded;
}

//---------------------------------------------------
// コマンド共通：操作可能なカレントレイヤを返す
//---------------------------------------------------
IwLayer* IwTimeLineSelection::getAvailableLayer() {
  // これがカレントでなければreturn 0
  // 選択範囲が空の場合return 0
  if (IwSelection::getCurrent() != this || isEmpty()) return 0;
  // カレントロールを取得してreturn
  return IwApp::instance()->getCurrentLayer()->getLayer();
}
//---------------------------------------------------
// コピー (Undoは無い)
//---------------------------------------------------
void IwTimeLineSelection::copyFrames() {
  // 操作可能なカレントレイヤを取得
  IwLayer* layer = getAvailableLayer();
  // カレントレイヤが無い場合return
  if (!layer) return;

  // SequenceDataデータを作り、選択範囲データを格納
  TimeLineData* timeLineData = new TimeLineData();

  timeLineData->setFrameData(layer, m_start, m_end, m_rightFrameIncluded);

  // クリップボードにSequenceDataデータを入れる
  QClipboard* clipboard = QApplication::clipboard();
  clipboard->setMimeData(timeLineData, QClipboard::Clipboard);
}
//---------------------------------------------------
// ペースト
//---------------------------------------------------
void IwTimeLineSelection::pasteFrames() {
  // 操作可能なカレントレイヤを取得
  IwLayer* layer = getAvailableLayer();
  // カレントレイヤが無い場合return
  if (!layer) return;

  // クリップボードからデータを取り、TimeLineDataかどうか確認
  QClipboard* clipboard     = QApplication::clipboard();
  const QMimeData* mimeData = clipboard->mimeData();
  const TimeLineData* timeLineData =
      dynamic_cast<const TimeLineData*>(mimeData);
  // データが違っていたらreturn
  if (!timeLineData) {
    // ShapePairはタイムライン選択中にペースト可能にする
    const ShapePairData* shapePairData =
        dynamic_cast<const ShapePairData*>(mimeData);
    if (shapePairData) {
      IwShapePairSelection::instance()->makeCurrent();
      IwShapePairSelection::instance()->pasteShapes();
    }
    return;
  }

  // 選択範囲をキープ
  // ここで作ったsequenceDataは、PasteSequenceFrameUndoのデストラクタで開放される
  TimeLineData* beforeData = new TimeLineData();
  beforeData->setFrameData(layer, m_start, m_end, m_rightFrameIncluded);

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new PasteTimeLineFrameUndo(layer, m_start, beforeData, timeLineData));
}
//---------------------------------------------------
// 消去
//---------------------------------------------------
void IwTimeLineSelection::deleteFrames() {
  // 操作可能なカレントレイヤを取得
  IwLayer* layer = getAvailableLayer();
  // カレントレイヤが無い場合return
  if (!layer) return;

  // SequenceDataデータを作り、選択範囲データを格納
  TimeLineData* beforeData = new TimeLineData();
  beforeData->setFrameData(layer, m_start, m_end, m_rightFrameIncluded);

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new DeleteTimeLineFrameUndo(layer, m_start, beforeData, true));
}
//---------------------------------------------------
// カット
//---------------------------------------------------
void IwTimeLineSelection::cutFrames() {
  std::cout << "cutFrames" << std::endl;

  // 操作可能なカレントレイヤを取得
  IwLayer* layer = getAvailableLayer();
  // カレントレイヤが無い場合return
  if (!layer) return;

  // SequenceDataデータを作り、選択範囲データを格納
  TimeLineData* beforeData = new TimeLineData();
  beforeData->setFrameData(layer, m_start, m_end, m_rightFrameIncluded);

  // クリップボードにSequenceDataデータを入れる
  QClipboard* clipboard = QApplication::clipboard();
  clipboard->setMimeData(beforeData, QClipboard::Clipboard);

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new DeleteTimeLineFrameUndo(layer, m_start, beforeData, false));
}

//---------------------------------------------------
// 空きセルを入れる
//---------------------------------------------------
void IwTimeLineSelection::insertEmptyFrame() {
  // 操作可能なカレントレイヤを取得
  IwLayer* layer = getAvailableLayer();
  // カレントレイヤが無い場合return
  if (!layer) return;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new InsertEmptyFrameUndo(layer, m_start));
}

//---------------------------------------------------
//  素材の差し替え
//---------------------------------------------------
void IwTimeLineSelection::replaceImages() {
  // 操作可能なカレントレイヤを取得
  IwLayer* layer = getAvailableLayer();
  // カレントレイヤが無い場合return
  if (!layer) return;

  // Undoを作ると、コンストラクタ内で現在のデータは確保される
  ReplaceImagesUndo* undo = new ReplaceImagesUndo(layer, m_start, m_end);

  // フレーム範囲を指定
  IoCmd::doReplaceImages(layer, m_start, m_end);

  // 操作後のデータを確保
  undo->storeImageData(false);

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行されるが、フラグで回避
  IwUndoManager::instance()->push(undo);
}

//---------------------------------------------------
//  素材の再読み込み
//---------------------------------------------------
void IwTimeLineSelection::reloadImages() {
  // 操作可能なカレントレイヤを取得
  IwLayer* layer = getAvailableLayer();
  // カレントレイヤが無い場合return
  if (!layer) return;
  ViewSettings* vs =
      IwApp::instance()->getCurrentProject()->getProject()->getViewSettings();

  QStringList pathsToBeReloaded;
  for (int f = m_start; f <= m_end; f++) {
    QString path = layer->getImageFilePath(f);
    if (path.isEmpty() || pathsToBeReloaded.contains(path)) continue;
    pathsToBeReloaded.append(path);
  }
  for (const auto& path : pathsToBeReloaded) {
    if (IwImageCache::instance()->isCached(path))
      IwImageCache::instance()->remove(path);
    vs->releaseTexture(path);
  }

  // invalidate
  for (int s = 0; s < layer->getShapePairCount(); s++) {
    ShapePair* shape = layer->getShapePair(s);
    if (!shape) continue;
    IwTriangleCache::instance()->invalidateCache(m_start, m_end, shape);
  }
  // Viewerを再描画するためのシグナルを出す
  IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
}

//===================================================
//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------
//---------------------------------------------------
// ペースト
//---------------------------------------------------
PasteTimeLineFrameUndo::PasteTimeLineFrameUndo(IwLayer* layer, int startIndex,
                                               TimeLineData* before,
                                               const TimeLineData* after)
    : m_layer(layer)
    , m_startIndex(startIndex)
    , m_beforeData(before)
    , m_afterData(new TimeLineData(after)) {
  setText(QObject::tr("Paste Timeline Frames"));
}

PasteTimeLineFrameUndo::~PasteTimeLineFrameUndo() {
  delete m_beforeData;
  delete m_afterData;
}

void PasteTimeLineFrameUndo::undo() {
  // startIndexからafterDataの枚数分消去
  // 消去する枚数
  int amount = m_afterData->getFrameLength();
  m_layer->deletePaths(m_startIndex, amount);
  // beforeDataの挿入
  m_beforeData->getFrameData(m_layer, m_startIndex);
  // SequenceChangedをエミット
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
}

void PasteTimeLineFrameUndo::redo() {
  //---選択範囲を消去
  // 消去する枚数
  int amount = m_beforeData->getFrameLength();
  // 消去
  m_layer->deletePaths(m_startIndex, amount);
  // コピーされたSequenceDataを挿入
  m_afterData->getFrameData(m_layer, m_startIndex);
  // SequenceChangedをエミット
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
}

//---------------------------------------------------
// カット/消去
//---------------------------------------------------
DeleteTimeLineFrameUndo::DeleteTimeLineFrameUndo(IwLayer* layer, int startIndex,
                                                 TimeLineData* before,
                                                 bool isDelete)
    : m_layer(layer), m_startIndex(startIndex), m_beforeData(before) {
  if (isDelete)
    setText(QObject::tr("Delete Timeline Frames"));
  else
    setText(QObject::tr("Cut Timeline Frames"));
}

DeleteTimeLineFrameUndo::~DeleteTimeLineFrameUndo() { delete m_beforeData; }

void DeleteTimeLineFrameUndo::undo() {
  // 空きセルを削除
  int amount = m_beforeData->getFrameLength();
  m_layer->deletePaths(m_startIndex, amount);
  // beforeDataの挿入
  m_beforeData->getFrameData(m_layer, m_startIndex);
  // SequenceChangedをエミット
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
  // IwApp::instance()->getCurrentSequence()->notifySequenceChanged(m_startIndex);
}

void DeleteTimeLineFrameUndo::redo() {
  //---範囲を消去
  // 消去する枚数
  int amount = m_beforeData->getFrameLength();
  // 消去 140313 空セルにする
  for (int f = 0; f < amount; f++)
    m_layer->replacePath(m_startIndex + f, QPair<int, QString>(0, QString()));
  // m_layer->deletePaths(m_startIndex,amount);

  // SequenceChangedをエミット
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
  // IwApp::instance()->getCurrentSequence()->notifySequenceChanged(m_startIndex);
}

//---------------------------------------------------
// 空きセルを入れる
//---------------------------------------------------

InsertEmptyFrameUndo::InsertEmptyFrameUndo(IwLayer* layer, int index)
    : m_layer(layer), m_index(index) {}

void InsertEmptyFrameUndo::undo() {
  m_layer->deletePaths(m_index, 1);
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
}

void InsertEmptyFrameUndo::redo() {
  m_layer->insertPath(m_index, QPair<int, QString>(0, ""));
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
}

//---------------------------------------------------
//  素材の差し替えのUndo
//---------------------------------------------------
ReplaceImagesUndo::ReplaceImagesUndo(IwLayer* layer, int from, int to)
    : m_layer(layer), m_from(from), m_to(to), m_firstFlag(true) {
  // 操作前のファイルパス情報を取得
  storeImageData(true);
}

void ReplaceImagesUndo::undo() {
  // Beforeのデータに戻す
  for (int f = m_from, i = 0; f <= m_to; f++, i++)
    m_layer->replacePath(f, m_beforeData.at(i));

  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
}

void ReplaceImagesUndo::redo() {
  // 最初の操作は飛ばす
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }
  // Afterのデータに戻す
  for (int f = m_from, i = 0; f <= m_to; f++, i++)
    m_layer->replacePath(f, m_afterData.at(i));

  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
}

// ファイルパス情報を取得する
void ReplaceImagesUndo::storeImageData(bool isBefore) {
  for (int f = m_from; f <= m_to; f++) {
    if (isBefore)
      m_beforeData.append(m_layer->getPathData(f));
    else
      m_afterData.append(m_layer->getPathData(f));
  }
}