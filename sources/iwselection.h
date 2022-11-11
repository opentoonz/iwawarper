//-----------------------------------------------------------------------------
// IwSelection
// 選択をやりくりするクラス
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

  // カレントの選択をこれにする
  void makeCurrent();
  // カレントの選択がこれだった場合、選択を解除する
  void makeNotCurrent();
  // カレントの選択を返す。自分が何者かは関係ない。
  static IwSelection *getCurrent();
  // 引数の選択をカレントに格納。自分が何者かは関係ない。
  static void setCurrent(IwSelection *selection);

  virtual bool isEmpty() const = 0;
  virtual void selectNone()    = 0;

  virtual bool addMenuActions(QMenu *menu) { return false; }
  void addMenuAction(QMenu *menu, CommandId cmdId);

protected slots:
  // プロジェクトが切り替わったら、選択を解除する
  void onProjectSwitched();
};

#endif
