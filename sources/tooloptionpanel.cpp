//------------------------------------------
// ToolOptionPanel
// �펞�\������c�[���I�v�V����
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
// ReshapeTool "Lock Points"�R�}���h�̋�����臒l�̃X���C�_
//------------------------------------------

ReshapeToolOptionPanel::ReshapeToolOptionPanel(QWidget* parent)
    : QWidget(parent) {
  // �I�u�W�F�N�g�̐錾
  m_lockThresholdSlider = new MyIntSlider(1, 500, this);
  m_transformHandlesCB  = new QCheckBox(tr("Transform Active Points"), this);

  //--- �����l��Ԃ̐ݒ�
  ReshapeTool* tool = dynamic_cast<ReshapeTool*>(IwTool::getTool("T_Reshape"));
  if (tool) {
    m_transformHandlesCB->setChecked(tool->isTransformHandlesEnabled());
  }

  // ���C�A�E�g
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

  // �V�O�i���^�X���b�g�ڑ�
  // �v���W�F�N�g���؂�ւ������\�����X�V
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(projectSwitched()),
          this, SLOT(onProjectSwitched()));
  // �X���C�_��������ꂽ��p�����[�^�ɔ��f
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
  // �I�u�W�F�N�g�̐錾
  m_stackedPanels = new QStackedWidget(this);

  setWidget(m_stackedPanels);

  // �V�O�i���^�X���b�g�ڑ�
  connect(IwApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()), this,
          SLOT(onToolSwitched()));

  onToolSwitched();
}

//------------------------------------------
// �c�[�����؂�ւ������A���̂��߂̃p�l����panelMap������o���Đ؂�ւ���
// �܂�������΍����panelMap��stackedWidget�ɓo�^����
//------------------------------------------
void ToolOptionPanel::onToolSwitched() {
  // ���݂̃c�[�����擾
  IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
  if (!tool) return;

  if (m_panelMap.contains(tool)) {
    m_stackedPanels->setCurrentWidget(m_panelMap.value(tool));
  }
  // ���߂Ă��̃c�[���ɓ����Ă����ꍇ
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
// �c�[������p�l��������ĕԂ��B�Ή��p�l����������΋��Widget��Ԃ�
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