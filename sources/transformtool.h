//--------------------------------------------------------
// Transform Tool
// �V�F�C�v�ό`�c�[��
//--------------------------------------------------------
#ifndef TRANSFORMTOOL_H
#define TRANSFORMTOOL_H

#include "iwtool.h"

#include "transformdragtool.h"

#include <QUndoCommand>
#include <QCoreApplication>

class IwShapePairSelection;
class IwProject;
class IwShape;
class QPointF;

class IwLayer;
struct OneShape;

//--------------------------------------------------------
class TransformTool : public IwTool {
  Q_DECLARE_TR_FUNCTIONS(TransformTool)

  bool m_ctrlPressed;

  IwShapePairSelection* m_shapePairSelection;

  TransformDragTool* m_dragTool;

  // �V�F�C�v��Rect�I��
  bool m_isRubberBanding;
  QPointF m_rubberStartPos;
  QPointF m_currentMousePos;

  // �ȉ��ATransformOption�̓��e
  bool m_scaleAroundCenter;
  bool m_rotateAroundCenter;
  bool m_shapeIndependentTransforms;
  // MultiFrame��ON�̂Ƃ��A���̃t���[���̕ό`�i��]/�L�k/�V�A�[�j��
  // ���S���A���ҏW���̃t���[���̒��S���g�����iOFF�̂Ƃ��j�A
  // ���ꂼ��̃t���[���ł̒��S���g�����B�iON�̂Ƃ��j
  bool m_frameIndependentTransforms;

  bool m_translateOnly;

public:
  TransformTool();
  ~TransformTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*);
  bool leftButtonDrag(const QPointF&, const QMouseEvent*);
  bool leftButtonUp(const QPointF&, const QMouseEvent*);
  void leftButtonDoubleClick(const QPointF&, const QMouseEvent*);

  bool keyDown(int key, bool ctrl, bool shift, bool alt);

  // �I������Ă���V�F�C�v�ɘg������ �n���h���ɂ͖��O������
  void draw();

  int getCursorId(const QMouseEvent*);

  // �n���h�����������`��
  void drawHandle(IwLayer* layer, OneShape shape, TransformHandleId handleId,
                  const QPointF& onePix, const QPointF& pos);
  // void drawHandle(IwProject* project, IwShape* shape,
  //				TransformHandleId handleId,
  //				QPointF & onePix, QPointF & pos);

  // 140110 iwasawa Ctrl�������Ƃ��Ӄh���b�O�ŃT�C�Y�ύX�̂��߂̕�
  void drawEdgeForResize(IwLayer* layer, OneShape shape,
                         TransformHandleId handleId, const QPointF& p1,
                         const QPointF& p2);

  // �ȉ��ATransformOption�̓��e�̑���
  bool IsScaleAroundCenter() { return m_scaleAroundCenter; }
  void setScaleAroundCenter(bool on) { m_scaleAroundCenter = on; }
  bool IsRotateAroundCenter() { return m_rotateAroundCenter; }
  void setRotateAroundCenter(bool on) { m_rotateAroundCenter = on; }
  bool IsShapeIndependentTransforms() { return m_shapeIndependentTransforms; }
  void setShapeIndependentTransforms(bool on) {
    m_shapeIndependentTransforms = on;
  }
  bool IsFrameIndependentTransforms() { return m_frameIndependentTransforms; }
  void setFrameIndependentTransforms(bool on) {
    m_frameIndependentTransforms = on;
  }

  bool isTranslateOnly() { return m_translateOnly; }
  void setTranslateOnly(bool on) { m_translateOnly = on; }

protected:
  // Ctrl+���L�[��0.25�s�N�Z�������ړ�������
  void arrowKeyMove(QPointF& delta);
};

//--------------------------------------------------------
// �ȉ��AUndo�R�}���h
//--------------------------------------------------------

//--------------------------------------------------------
// Ctrl+���L�[��0.25�s�N�Z�������ړ������� ��Undo
//--------------------------------------------------------
class ArrowKeyMoveUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;

  QList<OneShape> m_shapes;
  // QList<IwShape*> m_shapes;

  // �V�F�C�v���Ƃ̌��̌`��̃��X�g
  QList<BezierPointList> m_beforePoints;
  // �V�F�C�v���Ƃ̑����̌`��̃��X�g
  QList<BezierPointList> m_afterPoints;
  // ���̃V�F�C�v���L�[�t���[�����������ǂ���
  QList<bool> m_wasKeyFrame;
  int m_frame;

  // redo������Ȃ��悤�ɂ���t���O
  int m_firstFlag;

public:
  ArrowKeyMoveUndo(const QList<OneShape>& shapes,
                   QList<BezierPointList>& beforePoints,
                   QList<BezierPointList>& afterPoints,
                   QList<bool>& wasKeyFrame, IwProject* project, IwLayer* layer,
                   int frame);
  // ArrowKeyMoveUndo(	QList<IwShape*> & shapes,
  //					QList<BezierPointList> & beforePoints,
  //					QList<BezierPointList> & afterPoints,
  //					QList<bool> & wasKeyFrame,
  //					IwProject* project,
  //					int frame);
  void undo();
  void redo();
};

#endif
