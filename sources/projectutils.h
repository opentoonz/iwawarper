#ifndef PROJECTUTILS_H
#define PROJECTUTILS_H

//---------------------------------------
// ProjectUtils
// シーン操作のコマンド＋Undo
//---------------------------------------

#include <QUndoCommand>
#include <QMap>
#include <QList>
#include <QSet>

#include "shapepair.h"

class QString;

class IwProject;
class IwLayer;
class ShapePair;

namespace ProjectUtils {
//-----
// IwLayerに対する操作
//-----
// レイヤ順序の入れ替え
void SwapLayers(int, int);
bool ReorderLayers(QList<IwLayer*>);
// リネーム
void RenameLayer(IwLayer*, QString&);
// レイヤ削除
void DeleteLayer(int);

void CloneLayer(int);

// レイヤ情報の変更
enum LayerPropertyType { Visibility, Render, Lock };
void toggleLayerProperty(IwLayer*, LayerPropertyType type);

//-----
// ShapePairに対する操作
//-----
// シェイプ順序の入れ替え
void SwapShapes(IwLayer*, int, int);
// シェイプの移動
void MoveShape(IwLayer*, int, int);

// レイヤーをまたいでドラッグ移動。最後のboolはisParentの情報
void ReorderShapes(QMap<IwLayer*, QList<QPair<ShapePair*, bool>>>);
// リネーム
void RenameShapePair(ShapePair*, QString&);
// ポイント消去
void deleteFormPoints(QMap<ShapePair*, QList<int>>& pointList);

// 親子を切り替える
void switchParentChild(ShapePair*);

// ロックを切り替える
void switchLock(OneShape);
// ピンを切り替える
void switchPin(OneShape);
// 表示非表示を切り替える
void switchShapeVisibility(ShapePair*);

// プロジェクト内にロックされたシェイプがあるかどうか
bool hasLockedShape();
// 全てのロックされたシェイプをアンロック
void unlockAllShapes();

//-----
// ワークエリアのサイズ変更
// returns true when accepted
//-----
bool resizeWorkArea(QSize& newSize);

void importProjectData(QXmlStreamReader& reader);
void importShapesData(QXmlStreamReader& reader);

// インポートしたときシェイプのサイズを変更する
void resizeImportedShapes(QList<IwLayer*> importedLayers, QSize workAreaSize,
                          QSize childSize);

void setPreviewRange(int from, int to);
void toggleOnionSkinFrame(int frame);

void openInterpolationPopup(ShapePair* shape, QPoint pos);
void openMaskInfoPopup();
}  // namespace ProjectUtils

//=========================================
// 以下、Undoを登録
//---------------------------------------------------

class ChangeLayerProperyUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  ProjectUtils::LayerPropertyType m_type;

public:
  ChangeLayerProperyUndo(IwProject*, IwLayer*, ProjectUtils::LayerPropertyType);
  void doChange(bool isUndo);
  void undo();
  void redo();
};

class resizeWorkAreaUndo : public QUndoCommand {
  IwProject* m_project;
  QList<ShapePair*> m_shapePairs;
  QSize m_oldSize, m_newSize;
  bool m_keepShapeSize;

public:
  resizeWorkAreaUndo(IwProject*, QList<ShapePair*> shapePairs, QSize oldSize,
                     QSize newSize, bool keepShapeSize);

  void undo();
  void redo();
};

class SwapLayersUndo : public QUndoCommand {
  IwProject* m_project;
  int m_index1, m_index2;

public:
  SwapLayersUndo(IwProject*, int index1, int index2);

  void undo();
  void redo();
};

class ReorderLayersUndo : public QUndoCommand {
  IwProject* m_project;
  QList<IwLayer*> m_before, m_after;

public:
  ReorderLayersUndo(IwProject*, QList<IwLayer*>);

  void undo();
  void redo();
};

class RenameLayerUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  QString m_oldName;
  QString m_newName;

public:
  RenameLayerUndo(IwProject*, IwLayer*, QString& oldName, QString& newName);

  void undo();
  void redo();
};

class DeleteLayerUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  int m_index;

public:
  DeleteLayerUndo(IwProject*, int index);
  ~DeleteLayerUndo();

  void undo();
  void redo();
};

class CloneLayerUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  int m_index;

public:
  CloneLayerUndo(IwProject*, int index);
  ~CloneLayerUndo();

  void undo();
  void redo();
};

class SwapShapesUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  int m_index1, m_index2;

  void doSwap();

public:
  SwapShapesUndo(IwProject*, IwLayer*, int index1, int index2);

  void undo();
  void redo();
};

class MoveShapeUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  int m_index, m_dstIndex;

  void doMove(int from, int to);

public:
  MoveShapeUndo(IwProject*, IwLayer*, int index, int dstIndex);

  void undo();
  void redo();
};

class ReorderShapesUndo : public QUndoCommand {
  IwProject* m_project;

  QMap<IwLayer*, QList<QPair<ShapePair*, bool>>> m_before, m_after;

public:
  ReorderShapesUndo(IwProject*,
                    QMap<IwLayer*, QList<QPair<ShapePair*, bool>>> after);

  void undo();
  void redo();
};

class RenameShapePairUndo : public QUndoCommand {
  IwProject* m_project;
  ShapePair* m_shape;
  QString m_oldName;
  QString m_newName;

public:
  RenameShapePairUndo(IwProject*, ShapePair*, QString& oldName,
                      QString& newName);

  void undo();
  void redo();
};

class DeleteFormPointsUndo : public QUndoCommand {
  struct ShapeUndoInfo {
    ShapePair* shape;
    QMap<int, BezierPointList> beforeForm[2];
    QMap<int, CorrPointList> beforeCorr[2];
    QMap<int, BezierPointList> afterForm[2];
    QMap<int, CorrPointList> afterCorr[2];
  };

  IwProject* m_project;
  QList<ShapeUndoInfo> m_shapeInfo;
  // redoをされないようにするフラグ
  int m_firstFlag;

public:
  DeleteFormPointsUndo(IwProject*, const QList<ShapePair*>&);
  void storeAfterData();
  void undo();
  void redo();
};

class SwitchParentChildUndo : public QUndoCommand {
  IwProject* m_project;
  ShapePair* m_shape;

public:
  SwitchParentChildUndo(IwProject*, ShapePair*);
  void undo();
  void redo() { undo(); }
};

class SwitchLockUndo : public QUndoCommand {
  IwProject* m_project;
  OneShape m_shape;

public:
  SwitchLockUndo(IwProject*, OneShape);
  void undo();
  void redo() { undo(); }
};

class SwitchPinUndo : public QUndoCommand {
  IwProject* m_project;
  OneShape m_shape;

public:
  SwitchPinUndo(IwProject*, OneShape);
  void undo();
  void redo() { undo(); }
};

class SwitchShapeVisibilityUndo : public QUndoCommand {
  IwProject* m_project;
  ShapePair* m_shapePair;

public:
  SwitchShapeVisibilityUndo(IwProject*, ShapePair*);
  void undo();
  void redo() { undo(); }
};

class ImportProjectUndo : public QUndoCommand {
  IwProject* m_project;
  QList<IwLayer*> m_layers;
  bool m_onPush;

public:
  ImportProjectUndo(IwProject*, const QList<IwLayer*>&);
  ~ImportProjectUndo();
  void undo();
  void redo();
};

class ImportShapesUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  QList<ShapePair*> m_shapes;
  // キャッシュクリア用に親シェイプのセットを保持
  QSet<ShapePair*> m_parentShapes;
  bool m_onPush;

public:
  ImportShapesUndo(IwProject*, IwLayer*, const QList<ShapePair*>&);
  ~ImportShapesUndo();
  void undo();
  void redo();
};

class PreviewRangeUndo : public QUndoCommand {
  IwProject* m_project;
  struct PreviewRangeInfo {
    int start;
    int end;
    bool isSpecified;
  };
  PreviewRangeInfo m_before, m_after;
  bool m_onPush;

public:
  PreviewRangeUndo(IwProject* project);
  // 何か変わっていたら true を返す
  bool registerAfter();
  void undo();
  void redo();
};

class ToggleOnionUndo : public QUndoCommand {
  IwProject* m_project;
  int m_frame;

public:
  ToggleOnionUndo(IwProject* project, int frame);
  void undo();
  void redo();
};

#endif
