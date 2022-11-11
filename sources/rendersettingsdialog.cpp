//---------------------------------
// Render Sewttings Dialog
// モーフィング、レンダリングの設定を行う
//---------------------------------

#include "rendersettingsdialog.h"

#include "iwapp.h"
#include "mainwindow.h"
#include "menubarcommandids.h"
#include "iwprojecthandle.h"
#include "iwproject.h"

#include "rendersettings.h"

#include <QComboBox>
#include <QLabel>
#include <QStringList>

#include <QHBoxLayout>
#include <QVBoxLayout>

//---------------------------------
RenderSettingsDialog::RenderSettingsDialog()
    : IwDialog(IwApp::instance()->getMainWindow(), "RenderSettingsDialog",
               false) {
  setSizeGripEnabled(false);
  //--- オブジェクトの宣言
  m_warpPrecisionCombo = new QComboBox(this);
  //--- プロパティの設定
  QStringList precList;
  precList << "Linear"
           << "Low"
           << "Medium"
           << "High"
           << "Very High"
           << "Super High";
  m_warpPrecisionCombo->addItems(precList);

  //--- レイアウト
  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setSpacing(5);
  mainLay->setMargin(10);
  {
    mainLay->addWidget(new QLabel(tr("Precision:"), this), 0);
    mainLay->addWidget(m_warpPrecisionCombo, 1);
  }
  setLayout(mainLay);

  //------ シグナル/スロット接続
  connect(m_warpPrecisionCombo, SIGNAL(activated(int)), this,
          SLOT(onPrecisionComboActivated(int)));
}

//---------------------------------
// projectが切り替わったら、内容を更新する
//---------------------------------
void RenderSettingsDialog::showEvent(QShowEvent* event) {
  IwDialog::showEvent(event);

  IwProjectHandle* ph = IwApp::instance()->getCurrentProject();
  if (ph) connect(ph, SIGNAL(projectSwitched()), this, SLOT(updateGuis()));

  updateGuis();
}

//---------------------------------
void RenderSettingsDialog::hideEvent(QHideEvent* event) {
  IwDialog::hideEvent(event);

  IwProjectHandle* ph = IwApp::instance()->getCurrentProject();
  if (ph) disconnect(ph, SIGNAL(projectSwitched()), this, SLOT(updateGuis()));
}

//---------------------------------
// 現在のOutputSettingsに合わせて表示を更新する
//---------------------------------
void RenderSettingsDialog::updateGuis() {
  // 現在のプロジェクトのOutputSettingsを取得
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  RenderSettings* settings = project->getRenderSettings();
  if (!settings) return;

  m_warpPrecisionCombo->setCurrentIndex(settings->getWarpPrecision());

  update();
}

//---------------------------------
void RenderSettingsDialog::onPrecisionComboActivated(int index) {
  // 現在のプロジェクトのOutputSettingsを取得
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  RenderSettings* settings = project->getRenderSettings();
  if (!settings) return;

  if (index == settings->getWarpPrecision()) return;

  settings->setWarpPrecision(index);
}

OpenPopupCommandHandler<RenderSettingsDialog> openRenderOptions(
    MI_RenderOptions);