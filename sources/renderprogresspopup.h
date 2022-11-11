//---------------------------------------------------
// RenderProgressPopup
// レンダリングの進行状況を示すポップアップ
//---------------------------------------------------

#ifndef RENDERPROGRESSPOPUP_H
#define RENDERPROGRESSPOPUP_H

#include <QDialog>

class QLabel;
class QProgressBar;
class IwProject;

class RenderProgressPopup : public QDialog {
  Q_OBJECT
  bool m_isCanceled;
  IwProject* m_project;
  QLabel* m_statusLabel;
  QProgressBar* m_progress;

public:
  RenderProgressPopup(IwProject* project);

  bool isCanceled() { return m_isCanceled; }

public slots:
  void onCancelButtonClicked();
  void onFrameFinished();
};

#endif