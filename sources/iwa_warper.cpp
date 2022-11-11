#include "iwa_warper.h"

#include "sceneviewer.h"
#include "timelinewindow.h"

iwa_warper::iwa_warper(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags) {
  //--- オブジェクトの宣言

  // ビューア
  m_mainViewer = new SceneViewer(this);
  // タイムライン
  m_timeLineWindow = new TimeLineWindow(this);

  // Widgetのセット
  setCentralWidget(m_mainViewer);

  addDockWidget(Qt::BottomDockWidgetArea, m_timeLineWindow);
}

iwa_warper::~iwa_warper() {}
