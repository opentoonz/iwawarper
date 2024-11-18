//--------------------------------------------------------
// Reshape Tool
// コントロールポイント編集ツール
//--------------------------------------------------------
#ifndef RESHAPETOOL_H
#define RESHAPETOOL_H

#include "iwtool.h"
#include "shapepair.h"

#include <QUndoCommand>
#include <QCoreApplication>

class IwShapePairSelection;
class PointDragTool;
class ReshapeToolContextMenu;
class IwLayer;
class SceneViewer;

class ReshapeTool : public IwTool {
  Q_DECLARE_TR_FUNCTIONS(ReshapeTool)

  IwShapePairSelection* m_selection;
  PointDragTool* m_dragTool;

  // ポイントをRect選択
  bool m_isRubberBanding;
  QPointF m_rubberStartPos;
  QPointF m_currentMousePos;

  bool isRubberBandValid();
  // クリック位置から、操作対象となるシェイプ／ハンドルを得る
  OneShape getClickedShapeAndIndex(int& pointIndex, int& handleId,
                                   const QMouseEvent* e);

  // show handles for transforming active points
  bool m_transformHandles;
  bool m_ctrlPressed;
  QRectF m_handleRect;

public:
  ReshapeTool();
  ~ReshapeTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonDrag(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonUp(const QPointF&, const QMouseEvent*) final override;
  void leftButtonDoubleClick(const QPointF&, const QMouseEvent*) final override;

  // 右クリックメニュー
  bool rightButtonDown(const QPointF&, const QMouseEvent*, bool& canOpenMenu,
                       QMenu& menu) final override;
  // bool rightButtonDown(const QPointF &, const QMouseEvent*);

  bool keyDown(int key, bool ctrl, bool shift, bool alt) final override;

  // アクティブなシェイプにコントロールポイントを描く
  // 選択ポイントは色を変え、ハンドルも描く
  void draw() final override;
  int getCursorId(const QMouseEvent*) final override;

  // コントロールポイントを描画する。ハンドルも付けるかどうかも引数で決める
  //  PenToolでも使いたいので、static化する
  static void drawControlPoint(
      SceneViewer* viewer, OneShape shape, BezierPointList& pointList,
      int cpIndex, bool drawHandles, const QPointF& onePix,
      int specialShapeIndex =
          0,  // PenToolで使う。Projectに属していないシェイプに使う
      bool fillPoint = false, QColor fillColor = QColor());
  // static void drawControlPoint(IwShape* shape,
  //							BezierPointList&
  // pointList, 							int
  // cpIndex, bool
  // drawHandles, 							QPointF
  // & onePix, int specialShapeIndex =
  // 0);//PenToolで使う。Projectに属していないシェイプに使う

  // ハンドル用の円を描く
  //  PenToolでも使いたいので、static化する
  static void drawCircle(SceneViewer* viewer);

  // アクティブなシェイプをAltクリックした場合、コントロールポイントを追加する
  void addControlPoint(OneShape shape, const QPointF& pos);
  // void addControlPoint(IwShape* shape, const QPointF & pos);

  bool setSpecialShapeColor(OneShape) override;
  bool setSpecialGridColor(int gId, bool isVertical) override;

  bool isTransformHandlesEnabled() { return m_transformHandles; }
  void enableTransformHandles(bool on) { m_transformHandles = on; }

protected:
  // Ctrl+矢印キーで0.25ピクセル分ずつ移動させる
  void arrowKeyMove(QPointF& delta);
};

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

class AddControlPointUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  OneShape m_shape;
  // IwShape*	m_shape;

  // 元の形状データ
  QMap<int, BezierPointList> m_beforeFormData[2];
  // 元の対応点データ
  QMap<int, CorrPointList> m_beforeCorrData[2];
  // 操作後の形状データ
  QMap<int, BezierPointList> m_afterFormData[2];
  // 操作後の対応点データ
  QMap<int, CorrPointList> m_afterCorrData[2];

  // redoをされないようにするフラグ
  int m_firstFlag;

public:
  AddControlPointUndo(const QMap<int, BezierPointList>& beforeFormDataFrom,
                      const QMap<int, CorrPointList>& beforeCorrDataFrom,
                      const QMap<int, BezierPointList>& beforeFormDataTo,
                      const QMap<int, CorrPointList>& beforeCorrDataTo,
                      OneShape shape, IwProject* project, IwLayer* layer);
  // AddControlPointUndo(	QMap<int, BezierPointList> & beforeFormData,
  //						QMap<int, CorrPointList> &
  // beforeCorrData, 						IwShape* shape,
  // IwProject* project);

  void setAfterData(const QMap<int, BezierPointList>& afterFormDataFrom,
                    const QMap<int, CorrPointList>& afterCorrDataFrom,
                    const QMap<int, BezierPointList>& afterFormDataTo,
                    const QMap<int, CorrPointList>& afterCorrDataTo);
  void undo();
  void redo();
};

#endif
