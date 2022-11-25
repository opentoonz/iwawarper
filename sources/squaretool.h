//--------------------------------------------------------
// Square Tool
// 矩形描画ツール
// 作成後はそのシェイプがカレントになる
// カレントシェイプの変形もできる（TransformToolと同じ機能）
//--------------------------------------------------------
#ifndef SQUARETOOL_H
#define SQUARETOOL_H

#include "iwtool.h"
#include <QPointF>
#include <QRectF>
#include <QUndoCommand>
#include <QCoreApplication>

class IwProject;
class IwShape;

class ShapePair;
class IwLayer;

class SquareTool : public IwTool {
  Q_DECLARE_TR_FUNCTIONS(SquareTool)

  bool m_startDefined;
  QPointF m_startPos;
  QPointF m_endPos;

public:
  SquareTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*);
  bool leftButtonDrag(const QPointF&, const QMouseEvent*);
  bool leftButtonUp(const QPointF&, const QMouseEvent*);
  void leftButtonDoubleClick(const QPointF&, const QMouseEvent*);
  void draw();

  int getCursorId(const QMouseEvent*) override;
};

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

class CreateSquareShapeUndo : public QUndoCommand {
  IwProject* m_project;
  int m_index;
  IwShape* m_shape;

  ShapePair* m_shapePair;
  IwLayer* m_layer;

public:
  CreateSquareShapeUndo(QRectF& rect, IwProject* prj, IwLayer* layer);
  ~CreateSquareShapeUndo();

  void undo();
  void redo();
};

#endif