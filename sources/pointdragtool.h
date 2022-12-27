//--------------------------------------------------------
// Reshape Tool用の ドラッグツール
// コントロールポイント編集ツール
//--------------------------------------------------------
#ifndef POINTDRAGTOOL_H
#define POINTDRAGTOOL_H

#include "shapepair.h"

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
  virtual void onClick(const QPointF &, const QMouseEvent *) = 0;
  virtual void onDrag(const QPointF &, const QMouseEvent *)  = 0;

  // Click時からマウス位置が変わっていなければfalseを返すようにする
  virtual bool onRelease(const QPointF &, const QMouseEvent *) = 0;

  virtual bool setSpecialShapeColor(OneShape) { return false; }
  virtual bool setSpecialGridColor(int, bool) { return false; }

  virtual void draw(const QPointF &onePixelLength) {}
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

  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
  bool onRelease(const QPointF &, const QMouseEvent *);

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

  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
  bool onRelease(const QPointF &, const QMouseEvent *);

  // PenTool用。ポイントを増やしたと同時に伸ばすハンドルは、
  // Ctrlを押して伸ばしているかのようにふるまう。
  // すなわち、反対側のハンドルも点対称に伸びる。
  void setIsInitial() { m_isInitial = true; }

  void calculateHandleSnap(const QPointF, QPointF &);
  void draw(const QPointF &onePixelLength) override;
};

//--------------------------------------------------------
#endif
