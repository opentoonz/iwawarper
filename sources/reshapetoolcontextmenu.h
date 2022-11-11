//------------------------------------------
// ReshapeToolContextMenu
// Reshape Tool の右クリックメニュー用クラス
//------------------------------------------

#ifndef RESHAPETOOLCONTEXTMENU_H
#define RESHAPETOOLCONTEXTMENU_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QPair>

#include <QUndoCommand>

#include "shapepair.h"

class IwShapePairSelection;
class IwProject;
class QMouseEvent;
class QAction;
class QMenu;

enum RESHAPE_MENU_MODE {
  // Selectionに関係なく、カーソル下のポイントだけを操作
  CurrentPointMode,
  // カーソル下のシェイプの中の、選択されたポイントだけを操作
  CurrentShapeMode,
  // Selectionで選択されたシェイプ／ポイントを操作
  SelectedShapeMode
};

class ReshapeToolContextMenu : public QObject  // singleton
{
  Q_OBJECT

  RESHAPE_MENU_MODE m_mode;
  IwShapePairSelection* m_selection;

  // 現在のViewerの、1ピクセルあたりの距離
  QPointF m_onePixelLength;

  // 右クリックしたときのカーソル下のアイテム情報
  OneShape m_shape;
  int m_pointIndex, m_handleId;

  // このメニューの操作で影響を与えるポイント
  QList<QPair<OneShape, int>> m_effectedPoints;

  // ActivePointを含むシェイプのリスト
  QList<OneShape> m_activePointShapes;

  // アクション一覧
  QAction *m_lockPointsAct, *m_cuspAct, *m_linearAct, *m_smoothAct,
      *m_centerAct, *m_tweenPointAct, *m_reverseShapesAct;

  // このメニューの操作で影響を与えるポイントを更新
  void updateEffectedPoints();
  // アクションの有効／無効を切り替える
  void enableActions();
  // 有効／無効の判断
  //  bool canBreakShape();
  bool canLockPoints();
  bool canModifyHandles();  // Cusp, Linear, Smooth用
  bool canCenter();

  // ハンドル操作関係をひとつにまとめる
  enum ModifyHandleActId { Cusp, Linear, Smooth, Center };
  void doModifyHandle(ModifyHandleActId actId);

  // doCenterの実体
  void doCenter_Imp(OneShape shape, int index, BezierPointList& bpList);
  ReshapeToolContextMenu();

public:
  static ReshapeToolContextMenu* instance();

  void init(IwShapePairSelection* selection, QPointF onePixelLength);
  void openMenu(const QMouseEvent* e, OneShape shape, int pointIndex,
                int handleId, QMenu& menu);

public slots:
  // コマンドの実装
  void doLockPoints();
  void doCusp();
  void doLinear();
  void doSmooth();
  void doCenter();
  void doTween();
  void doReverse();
};

//---------------------------------------------------
// 以下、Undoコマンド
//---------------------------------------------------
// ReshapeContextMenuUndo
//---------------------------------------------------
class ReshapeContextMenuUndo : public QUndoCommand {
  IwProject* m_project;

  QList<OneShape> m_shapes;

  // シェイプごとの、編集されたフレームごとの元の形状のリスト
  QList<QMap<int, BezierPointList>> m_beforePoints;
  // シェイプごとの、編集されたフレームごとの操作後の形状のリスト
  QList<QMap<int, BezierPointList>> m_afterPoints;
  // そのシェイプがキーフレームだったかどうか
  QList<bool> m_wasKeyFrame;
  // 操作を行ったフレーム
  int m_frame;
  // redoをされないようにするフラグ
  int m_firstFlag;

public:
  ReshapeContextMenuUndo(QList<OneShape>& shapes,
                         QList<QMap<int, BezierPointList>>& beforePoints,
                         QList<QMap<int, BezierPointList>>& afterPoints,
                         QList<bool>& wasKeyFrame, int frame,
                         IwProject* project);
  void undo();
  void redo();
};

#endif