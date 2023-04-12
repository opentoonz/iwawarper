//--------------------------------------------------------
// Reshape Tool�p�� �h���b�O�c�[��
// �R���g���[���|�C���g�ҏW�c�[��
//--------------------------------------------------------
#ifndef POINTDRAGTOOL_H
#define POINTDRAGTOOL_H

#include "shapepair.h"
#include "transformtool.h"

#include <QPointF>
#include <QList>
#include <QSet>

class IwProject;
class IwLayer;
class QMouseEvent;

//--------------------------------------------------------

class PointDragTool {
protected:
  IwProject *m_project;
  IwLayer *m_layer;

  QPointF m_startPos;

  // PenTool�p
  bool m_undoEnabled;

public:
  PointDragTool();
  virtual ~PointDragTool(){};
  virtual void onClick(const QPointF &, const QMouseEvent *) = 0;
  virtual void onDrag(const QPointF &, const QMouseEvent *)  = 0;

  // Click������}�E�X�ʒu���ς���Ă��Ȃ����false��Ԃ��悤�ɂ���
  virtual bool onRelease(const QPointF &, const QMouseEvent *) = 0;

  virtual bool setSpecialShapeColor(OneShape) { return false; }
  virtual bool setSpecialGridColor(int, bool) { return false; }

  virtual void draw(const QPointF & /*onePixelLength*/) {}
};

//--------------------------------------------------------
//--------------------------------------------------------
// ���s�ړ��c�[��
//--------------------------------------------------------
class TranslatePointDragTool : public PointDragTool {
  QList<QList<int>>
      m_pointsInShapes;  // �I���|�C���g���A�e�V�F�C�v���Ƃ̃��X�g�ɐU�蕪���Ċi�[

  QList<OneShape> m_shapes;

  // ���̌`��
  QList<QMap<int, BezierPointList>> m_orgPoints;
  // ���̃V�F�C�v���L�[�t���[�����������ǂ���
  QList<bool> m_wasKeyFrame;

  // ������ł���|�C���g�̌��̈ʒu
  QPointF m_grabbedPointOrgPos;

  QPointF m_onePixLength;

  OneShape m_snapTarget;

  int m_snapHGrid, m_snapVGrid;

  int m_pointIndex;
  OneShape m_shape;
  bool m_handleSnapped;

public:
  TranslatePointDragTool(const QSet<int> &, QPointF &grabbedPointOrgPos,
                         const QPointF &onePix, OneShape shape, int pointIndex);

  // PenTool�p
  TranslatePointDragTool(OneShape, QList<int>, const QPointF &onePix);

  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
  bool onRelease(const QPointF &, const QMouseEvent *) final override;

  int calculateSnap(QPointF &);

  bool setSpecialShapeColor(OneShape) override;
  bool setSpecialGridColor(int, bool) override;

  void draw(const QPointF &onePixelLength) override;
};

//--------------------------------------------------------
//--------------------------------------------------------
// �n���h������c�[��
//--------------------------------------------------------

class TranslateHandleDragTool : public PointDragTool {
  int m_pointIndex;
  int m_handleIndex;

  OneShape m_shape;
  // IwShape* m_shape;
  BezierPointList m_orgPoints;
  bool m_wasKeyFrame;

  QPointF m_onePixLength;

  struct CP {
    OneShape shape;
    int pointIndex;
  };
  QList<CP> m_snapCandidates;
  bool m_isHandleSnapped;

  // PenTool�p�B�|�C���g�𑝂₵���Ɠ����ɐL�΂��n���h���́A
  // Ctrl�������ĐL�΂��Ă��邩�̂悤�ɂӂ�܂��B
  // ���Ȃ킿�A���Α��̃n���h�����_�Ώ̂ɐL�т�B
  bool m_isInitial;

public:
  TranslateHandleDragTool(int, const QPointF &);

  // PenTool�p
  TranslateHandleDragTool(OneShape, int, const QPointF &);
  TranslateHandleDragTool(OneShape shape, int pointIndex, int handleIndex,
                          const QPointF &onePix);

  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
  bool onRelease(const QPointF &, const QMouseEvent *) final override;

  // PenTool�p�B�|�C���g�𑝂₵���Ɠ����ɐL�΂��n���h���́A
  // Ctrl�������ĐL�΂��Ă��邩�̂悤�ɂӂ�܂��B
  // ���Ȃ킿�A���Α��̃n���h�����_�Ώ̂ɐL�т�B
  void setIsInitial() { m_isInitial = true; }

  void calculateHandleSnap(const QPointF, QPointF &);
  void draw(const QPointF &onePixelLength) override;
};

//--------------------------------------------------------

//--------------------------------------------------------

class ActivePointsDragTool : public PointDragTool {
protected:
  IwProject *m_project;
  IwLayer *m_layer;
  QPointF m_startPos;

  QList<QList<int>>
      m_pointsInShapes;  // �I���|�C���g���A�e�V�F�C�v���Ƃ̃��X�g�ɐU�蕪���Ċi�[

