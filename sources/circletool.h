//--------------------------------------------------------
// Circle Tool
// �~�`�`��c�[��
// �쐬��͂��̃V�F�C�v���J�����g�ɂȂ�
// �J�����g�V�F�C�v�̕ό`���ł���iTransformTool�Ɠ����@�\�j
//--------------------------------------------------------
#ifndef CIRCLETOOL_H
#define CIRCLETOOL_H

#include "iwtool.h"
#include <QPointF>
#include <QRectF>
#include <QUndoCommand>
#include <QCoreApplication>

class IwProject;
class IwShape;

class ShapePair;
class IwLayer;

class CircleTool : public IwTool {
  Q_DECLARE_TR_FUNCTIONS(CircleTool)

  bool m_startDefined;
  QPointF m_startPos;
  QPointF m_endPos;

public:
  CircleTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonDrag(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonUp(const QPointF&, const QMouseEvent*) final override;
  void leftButtonDoubleClick(const QPointF&, const QMouseEvent*) final override;
  void draw() final override;

  int getCursorId(const QMouseEvent*) final override;
};

//---------------------------------------------------
// �ȉ��AUndo�R�}���h��o�^
//---------------------------------------------------

class CreateCircleShapeUndo : public QUndoCommand {
  IwProject* m_project;
  int m_index;
  IwShape* m_shape;

  ShapePair* m_shapePair;
  IwLayer* m_layer;

public:
  CreateCircleShapeUndo(QRectF& rect, IwProject* prj, IwLayer* layer);
  ~CreateCircleShapeUndo();

  void undo();
  void redo();
};

#endif