//---------------------------------
// Render Settings Dialog
// モーフィング、レンダリングの設定を行う
//---------------------------------
#ifndef RENDERSETTINGSDIALOG_H
#define RENDERSETTINGSDIALOG_H

#include "iwdialog.h"

class QComboBox;

class RenderSettingsDialog : public IwDialog {
  Q_OBJECT

  QComboBox* m_warpPrecisionCombo;

public:
  RenderSettingsDialog();

protected:
  // projectが切り替わったら、内容を更新する
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);
protected slots:
  // 現在のOutputSettingsに合わせて表示を更新する
  void updateGuis();

  void onPrecisionComboActivated(int index);
};

#endif