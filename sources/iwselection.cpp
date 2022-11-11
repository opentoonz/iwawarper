//-----------------------------------------------------------------------------
// IwSelection
// 選択をやりくりするクラス
//-----------------------------------------------------------------------------
#include "iwselection.h"

#include "iwselectionhandle.h"

#include "iwapp.h"
#include "iwprojecthandle.h"

#include <QMenu>

IwSelection::IwSelection() {}

//-----------------------------------------------------------------------------

IwSelection::~IwSelection() {}

//-----------------------------------------------------------------------------
// カレントの選択をこれにする
void IwSelection::makeCurrent() {
  IwSelectionHandle::getCurrent()->setSelection(this);
}

//-----------------------------------------------------------------------------
// カレントの選択がこれだった場合、選択を解除する
void IwSelection::makeNotCurrent() {
  IwSelectionHandle *sh = IwSelectionHandle::getCurrent();
  if (sh->getSelection() == this) sh->setSelection(0);
}

//-----------------------------------------------------------------------------
// カレントの選択を返す。自分が何者かは関係ない。
IwSelection *IwSelection::getCurrent() {
  return IwSelectionHandle::getCurrent()->getSelection();
}

//-----------------------------------------------------------------------------
// 引数の選択をカレントに格納。自分が何者かは関係ない。
void IwSelection::setCurrent(IwSelection *selection) {
  IwSelectionHandle::getCurrent()->setSelection(selection);
}

//-----------------------------------------------------------------------------
// CommandIdで指定したコマンドを有効にする
void IwSelection::enableCommand(CommandId cmdId,
                                CommandHandlerInterface *handler) {
  IwSelectionHandle::getCurrent()->enableCommand(cmdId, handler);
}

void IwSelection::addMenuAction(QMenu *menu, CommandId cmdId) {
  menu->addAction(CommandManager::instance()->getAction(cmdId));
}

//-----------------------------------------------------------------------------
// プロジェクトが切り替わったら、選択を解除する
//-----------------------------------------------------------------------------
void IwSelection::onProjectSwitched() { selectNone(); }