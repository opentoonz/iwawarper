//---------------------------------
// Output Settings Dialog
// レンダリング結果の保存ファイル形式/パスを指定する
//---------------------------------
#ifndef OUTPUTSETTINGSDIALOG_H
#define OUTPUTSETTINGSDIALOG_H

#include "iwdialog.h"

class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;
class OutputSettings;

class OutputSettingsDialog : public IwDialog {
  Q_OBJECT

  QLineEdit* m_startFrameEdit;
  QLineEdit* m_endFrameEdit;
  QLineEdit* m_stepFrameEdit;

  QComboBox* m_shapeTagCombo;

  QComboBox* m_saverCombo;
  QLineEdit* m_directoryEdit;

  QLineEdit* m_initialFrameNumberEdit;
  QLineEdit* m_incrementEdit;
  QLineEdit* m_numberOfDigitsEdit;
  QLineEdit* m_extensionEdit;

  QCheckBox* m_useSourceCB;
  QCheckBox* m_addNumberCB;
  QCheckBox* m_replaceExtCB;

  QLineEdit* m_formatEdit;
  QLabel* m_exampleLabel;

  OutputSettings* getCurrentSettings();

public:
  OutputSettingsDialog();

protected:
  // projectが切り替わったら、内容を更新する
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);

protected slots:

  // 現在のOutputSettingsに合わせて表示を更新する
  void updateGuis();

  void onStartFrameEditted();
  void onEndFrameEditted();
  void onStepFrameEditted();

  void onSaverComboActivated(const QString& text);
  void onParametersBtnClicked();
  void onDirectoryEditted();
  void onOpenBrowserBtnClicked();
  void onInitialFrameNumberEditted();
  void onIncrementEditted();
  void onNumberOfDigitsEditted();
  // チェックボックスは３つまとめて同じSLOTにする
  void onCheckBoxClicked();
  void onFormatEditted();

  void updateShapeTagComboItems();
  void onShapeTagComboActivated();

  void onRenderButtonClicked();
};

#endif
