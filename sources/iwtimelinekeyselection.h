//------------------------------------------------
// IwTimeLineKeySelection
// TimeLineWindow 内のキーフレームの選択
//------------------------------------------------
#ifndef IWTIMELINEKEYSELECTION_H
#define IWTIMELINEKEYSELECTION_H

#include "iwselection.h"

#include "shapepair.h"

#include <QList>
#include <QUndoCommand>

class IwTimeLineKeySelection : public IwSelection {
  OneShape m_shape;
  QList<int> m_selectedFrames;
  bool m_isFormKeySelected;

  int m_rangeSelectionStartPos;

  IwTimeLineKeySelection();

public:
  static IwTimeLineKeySelection* instance();

  void enableCommands();
  bool isEmpty() const;

  // これまでの選択を解除して、現在のシェイプを切り替える。
  void setShape(OneShape shape, bool isForm);
  OneShape getShape() { return m_shape; }
  bool isFormKeySelected() { return m_isFormKeySelected; }

  // 単に選択フレームの追加
  void selectFrame(int selectedFrame);
  // 指定フレームを選択から外す
  void unselectFrame(int frame);
  // 選択の解除
  void selectNone();

  // フレーム範囲選択
  void doRangeSelect(int selectedFrame, bool ctrlPressed);

  // 選択カウント
  int selectionCount();
  // フレームを得る
  int getFrame(int index);

  // そのフレームが選択されているか
  bool isFrameSelected(int frame);

  // コピー
  void copyKeyFrames();
  // ペースト
  void pasteKeyFrames();
  // カット
  void cutKeyFrames();

  // 選択セルをキーフレームにする
  void setKey();
  // 選択セルのキーフレームを解除する
  void removeKey();
  // 補間を0.5(線形)に戻す
  void resetInterpolation();

  void setRangeSelectionStartPos(int pos = -1) {
    m_rangeSelectionStartPos = pos;
  }
};

//---------------------------------------------------
// 以下、Undoコマンド
//---------------------------------------------------

class TimeLineKeyEditUndo : public QUndoCommand {
  IwProject* m_project;

  struct KeyData {
    // 形状データ
    QMap<int, BezierPointList> formData;
    // Correspondence(対応点)データ
    QMap<int, CorrPointList> corrData;
  };

  OneShape m_shape;
  // 全てのキーフレームデータを保存
  KeyData m_beforeKeys;
  KeyData m_afterKeys;
  // 移動モード（必要なキーだけ保持する）
  bool m_isFormKeyEdited;
  // KeyFrameEditor::KEYMODE m_keyMode;

  // redoをされないようにするフラグ
  int m_firstFlag;

public:
  TimeLineKeyEditUndo(OneShape& shape, bool isForm, IwProject* project);

  ~TimeLineKeyEditUndo() {}

  void storeKeyData(bool before);

  void undo();
  void redo();
};

//------------------------------------------------
// 選択セルをキーフレームにする のUndo
//------------------------------------------------
class SetTimeLineKeyUndo : public QUndoCommand {
public:
  // キーフレームにしたフレームのリスト
  struct SetKeyData {
    OneShape shape;
    QMap<int, BezierPointList> formKeyFrames;
    QMap<int, CorrPointList> corrKeyFrames;
  };

private:
  IwProject* m_project;
  // QList<SetKeyData> m_setKeyData;
  SetKeyData m_setKeyData;

public:
  SetTimeLineKeyUndo(SetKeyData& setKeyData, IwProject* project);
  void undo();
  void redo();
};

//------------------------------------------------
// 選択セルのキーフレームを解除する のUndo
//------------------------------------------------
class UnsetTimeLineKeyUndo : public QUndoCommand {
public:
  // キーを解除したフレームのリスト
  struct UnsetKeyData {
    OneShape shape;
    QMap<int, BezierPointList> formKeyFrames;
    QMap<int, CorrPointList> corrKeyFrames;
  };

private:
  IwProject* m_project;
  UnsetKeyData m_unsetKeyData;

public:
  UnsetTimeLineKeyUndo(UnsetKeyData& unsetKeyData, IwProject* project);
  void undo();
  void redo();
};

//------------------------------------------------
// 補間値を0.5にリセット のUndo
//------------------------------------------------
class ResetInterpolationUndo : public QUndoCommand {
  OneShape m_shape;
  bool m_isForm;
  IwProject* m_project;

  QMap<int, double> m_interp_before;

public:
  ResetInterpolationUndo(OneShape& shape, bool isForm,
                         QMap<int, double> interps, IwProject* project);
  void undo();
  void redo();
};
#endif