//------------------------------------
// SettingsDialog
//------------------------------------
#include "settingsdialog.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "mainwindow.h"
#include "menubarcommandids.h"

#include "preferences.h"
#include "rendersettings.h"

#include "myslider.h"
#include "iwtrianglecache.h"

#include <QComboBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>

//------------------------------------
SettingsDialog::SettingsDialog()
    : IwDialog(IwApp::instance()->getMainWindow(), "SettingsDialog", false) {
  setWindowTitle(tr("Settings"));
  //-- オブジェクトの宣言
  // Preferenceより
  m_bezierPrecisionCombo = new QComboBox(this);
  // RenderSettingsより
  m_warpPrecisionSlider = new MyIntSlider(0, 5, this);
  m_faceSizeThresSlider = new MyIntSlider(10, 50, this);
  m_alphaModeCombo      = new QComboBox(this);
  m_resampleModeCombo   = new QComboBox(this);
  m_imageShrinkSlider   = new MyIntSlider(1, 4, this);
  m_antialiasCheckBox   = new QCheckBox(tr("Shape Antialias"), this);

  //-- プロパティの設定
  m_bezierPrecisionCombo->addItem(tr("Low"), Preferences::LOW);
  m_bezierPrecisionCombo->addItem(tr("Medium"), Preferences::MEDIUM);
  m_bezierPrecisionCombo->addItem(tr("High"), Preferences::HIGH);
  m_bezierPrecisionCombo->addItem(tr("Super High"), Preferences::SUPERHIGH);

  QStringList aModeList;
  aModeList << "Source"
            << "Shape";
  m_alphaModeCombo->addItems(aModeList);

  m_resampleModeCombo->addItem(tr("Area Average"), AreaAverage);
  m_resampleModeCombo->addItem(tr("Nearest Neighbor"), NearestNeighbor);

  //-- レイアウト
  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setSpacing(5);
  mainLay->setMargin(10);
  {
    mainLay->addWidget(new QLabel(tr("Bezier Precision:")), 0);
    mainLay->addWidget(m_bezierPrecisionCombo, 0);
    mainLay->addSpacing(3);
    mainLay->addWidget(new QLabel(tr("Number of subdivision:")), 0);
    mainLay->addWidget(m_warpPrecisionSlider, 0);
    mainLay->addSpacing(3);
    mainLay->addWidget(new QLabel(tr("Maximum face size:")), 0);
    mainLay->addWidget(m_faceSizeThresSlider, 0);
    mainLay->addSpacing(3);
    mainLay->addWidget(new QLabel(tr("Alpha Mode:")), 0);
    mainLay->addWidget(m_alphaModeCombo, 0);
    mainLay->addSpacing(3);
    mainLay->addWidget(new QLabel(tr("Resample Mode:")), 0);
    mainLay->addWidget(m_resampleModeCombo, 0);
    mainLay->addSpacing(3);
    mainLay->addWidget(m_antialiasCheckBox, 0);
    mainLay->addSpacing(3);
    mainLay->addWidget(new QLabel(tr("Image Shrink:")), 0);
    mainLay->addWidget(m_imageShrinkSlider, 0);
    mainLay->addStretch(1);
  }
  setLayout(mainLay);

  //-- シグナル／スロット接続
  connect(m_bezierPrecisionCombo, SIGNAL(activated(int)), this,
          SLOT(onBezierPrecisionComboChanged(int)));
  connect(m_warpPrecisionSlider, SIGNAL(valueChanged(bool)), this,
          SLOT(onPrecisionValueChanged(bool)));
  connect(m_faceSizeThresSlider, SIGNAL(valueChanged(bool)), this,
          SLOT(onFaceSizeValueChanged(bool)));
  connect(m_alphaModeCombo, SIGNAL(activated(int)), this,
          SLOT(onAlphaModeComboActivated(int)));
  connect(m_resampleModeCombo, SIGNAL(activated(int)), this,
          SLOT(onResampleModeComboActivated()));
  connect(m_imageShrinkSlider, SIGNAL(valueChanged(bool)), this,
          SLOT(onImageShrinkChanged(bool)));
  connect(m_antialiasCheckBox, SIGNAL(clicked(bool)), this,
          SLOT(onAntialiasClicked(bool)));
}

