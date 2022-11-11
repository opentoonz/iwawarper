//--------------------------------------------------------
// Pen Tool
// ペンツール
//--------------------------------------------------------
#ifndef PENTOOL_H
#define PENTOOL_H

#include "iwtool.h"

#include <QUndoCommand>

class PointDragTool;
class ShapePair;
class IwLayer;

class PenTool : public IwTool {
  ShapePair* m_editingShape;

  // 現在アクティブな点。10の位がポイントのID、
  //  1の位が1:CP 2:First 3:Second
  QList<int> m_activePoints;

  PointDragTool* m_dragTool;

  // m_editingShapeをViewer上に描くときに付ける名前の、
  // Shapeのインデックスの代わりに使う数字。
  // 999 **** + 　となる。
  //		**** : コントロールポイントのインデックス
  //		+ :
  // 1ならコントロールポイント、2ならFirstHandle、3ならSecondHandle
  static const int EDITING_SHAPE_ID = 999;

public:
  PenTool();
  ~PenTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*);
  bool leftButtonDrag(const QPointF&, const QMouseEvent*);
  bool leftButtonUp(const QPointF&, const QMouseEvent*);
  bool rightButtonDown(const QPointF&, const QMouseEvent*, bool& canOpenMenu,
                       QMenu& menu);

  // ツール解除のとき、描画中のシェイプがあったら確定する
  void onDeactivate();

  void draw();

  int getCursorId(const QMouseEvent* e);

  // ポイントがアクティブかどうかを返す
  bool isPointActive(int index) {
    for (int p = 0; p < m_activePoints.size(); p++) {
      if ((int)(m_activePoints[p] / 10) == index) return true;
    }

    return false;
  }

  // 対応点を更新する
  void updateCorrPoints(int fromTo);

  // シェイプを完成させる うまくいったら、
  // 右クリックメニューは出さないのでfalseを返す
  bool finishShape();

  bool setSpecialShapeColor(OneShape) override;
  bool setSpecialGridColor(int gId, bool isVertical) override;
};

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

class CreatePenShapeUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  int m_index;
  ShapePair* m_shape;

public:
  CreatePenShapeUndo(ShapePair* shape, IwProject* prj, IwLayer* layer);
  ~CreatePenShapeUndo();

  void undo();
  void redo();
};

#endif