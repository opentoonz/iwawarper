//----------------------------------
// MainStatusBar
// メインウィンドウのステータスバー
//----------------------------------
#ifndef MAINSTATUSBAR_H
#define MAINSTATUSBAR_H

#include <QToolBar>

class QStatusBar;
class QLineEdit;
class QSlider;
class QIntValidator;

class MainStatusBar : public QToolBar {
  Q_OBJECT

  QStatusBar* m_status;
  QLineEdit* m_frameEdit;
  QSlider* m_frameSlider;

  QIntValidator* m_validator;

public:
  MainStatusBar(QWidget* parent);

  // ステータスバーにメッセージを表示する
  void showMessageOnStatusBar(const QString& message, int timeout = 0);

protected slots:
  // プロジェクト長が変わったらスライダの範囲を更新する
  void onProjectChanged();
  // カレントフレームを変更する操作
  void onFrameSliderMoved(int);
  void onFrameEditFinished();
  // 他の操作（KeyFrameEditorなど）でカレントフレームが変わった時、スライダを同期する]
  void onViewFrameChanged();
};

#endif