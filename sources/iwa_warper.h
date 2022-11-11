#ifndef IWA_WARPER_H
#define IWA_WARPER_H

#include <QMainWindow>

class SceneViewer;
class TimeLineWindow;

class iwa_warper : public QMainWindow {
  Q_OBJECT

  // ビューア
  SceneViewer *m_mainViewer;
  // タイムライン
  TimeLineWindow *m_timeLineWindow;

public:
  iwa_warper(QWidget *parent = 0, Qt::WindowFlags flags = 0);
  ~iwa_warper();

private:
};

#endif  // IWA_WARPER_H
