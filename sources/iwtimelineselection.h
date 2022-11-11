//---------------------------------------------------
// IwTimeLineSelection
// タイムライン内の素材フレームの選択範囲のクラス
//---------------------------------------------------
#ifndef IWTIMELINESELECTION_H
#define IWTIMELINESELECTION_H

#include "iwselection.h"
#include "iwlayer.h"

#include <QUndoCommand>

class TimeLineData;

class IwTimeLineSelection : public IwSelection  // singleton
{
  // start,endはframe間の境界の範囲。
  // こんなかんじ
  //	　　∥　　　∥　　　∥　　　∥　　　∥
  // 　　　∥ Ｆ０ ∥ Ｆ１ ∥ Ｆ２ ∥ Ｆ３ ∥
  // 　　　∥　　　∥　　　∥　　　∥　　　∥
  // 　境界０　　　１　　　２　　　３　　　４
  int m_start;
  int m_end;
  // endの右隣のフレームが選択に含まれているか
  bool m_rightFrameIncluded;

  IwTimeLineSelection();

public:
  static IwTimeLineSelection* instance();
  ~IwTimeLineSelection();

  void enableCommands();

  bool isEmpty() const;

  // 複数フレームの選択 選択が変わったらtrue、変わらなければfalseを返す
  bool selectFrames(int start, int end, bool includeRightFrame);
  // 単体フレームの選択
  void selectFrame(int frame);
  // 境界ひとつの選択
  void selectBorder(int frame);
  // 選択の解除
  void selectNone();
  // 選択範囲を得る
  void getRange(int& start, int& end, bool& rightFrameIncluded);

  // フレームが選択されているか
  bool isFrameSelected(int frame) const;
  // 境界が選択されているか
  bool isBorderSelected(int frame) const;

  //-- 以下、この選択で使えるコマンド群
  // コマンド共通：操作可能なカレントレイヤを返す
  IwLayer* getAvailableLayer();
  // コピー
  void copyFrames();
  // ペースト
  void pasteFrames();
  // 消去
  void deleteFrames();
  // カット
  void cutFrames();
  // 空きセルを入れる
  void insertEmptyFrame();

  // 素材の差し替え
  void replaceImages();
  // 再読み込み
  void reloadImages();
};

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

//---------------------------------------------------
// ペースト
//---------------------------------------------------
class PasteTimeLineFrameUndo : public QUndoCommand {
  IwLayer* m_layer;
  int m_startIndex;
  TimeLineData* m_beforeData;
  TimeLineData* m_afterData;

public:
  PasteTimeLineFrameUndo(IwLayer* layer, int startIndex, TimeLineData* before,
                         const TimeLineData* after);
  ~PasteTimeLineFrameUndo();
  void undo();
  void redo();
};
//---------------------------------------------------
// カット/消去
//---------------------------------------------------
class DeleteTimeLineFrameUndo : public QUndoCommand {
  IwLayer* m_layer;
  int m_startIndex;
  TimeLineData* m_beforeData;

public:
  DeleteTimeLineFrameUndo(IwLayer* layer, int startIndex, TimeLineData* before,
                          bool isDelete);
  ~DeleteTimeLineFrameUndo();
  void undo();
  void redo();
};

//---------------------------------------------------
// 空きセルを入れる
//---------------------------------------------------
class InsertEmptyFrameUndo : public QUndoCommand {
  IwLayer* m_layer;
  int m_index;

public:
  InsertEmptyFrameUndo(IwLayer* layer, int index);
  void undo();
  void redo();
};

//---------------------------------------------------
//  素材の差し替え
//---------------------------------------------------
class ReplaceImagesUndo : public QUndoCommand {
  IwLayer* m_layer;
  int m_from, m_to;
  QList<QPair<int, QString>> m_beforeData, m_afterData;
  bool m_firstFlag;

public:
  ReplaceImagesUndo(IwLayer* layer, int from, int to);
  void undo();
  void redo();
  void storeImageData(bool isBefore);
};

#endif