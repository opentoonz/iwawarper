//--------------------------------------------------------
// Reshape Tool用の ドラッグツール
// コントロールポイント編集ツール
//--------------------------------------------------------
#ifndef POINTDRAGTOOL_H
#define POINTDRAGTOOL_H

#include "shapepair.h"
#include "transformtool.h"

#include <QPointF>
#include <QList>
#include <QSet>

class IwProject;
class IwLayer;
class QMouseEvent;

//--------------------------------------------------------

class PointDragTool {
protected:
  IwProject *m_project;
  IwLayer *m_layer;

  QPointF m_startPos;

  // PenTool用
  bool m_undoEnabled;

public:
  PointDragTool();
  virtual ~PointDragTool(){};
  virtual void onClick(const QPointF &, const QMouseEvent *) = 0;
  virtual void onDrag(const QPointF &, const QMouseEvent *)  = 0;

  // Click時からマウス位置が変わっていなければfalseを返すようにする
  virtual bool onRelease(const QPointF &, const QMouseEvent *) = 0;

  virtual bool setSpecialShapeColor(OneShape) { return false; }
  virtual bool setSpecialGridColor(int, bool) { return false; }

  virtual void draw(const QPointF & /*onePixelLength*/) {}
};

//--------------------------------------------------------
//--------------------------------------------------------
// 並行移動ツール
//--------------------------------------------------------
class TranslatePointDragTool : public PointDragTool {
  QList<QList<int>>
      m_pointsInShapes;  // 選択ポイントを、各シェイプごとのリストに振り分けて格納

  QList<OneShape> m_shapes;

  // 元の形状
  QList<QMap<int, BezierPointList>> m_orgPoints;
  // そのシェイプがキーフレームだったかどうか
  QList<bool> m_wasKeyFrame;

  // 今つかんでいるポイントの元の位置
  QPointF m_grabbedPointOrgPos;

  QPointF m_onePixLength;

  OneShape m_snapTarget;

  int m_snapHGrid, m_snapVGrid;

  int m_pointIndex;
  OneShape m_shape;
  bool m_handleSnapped;

public:
  TranslatePointDragTool(const QSet<int> &, QPointF &grabbedPointOrgPos,
                         const QPointF &onePix, OneShape shape, int pointIndex);

  // PenTool用
  TranslatePointDragTool(OneShape, QList<int>, const QPointF &onePix);

  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
  bool onRelease(const QPointF &, const QMouseEvent *) final override;

  int calculateSnap(QPointF &);

  bool setSpecialShapeColor(OneShape) override;
  bool setSpecialGridColor(int, bool) override;

  void draw(const QPointF &onePixelLength) override;
};

//--------------------------------------------------------
//--------------------------------------------------------
// ハンドル操作ツール
//--------------------------------------------------------

class TranslateHandleDragTool : public PointDragTool {
  int m_pointIndex;
  int m_handleIndex;

  OneShape m_shape;
  // IwShape* m_shape;
  BezierPointList m_orgPoints;
  bool m_wasKeyFrame;

  QPointF m_onePixLength;

  struct CP {
    OneShape shape;
    int pointIndex;
  };
  QList<CP> m_snapCandidates;
  bool m_isHandleSnapped;

  // PenTool用。ポイントを増やしたと同時に伸ばすハンドルは、
  // Ctrlを押して伸ばしているかのようにふるまう。
  // すなわち、反対側のハンドルも点対称に伸びる。
  bool m_isInitial;

public:
  TranslateHandleDragTool(int, const QPointF &);

  // PenTool用
  TranslateHandleDragTool(OneShape, int, const QPointF &);
  TranslateHandleDragTool(OneShape shape, int pointIndex, int handleIndex,
                          const QPointF &onePix);

  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
  bool onRelease(const QPointF &, const QMouseEvent *) final override;

  // PenTool用。ポイントを増やしたと同時に伸ばすハンドルは、
  // Ctrlを押して伸ばしているかのようにふるまう。
  // すなわち、反対側のハンドルも点対称に伸びる。
  void setIsInitial() { m_isInitial = true; }

  void calculateHandleSnap(const QPointF, QPointF &);
  void draw(const QPointF &onePixelLength) override;
};

//--------------------------------------------------------

//--------------------------------------------------------

class ActivePointsDragTool : public PointDragTool {
protected:
  IwProject *m_project;
  IwLayer *m_layer;
  QPointF m_startPos;

  QList<QList<int>>
      m_pointsInShapes;  // 選択ポイントを、各シェイプごとのリストに振り分けて格納

