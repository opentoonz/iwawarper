#pragma once
//---------------------------------
// Render Shape Image Dialog
// シェイプ画像の書き出し設定
//---------------------------------

#ifndef RENDERSHAPEIMAGEDIALOG_H
#define RENDERSHAPEIMAGEDIALOG_H

#include "iwdialog.h"

class QLineEdit;
class QComboBox;
class OutputSettings;

class RenderShapeImageDialog : public IwDialog {
  Q_OBJECT

  QLineEdit* m_startFrameEdit;
  QLineEdit* m_endFrameEdit;
  QLineEdit* m_fileNameEdit;

  QLineEdit* m_directoryEdit;

  QComboBox* m_sizeCombo;

  OutputSettings* getCurrentSettings();

public:
  RenderShapeImageDialog();

protected:
  // projectが切り替わったら、内容を更新する
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);

protected slots:

  // 現在のOutputSettingsに合わせて表示を更新する
  void updateGuis();
  void onStartFrameEditted();
  void onEndFrameEditted();
  void onFileNameEditted();
  void onDirectoryEditted();
  void onOpenBrowserBtnClicked();
  void onSizeComboActivated();
  void onRenderButtonClicked();
};

#endif