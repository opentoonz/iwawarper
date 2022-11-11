/*
 CommandManager コマンドの一元管理のためのクラス
*/

#ifndef MENUBARCOMMAND_H
#define MENUBARCOMMAND_H

#include <string>
#include <map>

#include <QAction>

//
// base class
//
class CommandHandlerInterface {
public:
  virtual ~CommandHandlerInterface() {}
  virtual void execute() = 0;
};

typedef const char *CommandId;

// コマンドのタイプ。ショートカットのリスト
enum CommandType {
  UndefinedCommandType = 0,
  MenuFileCommandType,
  MenuEditCommandType,
  MenuShapeCommandType,
  MenuViewCommandType,
  MenuRenderCommandType,
  ToolCommandType,
  ViewToggleCommandType
};

//-----------------------------------------------------------------------------

class AuxActionsCreator {
public:
  AuxActionsCreator();
  virtual ~AuxActionsCreator(){};
  virtual void createActions(QObject *parent) = 0;
};

//-----------------------------------------------------------------------------

class AuxActionsCreatorManager {
  bool m_auxActionsCreated;
  std::vector<AuxActionsCreator *> m_auxActionsCreators;
  AuxActionsCreatorManager();

public:
  static AuxActionsCreatorManager *instance();
  void addAuxActionsCreator(AuxActionsCreator *auxActionsCreator);
  void createAuxActions(QObject *parent);
};

//-----------------------------------------------------------------------------

class CommandManager {
  class Node {
  public:
    std::string m_id;
    CommandType m_type;
    QAction *m_qaction;
    CommandHandlerInterface *m_handler;
    bool m_enabled;
    QString m_onText,
        m_offText;  // for toggle commands. e.g. show/hide something
    Node(CommandId id)
        : m_id(id)
        , m_type(UndefinedCommandType)
        , m_qaction(0)
        , m_handler(0)
        , m_enabled(true) {}
    ~Node() {
      if (m_handler) delete m_handler;
    }
  };

  std::map<std::string, Node *> m_idTable;
  std::map<QAction *, Node *> m_qactionTable;
  std::map<std::string, Node *> m_shortcutTable;

  CommandManager();
  Node *getNode(CommandId id, bool createIfNeeded = true);

public:
  static CommandManager *instance();
  ~CommandManager();

  void setHandler(CommandId id, CommandHandlerInterface *handler);

  void define(CommandId id, CommandType type, std::string defaultShortcutString,
              QAction *action);

  void execute(QAction *action);
  void execute(CommandId id);

  QAction *getAction(CommandId id, bool createIfNeeded = false);
};

//-----------------------------------------------------------------------------

template <class T>
class CommandHandlerHelper : public CommandHandlerInterface {
  T *m_target;
  void (T::*m_method)();

public:
  CommandHandlerHelper(T *target, void (T::*method)())
      : m_target(target), m_method(method) {}
  void execute() { (m_target->*m_method)(); }
};

//-----------------------------------------------------------------------------

template <class T, typename R>
class CommandHandlerHelper2 : public CommandHandlerInterface {
  T *m_target;
  void (T::*m_method)(R value);
  R m_value;

public:
  CommandHandlerHelper2(T *target, void (T::*method)(R), R value)
      : m_target(target), m_method(method), m_value(value) {}
  void execute() { (m_target->*m_method)(m_value); }
};

//-----------------------------------------------------------------------------

template <class T>
inline void setCommandHandler(CommandId id, T *target, void (T::*method)()) {
  CommandManager::instance()->setHandler(
      id, new CommandHandlerHelper<T>(target, method));
}

//-----------------------------------------------------------------------------

class MenuItemHandler {
public:
  MenuItemHandler(CommandId cmdId);
  virtual ~MenuItemHandler(){};
  virtual void execute() = 0;
};

template <class T>
class OpenPopupCommandHandler : public MenuItemHandler {
  T *m_popup;
  CommandId m_id;

public:
  OpenPopupCommandHandler(CommandId cmdId)
      : MenuItemHandler(cmdId), m_popup(0) {}

  void execute() {
    // std::cout<<"popup execute "<<std::endl;
    if (!m_popup) m_popup = new T();
    m_popup->show();
    m_popup->raise();
    m_popup->activateWindow();
  }
};

//-----------------------------------------------------------------------------

class MyAction : public QAction {
  Q_OBJECT
public:
  MyAction(const QString &text, QObject *parent);
  MyAction(const QIcon &icon, const QString &text, QObject *parent);

public slots:
  void onTriggered();
};

#endif