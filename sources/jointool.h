//--------------------------------------------------------
// Join Tool
// 接続ツール。シェイプ２つをモーフィングのペアにする
//--------------------------------------------------------
#ifndef JOINTOOL_H
#define JOINTOOL_H

#include "iwtool.h"

#include <QPointF>
#include <QUndoCommand>

class IwProject;
class IwShape;

class JoinTool : public IwTool {
  // 現在マウスがオーバーしているシェイプ
  IwShape* m_overShape;

  IwShape* m_srcShape;
  IwShape* m_dstShape;

  QPointF m_startPos;
  QPointF m_currentPos;

public:
  JoinTool();
  ~JoinTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*);
  bool leftButtonDrag(const QPointF&, const QMouseEvent*);
  bool leftButtonUp(const QPointF&, const QMouseEvent*);

  bool mouseMove(const QPointF&, const QMouseEvent*);

  void draw();

  int getCursorId(const QMouseEvent* e);

  // 対応点同士を繋ぐプレビュー線を描画する
  void drawJoinPreviewLine();
};

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

class JoinShapeUndo : public QUndoCommand {
  IwProject* m_project;
  IwShape* m_srcShape;
  IwShape* m_dstShape;

  int m_srcPrec;
  int m_dstPrec;

  // ABロールに属していたときtrue
  bool m_srcWasInAB;
  bool m_dstWasInAB;

public:
  JoinShapeUndo(IwShape*, IwShape*, IwProject*);
  ~JoinShapeUndo();

  void undo();
  void redo();
};

class UnjoinShapeUndo : public QUndoCommand {
  IwProject* m_project;
  IwShape* m_srcShape;
  IwShape* m_dstShape;

public:
  UnjoinShapeUndo(IwShape*);
  ~UnjoinShapeUndo();

  void undo();
  void redo();
};
#endif