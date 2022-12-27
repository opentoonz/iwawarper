//--------------------------------------------------------
// Transform Tool
// シェイプ変形ツール
//--------------------------------------------------------
#ifndef TRANSFORMTOOL_H
#define TRANSFORMTOOL_H

#include "iwtool.h"

#include "transformdragtool.h"

#include <QUndoCommand>
#include <QCoreApplication>

class IwShapePairSelection;
class IwProject;
class IwShape;
class QPointF;

class IwLayer;
struct OneShape;

//--------------------------------------------------------
class TransformTool : public IwTool {
  Q_DECLARE_TR_FUNCTIONS(TransformTool)

  bool m_ctrlPressed;

  IwShapePairSelection* m_shapePairSelection;

  TransformDragTool* m_dragTool;

  // シェイプをRect選択
  bool m_isRubberBanding;
  QPointF m_rubberStartPos;
  QPointF m_currentMousePos;

  // 以下、TransformOptionの内容
  bool m_scaleAroundCenter;
  bool m_rotateAroundCenter;
  bool m_shapeIndependentTransforms;
  // MultiFrameがONのとき、他のフレームの変形（回転/伸縮/シアー）の
  // 中心を、今編集中のフレームの中心を使うか（OFFのとき）、
  // それぞれのフレームでの中心を使うか。（ONのとき）
  bool m_frameIndependentTransforms;

  bool m_translateOnly;

public:
  TransformTool();
  ~TransformTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*);
  bool leftButtonDrag(const QPointF&, const QMouseEvent*);
  bool leftButtonUp(const QPointF&, const QMouseEvent*);
  void leftButtonDoubleClick(const QPointF&, const QMouseEvent*);

  bool keyDown(int key, bool ctrl, bool shift, bool alt);

  // 選択されているシェイプに枠を書く ハンドルには名前をつける
  void draw();

  int getCursorId(const QMouseEvent*);

  // ハンドルをいっこ描く
  void drawHandle(IwLayer* layer, OneShape shape, TransformHandleId handleId,
                  const QPointF& onePix, const QPointF& pos);
  // void drawHandle(IwProject* project, IwShape* shape,
  //				TransformHandleId handleId,
  //				QPointF & onePix, QPointF & pos);

  // 140110 iwasawa Ctrl押したとき辺ドラッグでサイズ変更のための辺
  void drawEdgeForResize(IwLayer* layer, OneShape shape,
                         TransformHandleId handleId, const QPointF& p1,
                         const QPointF& p2);

  // 以下、TransformOptionの内容の操作
  bool IsScaleAroundCenter() { return m_scaleAroundCenter; }
  void setScaleAroundCenter(bool on) { m_scaleAroundCenter = on; }
  bool IsRotateAroundCenter() { return m_rotateAroundCenter; }
  void setRotateAroundCenter(bool on) { m_rotateAroundCenter = on; }
  bool IsShapeIndependentTransforms() { return m_shapeIndependentTransforms; }
  void setShapeIndependentTransforms(bool on) {
    m_shapeIndependentTransforms = on;
  }
  bool IsFrameIndependentTransforms() { return m_frameIndependentTransforms; }
  void setFrameIndependentTransforms(bool on) {
    m_frameIndependentTransforms = on;
  }

  bool isTranslateOnly() { return m_translateOnly; }
  void setTranslateOnly(bool on) { m_translateOnly = on; }

protected:
  // Ctrl+矢印キーで0.25ピクセル分ずつ移動させる
  void arrowKeyMove(QPointF& delta);
};

//--------------------------------------------------------
// 以下、Undoコマンド
//--------------------------------------------------------

//--------------------------------------------------------
// Ctrl+矢印キーで0.25ピクセル分ずつ移動させる のUndo
//--------------------------------------------------------
class ArrowKeyMoveUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;

  QList<OneShape> m_shapes;
  // QList<IwShape*> m_shapes;

  // シェイプごとの元の形状のリスト
  QList<BezierPointList> m_beforePoints;
  // シェイプごとの操作後の形状のリスト
  QList<BezierPointList> m_afterPoints;
  // そのシェイプがキーフレームだったかどうか
  QList<bool> m_wasKeyFrame;
  int m_frame;

  // redoをされないようにするフラグ
  int m_firstFlag;

public:
  ArrowKeyMoveUndo(const QList<OneShape>& shapes,
                   QList<BezierPointList>& beforePoints,
                   QList<BezierPointList>& afterPoints,
                   QList<bool>& wasKeyFrame, IwProject* project, IwLayer* layer,
                   int frame);
  // ArrowKeyMoveUndo(	QList<IwShape*> & shapes,
  //					QList<BezierPointList> & beforePoints,
  //					QList<BezierPointList> & afterPoints,
  //					QList<bool> & wasKeyFrame,
  //					IwProject* project,
  //					int frame);
  void undo();
  void redo();
};

#endif
