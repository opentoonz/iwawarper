#include "menubarcommand.h"

#include <QAction>
#include <QApplication>
//=========================================================

AuxActionsCreator::AuxActionsCreator() {
  AuxActionsCreatorManager::instance()->addAuxActionsCreator(this);
}
//-----------------------------------------------------------------------------

AuxActionsCreatorManager::AuxActionsCreatorManager()
    : m_auxActionsCreated(false) {}

AuxActionsCreatorManager *AuxActionsCreatorManager::instance() {
  static AuxActionsCreatorManager _instance;
  return &_instance;
}

void AuxActionsCreatorManager::addAuxActionsCreator(
    AuxActionsCreator *auxActionsCreator) {
  m_auxActionsCreators.push_back(auxActionsCreator);
}

void AuxActionsCreatorManager::createAuxActions(QObject *parent) {
  if (m_auxActionsCreated) return;
  m_auxActionsCreated = true;
  for (int i = 0; i < (int)m_auxActionsCreators.size(); i++)
    m_auxActionsCreators[i]->createActions(parent);
}

//=========================================================

CommandManager::CommandManager() {}

//---------------------------------------------------------

CommandManager::~CommandManager() {
  std::map<std::string, Node *>::iterator it;
  for (it = m_idTable.begin(); it != m_idTable.end(); ++it) delete it->second;
}

//---------------------------------------------------------

CommandManager *CommandManager::instance() {
  static CommandManager _instance;
  return &_instance;
}

//---------------------------------------------------------

void CommandManager::define(CommandId id, CommandType type,
                            std::string defaultShortcutString,
                            QAction *qaction) {
  Node *node = getNode(id);

  node->m_type    = type;
  node->m_qaction = qaction;
  node->m_qaction->setEnabled(
      node->m_enabled &&
      (node->m_handler || node->m_qaction->actionGroup() != 0));

  m_qactionTable[qaction] = node;
  qaction->setShortcutContext(Qt::ApplicationShortcut);

  if (defaultShortcutString != "") {
    qaction->setShortcut(
        QKeySequence(QString::fromStdString(defaultShortcutString)));
    m_shortcutTable[defaultShortcutString] = node;
  }
}

//---------------------------------------------------------

//
// set handler (id, handler)
//   possibly changes enable/disable qaction state
//
void CommandManager::setHandler(CommandId id,
                                CommandHandlerInterface *handler) {
  Node *node = getNode(id);
  if (node->m_handler != handler) {
    delete node->m_handler;
    node->m_handler = handler;
  }
  if (node->m_qaction) {
    node->m_qaction->setEnabled(
        node->m_enabled &&
        (!!node->m_handler || node->m_qaction->actionGroup() != 0));
  }
}

//---------------------------------------------------------

//
// command id => command
//
CommandManager::Node *CommandManager::getNode(CommandId id,
                                              bool createIfNeeded) {
  AuxActionsCreatorManager::instance()->createAuxActions(qApp);
  std::map<std::string, Node *>::iterator it = m_idTable.find(id);
  if (it != m_idTable.end()) return it->second;
  if (createIfNeeded) {
    Node *node    = new Node(id);
    m_idTable[id] = node;
    return node;
  } else
    return 0;
}

//---------------------------------------------------------
//
// qaction -> command; execute
//

void CommandManager::execute(QAction *qaction) {
  std::map<QAction *, Node *>::iterator it = m_qactionTable.find(qaction);
  if (it != m_qactionTable.end() && it->second->m_handler) {
    it->second->m_handler->execute();
  }
}

//---------------------------------------------------------

void CommandManager::execute(CommandId id) {
  Node *node = getNode(id, false);
  if (node && node->m_handler) {
    QAction *action = node->m_qaction;
    if (action && action->isCheckable()) {
      // principalmente per i tool
      action->setChecked(true);
    }
    node->m_handler->execute();
  }
}

//---------------------------------------------------------

QAction *CommandManager::getAction(CommandId id, bool createIfNeeded) {
  Node *node = getNode(id, createIfNeeded);
  if (node) {
    return node->m_qaction;
  } else {
    return 0;
  }
}

//---------------------------------------------------------

MenuItemHandler::MenuItemHandler(const char *cmdId) {
  CommandManager::instance()->setHandler(
      cmdId, new CommandHandlerHelper<MenuItemHandler>(
                 this, &MenuItemHandler::execute));
}

//=============================================================================
// MyAction
//-----------------------------------------------------------------------------

MyAction::MyAction(const QString &text, QObject *parent)
    : QAction(text, parent) {
  connect(this, SIGNAL(triggered()), this, SLOT(onTriggered()));
}

//-----------------------------------------------------------------------------

MyAction::MyAction(const QIcon &icon, const QString &text, QObject *parent)
    : QAction(icon, text, parent) {
  connect(this, SIGNAL(triggered()), this, SLOT(onTriggered()));
}

//-----------------------------------------------------------------------------

void MyAction::onTriggered() { CommandManager::instance()->execute(this); }
