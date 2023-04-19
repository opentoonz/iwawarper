//------------------------------------------
// ToolOptionPanel
// 常時表示するツールオプション
//------------------------------------------

#include "tooloptionpanel.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwtoolhandle.h"
#include "iwtool.h"
#include "iwproject.h"
#include "preferences.h"

#include "transformdialog.h"
#include "shapeoptionsdialog.h"
#include "freehanddialog.h"

#include "myslider.h"
#include "reshapetool.h"

//------------------------------------------
// ReshapeTool "Lock Points"コマンドの距離の閾値のスライダ
//------------------------------------------

ReshapeToolOptionPanel::ReshapeToolOptionPanel(QWidget* parent)
    : QWidget(parent) {
  // オブジェクトの宣言
  m_lockThresholdSlider = new MyIntSlider(1, 500, this);
  m_transformHandlesCB  = new QCheckBox(tr("Transform Active Points"), this);

  //--- 初期値状態の設定
  ReshapeTool* tool = dynamic_cast<ReshapeTool*>(IwTool::getTool("T_Reshape"));
  if (tool) {
    m_transformHandlesCB->setChecked(tool->isTransformHandlesEnabled());
  }

  // レイアウト
  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setSpacing(8);
  mainLay->setMargin(10);
  {
    mainLay->addWidget(new QLabel(tr("Lock threshold:")), 0);
    mainLay->addWidget(m_lockThresholdSlider, 0);
    mainLay->addWidget(m_transformHandlesCB, 0, Qt::AlignLeft);
    mainLay->addStretch(1);
  }
  setLayout(mainLay);

  // シグナル／スロット接続
  // プロジェクトが切り替わったら表示を更新
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(projectSwitched()),
          this, SLOT(onProjectSwitched()));
  // スライダがいじられたらパラメータに反映
  connect(m_lockThresholdSlider, SIGNAL(valueChanged(bool)), this,
          SLOT(onLockThresholdSliderChanged()));
  connect(m_transformHandlesCB, SIGNAL(clicked(bool)), this,
          SLOT(onTransformHandlesCBClicked(bool)));

  onProjectSwitched();
}

void ReshapeToolOptionPanel::onProjectSwitched() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  m_lockThresholdSlider->setValue(Preferences::instance()->getLockThreshold());

  update();
}

void ReshapeToolOptionPanel::onLockThresholdSliderChanged() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  Preferences::instance()->setLockThreshold(m_lockThresholdSlider->value());

  IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
}

void ReshapeToolOptionPanel::onTransformHandlesCBClicked(bool on) {
  ReshapeTool* tool = dynamic_cast<ReshapeTool*>(IwTool::getTool("T_Reshape"));
  if (tool) tool->enableTransformHandles(on);
  IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
}

//------------------------------------------
//------------------------------------------

ToolOptionPanel::ToolOptionPanel(QWidget* parent)
    : QDockWidget(tr("Tool Options"), parent) {
  setObjectName("ToolOptionPanel");
  setFixedWidth(220);
  // The dock widget cannot be closed
  setFeatures(QDockWidget::DockWidgetMovable |
              QDockWidget::DockWidgetFloatable);
  // オブジェクトの宣言
  m_stackedPanels = new QStackedWidget(this);

  setWidget(m_stackedPanels);

  // シグナル／スロット接続
  connect(IwApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()), this,
          SLOT(onToolSwitched()));

  onToolSwitched();
}

//------------------------------------------
// ツールが切り替わったら、そのためのパネルをpanelMapから取り出して切り替える
// まだ無ければ作ってpanelMapとstackedWidgetに登録する
//------------------------------------------
void ToolOptionPanel::onToolSwitched() {
  // 現在のツールを取得
  IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
  if (!tool) return;

  if (m_panelMap.contains(tool)) {
    m_stackedPanels->setCurrentWidget(m_panelMap.value(tool));
  }
  // 初めてこのツールに入ってきた場合
  else {
    QWidget* widget = getPanel(tool);
    if (!widget) return;
    m_panelMap[tool] = widget;
    m_stackedPanels->addWidget(widget);
    m_stackedPanels->setCurrentWidget(widget);
  }
  update();
}

//------------------------------------------
// ツールからパネルを作って返す。対応パネルが無ければ空のWidgetを返す
//------------------------------------------
QWidget* ToolOptionPanel::getPanel(IwTool* tool) {
  if (tool->getName() == "T_Transform")
    return new TransformDialog();
  else if (tool->getName() == "T_Reshape")
    return new ReshapeToolOptionPanel(this);
  else if (tool->getName() == "T_Correspondence")
    return new ShapeOptionsDialog();
  else if (tool->getName() == "T_Freehand")
    return new FreeHandDialog();
  else
    return new QWidget();
}