//------------------------------------
// projectが切り替わったら、内容を更新する
//------------------------------------
void SettingsDialog::showEvent(QShowEvent* event) {
  IwDialog::showEvent(event);

  IwProjectHandle* ph = IwApp::instance()->getCurrentProject();
  if (ph)
    connect(ph, SIGNAL(projectSwitched()), this, SLOT(onProjectSwitched()));

  onProjectSwitched();
}

//------------------------------------
void SettingsDialog::hideEvent(QHideEvent* event) {
  IwDialog::hideEvent(event);

  IwProjectHandle* ph = IwApp::instance()->getCurrentProject();
  if (ph)
    disconnect(ph, SIGNAL(projectSwitched()), this, SLOT(onProjectSwitched()));
}

//------------------------------------
// 表示更新
//------------------------------------

void SettingsDialog::onProjectSwitched() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  int prec = (int)Preferences::instance()->getBezierActivePrecision();
  m_bezierPrecisionCombo->setCurrentIndex(
      m_bezierPrecisionCombo->findData(prec));

  RenderSettings* settings = project->getRenderSettings();
  if (settings) {
    m_warpPrecisionSlider->setValue(settings->getWarpPrecision());
    m_faceSizeThresSlider->setValue(settings->getFaceSizeThreshold());
    m_alphaModeCombo->setCurrentIndex((int)settings->getAlphaMode());
    m_resampleModeCombo->setCurrentIndex(
        m_resampleModeCombo->findData((int)settings->getResampleMode()));
    m_imageShrinkSlider->setValue(settings->getImageShrink());
    m_antialiasCheckBox->setChecked(settings->getAntialias());
  }
  update();
}

//------------------------------------
// Preference
//------------------------------------
void SettingsDialog::onBezierPrecisionComboChanged(int /*index*/) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  Preferences::BezierActivePrecision prec =
      (Preferences::BezierActivePrecision)(
          m_bezierPrecisionCombo->currentData().toInt());
  Preferences::instance()->setBezierActivePrecision(prec);

  IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
}

//------------------------------------
// RenderSettings
//------------------------------------
void SettingsDialog::onPrecisionValueChanged(bool isDragging) {
  if (isDragging) return;
  // 現在のプロジェクトのOutputSettingsを取得
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  RenderSettings* settings = project->getRenderSettings();
  if (!settings) return;

  int val = m_warpPrecisionSlider->value();
  if (val == settings->getWarpPrecision()) return;

  settings->setWarpPrecision(val);
  IwTriangleCache::instance()->invalidateAllCache();
}

void SettingsDialog::onFaceSizeValueChanged(bool isDragging) {
  if (isDragging) return;
  // 現在のプロジェクトのOutputSettingsを取得
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  RenderSettings* settings = project->getRenderSettings();
  if (!settings) return;

  double val = (double)m_faceSizeThresSlider->value();
  if (val == settings->getFaceSizeThreshold()) return;

  settings->setFaceSizeThreshold(val);
  IwTriangleCache::instance()->invalidateAllCache();
}

void SettingsDialog::onAlphaModeComboActivated(int index) {
  // 現在のプロジェクトのOutputSettingsを取得
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  RenderSettings* settings = project->getRenderSettings();
  if (!settings) return;

  if (index == (int)settings->getAlphaMode()) return;

  settings->setAlphaMode((AlphaMode)index);
}

void SettingsDialog::onResampleModeComboActivated() {
  // 現在のプロジェクトのOutputSettingsを取得
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  RenderSettings* settings = project->getRenderSettings();
  if (!settings) return;

  ResampleMode rMode =
      (ResampleMode)(m_resampleModeCombo->currentData().toInt());
  if (rMode == settings->getResampleMode()) return;

  settings->setResampleMode(rMode);
}

void SettingsDialog::onImageShrinkChanged(bool isDragging) {
  if (isDragging) return;
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  RenderSettings* settings = project->getRenderSettings();
  if (!settings) return;

  int val = m_imageShrinkSlider->value();
  if (val == settings->getImageShrink()) return;

  settings->setImageShrink(val);
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void SettingsDialog::onAntialiasClicked(bool on) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  RenderSettings* settings = project->getRenderSettings();
  if (!settings) return;

  if (on == settings->getAntialias()) return;

  settings->setAntialias(on);
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
OpenPopupCommandHandler<SettingsDialog> openSettingsDialog(MI_Preferences);