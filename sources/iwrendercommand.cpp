//---------------------------------------------------
// IwRenderCommand
// Preview, Render�������N���X
//---------------------------------------------------

#include "iwrendercommand.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"

#include "ttoonzimage.h"
#include "trasterimage.h"

#include "iwrenderinstance.h"

#include "iwimagecache.h"

#include "renderprogresspopup.h"
#include "outputsettings.h"

#include <iostream>
#include <QMessageBox>
#include <QThreadPool>
#include <QCoreApplication>
#include <QDir>

IwRenderCommand::IwRenderCommand() {
  // �v���r���[
  setCommandHandler(MI_Preview, this, &IwRenderCommand::onPreview);
  setCommandHandler(MI_Render, this, &IwRenderCommand::onRender);
}

//---------------------------------------------------

void IwRenderCommand::onPreview() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project || project->getProjectFrameLength() == 0) return;

  int currentFrame = project->getViewFrame();
  if (currentFrame < 0 || currentFrame >= project->getProjectFrameLength())
    currentFrame = 0;

  int prevFrom, prevTo;
  project->getPreviewRange(prevFrom, prevTo);

  OutputSettings* settings = project->getRenderQueue()->currentOutputSettings();

  // ���݂̃t���[������^�X�N��ς�ł���
  QList<int> cuedFrames;
  if (currentFrame < prevFrom || currentFrame > prevTo) {
    cuedFrames.push_back(currentFrame);
    for (int f = prevFrom; f <= prevTo; f++) cuedFrames.append(f);
  } else {
    for (int f = currentFrame; f <= prevTo; f++) cuedFrames.append(f);
    for (int f = prevFrom; f < currentFrame; f++) cuedFrames.append(f);
  }

  for (auto frame : cuedFrames) {
    IwRenderInstance* previewTask =
        new IwRenderInstance(frame, project, settings, true);
    connect(previewTask, SIGNAL(renderStarted(int, unsigned int)),
            IwApp::instance()->getCurrentProject(),
            SLOT(onPreviewRenderStarted(int, unsigned int)),
            Qt::QueuedConnection);
    connect(previewTask, SIGNAL(renderFinished(int, unsigned int)),
            IwApp::instance()->getCurrentProject(),
            SLOT(onPreviewRenderFinished(int, unsigned int)),
            Qt::QueuedConnection);

    QThreadPool::globalInstance()->start(previewTask);
  }
}

//---------------------------------------------------

void RenderInvoke_Worker::run() {
  for (int frame : m_frames) {
    IwRenderInstance(frame, m_project, m_settings, false, m_popup).doRender();
    emit frameFinished();
  }
  // �����_�����O���I������犮���ɂ���
  // m_settings->setRenderState(OutputSettings::Done);
}

void IwRenderCommand::onRender() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  QList<OutputSettings*> activeItems = project->getRenderQueue()->activeItems();
  if (activeItems.isEmpty()) {
    QMessageBox::warning(
        0, QObject::tr("Output Settings Error"),
        QObject::tr("There is no active items in the Render Queue."));
    return;
  }

  // �v���O���X�o�[���
  RenderProgressPopup progressPopup(project);
  // progressPopup.show();
  std::cout << (void*)(&progressPopup) << std::endl;
  for (auto settings : activeItems) {
    progressPopup.startItem(settings);

    // �v�Z�͈͂����߂�
    // ���t���[���v�Z���邩
    // OutputSettings* settings            = project->getOutputSettings();
    OutputSettings::SaveRange saveRange = settings->getSaveRange();

    if (settings->getDirectory().isEmpty()) {
      QMessageBox::warning(0, QObject::tr("Output Settings Error"),
                           QObject::tr("Output directory is not set."));
      continue;
    }

    QDir dir(settings->getDirectory());
    if (!dir.exists()) {
      QMessageBox::StandardButton ret = QMessageBox::question(
          0, tr("Do you want to create folder?"),
          QString("The folder %1 does not exist.\nDo you want to create it?")
              .arg(settings->getDirectory()),
          QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
      if (ret == QMessageBox::Yes) {
        std::cout << "yes" << std::endl;
        // �t�H���_���
        bool ok = dir.mkpath(dir.path());
        if (!ok) {
          QMessageBox::critical(
              0, tr("Failed to create folder."),
              QString("Failed to create folder %1.").arg(dir.path()));
          continue;
        }
      } else {
        continue;
      }
    }

    if (saveRange.endFrame == -1)
      saveRange.endFrame = project->getProjectFrameLength() - 1;

    int frameAmount = (int)((saveRange.endFrame - saveRange.startFrame) /
                            saveRange.stepFrame) +
                      1;

    // �e�t���[���ɂ���
    QList<int> frames;
    for (int i = 0; i < frameAmount; i++) {
      // �t���[�������߂�
      int frame = saveRange.startFrame + i * saveRange.stepFrame;
      frames.append(frame);
    }

    // �}�b�g���C�������i�[
    settings->obtainMatteLayerNames();

    RenderInvoke_Worker* task =
        new RenderInvoke_Worker(frames, project, settings, &progressPopup);
    connect(task, SIGNAL(frameFinished()), &progressPopup,
            SLOT(onFrameFinished()), Qt::QueuedConnection);
    QThreadPool::globalInstance()->reserveThread();
    task->start();
    progressPopup.exec();
    QThreadPool::globalInstance()->releaseThread();

    // QThreadPool::globalInstance()->waitForDone();
  }
}

IwRenderCommand renderCommand;