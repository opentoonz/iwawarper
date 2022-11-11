//----------------------------------
// MainStatusBar
// メインウィンドウのステータスバー
// 左半分にメッセージ、右にフレームスライダを表示
//----------------------------------

#include "mainstatusbar.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"

#include <QStatusBar>
#include <QLineEdit>
#include <QSlider>
#include <QHBoxLayout>
#include <QIntValidator>

MainStatusBar::MainStatusBar(QWidget* parent) : QToolBar(parent) {
  setMovable(false);

  //--- オブジェクトの宣言
  m_status      = new QStatusBar(this);
  m_frameEdit   = new QLineEdit(this);
  m_frameSlider = new QSlider(this);

  //--- プロパティの設定
  m_status->setSizeGripEnabled(false);
  m_frameSlider->setOrientation(Qt::Horizontal);
  m_frameSlider->setRange(1, 1);

  m_validator = new QIntValidator(1, 1, this);
  m_frameEdit->setValidator(m_validator);

  m_frameSlider->setValue(1);
  m_frameEdit->setText(QString::number(1));

  //--- 右に収まるフレームスライダ
  QWidget* fsWidget     = new QWidget(this);
  QHBoxLayout* fsLayout = new QHBoxLayout();
  fsLayout->setMargin(0);
  fsLayout->setSpacing(3);
  {
    fsLayout->addWidget(m_status, 1);
    fsLayout->addWidget(m_frameSlider, 1);
    fsLayout->addWidget(m_frameEdit, 0);
  }
  fsWidget->setLayout(fsLayout);

  addWidget(fsWidget);

  // シグナル／スロット接続

  // プロジェクト長が変わったらスライダの範囲を更新する
  IwProjectHandle* projectHandle = IwApp::instance()->getCurrentProject();
  connect(projectHandle, SIGNAL(projectChanged()), this,
          SLOT(onProjectChanged()));
  // 他の操作（KeyFrameEditorなど）でカレントフレームが変わった時、スライダを同期する
  connect(projectHandle, SIGNAL(viewFrameChanged()), this,
          SLOT(onViewFrameChanged()));

  // カレントフレームを変更する操作
  connect(m_frameSlider, SIGNAL(sliderMoved(int)), this,
          SLOT(onFrameSliderMoved(int)));
  connect(m_frameEdit, SIGNAL(editingFinished()), this,
          SLOT(onFrameEditFinished()));
}

//----------------------------------
// ステータスバーにメッセージを表示する
//----------------------------------
void MainStatusBar::showMessageOnStatusBar(const QString& message,
                                           int timeout) {
  m_status->showMessage(message, timeout);
}

// プロジェクト長が変わったらスライダの範囲を更新する
void MainStatusBar::onProjectChanged() {
  IwProject* prj  = IwApp::instance()->getCurrentProject()->getProject();
  int frameLength = prj->getProjectFrameLength();

  if (frameLength == 0) return;
  // 前と同じ値ならreturn
  if (m_frameSlider->maximum() == frameLength) return;

  m_frameSlider->setMaximum(frameLength);
  m_validator->setRange(1, frameLength);

  update();
}

// カレントフレームを変更する操作
void MainStatusBar::onFrameSliderMoved(int frame) {
  // エディットを同期
  m_frameEdit->setText(QString::number(frame));

  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();

  prj->setViewFrame(frame - 1);
}

void MainStatusBar::onFrameEditFinished() {
  int frame = m_frameEdit->text().toInt();

  m_frameSlider->setValue(frame);

  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();

  prj->setViewFrame(frame - 1);
}

// 他の操作（KeyFrameEditorなど）でカレントフレームが変わった時、スライダを同期する
void MainStatusBar::onViewFrameChanged() {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  int frame = prj->getViewFrame() + 1;

  // スライダを同期
  m_frameSlider->setValue(frame);
  // エディットを同期
  m_frameEdit->setText(QString::number(frame));

  update();
}