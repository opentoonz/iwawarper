#include "iwselectionhandle.h"
#include "iwselection.h"

IwSelectionHandle::IwSelectionHandle() { m_selectionStack.push_back(0); }

//-----------------------------------------------------------------------------

IwSelectionHandle::~IwSelectionHandle() {}

//-----------------------------------------------------------------------------

IwSelection* IwSelectionHandle::getSelection() const {
  return m_selectionStack.back();
}

//-----------------------------------------------------------------------------

void IwSelectionHandle::setSelection(IwSelection* selection) {
  if (getSelection() == selection) return;
  IwSelection* oldSelection = getSelection();
  if (oldSelection) {
    // disable selection related commands
    CommandManager* commandManager = CommandManager::instance();
    int i;
    for (i = 0; i < (int)m_enabledCommandIds.size(); i++)
      commandManager->setHandler(m_enabledCommandIds[i].c_str(), 0);
    m_enabledCommandIds.clear();
  }
  m_selectionStack.back() = selection;
  if (selection) selection->enableCommands();
  emit selectionSwitched(oldSelection, selection);
}

//-----------------------------------------------------------------------------

void IwSelectionHandle::pushSelection() { m_selectionStack.push_back(0); }

//-----------------------------------------------------------------------------

void IwSelectionHandle::popSelection() {
  if (m_selectionStack.size() > 1) m_selectionStack.pop_back();
  IwSelection* selection = getSelection();
  if (selection) selection->enableCommands();
}

//-----------------------------------------------------------------------------

void IwSelectionHandle::enableCommand(std::string cmdId,
                                      CommandHandlerInterface* handler) {
  CommandManager::instance()->setHandler(cmdId.c_str(), handler);
  m_enabledCommandIds.push_back(cmdId);
}

//-----------------------------------------------------------------------------

void IwSelectionHandle::notifySelectionChanged() {
  if (!m_selectionStack.empty()) emit selectionChanged(m_selectionStack.back());
}

//-----------------------------------------------------------------------------

IwSelectionHandle* IwSelectionHandle::getCurrent() {
  static IwSelectionHandle _currentSelection;
  return &_currentSelection;
}

//-----------------------------------------------------------------------------

void IwSelectionHandle::onProjectSwitched() {
  IwSelection* selection = getSelection();
  if (selection) selection->selectNone();
}
