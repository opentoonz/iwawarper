//--------------------------------------------------------
// Transform Tool�p �� �h���b�O�c�[��
// �V�F�C�v�ό`�c�[��
//--------------------------------------------------------
#ifndef TRANSFORMDRAGTOOL_H
#define TRANSFORMDRAGTOOL_H

#include <QPointF>
#include <QList>

#include "shapepair.h"

#include <QUndoCommand>

class QMouseEvent;
class IwProject;
class TransformTool;

class IwLayer;

enum TransformHandleId {
  Handle_None = 0,
  Handle_BottomRight,
  Handle_Right,
  Handle_TopRight,
  Handle_Top,
  Handle_TopLeft,
  Handle_Left,
  Handle_BottomLeft,
  Handle_Bottom,
  // Ctrl�������Ƃ��Ӄh���b�O�ŃT�C�Y�ύX
  Handle_RightEdge,
  Handle_TopEdge,
  Handle_LeftEdge,
  Handle_BottomEdge
};

//--------------------------------------------------------

class TransformDragTool {
protected:
  IwProject *m_project;
  IwLayer *m_layer;
  QPointF m_startPos;
  QList<OneShape> m_shapes;

  // ���̌`��B �V�F�C�v���Ƃ� (�t���[��,�`����)�̃}�b�v������
  QList<QMap<int, BezierPointList>> m_orgPoints;
  // ���̃V�F�C�v���L�[�t���[�����������ǂ���
  QList<bool> m_wasKeyFrame;

  TransformHandleId m_handleId;

  OneShape m_grabbingShape;

  // TransformOption�̏��𓾂邽�߂ɁATransformTool�ւ̃|�C���^���T���Ă���
  TransformTool *m_tool;

  // ���񂾃n���h���̔��Α��̃n���h���ʒu��Ԃ�
  QPointF getOppositeHandlePos(OneShape shape = 0, int frame = -1);

  // �V�F�C�v�̒��S�ʒu��Ԃ�
  QPointF getCenterPos(OneShape shape = 0, int frame = -1);

public:
  TransformDragTool(OneShape, TransformHandleId, const QList<OneShape> &);
  virtual void onClick(const QPointF &, const QMouseEvent *) = 0;
  virtual void onDrag(const QPointF &, const QMouseEvent *)  = 0;
  virtual void onRelease(const QPointF &, const QMouseEvent *);
};

//--------------------------------------------------------
// �n���h��Ctrl�N���b�N   �� ��]���[�h (�{Shift��45�x����)
//--------------------------------------------------------

class RotateDragTool : public TransformDragTool {
  // ��]�̊�ʒu
  QPointF m_anchorPos;
  // ��]�Ɏg�����S�_�̃��X�g
  QList<QMap<int, QPointF>> m_centerPos;

  double m_initialAngle;

  QPointF m_onePixLength;

public:
  RotateDragTool(OneShape, TransformHandleId, const QList<OneShape> &,
                 const QPointF &onePix);
  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
};

//--------------------------------------------------------
// �c���n���h��Alt�N���b�N�@  �� Shear�ό`���[�h�i�{Shift�ŕ��s�ɕό`�j
//--------------------------------------------------------

class ShearDragTool : public TransformDragTool {
  // �V�A�[�ό`�̊�ʒu
  QList<QMap<int, QPointF>> m_anchorPos;
  bool m_isVertical;
  bool m_isInv;

public:
  ShearDragTool(OneShape shape, TransformHandleId, const QList<OneShape> &);
  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
};

//--------------------------------------------------------
// �΂߃n���h��Alt�N���b�N�@  �� ��`�ό`���[�h�i�{Shift�őΏ̕ό`�j
//--------------------------------------------------------

class TrapezoidDragTool : public TransformDragTool {
  // ��`�ό`�̊�ƂȂ�o�E���f�B���O�{�b�N�X
  QList<QMap<int, QRectF>> m_initialBox;

public:
  TrapezoidDragTool(OneShape shape, TransformHandleId, const QList<OneShape> &);

  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);

  // �S�̕ψڃx�N�g���̐��`��Ԃ𑫂����Ƃňړ�������
  QPointF map(const QRectF &bBox, const QPointF &bottomRightVec,
              const QPointF &topRightVec, const QPointF &topLeftVec,
              const QPointF &bottomLeftVec, QPointF &targetPoint);
};

//--------------------------------------------------------
// �n���h�����ʂ̃N���b�N �� �g��^�k�����[�h�i�{Shift�ŏc����Œ�j
//--------------------------------------------------------

class ScaleDragTool : public TransformDragTool {
  // �g��k���̊�ʒu
  QPointF m_anchorPos;
  // �g��k���̒��S�ʒu
  QList<QMap<int, QPointF>> m_scaleCenter;

  bool m_scaleHorizontal;
  bool m_scaleVertical;

public:
  ScaleDragTool(OneShape shape, TransformHandleId, const QList<OneShape> &);
  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
};

//--------------------------------------------------------
// �V�F�C�v�̃n���h���ȊO���N���b�N  �� �ړ����[�h�i�{Shift�ŕ��s�ړ��j
//--------------------------------------------------------

class TranslateDragTool : public TransformDragTool {
public:
  TranslateDragTool(OneShape shape, TransformHandleId, const QList<OneShape> &);
  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
};

//---------------------------------------------------
// �ȉ��AUndo�R�}���h
//---------------------------------------------------
class TransformDragToolUndo : public QUndoCommand {
  IwProject *m_project;

  QList<OneShape> m_shapes;

  // �V�F�C�v���Ƃ́A�ҏW���ꂽ�t���[�����Ƃ̌��̌`��̃��X�g
  QList<QMap<int, BezierPointList>> m_beforePoints;
  // �V�F�C�v���Ƃ́A�ҏW���ꂽ�t���[�����Ƃ̑����̌`��̃��X�g
  QList<QMap<int, BezierPointList>> m_afterPoints;
  // ���̃V�F�C�v���L�[�t���[�����������ǂ���
  QList<bool> m_wasKeyFrame;

  int m_frame;

  // redo������Ȃ��悤�ɂ���t���O
  bool m_firstFlag;

public:
  TransformDragToolUndo(QList<OneShape> &shapes,
                        QList<QMap<int, BezierPointList>> &beforePoints,
                        QList<QMap<int, BezierPointList>> &afterPoints,
                        QList<bool> &wasKeyFrame, IwProject *project,
                        int frame);
  ~TransformDragToolUndo();
  void undo();
  void redo();
};

#endif
