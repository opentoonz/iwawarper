//---------------------------------
// Shape Options Dialog
// �I�𒆂̃V�F�C�v�̃v���p�e�B��\������_�C�A���O
// ���[�_���ł͂Ȃ�
//---------------------------------
#ifndef SHAPEOPTIONSDIALOG_H
#define SHAPEOPTIONSDIALOG_H

#include "iwdialog.h"

#include <QUndoCommand>
#include <QList>

#include "shapepair.h"

// class IwShape;
class QSlider;
class QLineEdit;
class IwSelection;
class IwProject;

class ShapeOptionsDialog : public IwDialog {
  Q_OBJECT

  // ���ݑI������Ă���V�F�C�v
  QList<OneShape> m_selectedShapes;
  // �I�𒆂̑Ή��_
  QPair<OneShape, int> m_activeCorrPoint;

  // �Ή��_�̖��x�X���C�_
  QSlider* m_edgeDensitySlider;
  QLineEdit* m_edgeDensityEdit;

  // �E�F�C�g�̃X���C�_(�傫���قǃ��b�V���������񂹂�)
  QSlider* m_weightSlider;
  QLineEdit* m_weightEdit;

public:
  ShapeOptionsDialog();

  void setDensity(int value);
  void setWeight(double weight);

protected:
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);

protected slots:

  void onSelectionSwitched(IwSelection*, IwSelection*);
  void onSelectionChanged(IwSelection*);
  void onViewFrameChanged();

  void onEdgeDensitySliderMoved(int val);
  void onEdgeDensityEditEdited();
  void onWeightSliderMoved(int val);
  void onWeightEditEdited();
};

//-------------------------------------
// �ȉ��AUndo�R�}���h
//-------------------------------------

class ChangeEdgeDensityUndo : public QUndoCommand {
public:
  struct EdgeDensityInfo {
    OneShape shape;
    int beforeED;
  };

private:
  IwProject* m_project;
  QList<EdgeDensityInfo> m_info;
  int m_afterED;

public:
  ChangeEdgeDensityUndo(QList<EdgeDensityInfo>& info, int afterED,
                        IwProject* project);

  void undo();
  void redo();
};

class ChangeWeightUndo : public QUndoCommand {
  IwProject* m_project;
  QList<QPair<OneShape, int>>
      m_targetCorrs;  // �V�F�C�v�S�̂̏ꍇ��second��-1������
  QList<double> m_beforeWeights;
  double m_afterWeight;
  int m_frame;
  QList<OneShape> m_wasKeyShapes;

public:
  ChangeWeightUndo(QList<QPair<OneShape, int>>& targets, double afterWeight,
                   IwProject* project);

  void undo();
  void redo();
};
#endif