//---------------------------------------------------
// IwProjectHandle
// 現在のプロジェクト情報を管理する。
//---------------------------------------------------

#include "iwprojecthandle.h"
#include "iwproject.h"

#include "iwrenderinstance.h"
#include "iwimagecache.h"
#include "iwtrianglecache.h"

IwProjectHandle::IwProjectHandle() : m_project(0), m_dirtyFlag(false) {}

//-----------------------------------------------------------------------------

IwProjectHandle::~IwProjectHandle() {}

//-----------------------------------------------------------------------------

IwProject* IwProjectHandle::getProject() const { return m_project; }

//-----------------------------------------------------------------------------

void IwProjectHandle::setProject(IwProject* project) {
  if (m_project == project) return;

  IwImageCache::instance()->clear();
  IwTriangleCache::instance()->removeAllCache();

  m_runningInstances.clear();
  m_project = project;
  // projectが切り替わったらシグナルをエミット
  if (m_project) emit projectSwitched();
}

//-----------------------------------------------------------------------------

void IwProjectHandle::onSequenceChanged() {
  if (m_project) m_project->initMultiFrames();
}

//-----------------------------------------------------------------------------

void IwProjectHandle::setDirty(bool dirty) {
  if (m_dirtyFlag == dirty) return;
  m_dirtyFlag = dirty;
  emit dirtyFlagChanged();
}

//-----------------------------------------------------------------------------

void IwProjectHandle::onPreviewRenderStarted(int frame, unsigned int taskId) {
  if (m_runningInstances.contains(frame)) {
    // 中止の命令を出す?
  }
  m_runningInstances[frame] = taskId;
  emit previewRenderStarted(frame);
}

void IwProjectHandle::onPreviewRenderFinished(int frame, unsigned int taskId) {
  // discard the result
  if (!m_runningInstances.contains(frame) ||
      m_runningInstances.value(frame) != taskId)
    return;
  m_runningInstances.remove(frame);
  std::cout << "preview render finished : frame " << frame << std::endl;
  emit previewRenderFinished(frame);
}

void IwProjectHandle::suspendRender(int frame) {
  if (!m_runningInstances.contains(frame)) return;
  // 中止の命令を出す？
  m_runningInstances.remove(frame);
}

bool IwProjectHandle::isRunningPreview(int frame) {
  return (m_runningInstances.contains(frame));
}