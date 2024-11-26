#pragma once
#ifndef MATTEINFODIALOG_H
#define MATTEINFODIALOG_H

#include "iwdialog.h"

class ShapePair;
class QComboBox;
class QListWidget;
class QLineEdit;
class QListWidgetItem;
class IwSelection;

class MatteInfoDialog : public IwDialog {
  Q_OBJECT
  QList<ShapePair*> m_shapes;

  QComboBox* m_layerCombo;
  QListWidget* m_colorListWidget;
  QPushButton *m_addColorBtn, *m_removeColorBtn, *m_savePresetBtn,
      *m_loadPresetBtn;
  QLineEdit* m_toleranceEdit;

public:
  MatteInfoDialog();

protected:
  void showEvent(QShowEvent*) final override;
  void hideEvent(QHideEvent*) final override;

protected slots:
  void onLayerComboActivated();
  void onColorItemDoubleClicked(QListWidgetItem* item);
  void onAddColorClicked();
  void onRemoveColorClicked();
  void onSavePresetClicked();
  void onLoadPresetClicked();
  void onToleranceEdited();

  void onSelectionChanged(IwSelection* selection);
};

#endif