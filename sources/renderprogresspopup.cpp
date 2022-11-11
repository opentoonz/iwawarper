//---------------------------------------------------
// RenderProgressPopup
// レンダリングの進行状況を示すポップアップ
//---------------------------------------------------

#include "renderprogresspopup.h"

#include "iwapp.h"
#include "mainwindow.h"
#include "iwproject.h"
#include "outputsettings.h"

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

RenderProgressPopup::RenderProgressPopup(IwProject* project)
    : QDialog(IwApp::instance()->getMainWindow())
    , m_isCanceled(false)
    , m_project(project) {
  //--- オブジェクトの宣言
  m_statusLabel = new QLabel(tr("Start rendering..."), this);
  m_progress    = new QProgressBar(this);

  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);

  //--- プロパティの設定
  // 何フレーム計算するか
  OutputSettings* settings            = project->getOutputSettings();
  OutputSettings::SaveRange saveRange = settings->getSaveRange();

  if (saveRange.endFrame == -1)
    saveRange.endFrame = project->getProjectFrameLength() - 1;

  int frameAmount =
      (int)((saveRange.endFrame - saveRange.startFrame) / saveRange.stepFrame) +
      1;

  m_progress->setRange(0, frameAmount);
  m_progress->setValue(0);

  //--- レイアウト
  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setSpacing(10);
  mainLay->setMargin(10);
  {
    mainLay->addWidget(m_statusLabel, 0, Qt::AlignLeft);
    mainLay->addWidget(m_progress, 0);
    mainLay->addWidget(cancelBtn, 0, Qt::AlignRight);
  }
  setLayout(mainLay);

  //--- シグナル/スロット接続
  connect(cancelBtn, SIGNAL(clicked(bool)), this,
          SLOT(onCancelButtonClicked()));
}

//---------------------------------------------------

void RenderProgressPopup::onCancelButtonClicked() {
  m_statusLabel->setText(tr("Aborting..."));
  m_isCanceled = true;
  m_progress->setVisible(false);
  setEnabled(false);
}

//---------------------------------------------------

void RenderProgressPopup::onFrameFinished() {
  if (!m_isCanceled) {
    m_statusLabel->setText(tr("Rendered %1 of %2 frames")
                               .arg(m_progress->value() + 1)
                               .arg(m_progress->maximum()));
  }
  // プログレスを進める
  m_progress->setValue(m_progress->value() + 1);
  if (m_progress->value() == m_progress->maximum()) accept();
}