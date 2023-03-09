//--------------------------------------------------------------
// Free Hand Tool
// 自由曲線ツール
//
// マウスドラッグで離散的な点をトレースしていき、
// マウスリリースでベジエ曲線に近似してシェイプを作る。
// 設定ダイアログで近似の精度などを調整できる。
//--------------------------------------------------------------
#ifndef FREEHANDTOOL_H
#define FREEHANDTOOL_H

#include "iwtool.h"

#include <QList>
#include <QPointF>
#include <QUndoCommand>

#include "shapepair.h"

class IwProject;
class IwLayer;

class FreeHandTool : public IwTool {
  Q_OBJECT

  // トレースしたポイントのリスト
  QList<QPointF> m_points;
  // 閉じているかどうか
  bool m_isClosed;

  // 直前のマウス座標
  QPointF m_lastMousePos;
  bool m_isDragging;

  OneShape m_currentShape;
  // IwShape* m_currentShape;

  // 精度。許される誤差値
  double m_error;

  //------- 描線からベジエカーブを近似する部分
  BezierPointList fitCurve(QList<QPointF>& points, double error, bool isClosed);
  // 端点のタンジェント・ベクトルを求める
  QPointF computeLeftTangent(QList<QPointF>& points, int end);
  QPointF computeRightTangent(QList<QPointF>& points, int end);
  QPointF computeCenterTangent(QList<QPointF>& points, int center);
  void computeCenterTangents(QList<QPointF>& points, int center, QPointF* V1,
                             QPointF* V2);
  //  ポイントの（サブ）セットに、ベジエ曲線をフィットさせる
  void fitCubic(QList<QPointF>& points, int first, int last, QPointF& tHat1,
                QPointF& tHat2, double error, QList<QPointF>& outList);
  // 結果にベジエポイント4点を追加
  void addBezierCurve(QList<QPointF>& outList, QList<QPointF>& bezierCurve);
  // 各点の距離を０～１に正規化したものを格納
  QList<double> chordLengthParameterize(QList<QPointF>& points, int first,
                                        int last);

  // ベジエの生成
  QList<QPointF> generateBezier(QList<QPointF>& points, int first, int last,
                                QList<double>& uPrime, QPointF tHat1,
                                QPointF tHat2);
  // 適用したベジエで、最もズレている点のインデックスと、誤差の幅を得る
  double computeMaxError(QList<QPointF>& points, int first, int last,
                         QList<QPointF>& bezCurve, QList<double>& u,
                         int* splitPoint);
  // パラメータを再設定する
  void reparameterize(QList<double>& uPrime,  // ここに結果を収める
                      QList<QPointF>& points, int first, int last,
                      QList<double>& u, QList<QPointF>& bezCurve);
  //-------

  // 現在のパラメータからシェイプを作る
  void createShape();

public:
  FreeHandTool();
  ~FreeHandTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonDrag(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonUp(const QPointF&, const QMouseEvent*) final override;

  bool rightButtonDown(const QPointF&, const QMouseEvent*, bool& canOpenMenu,
                       QMenu& menu) final override;

  void draw() final override;

  void onDeactivate() final override;

  //(ダイアログから呼ばれる) 精度が変わったらシェイプを変更する
  void onPrecisionChanged(int prec);

  // シェイプがある場合は、それをプロジェクトに登録する
  // もしシェイプが完成したら、メニューを出さないためにfalseを返す
  bool finishShape();

  bool isClosed() { return m_isClosed; }

  void toggleCloseShape(bool close);

  int getCursorId(const QMouseEvent*) override;
public slots:
  void onDeleteCurrentShape();

signals:
  // シェイプを作り直す（あるいは消す）度に呼んで、ダイアログの値を更新する
  void shapeChanged(int);
};

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

class CreateFreeHandShapeUndo : public QUndoCommand {
  IwProject* m_project;
  int m_index;
  ShapePair* m_shape;
  IwLayer* m_layer;

public:
  CreateFreeHandShapeUndo(ShapePair* shape, IwProject* prj, IwLayer* layer);
  ~CreateFreeHandShapeUndo();

  void undo();
  void redo();
};

#endif