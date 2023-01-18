//--------------------------------------------------------
// Pen Tool
// �y���c�[��
//--------------------------------------------------------
#ifndef PENTOOL_H
#define PENTOOL_H

#include "iwtool.h"

#include <QUndoCommand>
#include <QCoreApplication>

class PointDragTool;
class ShapePair;
class IwLayer;

class PenTool : public IwTool {
  Q_DECLARE_TR_FUNCTIONS(PenTool)

  ShapePair* m_editingShape;

  // ���݃A�N�e�B�u�ȓ_�B10�̈ʂ��|�C���g��ID�A
  //  1�̈ʂ�1:CP 2:First 3:Second
  QList<int> m_activePoints;

  PointDragTool* m_dragTool;

  // m_editingShape��Viewer��ɕ`���Ƃ��ɕt���閼�O�́A
  // Shape�̃C���f�b�N�X�̑���Ɏg�������B
  // 999 **** + �@�ƂȂ�B
  //		**** : �R���g���[���|�C���g�̃C���f�b�N�X
  //		+ :
  // 1�Ȃ�R���g���[���|�C���g�A2�Ȃ�FirstHandle�A3�Ȃ�SecondHandle
  static const int EDITING_SHAPE_ID = 999;

public:
  PenTool();
  ~PenTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonDrag(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonUp(const QPointF&, const QMouseEvent*) final override;
  bool rightButtonDown(const QPointF&, const QMouseEvent*, bool& canOpenMenu,
                       QMenu& menu) final override;

  // �c�[�������̂Ƃ��A�`�撆�̃V�F�C�v����������m�肷��
  void onDeactivate() final override;

  void draw() final override;

  int getCursorId(const QMouseEvent* e) final override;

  // �|�C���g���A�N�e�B�u���ǂ�����Ԃ�
  bool isPointActive(int index) {
    for (int p = 0; p < m_activePoints.size(); p++) {
      if ((int)(m_activePoints[p] / 10) == index) return true;
    }

    return false;
  }

  // �Ή��_���X�V����
  void updateCorrPoints(int fromTo);

  // �V�F�C�v������������ ���܂���������A
  // �E�N���b�N���j���[�͏o���Ȃ��̂�false��Ԃ�
  bool finishShape();

  bool setSpecialShapeColor(OneShape) override;
  bool setSpecialGridColor(int gId, bool isVertical) override;
};

//---------------------------------------------------
// �ȉ��AUndo�R�}���h��o�^
//---------------------------------------------------

class CreatePenShapeUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  int m_index;
  ShapePair* m_shape;

public:
  CreatePenShapeUndo(ShapePair* shape, IwProject* prj, IwLayer* layer);
  ~CreatePenShapeUndo();

  void undo();
  void redo();
};

#endif