//---------------------------------------------------
// IwUndoManager
// QUndoStack‚ğg‚Á‚Äì‚é
//---------------------------------------------------
#ifndef IWUNDOMANAGER_H
#define IWUNDOMANAGER_H

#include <QUndoStack>

class IwUndoManager : public QUndoStack  // singleton
{
  IwUndoManager();

public:
  static IwUndoManager *instance();
  ~IwUndoManager();
};

#endif