//---------------------------------------------------
// IwUndoManager
// QUndoStack‚ğg‚Á‚Äì‚é
//---------------------------------------------------

#include "iwundomanager.h"

IwUndoManager::IwUndoManager() { setUndoLimit(100); }

//---------------------------------------------------

IwUndoManager* IwUndoManager::instance() {
  static IwUndoManager _instance;
  return &_instance;
}

//---------------------------------------------------

IwUndoManager::~IwUndoManager() {}