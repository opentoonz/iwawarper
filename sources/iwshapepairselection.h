//---------------------------------------------------
// IwShapePairSelection
// シェイプペアの選択のクラス
//---------------------------------------------------

#ifndef IWSHAPEPAIRSELECTION_H
#define IWSHAPEPAIRSELECTION_H

#include "iwselection.h"
#include "shapepair.h"

#include <QList>
#include <QPair>
#include <QSet>
#include <QUndoCommand>

class IwLayer;

struct ShapeInfo {
  int index;
  ShapePair* shapePair;
  IwLayer* layer;
};

class IwShapePairSelection : public IwSelection  // singleton
{
  Q_OBJECT

  // int == 0：From形状、int == 1：To形状
  QList<OneShape> m_shapes;

  // CorrespondenceToolで使用
  QPair<OneShape, int> m_activeCorrPoint;

  // Reshapeツールで使用
  // 選択ポイントIDのリスト
  // [ShapeId+1][FromTo][PointId] 0
  //				↑		↑
  //			　0又は1	３桁　　
  QSet<int> m_activePoints;

  IwShapePairSelection();

public:
  static IwShapePairSelection* instance();
  ~IwShapePairSelection();

  void enableCommands();
  bool isEmpty() const;
  // 選択の解除
  void selectNone();

  // これまでの選択を解除して、シェイプを１つ選択
  void selectOneShape(OneShape shape);
  // 選択シェイプを追加する
  void addShape(OneShape shape);
  // 選択シェイプを解除する
  int removeShape(OneShape shape);
  // FromTo指定で選択を解除
  bool removeFromToShapes(int fromTo);

  // 選択シェイプの個数
  int getCount() { return m_shapes.size(); }
  // シェイプを取得する
  OneShape getShape(int index);
  // シェイプのリストを取得する
  QList<OneShape> getShapes() { return m_shapes; }

  // 引数シェイプが選択範囲に含まれていたらtrue
  bool isSelected(OneShape shape) const;
  bool isSelected(ShapePair* shapePair) const;

  // 選択シェイプの個数を返す
  int getShapesCount() { return m_shapes.size(); }

  // アクティブ対応点の入出力
  QPair<OneShape, int> getActiveCorrPoint() { return m_activeCorrPoint; }
  void setActiveCorrPoint(OneShape shape, int index) {
    m_activeCorrPoint = QPair<OneShape, int>(shape, index);
  }
  bool isActiveCorrPoint(OneShape shape, int index) {
    return (m_activeCorrPoint.first == shape) &&
           (m_activeCorrPoint.second == index);
  }

  //---- Reshapeツールで使用
  // ポイントが選択されているかどうか
  bool isPointActive(OneShape, int index);
  // ポイントを選択する
  void activatePoint(int name);
  void activatePoint(OneShape, int index);
  // ポイント選択を解除する
  void deactivatePoint(int name);
  void deactivatePoint(OneShape, int index);
  // 選択ポイントのリストを返す
  QSet<int> getActivePointSet() { return m_activePoints; }
  // ↑のリスト版
  QList<QPair<OneShape, int>> getActivePointList();
  // 選択ポイントを全て解除する
  void deactivateAllPoints();

  //-- 以下、この選択で使えるコマンド群
  // コピー
  void copyShapes();
  // ペースト
  void pasteShapes();
  // カット
  void cutShapes();
  // 消去
  void deleteShapes();
  void onDeleteCorrPoint();

  // 表示中のShapeを全選択
  void selectAllShapes();

  // 選択シェイプのエクスポート
  void exportShapes();

  // 選択シェイプのロック切り替え
  void toggleLocks();

  // 選択シェイプのピン切り替え
  void togglePins();

  // 選択シェイプの表示非表示切り替え
  void toggleVisibility();

  // シェイプタグ切り替え
  void setShapeTag(int tagId, bool on);
};

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------

//---------------------------------------------------
// ペーストのUndo
//---------------------------------------------------

class PasteShapePairUndo : public QUndoCommand {
  QList<ShapePair*> m_shapes;
  IwProject* m_project;
  IwLayer* m_layer;

  // redoをされないようにするフラグ
  int m_firstFlag;

public:
  PasteShapePairUndo(QList<ShapePair*>& shapes, IwProject* project,
                     IwLayer* layer);
  ~PasteShapePairUndo();

  void undo();
  void redo();
};

//---------------------------------------------------
// カット/消去のUndo
//---------------------------------------------------

class DeleteShapePairUndo : public QUndoCommand {
  QList<ShapeInfo> m_shapes;
  // QMap<int,IwShape*> m_shapes;
  IwProject* m_project;

  // redoをされないようにするフラグ
  int m_firstFlag;

public:
  DeleteShapePairUndo(QList<ShapeInfo>& shapes, IwProject* project);
  ~DeleteShapePairUndo();

  // 要、Joinのつなぎなおし
  void undo();
  void redo();
};

//---------------------------------------------------
// ロック切り替えのUndo
//---------------------------------------------------

class LockShapesUndo : public QUndoCommand {
  QList<QPair<OneShape, bool>> m_shape_status;
  IwProject* m_project;

  bool m_doLock;

public:
  LockShapesUndo(QList<OneShape>& shapes, IwProject* project, bool doLock);
  void lockShapes(bool isUndo);
  void undo();
  void redo();
};

//---------------------------------------------------
// ピン切り替えのUndo
//---------------------------------------------------

class PinShapesUndo : public QUndoCommand {
  QList<QPair<OneShape, bool>> m_shape_status;
  IwProject* m_project;

  bool m_doPin;

public:
  PinShapesUndo(QList<OneShape>& shapes, IwProject* project, bool doPin);
  void pinShapes(bool isUndo);
  void undo();
  void redo();
};

//---------------------------------------------------
// 表示、非表示切り替えのUndo
//---------------------------------------------------

class SetVisibleShapesUndo : public QUndoCommand {
  QList<QPair<ShapePair*, bool>> m_shapePair_status;
  IwProject* m_project;

  bool m_setVisible;

public:
  SetVisibleShapesUndo(QList<ShapePair*>& shapes, IwProject* project,
                       bool setVisible);
  void setVisibleShapes(bool isUndo);
  void undo();
  void redo();
};

//---------------------------------------------------
// シェイプタグ切り替えのUndo
//---------------------------------------------------

class SetShapeTagUndo : public QUndoCommand {
  QList<QPair<OneShape, bool>> m_shape_status;
  IwProject* m_project;

  int m_tagId;
  bool m_on;

public:
  SetShapeTagUndo(QList<OneShape>& shapes, IwProject* project, int tagId,
                  bool on);
  void setTag(bool isUndo);
  void undo();
  void redo();
};
#endif