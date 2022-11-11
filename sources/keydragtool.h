#pragma once
#ifndef KEYDRAGTOOL_H
#define KEYDRAGTOOL_H

#include "shapepair.h"

#include <QSet>
#include <QUndoCommand>

class KeyDragTool {  // singleton

  OneShape m_shape;
  QList<int> m_selectedFrames;
  bool m_isFormKeySelected;

  int m_dragStart, m_dragCurrent;
  KeyDragTool();

public:
  static KeyDragTool* instance();

  bool isCurrentRow(OneShape, bool);
  bool isDragDestination(int);

  bool isDragging();
  // 現在のキー選択を登録、isDraggingオン
  void onClick(int frame);
  // ドラッグ先をシフトする
  void onMove(int frame);
  // ドラッグ先が動いていれば、Undo作成して処理
  void onRelease();
};

//------------------------------------------------------
// キー間の補間ハンドルのドラッグ
class InterpHandleDragTool {  // singleton
  OneShape m_shape;
  bool m_isForm;
  int m_key, m_nextKey;
  double m_frameOffset;
  double m_interpStart;
  InterpHandleDragTool();

public:
  static InterpHandleDragTool* instance();
  bool isDragging();

  void onClick(OneShape shape, bool isForm, double frameOffset, int key,
               int nextKey);
  // ドラッグ先をシフトする
  void onMove(double mouseFramePos, bool doSnap);
  // ドラッグ先が動いていれば、Undo作成して処理
  void onRelease();

  // 情報を取得
  void getInfo(OneShape& shape, bool& isForm, int& key, int& nextKey);
};

//------------------------------------------------------

class InterpHandleEditUndo : public QUndoCommand {
  IwProject* m_project;

  OneShape m_shape;
  bool m_isForm;
  int m_key;
  double m_before, m_after;

  // redoをされないようにするフラグ
  int m_firstFlag;

public:
  InterpHandleEditUndo(OneShape& shape, bool isForm, int key, double before,
                       double after, IwProject* project);

  ~InterpHandleEditUndo() {}

  void undo();
  void redo();
};
#endif