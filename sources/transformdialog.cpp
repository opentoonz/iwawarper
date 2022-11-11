//---------------------------------
// Transform Dialog
// Transform Tool のオプションを操作する
//---------------------------------

#include "transformdialog.h"
#include "iwapp.h"
#include "mainwindow.h"
#include "menubarcommandids.h"

#include "transformtool.h"

#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>

TransformDialog::TransformDialog()
    : IwDialog(IwApp::instance()->getMainWindow(), "TransformDialog", false) {
  setSizeGripEnabled(false);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  setWindowTitle("Transform Options");

  //--- オブジェクトの宣言
  m_scaleAroundCenterCB  = new QCheckBox(tr("Scale around center"), this);
  m_rotateAroundCenterCB = new QCheckBox(tr("Rotate around center"), this);
  m_shapeIndependentTransformsCB =
      new QCheckBox(tr("Shape-independent transforms"), this);
  // m_frameIndependentTransformsCB =
  //     new QCheckBox(tr("Frame-independent transforms"), this);
  m_translateOnlyCB = new QCheckBox(tr("Translate only"), this);

  //--- 初期値状態の設定
  TransformTool* tool =
      dynamic_cast<TransformTool*>(IwTool::getTool("T_Transform"));
  if (tool) {
    m_scaleAroundCenterCB->setChecked(tool->IsScaleAroundCenter());
    m_rotateAroundCenterCB->setChecked(tool->IsRotateAroundCenter());
    m_shapeIndependentTransformsCB->setChecked(
        tool->IsShapeIndependentTransforms());
    // m_frameIndependentTransformsCB->setChecked(
    //     tool->IsFrameIndependentTransforms());
    m_translateOnlyCB->setChecked(tool->isTranslateOnly());
  }

  //--- レイアウト
  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setSpacing(8);
  mainLay->setMargin(10);
  {
    mainLay->addWidget(m_scaleAroundCenterCB, 0, Qt::AlignLeft);
    mainLay->addWidget(m_rotateAroundCenterCB, 0, Qt::AlignLeft);
    mainLay->addWidget(m_shapeIndependentTransformsCB, 0, Qt::AlignLeft);
    // mainLay->addWidget(m_frameIndependentTransformsCB, 0, Qt::AlignLeft);
    mainLay->addWidget(m_translateOnlyCB, 0, Qt::AlignLeft);
    mainLay->addStretch(1);
  }
  setLayout(mainLay);

  //--- シグナル/スロット接続
  connect(m_scaleAroundCenterCB, SIGNAL(clicked(bool)), this,
          SLOT(onCheckBoxChanged()));
  connect(m_rotateAroundCenterCB, SIGNAL(clicked(bool)), this,
          SLOT(onCheckBoxChanged()));
  connect(m_shapeIndependentTransformsCB, SIGNAL(clicked(bool)), this,
          SLOT(onCheckBoxChanged()));
  // connect(m_frameIndependentTransformsCB, SIGNAL(clicked(bool)), this,
  //         SLOT(onCheckBoxChanged()));
  connect(m_translateOnlyCB, SIGNAL(clicked(bool)), this,
          SLOT(onCheckBoxChanged()));
}

// チェックボックス
void TransformDialog::onCheckBoxChanged() {
  TransformTool* tool =
      dynamic_cast<TransformTool*>(IwTool::getTool("T_Transform"));
  if (!tool) return;

  tool->setScaleAroundCenter(m_scaleAroundCenterCB->isChecked());
  tool->setRotateAroundCenter(m_rotateAroundCenterCB->isChecked());
  tool->setShapeIndependentTransforms(
      m_shapeIndependentTransformsCB->isChecked());
  // tool->setFrameIndependentTransforms(
  //     m_frameIndependentTransformsCB->isChecked());
  tool->setTranslateOnly(m_translateOnlyCB->isChecked());
}

OpenPopupCommandHandler<TransformDialog> openTransformOptions(
    MI_TransformOptions);