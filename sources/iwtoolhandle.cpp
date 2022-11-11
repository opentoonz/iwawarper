//-----------------------------------------------------------------------------
// IwToolHandle
// 現在のツールを管理する
//-----------------------------------------------------------------------------

#include "iwtoolhandle.h"

#include "iwtool.h"

IwToolHandle::IwToolHandle() : m_tool(0), m_toolName("") {}

//-----------------------------------------------------------------------------
// カレントツールを取得
//-----------------------------------------------------------------------------
IwTool* IwToolHandle::getTool() const { return m_tool; }

//-----------------------------------------------------------------------------
// カレントツールをセット
//-----------------------------------------------------------------------------
void IwToolHandle::setTool(QString name) {
  m_toolName   = name;
  IwTool* tool = IwTool::getTool(m_toolName);
  if (tool == m_tool) return;

  if (m_tool) m_tool->onDeactivate();

  m_tool = tool;

  if (m_tool) {
    m_tool->onActivate();
    emit toolSwitched();
  }
}
