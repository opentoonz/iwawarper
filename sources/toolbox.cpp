//---------------------------------------
// Tool Box
// MainWindow内左側のツールボックス
//---------------------------------------

#include "toolbox.h"
#include "toolcommandids.h"
#include "menubarcommandids.h"

#include <QMouseEvent>
#include <QAction>
#include <iostream>

#include <QToolButton>

// ダブルクリックで設定ダイアログを開くToolButton
class MyToolButton : public QToolButton {
  CommandId m_id;

public:
  MyToolButton(CommandId id, QWidget* parent = 0)
      : QToolButton(parent), m_id(id) {
    setFocusPolicy(Qt::NoFocus);
  }
};

ToolBoxDock::ToolBoxDock(QWidget* parent) : QDockWidget(parent) {
  setFixedWidth(35);
  // The dock widget cannot be closed, moved, or floated.
  setFeatures(QDockWidget::NoDockWidgetFeatures);
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