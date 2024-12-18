//---------------------------------
// Output Settings Dialog
// �����_�����O���ʂ̕ۑ��t�@�C���`��/�p�X���w�肷��
//---------------------------------
#ifndef OUTPUTSETTINGSDIALOG_H
#define OUTPUTSETTINGSDIALOG_H

#include "iwdialog.h"

#include <QListWidget>

class QComboBox;
class QLineEdit;
class QCheckBox;
class QLabel;
class OutputSettings;

class RenderQueueListWidget : public QListWidget {
  Q_OBJECT
public:
  RenderQueueListWidget(QWidget* parent = nullptr) : QListWidget(parent) {}

protected:
  QItemSelectionModel::SelectionFlags selectionCommand(
      const QModelIndex& index, const QEvent* event = nullptr) const override;
};

class OutputSettingsDialog : public IwDialog {
  Q_OBJECT

  RenderQueueListWidget* m_itemList;

  QLineEdit* m_startFrameEdit;
  QLineEdit* m_endFrameEdit;
  QLineEdit* m_stepFrameEdit;

  QComboBox* m_shapeTagCombo;

  QComboBox* m_saverCombo;
  QLineEdit* m_directoryEdit;

  QLineEdit* m_initialFrameNumberEdit;
  QLineEdit* m_incrementEdit;
  QLineEdit* m_numberOfDigitsEdit;

  QLabel* m_warningLabel;

  QLineEdit* m_formatEdit;
  QLabel* m_exampleLabel;

  QPushButton* m_removeTaskBtn;

  OutputSettings* getCurrentSettings();

public:
  OutputSettingsDialog();

protected:
  // project���؂�ւ������A���e���X�V����
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);

protected slots:

  // ���݂�OutputSettings�ɍ��킹�ĕ\�����X�V����
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
  void onFormatEditted();

  void updateShapeTagComboItems();
  void onShapeTagComboActivated();

  void onTaskClicked(QListWidgetItem*);
  void onAddTaskButtonClicked();
  void onRemoveTaskButtonClicked();

  void onRenderButtonClicked();
};

#endif
