#ifndef IWSELECTIONHANDLE_H
#define IWSELECTIONHANDLE_H

#include <QObject>
#include <string>
#include <vector>

#include "menubarcommand.h"

class IwSelection;
class CommandHandlerInterface;

class IwSelectionHandle : public QObject {
  Q_OBJECT

  std::vector<IwSelection *> m_selectionStack;
  std::vector<std::string> m_enabledCommandIds;

public:
  IwSelectionHandle();
  ~IwSelectionHandle();

  IwSelection *getSelection() const;
  void setSelection(IwSelection *selection);

  void pushSelection();
  void popSelection();

  // IwSelection::enableCommand ‚©‚çŒÄ‚Î‚ê‚é
  void enableCommand(std::string cmdId, CommandHandlerInterface *handler);

  void notifySelectionChanged();

  static IwSelectionHandle *getCurrent();

signals:
  void selectionSwitched(IwSelection *oldSelection, IwSelection *newSelection);
  void selectionChanged(IwSelection *selection);

protected slots:
  void onProjectSwitched();
};

#endif
