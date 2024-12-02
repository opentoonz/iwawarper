#ifndef PROJECTUTILS_H
#define PROJECTUTILS_H

//---------------------------------------
// ProjectUtils
// �V�[������̃R�}���h�{Undo
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
// IwLayer�ɑ΂��鑀��
//-----
// ���C�������̓���ւ�
void SwapLayers(int, int);
bool ReorderLayers(QList<IwLayer*>);
// ���l�[��
void RenameLayer(IwLayer*, QString&);
// ���C���폜
void DeleteLayer(int);

void CloneLayer(int);

// ���C�����̕ύX
enum LayerPropertyType { Visibility, Render, Lock };
void toggleLayerProperty(IwLayer*, LayerPropertyType type);

//-----
// ShapePair�ɑ΂��鑀��
//-----
// �V�F�C�v�����̓���ւ�
void SwapShapes(IwLayer*, int, int);
// �V�F�C�v�̈ړ�
void MoveShape(IwLayer*, int, int);

// ���C���[���܂����Ńh���b�O�ړ��B�Ō��bool��isParent�̏��
void ReorderShapes(QMap<IwLayer*, QList<QPair<ShapePair*, bool>>>);
// ���l�[��
void RenameShapePair(ShapePair*, QString&);
// �|�C���g����
void deleteFormPoints(QMap<ShapePair*, QList<int>>& pointList);

// �e�q��؂�ւ���
void switchParentChild(ShapePair*);

// ���b�N��؂�ւ���
void switchLock(OneShape);
// �s����؂�ւ���
void switchPin(OneShape);
// �\����\����؂�ւ���
void switchShapeVisibility(ShapePair*);

// �v���W�F�N�g���Ƀ��b�N���ꂽ�V�F�C�v�����邩�ǂ���
bool hasLockedShape();
// �S�Ẵ��b�N���ꂽ�V�F�C�v���A�����b�N
void unlockAllShapes();

//-----
// ���[�N�G���A�̃T�C�Y�ύX
// returns true when accepted
//-----
bool resizeWorkArea(QSize& newSize);

void importProjectData(QXmlStreamReader& reader);
void importShapesData(QXmlStreamReader& reader);

// �C���|�[�g�����Ƃ��V�F�C�v�̃T�C�Y��ύX����
void resizeImportedShapes(QList<IwLayer*> importedLayers, QSize workAreaSize,
                          QSize childSize);

void setPreviewRange(int from, int to);
void toggleOnionSkinFrame(int frame);

void openInterpolationPopup(ShapePair* shape, QPoint pos);
void openMaskInfoPopup();
}  // namespace ProjectUtils

//=========================================
// �ȉ��AUndo��o�^
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
  // redo������Ȃ��悤�ɂ���t���O
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
  // �L���b�V���N���A�p�ɐe�V�F�C�v�̃Z�b�g��ێ�
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
  // �����ς���Ă����� true ��Ԃ�
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
