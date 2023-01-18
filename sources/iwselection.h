//-----------------------------------------------------------------------------
// IwSelection
// �I������肭�肷��N���X
//-----------------------------------------------------------------------------
#ifndef IWSELECTION_H
#define IWSELECTION_H

#include "menubarcommand.h"

class QMenu;
class QWidget;

class IwSelection : public QObject {
  Q_OBJECT

public:
  IwSelection();
  virtual ~IwSelection();

  virtual void enableCommands() {}

  void enableCommand(CommandId cmdId, CommandHandlerInterface *handler);

  template <class T>
  inline void enableCommand(T *target, CommandId cmdId, void (T::*method)()) {
    enableCommand(cmdId, new CommandHandlerHelper<T>(target, method));
  }

  template <class T, typename R>
  inline void enableCommand(T *target, CommandId cmdId, void (T::*method)(R),
                            R value) {
    enableCommand(cmdId,
                  new CommandHandlerHelper2<T, R>(target, method, value));
  }

  // �J�����g�̑I��������ɂ���
  void makeCurrent();
  // �J�����g�̑I�������ꂾ�����ꍇ�A�I������������
  void makeNotCurrent();
  // �J�����g�̑I����Ԃ��B���������҂��͊֌W�Ȃ��B
  static IwSelection *getCurrent();
  // �����̑I�����J�����g�Ɋi�[�B���������҂��͊֌W�Ȃ��B
  static void setCurrent(IwSelection *selection);

  virtual bool isEmpty() const = 0;
  virtual void selectNone()    = 0;

  virtual bool addMenuActions(QMenu * /*menu*/) { return false; }
  void addMenuAction(QMenu *menu, CommandId cmdId);

protected slots:
  // �v���W�F�N�g���؂�ւ������A�I������������
  void onProjectSwitched();
};

#endif
