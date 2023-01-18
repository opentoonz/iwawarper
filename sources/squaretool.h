//--------------------------------------------------------
// Square Tool
// ��`�`��c�[��
// �쐬��͂��̃V�F�C�v���J�����g�ɂȂ�
// �J�����g�V�F�C�v�̕ό`���ł���iTransformTool�Ɠ����@�\�j
//--------------------------------------------------------
#ifndef SQUARETOOL_H
#define SQUARETOOL_H

#include "iwtool.h"
#include <QPointF>
#include <QRectF>
#include <QUndoCommand>
#include <QCoreApplication>

class IwProject;
class IwShape;

class ShapePair;
class IwLayer;

class SquareTool : public IwTool {
  Q_DECLARE_TR_FUNCTIONS(SquareTool)

  bool m_startDefined;
  QPointF m_startPos;
  QPointF m_endPos;

public:
  SquareTool();

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

class CreateSquareShapeUndo : public QUndoCommand {
  IwProject* m_project;
  int m_index;
  IwShape* m_shape;

  ShapePair* m_shapePair;
  IwLayer* m_layer;

public:
  CreateSquareShapeUndo(QRectF& rect, IwProject* prj, IwLayer* layer);
  ~CreateSquareShapeUndo();

  void undo();
  void redo();
};

#endif