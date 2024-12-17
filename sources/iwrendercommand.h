//---------------------------------------------------
// IwRenderCommand
// Preview, Render‚ðˆµ‚¤ƒNƒ‰ƒX
//---------------------------------------------------
#ifndef IWRENDERCOMMAND_H
#define IWRENDERCOMMAND_H

#include <QObject>
#include <QThread>
#include <QList>
class IwProject;
class RenderProgressPopup;
class OutputSettings;
//---------------------------------------------------

class RenderInvoke_Worker : public QThread {
  Q_OBJECT
  QList<int> m_frames;
  IwProject* m_project;
  RenderProgressPopup* m_popup = nullptr;
  OutputSettings* m_settings;
  void run() override;

public:
  RenderInvoke_Worker(QList<int> frames, IwProject* project,
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