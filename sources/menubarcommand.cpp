#include "menubarcommand.h"

#include "iwfolders.h"
#include <QAction>
#include <QApplication>
#include <QMessageBox>
#include <QSettings>
#include <QFileInfo>

//---------------------------------------------------------

namespace {

void updateToolTip(QAction *action) {
  QString tooltip  = action->text();
  QString shortcut = action->shortcut().toString();
  if (shortcut != "") tooltip += " (" + shortcut + ")";
  action->setToolTip(tooltip);
}

}  // namespace

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

  if (type == ToolCommandType) updateToolTip(qaction);
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

void CommandManager::getActions(CommandType type,
                                std::vector<QAction *> &actions) {
  AuxActionsCreatorManager::instance()->createAuxActions(qApp);
  std::map<QAction *, Node *>::iterator it;
  for (it = m_qactionTable.begin(); it != m_qactionTable.end(); ++it)
    if (it->second->m_type == type) actions.push_back(it->first);
}

//---------------------------------------------------------

QAction *CommandManager::getActionFromShortcut(std::string shortcutString) {
  std::map<std::string, Node *>::iterator it =
      m_shortcutTable.find(shortcutString);
  return it != m_shortcutTable.end() ? it->second->m_qaction : 0;
}

//---------------------------------------------------------

std::string CommandManager::getShortcutFromAction(QAction *action) {
  std::map<std::string, Node *>::iterator it = m_shortcutTable.begin();
  for (; it != m_shortcutTable.end(); ++it) {
    if (it->second->m_qaction == action) return it->first;
  }
  return "";
}

//---------------------------------------------------------

void CommandManager::setShortcut(QAction *action, std::string shortcutString,
                                 bool keepDefault) {
  QString shortcut = QString::fromStdString(shortcutString);

  std::string oldShortcutString = action->shortcut().toString().toStdString();
  if (oldShortcutString == shortcutString) return;

  // Cerco il nodo corrispondente ad action. Deve esistere
  std::map<QAction *, Node *>::iterator it = m_qactionTable.find(action);
  Node *node = it != m_qactionTable.end() ? it->second : 0;
  assert(node);
  assert(node->m_qaction == action);

  QKeySequence ks(shortcut);
  assert(ks.count() == 1 || ks.count() == 0 && shortcut == "");

  if (node->m_type == MenuViewCommandType && ks.count() > 1) {
    QMessageBox::warning(
        nullptr, QObject::tr("Warning"),
        QObject::tr("It is not possible to assign a shortcut with modifiers to "
                    "the visualization commands."));
    return;
  }
  // lo shortcut e' gia' assegnato?
  QString oldActionId;
  std::map<std::string, Node *>::iterator sit =
      m_shortcutTable.find(shortcutString);
  if (sit != m_shortcutTable.end()) {
    // la vecchia azione non ha piu' shortcut
    oldActionId = QString::fromStdString(sit->second->m_id);
    sit->second->m_qaction->setShortcut(QKeySequence());
    if (sit->second->m_type == ToolCommandType)
      updateToolTip(sit->second->m_qaction);
  }
  // assegno lo shortcut all'azione
  action->setShortcut(
      QKeySequence::fromString(QString::fromStdString(shortcutString)));
  if (node->m_type == ToolCommandType) updateToolTip(action);

  // Aggiorno la tabella shortcut -> azioni
  // Cancello il riferimento all'eventuale vecchio shortcut di action
  if (oldShortcutString != "") m_shortcutTable.erase(oldShortcutString);
  // e aggiungo il nuovo legame
  m_shortcutTable[shortcutString] = node;

  // registro il tutto
  QString profPath = IwFolders::getMyProfileFolderPath();
  if (profPath.isEmpty()) return;
  QSettings settings(profPath + "/shortcuts.ini", QSettings::IniFormat);

  settings.beginGroup("shortcuts");
  settings.setValue(QString::fromStdString(node->m_id),
                    QString::fromStdString(shortcutString));
  if (keepDefault) {
    if (oldActionId != "") settings.remove(oldActionId);
  }
  settings.endGroup();
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
// load user defined shortcuts

// In menubarcommand.cpp
void CommandManager::loadShortcuts() {
  // Load shortcuts file
  QString profPath = IwFolders::getMyProfileFolderPath();
  if (profPath.isEmpty()) return;
  QString fp = profPath + "/shortcuts.ini";

  if (!QFileInfo(fp).exists()) return;

  QSettings settings(fp, QSettings::IniFormat);
  settings.beginGroup("shortcuts");
  QStringList ids = settings.allKeys();

  for (int i = 0; i < ids.size(); i++) {
    std::string id   = ids.at(i).toStdString();
    QString shortcut = settings.value(ids.at(i), "").toString();

    QAction *action = getAction(&id[0], false);

    // Process non-reserved shortcuts
    if (action) {
      QString oldShortcut = action->shortcut().toString();

      if (oldShortcut == shortcut) {
        continue;
      }

      // Remove old shortcut mapping
      if (!oldShortcut.isEmpty()) {
        m_shortcutTable.erase(oldShortcut.toStdString());
      }

      // Set new shortcut
      if (!shortcut.isEmpty()) {
        QAction *other = getActionFromShortcut(shortcut.toStdString());
        if (other) {
          other->setShortcut(QKeySequence());
        }
        m_shortcutTable[shortcut.toStdString()] = getNode(&id[0]);
      }

      action->setShortcut(QKeySequence(shortcut));
    }
  }

  settings.endGroup();
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
