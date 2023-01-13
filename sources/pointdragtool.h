//--------------------------------------------------------
// Reshape Tool�p�� �h���b�O�c�[��
// �R���g���[���|�C���g�ҏW�c�[��
//--------------------------------------------------------
#ifndef POINTDRAGTOOL_H
#define POINTDRAGTOOL_H

#include "shapepair.h"

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
  virtual void onClick(const QPointF &, const QMouseEvent *) = 0;
  virtual void onDrag(const QPointF &, const QMouseEvent *)  = 0;

  // Click������}�E�X�ʒu���ς���Ă��Ȃ����false��Ԃ��悤�ɂ���
  virtual bool onRelease(const QPointF &, const QMouseEvent *) = 0;

  virtual bool setSpecialShapeColor(OneShape) { return false; }
  virtual bool setSpecialGridColor(int, bool) { return false; }

  virtual void draw(const QPointF &onePixelLength) {}
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

  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
  bool onRelease(const QPointF &, const QMouseEvent *);

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

  void onClick(const QPointF &, const QMouseEvent *);
  void onDrag(const QPointF &, const QMouseEvent *);
  bool onRelease(const QPointF &, const QMouseEvent *);

  // PenTool�p�B�|�C���g�𑝂₵���Ɠ����ɐL�΂��n���h���́A
  // Ctrl�������ĐL�΂��Ă��邩�̂悤�ɂӂ�܂��B
  // ���Ȃ킿�A���Α��̃n���h�����_�Ώ̂ɐL�т�B
  void setIsInitial() { m_isInitial = true; }

  void calculateHandleSnap(const QPointF, QPointF &);
  void draw(const QPointF &onePixelLength) override;
};

//--------------------------------------------------------
#endif