  QList<OneShape> m_shapes;

  // ���̌`��
  QList<BezierPointList> m_orgPoints;
  // ���̃V�F�C�v���L�[�t���[�����������ǂ���
  QList<bool> m_wasKeyFrame;

  TransformHandleId m_handleId;
  QRectF m_handleRect;

  // ���񂾃n���h���̔��Α��̃n���h���ʒu��Ԃ�
  QPointF getOppositeHandlePos();

  // �V�F�C�v�̒��S�ʒu��Ԃ�
  QPointF getCenterPos() { return m_handleRect.center(); }

public:
  ActivePointsDragTool(TransformHandleId, const QSet<int> &, const QRectF &);
  virtual ~ActivePointsDragTool() {}
  virtual void onClick(const QPointF &, const QMouseEvent *) = 0;
  virtual void onDrag(const QPointF &, const QMouseEvent *)  = 0;
  virtual bool onRelease(const QPointF &, const QMouseEvent *);
};

//--------------------------------------------------------
// �n���h��Ctrl�N���b�N   �� ��]���[�h (�{Shift��45�x����)
//--------------------------------------------------------

class ActivePointsRotateDragTool : public ActivePointsDragTool {
  // ��]�̊�ʒu
  QPointF m_centerPos;
  double m_initialAngle;

  QPointF m_onePixLength;

public:
  ActivePointsRotateDragTool(TransformHandleId, const QSet<int> &,
                             const QRectF &, const QPointF &onePix);
  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
};

//--------------------------------------------------------
// �c���n���h��Alt�N���b�N�@  �� Shear�ό`���[�h�i�{Shift�ŕ��s�ɕό`�j
//--------------------------------------------------------

class ActivePointsShearDragTool : public ActivePointsDragTool {
  // �V�A�[�ό`�̊�ʒu
  QPointF m_anchorPos;
  bool m_isVertical;
  bool m_isInv;

public:
  ActivePointsShearDragTool(TransformHandleId, const QSet<int> &,
                            const QRectF &);
  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
};

//--------------------------------------------------------
// �΂߃n���h��Alt�N���b�N�@  �� ��`�ό`���[�h�i�{Shift�őΏ̕ό`�j
//--------------------------------------------------------

class ActivePointsTrapezoidDragTool : public ActivePointsDragTool {
  // ��`�ό`�̊�ƂȂ�o�E���f�B���O�{�b�N�X
  QRectF m_initialBox;

public:
  ActivePointsTrapezoidDragTool(TransformHandleId, const QSet<int> &,
                                const QRectF &);

  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;

  // �S�̕ψڃx�N�g���̐��`��Ԃ𑫂����Ƃňړ�������
  QPointF map(const QRectF &bBox, const QPointF &bottomRightVec,
              const QPointF &topRightVec, const QPointF &topLeftVec,
              const QPointF &bottomLeftVec, QPointF &targetPoint);
};

//--------------------------------------------------------
// �n���h�����ʂ̃N���b�N �� �g��^�k�����[�h�i�{Shift�ŏc����Œ�j
//--------------------------------------------------------

class ActivePointsScaleDragTool : public ActivePointsDragTool {
  // �g��k���̊�ʒu
  QPointF m_centerPos;

  bool m_scaleHorizontal;
  bool m_scaleVertical;

public:
  ActivePointsScaleDragTool(TransformHandleId, const QSet<int> &,
                            const QRectF &);
  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
};

//--------------------------------------------------------
// �V�F�C�v�̃n���h���ȊO���N���b�N  �� �ړ����[�h�i�{Shift�ŕ��s�ړ��j
//--------------------------------------------------------

class ActivePointsTranslateDragTool : public ActivePointsDragTool {
public:
  ActivePointsTranslateDragTool(TransformHandleId, const QSet<int> &,
                                const QRectF &);
  void onClick(const QPointF &, const QMouseEvent *) final override;
  void onDrag(const QPointF &, const QMouseEvent *) final override;
};

//---------------------------------------------------
// �ȉ��AUndo�R�}���h
//---------------------------------------------------
class ActivePointsDragToolUndo : public QUndoCommand {
  IwProject *m_project;

  QList<OneShape> m_shapes;

  // �V�F�C�v���Ƃ̌��̌`��̃��X�g
  QList<BezierPointList> m_beforePoints;
  // �V�F�C�v���Ƃ̑����̌`��̃��X�g
  QList<BezierPointList> m_afterPoints;
  // ���̃V�F�C�v���L�[�t���[�����������ǂ���
  QList<bool> m_wasKeyFrame;

  int m_frame;

  // redo������Ȃ��悤�ɂ���t���O
  bool m_firstFlag;

public:
  ActivePointsDragToolUndo(QList<OneShape> &shapes,
                           QList<BezierPointList> &beforePoints,
                           QList<BezierPointList> &afterPoints,
                           QList<bool> &wasKeyFrame, IwProject *project,
                           int frame);
  ~ActivePointsDragToolUndo();
  void undo();
  void redo();
};

#endif
