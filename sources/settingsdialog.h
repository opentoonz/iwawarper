//------------------------------------
// SettingsDialog
//------------------------------------
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "iwdialog.h"

class QComboBox;
class QLineEdit;
class MyIntSlider;
class QCheckBox;

class SettingsDialog : public IwDialog {
  Q_OBJECT

  // Preference���
  QComboBox* m_bezierPrecisionCombo;
  // RenderSettings���
  MyIntSlider* m_warpPrecisionSlider;
  MyIntSlider* m_faceSizeThresSlider;
  QComboBox* m_alphaModeCombo;
  QComboBox* m_resampleModeCombo;
  MyIntSlider* m_imageShrinkSlider;
  QCheckBox* m_antialiasCheckBox;

public:
  SettingsDialog();

protected:
  // project���؂�ւ������A���e���X�V����
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);

protected slots:
  void onProjectSwitched();

  // Preference���
  void onBezierPrecisionComboChanged(int index);
  // RenderSettings���
  void onPrecisionValueChanged(bool isDragging);
  void onFaceSizeValueChanged(bool isDragging);
  void onAlphaModeComboActivated(int index);
  void onResampleModeComboActivated();
  void onImageShrinkChanged(bool isDragging);
  void onAntialiasClicked(bool on);
};

#endif