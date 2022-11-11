#include "keydragtool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwtimelinekeyselection.h"
#include "keyframedata.h"
#include "iwundomanager.h"
#include "shapepair.h"
#include "iwproject.h"
#include "iwtrianglecache.h"

KeyDragTool::KeyDragTool()
    : m_isFormKeySelected(true), m_dragStart(-1), m_dragCurrent(-1) {}

KeyDragTool* KeyDragTool::instance() {
  static KeyDragTool _instance;
  return &_instance;
}

bool KeyDragTool::isCurrentRow(OneShape shape, bool isForm) {
  return (m_shape == shape) && (m_isFormKeySelected == isForm);
}

bool KeyDragTool::isDragDestination(int frame) {
  if (!isDragging()) return false;

  int offset = m_dragCurrent - m_dragStart;

  return m_selectedFrames.contains(frame - offset);
}

bool KeyDragTool::isDragging() { return m_dragStart >= 0; }

// 現在のキー選択を登録、isDraggingオン
void KeyDragTool::onClick(int frame) {
  IwTimeLineKeySelection* selection = IwTimeLineKeySelection::instance();
  if (selection->isEmpty()) return;

  m_shape             = selection->getShape();
  m_isFormKeySelected = selection->isFormKeySelected();
  m_selectedFrames.clear();
  for (int i = 0; i < selection->selectionCount(); i++) {
    int frame = selection->getFrame(i);
    if (m_isFormKeySelected &&
        m_shape.shapePairP->isFormKey(frame, m_shape.fromTo))
      m_selectedFrames.append(frame);
    else if (!m_isFormKeySelected &&
             m_shape.shapePairP->isCorrKey(frame, m_shape.fromTo))
      m_selectedFrames.append(frame);
  }

  if (!m_selectedFrames.isEmpty()) {
    m_dragStart   = frame;
    m_dragCurrent = frame;
  }
}

// ドラッグ先をシフトする
void KeyDragTool::onMove(int frame) {
  if (isDragging()) m_dragCurrent = frame;
}

// ドラッグ先が動いていれば、Undo作成して処理
void KeyDragTool::onRelease() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  if (m_dragStart != m_dragCurrent) {
    int offset = m_dragCurrent - m_dragStart;

    QList<int> afterFrames;
    for (auto f : m_selectedFrames) {
      afterFrames.append(f + offset);
    }
    KeyFrameData* data = new KeyFrameData();

    data->setKeyFrameData(m_shape, m_selectedFrames,
                          (m_isFormKeySelected) ? KeyFrameEditor::KeyMode_Form
                                                : KeyFrameEditor::KeyMode_Corr);

    TimeLineKeyEditUndo* undo =
        new TimeLineKeyEditUndo(m_shape, m_isFormKeySelected, project);

    data->getKeyFrameData(m_shape, afterFrames,
                          (m_isFormKeySelected) ? KeyFrameEditor::KeyMode_Form
                                                : KeyFrameEditor::KeyMode_Corr);

    for (auto f : m_selectedFrames) {
      if (afterFrames.contains(f)) continue;
      if (m_isFormKeySelected)
        m_shape.shapePairP->removeFormKey(f, m_shape.fromTo);
      else
        m_shape.shapePairP->removeCorrKey(f, m_shape.fromTo);
    }

    // 操作後のシェイプ情報をUndoに格納
    undo->storeKeyData(false);
    IwUndoManager::instance()->push(undo);

    // 更新シグナルをエミット
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        project->getParentShape(m_shape.shapePairP));
  }

  m_dragStart   = -1;
  m_dragCurrent = -1;
}

//------------------------------------------------------
// キー間の補間ハンドルのドラッグ
//------------------------------------------------------

InterpHandleDragTool::InterpHandleDragTool()
    : m_shape()
    , m_isForm(true)
    , m_key(-1)
    , m_nextKey(-1)
    , m_frameOffset(0.0)
    , m_interpStart(0.0) {}

InterpHandleDragTool* InterpHandleDragTool::instance() {
  static InterpHandleDragTool _instance;
  return &_instance;
}

bool InterpHandleDragTool::isDragging() { return m_key >= 0; }

void InterpHandleDragTool::onClick(OneShape shape, bool isForm,
                                   double frameOffset, int key, int nextKey) {
  m_shape   = shape;
  m_isForm  = isForm;
  m_key     = key;
  m_nextKey = nextKey;
  if (isForm)
    m_interpStart = shape.shapePairP->getBezierInterpolation(key, shape.fromTo);
  else
    m_interpStart = shape.shapePairP->getCorrInterpolation(key, shape.fromTo);

  m_frameOffset = frameOffset;
}

void InterpHandleDragTool::onMove(double mouseFramePos, bool doSnap) {
  // オフセット込みのハンドル位置
  double handlePos = mouseFramePos - m_frameOffset;
  if (doSnap) {
    // 0.5フレーム毎にスナップ
    handlePos = std::round(handlePos * 2.0) * 0.5;
  }
  // 補間値
  double newInterp =
      (handlePos - (double)(m_key + 1)) / (double)(m_nextKey - m_key - 1);
  // クランプ
  if (newInterp < 0.1)
    newInterp = 0.1;
  else if (newInterp > 0.9)
    newInterp = 0.9;
  if (m_isForm)
    m_shape.shapePairP->setBezierInterpolation(m_key, m_shape.fromTo,
                                               newInterp);
  else
    m_shape.shapePairP->setCorrInterpolation(m_key, m_shape.fromTo, newInterp);
  // 更新シグナルをエミット
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

// ドラッグ先が動いていれば、Undo作成して処理
void InterpHandleDragTool::onRelease() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  double after;
  if (m_isForm)
    after = m_shape.shapePairP->getBezierInterpolation(m_key, m_shape.fromTo);
  else
    after = m_shape.shapePairP->getCorrInterpolation(m_key, m_shape.fromTo);

  if (after == m_interpStart) {
    m_shape   = OneShape();
    m_key     = -1;
    m_nextKey = -1;
    return;
  }

  InterpHandleEditUndo* undo = new InterpHandleEditUndo(
      m_shape, m_isForm, m_key, m_interpStart, after, project);

  IwUndoManager::instance()->push(undo);

  // 更新シグナルをエミット
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  IwTriangleCache::instance()->invalidateShapeCache(
      project->getParentShape(m_shape.shapePairP));

  m_shape   = OneShape();
  m_key     = -1;
  m_nextKey = -1;
}

// 情報を取得
void InterpHandleDragTool::getInfo(OneShape& shape, bool& isForm, int& key,
                                   int& nextKey) {
  shape   = m_shape;
  isForm  = m_isForm;
  key     = m_key;
  nextKey = m_nextKey;
}

//------------------------------------------------------

InterpHandleEditUndo::InterpHandleEditUndo(OneShape& shape, bool isForm,
                                           int key, double before, double after,
                                           IwProject* project)
    : m_project(project)
    , m_shape(shape)
    , m_isForm(isForm)
    , m_key(key)
    , m_before(before)
    , m_after(after)
    , m_firstFlag(true) {}

void InterpHandleEditUndo::undo() {
  if (m_isForm)
    m_shape.shapePairP->setBezierInterpolation(m_key, m_shape.fromTo, m_before);
  else
    m_shape.shapePairP->setCorrInterpolation(m_key, m_shape.fromTo, m_before);

  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

void InterpHandleEditUndo::redo() {
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  if (m_isForm)
    m_shape.shapePairP->setBezierInterpolation(m_key, m_shape.fromTo, m_after);
  else
    m_shape.shapePairP->setCorrInterpolation(m_key, m_shape.fromTo, m_after);

  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}