//--------------------------------------------------------
// Correspondence Tool
// 対応点編集ツール
//--------------------------------------------------------
#ifndef CORRESPONDENCETOOL_H
#define CORRESPONDENCETOOL_H

#include "iwtool.h"

#include "shapepair.h"

#include <QUndoCommand>
#include <QCoreApplication>

class QPointF;

class IwShapePairSelection;

//--------------------------------------------------------
// 対応点ドラッグツール
//--------------------------------------------------------
class CorrDragTool {
  IwProject *m_project;
  OneShape m_shape;

  // 掴んでいる対応点ハンドル
  int m_corrPointId;
  // そのフレームが対応点キーフレームだったかどうか
  bool m_wasKeyFrame;
  // 元の対応点リスト
  CorrPointList m_orgCorrs;

  QPointF m_startPos;
  QPointF m_onePixLength;

  int m_snappedCpId;

public:
  CorrDragTool(OneShape shape, int corrPointId, QPointF &onePix);
  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
  void onRelease(const QPointF &, const QMouseEvent *);
  OneShape shape() const { return m_shape; }
  int snappedCpId() const { return m_snappedCpId; }

  void doSnap(double &bezierPos, int frame, double rangeBefore = -1.,
              double rangeAfter = -1.);
};

//--------------------------------------------------------
// 　ツール本体
//--------------------------------------------------------

class CorrespondenceTool : public IwTool {
  Q_DECLARE_TR_FUNCTIONS(CorrespondenceTool)

  IwShapePairSelection *m_selection;

  CorrDragTool *m_dragTool;

  //+Altで対応点の追加
  void addCorrPoint(const QPointF &, OneShape);
  bool m_ctrlPressed;

public:
  CorrespondenceTool();
  ~CorrespondenceTool();

  bool leftButtonDown(const QPointF &, const QMouseEvent *) override;
  bool leftButtonDrag(const QPointF &, const QMouseEvent *) override;
  bool leftButtonUp(const QPointF &, const QMouseEvent *) override;
  void leftButtonDoubleClick(const QPointF &, const QMouseEvent *);

  // 対応点と分割された対応シェイプ、シェイプ同士の連結を表示する
  void draw() override;

  int getCursorId(const QMouseEvent *) override;

  // アクティブ時、Joinしているシェイプの片方だけが選択されていたら、
  // もう片方も選択する
  void onActivate() override;
  // アクティブな対応点をリセットする
  void onDeactivate() override;
};

//---------------------------------------------------
// 以下、Undoコマンドを登録
//--------------------------------------------------------
// 対応点移動のUndo
//--------------------------------------------------------
class CorrDragToolUndo : public QUndoCommand {
  IwProject *m_project;
  OneShape m_shape;

  // 元の対応点
  CorrPointList m_beforeCorrs;
  // 操作後の対応点
  CorrPointList m_afterCorrs;
  // そのフレームがキーフレームだったかどうか
  bool m_wasKeyFrame;

  int m_frame;
  // redoされないようにするフラグ
  bool m_firstFlag;

public:
  CorrDragToolUndo(OneShape shape, CorrPointList &beforeCorrs,
                   CorrPointList &afterCorrs, bool &wasKeyFrame,
                   IwProject *project, int frame);
  ~CorrDragToolUndo() {}
  void undo();
  void redo();
};

//--------------------------------------------------------
// 対応点追加のUndo
//--------------------------------------------------------
class AddCorrPointUndo : public QUndoCommand {
  IwProject *m_project;

  OneShape m_shape;

  // 元の対応点データ
  QMap<int, CorrPointList> m_beforeData;
  // 操作後の対応点データ
  QMap<int, CorrPointList> m_afterData;
  // Join相手
  OneShape m_partnerShape;
  // IwShape*	m_partnerShape;
  // Join相手 元の対応点データ
  QMap<int, CorrPointList> m_partnerBeforeData;
  // Join相手 操作後の対応点データ
  QMap<int, CorrPointList> m_partnerAfterData;

  // redoをされないようにするフラグ
  int m_firstFlag;

public:
  AddCorrPointUndo(OneShape shape, IwProject *project);

  void setAfterData();
  void undo();
  void redo();
};

#endif