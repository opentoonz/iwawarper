//-----------------------------------------------------------------------------
// IwTool
// �c�[���̃N���X
//-----------------------------------------------------------------------------
#ifndef IWTOOL_H
#define IWTOOL_H

#include "cursors.h"

#include "shapepair.h"

#include <QString>
#include <QObject>

class QPointF;
class QMouseEvent;

class SceneViewer;
class IwShape;

class QMenu;

class IwTool : public QObject {
  QString m_name;

protected:
  // ���݂̐e��SceneViewer��o�^
  SceneViewer *m_viewer;

public:
  IwTool(QString toolName);

  // �c�[��������c�[�����擾
  static IwTool *getTool(QString toolName);

  // ���O��Ԃ�
  QString getName() const { return m_name; }

  // Viewer��update�������ꍇ��true��Ԃ�
  virtual bool leftButtonDown(const QPointF &, const QMouseEvent *) {
    return false;
  }
  virtual bool leftButtonDrag(const QPointF &, const QMouseEvent *) {
    return false;
  }
  virtual bool leftButtonUp(const QPointF &, const QMouseEvent *) {
    return false;
  }

  virtual void leftButtonDoubleClick(const QPointF &, const QMouseEvent *) {}

  virtual bool rightButtonDown(const QPointF &, const QMouseEvent *,
                               bool & /*canOpenMenu*/, QMenu & /*menu*/) {
    return false;
  }

  virtual bool mouseMove(const QPointF &, const QMouseEvent *) { return false; }

  // �����V���[�g�J�b�g�L�[�������ꂽ�Ƃ��̉����B
  //  Tool�ŃV���[�g�J�b�g�����������ꍇ��true���Ԃ�
  virtual bool keyDown(int /*key*/, bool /*ctrl*/, bool /*shift*/,
                       bool /*alt*/) {
    return false;
  }

  virtual int getCursorId(const QMouseEvent *) {
    return ToolCursor::ArrowCursor;
  };

  virtual void draw() {}

  virtual void onDeactivate();
  virtual void onActivate() {}

  // SceneViewer��o�^
  void setViewer(SceneViewer *viewer) { m_viewer = viewer; }
  // SceneViewer�����o��
  SceneViewer *getViewer() { return m_viewer; }

  // �w��V�F�C�v�̑Ή��_��`�悷��
  void drawCorrLine(OneShape);

  // Join���Ă���V�F�C�v���q���Ή�����`��
  void drawJoinLine(ShapePair *);

  virtual bool setSpecialShapeColor(OneShape) { return false; }
  virtual bool setSpecialGridColor(int /*gId*/, bool /*isVertical*/) {
    return false;
  }
};

#endif