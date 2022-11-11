//--------------------------------------------------------
// Reshape Tool
// コントロールポイント編集ツール
//--------------------------------------------------------
#ifndef RESHAPETOOL_H
#define RESHAPETOOL_H

#include "iwtool.h"
#include "shapepair.h"

#include <QUndoCommand>

class IwShapePairSelection;
class PointDragTool;
class ReshapeToolContextMenu;
class IwLayer;

class ReshapeTool : public IwTool {
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

public:
  ReshapeTool();
  ~ReshapeTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*);
  bool leftButtonDrag(const QPointF&, const QMouseEvent*);
  bool leftButtonUp(const QPointF&, const QMouseEvent*);
  void leftButtonDoubleClick(const QPointF&, const QMouseEvent*);

  // 右クリックメニュー
  bool rightButtonDown(const QPointF&, const QMouseEvent*, bool& canOpenMenu,
                       QMenu& menu);
  // bool rightButtonDown(const QPointF &, const QMouseEvent*);

  bool keyDown(int key, bool ctrl, bool shift, bool alt);

  // アクティブなシェイプにコントロールポイントを描く
  // 選択ポイントは色を変え、ハンドルも描く
  void draw();
  int getCursorId(const QMouseEvent*);

  // コントロールポイントを描画する。ハンドルも付けるかどうかも引数で決める
  //  PenToolでも使いたいので、static化する
  static void drawControlPoint(
      OneShape shape, BezierPointList& pointList, int cpIndex, bool drawHandles,
      QPointF& onePix,
      int specialShapeIndex =
          0,  // PenToolで使う。Projectに属していないシェイプに使う
      bool fillPoint = false);
  // static void drawControlPoint(IwShape* shape,
  //							BezierPointList&
  // pointList, 							int cpIndex,
  // bool
  // drawHandles, 							QPointF &
  // onePix, int specialShapeIndex =
  // 0);//PenToolで使う。Projectに属していないシェイプに使う

  // ハンドル用の円を描く
  //  PenToolでも使いたいので、static化する
  static void drawCircle();

  // アクティブなシェイプをAltクリックした場合、コントロールポイントを追加する
  void addControlPoint(OneShape shape, const QPointF& pos);
  // void addControlPoint(IwShape* shape, const QPointF & pos);

  bool setSpecialShapeColor(OneShape) override;
  bool setSpecialGridColor(int gId, bool isVertical) override;

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
  AddControlPointUndo(QMap<int, BezierPointList>& beforeFormDataFrom,
                      QMap<int, CorrPointList>& beforeCorrDataFrom,
                      QMap<int, BezierPointList>& beforeFormDataTo,
                      QMap<int, CorrPointList>& beforeCorrDataTo,
                      OneShape shape, IwProject* project, IwLayer* layer);
  // AddControlPointUndo(	QMap<int, BezierPointList> & beforeFormData,
  //						QMap<int, CorrPointList> &
  // beforeCorrData, 						IwShape* shape,
  // IwProject* project);

  void setAfterData(QMap<int, BezierPointList>& afterFormDataFrom,
                    QMap<int, CorrPointList>& afterCorrDataFrom,
                    QMap<int, BezierPointList>& afterFormDataTo,
                    QMap<int, CorrPointList>& afterCorrDataTo);
  void undo();
  void redo();
};

#endif