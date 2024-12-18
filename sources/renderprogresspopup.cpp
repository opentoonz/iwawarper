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
  setModal(true);
  //--- オブジェクトの宣言
  m_statusLabel   = new QLabel(tr("Start rendering..."), this);
  m_itemProgress  = new QProgressBar(this);
  m_frameProgress = new QProgressBar(this);

  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);

  //--- プロパティの設定
  // 何フレーム計算するか
  // OutputSettings* settings            = project->getOutputSettings();
  // OutputSettings::SaveRange saveRange = settings->getSaveRange();
  //
  // if (saveRange.endFrame == -1)
  //  saveRange.endFrame = project->getProjectFrameLength() - 1;
  //
  // int frameAmount =
  //    (int)((saveRange.endFrame - saveRange.startFrame) / saveRange.stepFrame)
  //    + 1;
  //
  // m_progress->setRange(0, frameAmount);
  // m_progress->setValue(0);
  QList<OutputSettings*> activeItems = project->getRenderQueue()->activeItems();
  m_itemProgress->setRange(0, activeItems.size());
  m_itemProgress->setValue(0);

  //--- レイアウト
  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setSpacing(10);
  mainLay->setMargin(10);
  {
    mainLay->addWidget(m_statusLabel, 0, Qt::AlignLeft);
    mainLay->addWidget(m_itemProgress, 0);
    mainLay->addWidget(m_frameProgress, 0);
    mainLay->addWidget(cancelBtn, 0, Qt::AlignRight);
  }
  setLayout(mainLay);

  //--- シグナル/スロット接続
  connect(cancelBtn, SIGNAL(clicked(bool)), this,
          SLOT(onCancelButtonClicked()));
}

//---------------------------------------------------

void RenderProgressPopup::startItem(OutputSettings* settings) {
  OutputSettings::SaveRange saveRange = settings->getSaveRange();

  if (saveRange.endFrame == -1)
    saveRange.endFrame = m_project->getProjectFrameLength() - 1;

  int frameAmount =
      (int)((saveRange.endFrame - saveRange.startFrame) / saveRange.stepFrame) +
      1;

  m_itemProgress->setValue(m_itemProgress->value() + 1);
  m_frameProgress->setRange(0, frameAmount);
  m_frameProgress->setValue(0);
}

//---------------------------------------------------

void RenderProgressPopup::onCancelButtonClicked() {
  m_statusLabel->setText(tr("Aborting..."));
  m_isCanceled = true;
  m_itemProgress->setVisible(false);
  m_frameProgress->setVisible(false);
  setEnabled(false);
}

//---------------------------------------------------

void RenderProgressPopup::onFrameFinished() {
  if (!m_isCanceled) {
    m_statusLabel->setText(
        tr("Rendered %1 of %2 frames in %3 of %4 queue items.")
            .arg(m_frameProgress->value() + 1)
            .arg(m_frameProgress->maximum())
            .arg(m_itemProgress->value() + 1)
            .arg(m_itemProgress->maximum()));
  }
  // プログレスを進める
  m_frameProgress->setValue(m_frameProgress->value() + 1);
  if (m_frameProgress->value() == m_frameProgress->maximum()) accept();
}