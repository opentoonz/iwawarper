//--------------------------------------------------------
// Transform Tool用 の ドラッグツール
// シェイプ変形ツール
//--------------------------------------------------------
#ifndef TRANSFORMDRAGTOOL_H
#define TRANSFORMDRAGTOOL_H

#include <QPointF>
#include <QList>

#include "shapepair.h"

#include <QUndoCommand>

class QMouseEvent;
class IwProject;
class TransformTool;

class IwLayer;

enum TransformHandleId {
  Handle_None = 0,
  Handle_BottomRight,
  Handle_Right,
  Handle_TopRight,
  Handle_Top,
  Handle_TopLeft,
  Handle_Left,
  Handle_BottomLeft,
  Handle_Bottom,
  // Ctrl押したとき辺ドラッグでサイズ変更
  Handle_RightEdge,
  Handle_TopEdge,
  Handle_LeftEdge,
  Handle_BottomEdge
};

//--------------------------------------------------------

class TransformDragTool {
protected:
  IwProject *m_project;
  IwLayer *m_layer;
  QPointF m_startPos;
  QList<OneShape> m_shapes;

  // 元の形状。 シェイプごとに (フレーム,形状情報)のマップを持つ
  QList<QMap<int, BezierPointList>> m_orgPoints;
  // そのシェイプがキーフレームだったかどうか
  QList<bool> m_wasKeyFrame;

  TransformHandleId m_handleId;

  OneShape m_grabbingShape;

  // TransformOptionの情報を得るために、TransformToolへのポインタを控えておく
  TransformTool *m_tool;

  // つかんだハンドルの反対側のハンドル位置を返す
  QPointF getOppositeHandlePos(OneShape shape = 0, int frame = -1);

  // シェイプの中心位置を返す
  QPointF getCenterPos(OneShape shape = 0, int frame = -1);

public:
  TransformDragTool(OneShape, TransformHandleId, QList<OneShape> &);
  virtual void onClick(const QPointF &, const QMouseEvent *) = 0;
  virtual void onDrag(const QPointF &, const QMouseEvent *)  = 0;
  virtual void onRelease(const QPointF &, const QMouseEvent *);
};

//--------------------------------------------------------
// ハンドルCtrlクリック   → 回転モード (＋Shiftで45度ずつ)
//--------------------------------------------------------

class RotateDragTool : public TransformDragTool {
  // 回転の基準位置
  QPointF m_anchorPos;
  // 回転に使う中心点のリスト
  QList<QMap<int, QPointF>> m_centerPos;

  double m_initialAngle;

  QPointF m_onePixLength;

public:
  RotateDragTool(OneShape, TransformHandleId, QList<OneShape> &,
                 QPointF &onePix);
  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
};

//--------------------------------------------------------
// 縦横ハンドルAltクリック　  → Shear変形モード（＋Shiftで平行に変形）
//--------------------------------------------------------

class ShearDragTool : public TransformDragTool {
  // シアー変形の基準位置
  QList<QMap<int, QPointF>> m_anchorPos;
  bool m_isVertical;
  bool m_isInv;

public:
  ShearDragTool(OneShape shape, TransformHandleId, QList<OneShape> &);
  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
};

//--------------------------------------------------------
// 斜めハンドルAltクリック　  → 台形変形モード（＋Shiftで対称変形）
//--------------------------------------------------------

class TrapezoidDragTool : public TransformDragTool {
  // 台形変形の基準となるバウンディングボックス
  QList<QMap<int, QRectF>> m_initialBox;

public:
  TrapezoidDragTool(OneShape shape, TransformHandleId, QList<OneShape> &);

  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);

  // ４つの変移ベクトルの線形補間を足すことで移動させる
  QPointF map(const QRectF &bBox, const QPointF &bottomRightVec,
              const QPointF &topRightVec, const QPointF &topLeftVec,
              const QPointF &bottomLeftVec, QPointF &targetPoint);
};

//--------------------------------------------------------
// ハンドル普通のクリック → 拡大／縮小モード（＋Shiftで縦横比固定）
//--------------------------------------------------------

class ScaleDragTool : public TransformDragTool {
  // 拡大縮小の基準位置
  QPointF m_anchorPos;
  // 拡大縮小の中心位置
  QList<QMap<int, QPointF>> m_scaleCenter;

  bool m_scaleHorizontal;
  bool m_scaleVertical;

public:
  ScaleDragTool(OneShape shape, TransformHandleId, QList<OneShape> &);
  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
};

//--------------------------------------------------------
// シェイプのハンドル以外をクリック  → 移動モード（＋Shiftで平行移動）
//--------------------------------------------------------

class TranslateDragTool : public TransformDragTool {
public:
  TranslateDragTool(OneShape shape, TransformHandleId, QList<OneShape> &);
  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
};

//---------------------------------------------------
// 以下、Undoコマンド
//---------------------------------------------------
class TransformDragToolUndo : public QUndoCommand {
  IwProject *m_project;

  QList<OneShape> m_shapes;

  // シェイプごとの、編集されたフレームごとの元の形状のリスト
  QList<QMap<int, BezierPointList>> m_beforePoints;
  // シェイプごとの、編集されたフレームごとの操作後の形状のリスト
  QList<QMap<int, BezierPointList>> m_afterPoints;
  // そのシェイプがキーフレームだったかどうか
  QList<bool> m_wasKeyFrame;

  int m_frame;

  // redoをされないようにするフラグ
  bool m_firstFlag;

public:
  TransformDragToolUndo(QList<OneShape> &shapes,
                        QList<QMap<int, BezierPointList>> &beforePoints,
                        QList<QMap<int, BezierPointList>> &afterPoints,
                        QList<bool> &wasKeyFrame, IwProject *project,
                        int frame);
  ~TransformDragToolUndo();
  void undo();
  void redo();
};

#endif