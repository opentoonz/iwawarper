//---------------------------------
// Free Hand Dialog
// FreeHandToolのオプションを表示するダイアログ
// モーダルではない
//---------------------------------

#ifndef FREEHANDDIALOG_H
#define FREEHANDDIALOG_H

#include "iwdialog.h"

#include "freehandtool.h"

class QSlider;
class QLineEdit;
class QLabel;

class FreeHandDialog : public IwDialog {
  Q_OBJECT

  FreeHandTool* m_tool;

  // カーブの精度スライダ
  QSlider* m_precisionSlider;
  QLineEdit* m_precisionLineEdit;

  // 現在のカーブのコントロールポイント数
  QLabel* m_controlPointsLabel;

  // ボタン
  QPushButton* m_closeShapeButton;
  QPushButton* m_deleteShapeButton;

public:
  FreeHandDialog();

protected slots:
  void onPrecSliderMoved(int val);
  void onPrecLineEditEditingFinished();

  // コントロールポイント数の更新
  void updateCPCount(int amount);

  void onCloseShapeButtonClicked(bool);
};

#endif