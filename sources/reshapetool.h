//--------------------------------------------------------
// Reshape Tool
// �R���g���[���|�C���g�ҏW�c�[��
//--------------------------------------------------------
#ifndef RESHAPETOOL_H
#define RESHAPETOOL_H

#include "iwtool.h"
#include "shapepair.h"

#include <QUndoCommand>
#include <QCoreApplication>

class IwShapePairSelection;
class PointDragTool;
class ReshapeToolContextMenu;
class IwLayer;
class SceneViewer;

class ReshapeTool : public IwTool {
  Q_DECLARE_TR_FUNCTIONS(ReshapeTool)

  IwShapePairSelection* m_selection;
  PointDragTool* m_dragTool;

  // �|�C���g��Rect�I��
  bool m_isRubberBanding;
  QPointF m_rubberStartPos;
  QPointF m_currentMousePos;

  bool isRubberBandValid();
  // �N���b�N�ʒu����A����ΏۂƂȂ�V�F�C�v�^�n���h���𓾂�
  OneShape getClickedShapeAndIndex(int& pointIndex, int& handleId,
                                   const QMouseEvent* e);

  // show handles for transforming active points
  bool m_transformHandles;
  bool m_ctrlPressed;
  QRectF m_handleRect;

public:
  ReshapeTool();
  ~ReshapeTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonDrag(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonUp(const QPointF&, const QMouseEvent*) final override;
  void leftButtonDoubleClick(const QPointF&, const QMouseEvent*) final override;

  // �E�N���b�N���j���[
  bool rightButtonDown(const QPointF&, const QMouseEvent*, bool& canOpenMenu,
                       QMenu& menu) final override;
  // bool rightButtonDown(const QPointF &, const QMouseEvent*);

  bool keyDown(int key, bool ctrl, bool shift, bool alt) final override;

  // �A�N�e�B�u�ȃV�F�C�v�ɃR���g���[���|�C���g��`��
  // �I���|�C���g�͐F��ς��A�n���h�����`��
  void draw() final override;
  int getCursorId(const QMouseEvent*) final override;

  // �R���g���[���|�C���g��`�悷��B�n���h�����t���邩�ǂ����������Ō��߂�
  //  PenTool�ł��g�������̂ŁAstatic������
  static void drawControlPoint(
      SceneViewer* viewer, OneShape shape, BezierPointList& pointList,
      int cpIndex, bool drawHandles, const QPointF& onePix,
      int specialShapeIndex =
          0,  // PenTool�Ŏg���BProject�ɑ����Ă��Ȃ��V�F�C�v�Ɏg��
      bool fillPoint = false, QColor fillColor = QColor());
  // static void drawControlPoint(IwShape* shape,
  //							BezierPointList&
  // pointList, 							int
  // cpIndex, bool
  // drawHandles, 							QPointF
  // & onePix, int specialShapeIndex =
  // 0);//PenTool�Ŏg���BProject�ɑ����Ă��Ȃ��V�F�C�v�Ɏg��

  // �n���h���p�̉~��`��
  //  PenTool�ł��g�������̂ŁAstatic������
  static void drawCircle(SceneViewer* viewer);

  // �A�N�e�B�u�ȃV�F�C�v��Alt�N���b�N�����ꍇ�A�R���g���[���|�C���g��ǉ�����
  void addControlPoint(OneShape shape, const QPointF& pos);
  // void addControlPoint(IwShape* shape, const QPointF & pos);

  bool setSpecialShapeColor(OneShape) override;
  bool setSpecialGridColor(int gId, bool isVertical) override;

  bool isTransformHandlesEnabled() { return m_transformHandles; }
  void enableTransformHandles(bool on) { m_transformHandles = on; }

protected:
  // Ctrl+���L�[��0.25�s�N�Z�������ړ�������
  void arrowKeyMove(QPointF& delta);
};

//---------------------------------------------------
// �ȉ��AUndo�R�}���h��o�^
//---------------------------------------------------

class AddControlPointUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;
  OneShape m_shape;
  // IwShape*	m_shape;

  // ���̌`��f�[�^
  QMap<int, BezierPointList> m_beforeFormData[2];
  // ���̑Ή��_�f�[�^
  QMap<int, CorrPointList> m_beforeCorrData[2];
  // �����̌`��f�[�^
  QMap<int, BezierPointList> m_afterFormData[2];
  // �����̑Ή��_�f�[�^
  QMap<int, CorrPointList> m_afterCorrData[2];

  // redo������Ȃ��悤�ɂ���t���O
  int m_firstFlag;

public:
  AddControlPointUndo(const QMap<int, BezierPointList>& beforeFormDataFrom,
                      const QMap<int, CorrPointList>& beforeCorrDataFrom,
                      const QMap<int, BezierPointList>& beforeFormDataTo,
                      const QMap<int, CorrPointList>& beforeCorrDataTo,
                      OneShape shape, IwProject* project, IwLayer* layer);
  // AddControlPointUndo(	QMap<int, BezierPointList> & beforeFormData,
  //						QMap<int, CorrPointList> &
  // beforeCorrData, 						IwShape* shape,
  // IwProject* project);

  void setAfterData(const QMap<int, BezierPointList>& afterFormDataFrom,
                    const QMap<int, CorrPointList>& afterCorrDataFrom,
                    const QMap<int, BezierPointList>& afterFormDataTo,
                    const QMap<int, CorrPointList>& afterCorrDataTo);
  void undo();
  void redo();
};

#endif
