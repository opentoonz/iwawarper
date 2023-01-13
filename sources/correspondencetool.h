//--------------------------------------------------------
// Correspondence Tool
// �Ή��_�ҏW�c�[��
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
// �Ή��_�h���b�O�c�[��
//--------------------------------------------------------
class CorrDragTool {
  IwProject *m_project;
  OneShape m_shape;

  // �͂�ł���Ή��_�n���h��
  int m_corrPointId;
  // ���̃t���[�����Ή��_�L�[�t���[�����������ǂ���
  bool m_wasKeyFrame;
  // ���̑Ή��_���X�g
  CorrPointList m_orgCorrs;

  QPointF m_startPos;
  QPointF m_onePixLength;

  int m_snappedCpId;

public:
  CorrDragTool(OneShape shape, int corrPointId, const QPointF &onePix);
  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
  void onRelease(const QPointF &, const QMouseEvent *);
  OneShape shape() const { return m_shape; }
  int snappedCpId() const { return m_snappedCpId; }

  void doSnap(double &bezierPos, int frame, double rangeBefore = -1.,
              double rangeAfter = -1.);
};

//--------------------------------------------------------
// �@�c�[���{��
//--------------------------------------------------------

class CorrespondenceTool : public IwTool {
  Q_DECLARE_TR_FUNCTIONS(CorrespondenceTool)

  IwShapePairSelection *m_selection;

  CorrDragTool *m_dragTool;

  //+Alt�őΉ��_�̒ǉ�
  void addCorrPoint(const QPointF &, OneShape);
  bool m_ctrlPressed;

public:
  CorrespondenceTool();
  ~CorrespondenceTool();

  bool leftButtonDown(const QPointF &, const QMouseEvent *) override;
  bool leftButtonDrag(const QPointF &, const QMouseEvent *) override;
  bool leftButtonUp(const QPointF &, const QMouseEvent *) override;
  void leftButtonDoubleClick(const QPointF &, const QMouseEvent *);

  // �Ή��_�ƕ������ꂽ�Ή��V�F�C�v�A�V�F�C�v���m�̘A����\������
  void draw() override;

  int getCursorId(const QMouseEvent *) override;

  // �A�N�e�B�u���AJoin���Ă���V�F�C�v�̕Е��������I������Ă�����A
  // �����Е����I������
  void onActivate() override;
  // �A�N�e�B�u�ȑΉ��_�����Z�b�g����
  void onDeactivate() override;
};

//---------------------------------------------------
// �ȉ��AUndo�R�}���h��o�^
//--------------------------------------------------------
// �Ή��_�ړ���Undo
//--------------------------------------------------------
class CorrDragToolUndo : public QUndoCommand {
  IwProject *m_project;
  OneShape m_shape;

  // ���̑Ή��_
  CorrPointList m_beforeCorrs;
  // �����̑Ή��_
  CorrPointList m_afterCorrs;
  // ���̃t���[�����L�[�t���[�����������ǂ���
  bool m_wasKeyFrame;

  int m_frame;
  // redo����Ȃ��悤�ɂ���t���O
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
// �Ή��_�ǉ���Undo
//--------------------------------------------------------
class AddCorrPointUndo : public QUndoCommand {
  IwProject *m_project;

  OneShape m_shape;

  // ���̑Ή��_�f�[�^
  QMap<int, CorrPointList> m_beforeData;
  // �����̑Ή��_�f�[�^
  QMap<int, CorrPointList> m_afterData;
  // Join����
  OneShape m_partnerShape;
  // IwShape*	m_partnerShape;
  // Join���� ���̑Ή��_�f�[�^
  QMap<int, CorrPointList> m_partnerBeforeData;
  // Join���� �����̑Ή��_�f�[�^
  QMap<int, CorrPointList> m_partnerAfterData;

  // redo������Ȃ��悤�ɂ���t���O
  int m_firstFlag;

public:
  AddCorrPointUndo(OneShape shape, IwProject *project);

  void setAfterData();
  void undo();
  void redo();
};

#endif
