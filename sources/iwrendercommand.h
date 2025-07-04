//---------------------------------------------------
// IwRenderCommand
// Preview, Render‚ðˆµ‚¤ƒNƒ‰ƒX
//---------------------------------------------------
#ifndef IWRENDERCOMMAND_H
#define IWRENDERCOMMAND_H

#include <QObject>
#include <QThread>
#include <QList>
#include <QPair>
class IwProject;
class RenderProgressPopup;
class OutputSettings;
//---------------------------------------------------

class RenderInvoke_Worker : public QThread {
  Q_OBJECT
  QList<QPair<int, int>> m_frames;
  IwProject* m_project;
  RenderProgressPopup* m_popup = nullptr;
  OutputSettings* m_settings;
  void run() override;

public:
  RenderInvoke_Worker(QList<QPair<int, int>> frames, IwProject* project,
                      OutputSettings* settings, RenderProgressPopup* popup)
      : m_frames(frames)
      , m_project(project)
      , m_popup(popup)
      , m_settings(settings) {}

signals:
  void frameFinished();
};

//---------------------------------------------------

class IwRenderCommand : public QObject {
  Q_OBJECT
public:
  IwRenderCommand();

  void onPreview();
  void onRender();
};

#endif