  QList<OneShape> m_shapes;

  // 元の形状
  QList<BezierPointList> m_orgPoints;
  // そのシェイプがキーフレームだったかどうか
  QList<bool> m_wasKeyFrame;

  TransformHandleId m_handleId;
  QRectF m_handleRect;

  // つかんだハンドルの反対側のハンドル位置を返す
  QPointF getOppositeHandlePos();

  // シェイプの中心位置を返す
  QPointF getCenterPos() { return m_handleRect.center(); }

public:
  ActivePointsDragTool(TransformHandleId, const QSet<int> &, const QRectF &);
  virtual ~ActivePointsDragTool() {}
  virtual void onClick(const QPointF &, const QMouseEvent *) = 0;
  virtual void onDrag(const QPointF &, const QMouseEvent *)  = 0;
  virtual bool onRelease(const QPointF &, const QMouseEvent *);
};

//--------------------------------------------------------
// ハンドルCtrlクリック   → 回転モード (＋Shiftで45度ずつ)
//--------------------------------------------------------

class ActivePointsRotateDragTool : public ActivePointsDragTool {
  // 回転の基準位置
  QPointF m_centerPos;
  double m_initialAngle;

  QPointF m_onePixLength;

public:
  ActivePointsRotateDragTool(TransformHandleId, const QSet<int> &,
                             const QRectF &, const QPointF &onePix);
  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
};

//--------------------------------------------------------
// 縦横ハンドルAltクリック　  → Shear変形モード（＋Shiftで平行に変形）
//--------------------------------------------------------

class ActivePointsShearDragTool : public ActivePointsDragTool {
  // シアー変形の基準位置
  QPointF m_anchorPos;
  bool m_isVertical;
  bool m_isInv;

public:
  ActivePointsShearDragTool(TransformHandleId, const QSet<int> &,
                            const QRectF &);
  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
};

//--------------------------------------------------------
// 斜めハンドルAltクリック　  → 台形変形モード（＋Shiftで対称変形）
//--------------------------------------------------------

class ActivePointsTrapezoidDragTool : public ActivePointsDragTool {
  // 台形変形の基準となるバウンディングボックス
  QRectF m_initialBox;

public:
  ActivePointsTrapezoidDragTool(TransformHandleId, const QSet<int> &,
                                const QRectF &);

  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;

  // ４つの変移ベクトルの線形補間を足すことで移動させる
  QPointF map(const QRectF &bBox, const QPointF &bottomRightVec,
              const QPointF &topRightVec, const QPointF &topLeftVec,
              const QPointF &bottomLeftVec, QPointF &targetPoint);
};

//--------------------------------------------------------
// ハンドル普通のクリック → 拡大／縮小モード（＋Shiftで縦横比固定）
//--------------------------------------------------------

class ActivePointsScaleDragTool : public ActivePointsDragTool {
  // 拡大縮小の基準位置
  QPointF m_centerPos;

  bool m_scaleHorizontal;
  bool m_scaleVertical;

public:
  ActivePointsScaleDragTool(TransformHandleId, const QSet<int> &,
                            const QRectF &);
  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
};

//--------------------------------------------------------
// シェイプのハンドル以外をクリック  → 移動モード（＋Shiftで平行移動）
//--------------------------------------------------------

class ActivePointsTranslateDragTool : public ActivePointsDragTool {
public:
  ActivePointsTranslateDragTool(TransformHandleId, const QSet<int> &,
                                const QRectF &);
  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
};

//---------------------------------------------------
// 以下、Undoコマンド
//---------------------------------------------------
class ActivePointsDragToolUndo : public QUndoCommand {
  IwProject *m_project;

  QList<OneShape> m_shapes;

  // シェイプごとの元の形状のリスト
  QList<BezierPointList> m_beforePoints;
  // シェイプごとの操作後の形状のリスト
  QList<BezierPointList> m_afterPoints;
  // そのシェイプがキーフレームだったかどうか
  QList<bool> m_wasKeyFrame;

  int m_frame;

  // redoをされないようにするフラグ
  bool m_firstFlag;

public:
  ActivePointsDragToolUndo(QList<OneShape> &shapes,
                           QList<BezierPointList> &beforePoints,
                           QList<BezierPointList> &afterPoints,
                           QList<bool> &wasKeyFrame, IwProject *project,
                           int frame);
  ~ActivePointsDragToolUndo();
  void undo();
  void redo();
};

#endif
