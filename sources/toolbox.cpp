//---------------------------------------
// Tool Box
// MainWindow�������̃c�[���{�b�N�X
//---------------------------------------

#include "toolbox.h"
#include "toolcommandids.h"
#include "menubarcommandids.h"

#include <QMouseEvent>
#include <QAction>
#include <iostream>

#include <QToolButton>

// �_�u���N���b�N�Őݒ�_�C�A���O���J��ToolButton
class MyToolButton : public QToolButton {
  CommandId m_id;

public:
  MyToolButton(CommandId id, QWidget* parent = 0)
      : QToolButton(parent), m_id(id) {
    setFocusPolicy(Qt::NoFocus);
  }
};

ToolBoxDock::ToolBoxDock(QWidget* parent) : QDockWidget(parent) {
  setObjectName("ToolBoxDock");
  setFixedWidth(35);
  // The dock widget cannot be closed
  setFeatures(QDockWidget::DockWidgetMovable |
              QDockWidget::DockWidgetFloatable);
  ToolBox* toolBox = new ToolBox(this);
  setWidget(toolBox);
}

ToolBox::ToolBox(QWidget* parent) : QToolBar(parent) {
  setOrientation(Qt::Vertical);
  setMovable(false);

  setIconSize(QSize(23, 23));
  setToolButtonStyle(Qt::ToolButtonIconOnly);

  addActionToToolBox(T_Transform);
  addActionToToolBox(T_Reshape);
  addActionToToolBox(T_Correspondence);
  addActionToToolBox(T_Pen);
  addActionToToolBox(T_Square);
  addActionToToolBox(T_Circle);
  addActionToToolBox(T_Freehand);
}

void ToolBox::addActionToToolBox(CommandId id) {
  MyToolButton* toolButton = new MyToolButton(id, this);

  toolButton->setDefaultAction(CommandManager::instance()->getAction(id));

  addWidget(toolButton);
}