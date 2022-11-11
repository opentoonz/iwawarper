//------------------------------------------------
// IwTimeLineKeySelection
// TimeLineWindow 内のキーフレームの選択
//------------------------------------------------

#include "iwtimelinekeyselection.h"

#include "iwapp.h"
#include "iwprojecthandle.h"

#include "keyframedata.h"
#include "menubarcommandids.h"
#include "keyframeeditor.h"
#include "iwproject.h"
#include "iwundomanager.h"

#include "iwshapepairselection.h"
#include "iwtrianglecache.h"

#include <QClipboard>
#include <QApplication>

IwTimeLineKeySelection::IwTimeLineKeySelection()
    : m_shape(0, 0), m_rangeSelectionStartPos(-1) {}

//-----------------------------------------------------------------------------

IwTimeLineKeySelection* IwTimeLineKeySelection::instance() {
  static IwTimeLineKeySelection _instance;
  return &_instance;
}

//------------------------------------------------

void IwTimeLineKeySelection::enableCommands() {
  enableCommand(this, MI_Copy, &IwTimeLineKeySelection::copyKeyFrames);
  enableCommand(this, MI_Paste, &IwTimeLineKeySelection::pasteKeyFrames);
  enableCommand(this, MI_Cut, &IwTimeLineKeySelection::cutKeyFrames);
  enableCommand(this, MI_Delete, &IwTimeLineKeySelection::removeKey);

  enableCommand(this, MI_Key, &IwTimeLineKeySelection::setKey);
  enableCommand(this, MI_Unkey, &IwTimeLineKeySelection::removeKey);
  enableCommand(this, MI_ResetInterpolation,
                &IwTimeLineKeySelection::resetInterpolation);
}

//------------------------------------------------

bool IwTimeLineKeySelection::isEmpty() const {
  return m_shape.shapePairP == 0 || m_selectedFrames.isEmpty();
}

//------------------------------------------------
// これまでの選択を解除して、現在のシェイプを切り替える。
//------------------------------------------------

void IwTimeLineKeySelection::setShape(OneShape shape, bool isForm) {
  if (shape == m_shape && isForm == m_isFormKeySelected) return;

  // 以下、新規キーフレームの行に切り替わった場合
  selectNone();
  m_shape                  = shape;
  m_isFormKeySelected      = isForm;
  m_rangeSelectionStartPos = -1;
}

//------------------------------------------------
// 単に選択フレームの追加
//------------------------------------------------

void IwTimeLineKeySelection::selectFrame(int selectedFrame) {
  if (!m_selectedFrames.contains(selectedFrame))
    m_selectedFrames.push_back(selectedFrame);
}

//------------------------------------------------
// 指定セルを選択から外す
//------------------------------------------------

void IwTimeLineKeySelection::unselectFrame(int frame) {
  m_selectedFrames.removeAll(frame);
}

//------------------------------------------------
// 選択の解除
//------------------------------------------------

void IwTimeLineKeySelection::selectNone() { m_selectedFrames.clear(); }

//------------------------------------------------
// フレーム範囲選択
//------------------------------------------------
void IwTimeLineKeySelection::doRangeSelect(int selectedFrame,
                                           bool ctrlPressed) {
  if (m_rangeSelectionStartPos == -1) m_rangeSelectionStartPos = selectedFrame;

  int from = (selectedFrame <= m_rangeSelectionStartPos)
                 ? selectedFrame
                 : m_rangeSelectionStartPos;
  int to   = (selectedFrame <= m_rangeSelectionStartPos)
                 ? m_rangeSelectionStartPos
                 : selectedFrame;

  if (!ctrlPressed) selectNone();
  for (int f = from; f <= to; f++) selectFrame(f);
}

//------------------------------------------------
// 選択カウント
//------------------------------------------------

int IwTimeLineKeySelection::selectionCount() { return m_selectedFrames.size(); }

//------------------------------------------------
// セルを得る
//------------------------------------------------

int IwTimeLineKeySelection::getFrame(int index) {
  if (index >= m_selectedFrames.size()) return -1;

  return m_selectedFrames.at(index);
}

//------------------------------------------------
// そのセルが選択されているか
//------------------------------------------------

bool IwTimeLineKeySelection::isFrameSelected(int frame) {
  return m_selectedFrames.contains(frame);
}

//------------------------------------------------
// コピー (Undoは無い)
//------------------------------------------------

void IwTimeLineKeySelection::copyKeyFrames() {
  // セル選択範囲が無ければreturn
  if (m_selectedFrames.isEmpty()) return;

  // KeyFrameDataを作り、データを格納
  KeyFrameData* data = new KeyFrameData();

  data->setKeyFrameData(m_shape, m_selectedFrames,
                        (m_isFormKeySelected) ? KeyFrameEditor::KeyMode_Form
                                              : KeyFrameEditor::KeyMode_Corr);

  // クリップボードにデータを入れる
  QClipboard* clipboard = QApplication::clipboard();
  clipboard->setMimeData(data, QClipboard::Clipboard);
}

//------------------------------------------------
// ペースト
//------------------------------------------------

void IwTimeLineKeySelection::pasteKeyFrames() {
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // シェイプ選択が無ければreturn
  if (!m_shape.shapePairP) return;
  // セル選択範囲が無ければreturn
  if (m_selectedFrames.isEmpty()) return;

  // クリップボードからデータを取り、KeyFrameDataかどうか確認
  QClipboard* clipboard     = QApplication::clipboard();
  const QMimeData* mimeData = clipboard->mimeData();
  const KeyFrameData* keyFrameData =
      dynamic_cast<const KeyFrameData*>(mimeData);
  // データが違っていたらreturn
  if (!keyFrameData) return;

  // ★方針：ペースト操作時のすべての選択シェイプの情報をキープする。
  // ペースト操作前のシェイプ情報をキープしておく
  TimeLineKeyEditUndo* undo =
      new TimeLineKeyEditUndo(m_shape, m_isFormKeySelected, project);

  keyFrameData->getKeyFrameData(m_shape, m_selectedFrames,
                                (m_isFormKeySelected)
                                    ? KeyFrameEditor::KeyMode_Form
                                    : KeyFrameEditor::KeyMode_Corr);

  // ペースト操作後のシェイプ情報をUndoに格納
  undo->storeKeyData(false);
  IwUndoManager::instance()->push(undo);

  // 更新シグナルをエミット
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//------------------------------------------------
// カット
//------------------------------------------------
void IwTimeLineKeySelection::cutKeyFrames() {
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // シェイプ選択が無ければreturn
  if (!m_shape.shapePairP) return;
  // フレーム選択範囲が無ければreturn
  if (m_selectedFrames.isEmpty()) return;

  // コピーして、消せるセルは消す
  copyKeyFrames();

  // 方針：カット操作時のすべての選択シェイプの情報をキープする。
  // カット操作前のシェイプ情報をキープしておく
  TimeLineKeyEditUndo* undo =
      new TimeLineKeyEditUndo(m_shape, m_isFormKeySelected, project);
  int firstFormKeyFrame = -1;
  int firstCorrKeyFrame = -1;
  // 各セルを見る
  for (int f = 0; f < m_selectedFrames.size(); f++) {
    // 一つ目のキーフレームはスキップ
    // 形状キー
    if (m_isFormKeySelected) {
      // キーのとき
      if (m_shape.shapePairP->isFormKey(m_selectedFrames.at(f),
                                        m_shape.fromTo)) {
        // 一つ目は消さずにインデックスをキープしておく
        if (firstFormKeyFrame == -1) firstFormKeyFrame = m_selectedFrames.at(f);
        // それ以外のキーフレームは消す
        else {
          m_shape.shapePairP->removeFormKey(m_selectedFrames.at(f),
                                            m_shape.fromTo);
          // 変更されるフレームをinvalidate
          int start, end;
          m_shape.shapePairP->getFormKeyRange(
              start, end, m_selectedFrames.at(f), m_shape.fromTo);
          IwTriangleCache::instance()->invalidateCache(
              start, end, project->getParentShape(m_shape.shapePairP));
        }
      }
    }
    // 対応点キー
    else {
      // キーのとき
      if (m_shape.shapePairP->isCorrKey(m_selectedFrames.at(f),
                                        m_shape.fromTo)) {
        // 一つ目は消さずにインデックスをキープしておく
        if (firstCorrKeyFrame == -1) firstCorrKeyFrame = m_selectedFrames.at(f);
        // それ以外のキーフレームは消す
        else {
          m_shape.shapePairP->removeCorrKey(m_selectedFrames.at(f),
                                            m_shape.fromTo);
          // 変更されるフレームをinvalidate
          int start, end;
          m_shape.shapePairP->getCorrKeyRange(
              start, end, m_selectedFrames.at(f), m_shape.fromTo);
          IwTriangleCache::instance()->invalidateCache(
              start, end, project->getParentShape(m_shape.shapePairP));
        }
      }
    }

    f++;
  }

  // シェイプのキーが残り１つになったら消さない
  // 形状キー
  if (m_isFormKeySelected &&
      firstFormKeyFrame != -1 &&  // 先頭キーが見つかっている
      m_shape.shapePairP->getFormKeyFrameAmount(m_shape.fromTo) !=
          1)  // 残りキーが２つ以上
  {
    m_shape.shapePairP->removeFormKey(firstFormKeyFrame, m_shape.fromTo);
    // 変更されるフレームをinvalidate
    int start, end;
    m_shape.shapePairP->getFormKeyRange(start, end, firstFormKeyFrame,
                                        m_shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, project->getParentShape(m_shape.shapePairP));
  }

  // 対応点キー
  if (!m_isFormKeySelected &&
      firstCorrKeyFrame != -1 &&  // 先頭キーが見つかっている
      m_shape.shapePairP->getCorrKeyFrameAmount(m_shape.fromTo) !=
          1)  // 残りキーが２つ以上
  {
    m_shape.shapePairP->removeCorrKey(firstCorrKeyFrame, m_shape.fromTo);
    // 変更されるフレームをinvalidate
    int start, end;
    m_shape.shapePairP->getCorrKeyRange(start, end, firstFormKeyFrame,
                                        m_shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, project->getParentShape(m_shape.shapePairP));
  }

  // カット操作後のシェイプ情報をUndoに格納
  undo->storeKeyData(false);
  IwUndoManager::instance()->push(undo);

  // 更新シグナルをエミット
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//------------------------------------------------
// 選択セルをキーフレームにする
//------------------------------------------------
void IwTimeLineKeySelection::setKey() {
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // シェイプ選択が無ければreturn
  if (!m_shape.shapePairP) return;
  // セル選択範囲が無ければreturn
  if (m_selectedFrames.isEmpty()) return;

  // 何かキーを追加した場合のフラグ
  bool somethingChanged = false;

  // Undoに格納するデータ
  // QList<SetTimeLineKeyUndo::SetKeyData> setKeyDataList;
  SetTimeLineKeyUndo::SetKeyData setKeyData;
  setKeyData.shape = m_shape;
  setKeyData.corrKeyFrames.clear();
  setKeyData.formKeyFrames.clear();

  // 各セルを見る
  for (int f = 0; f < m_selectedFrames.size(); f++) {
    int tmpFrame = m_selectedFrames.at(f);

    // 形状キー
    //  キーでないときのみ追加
    if (m_isFormKeySelected &&
        !m_shape.shapePairP->isFormKey(tmpFrame, m_shape.fromTo)) {
      setKeyData.formKeyFrames[tmpFrame] =
          m_shape.shapePairP->getBezierPointList(tmpFrame, m_shape.fromTo);
      somethingChanged = true;
    }

    // 対応点キー
    //  キーでないときのみ追加
    if (!m_isFormKeySelected &&
        !m_shape.shapePairP->isCorrKey(tmpFrame, m_shape.fromTo)) {
      setKeyData.corrKeyFrames[tmpFrame] =
          m_shape.shapePairP->getCorrPointList(tmpFrame, m_shape.fromTo);
      somethingChanged = true;
    }
  }
  if (!somethingChanged) return;

  // Undo追加 redoが呼ばれ、キーがセットされる
  IwUndoManager::instance()->push(new SetTimeLineKeyUndo(setKeyData, project));
}

//------------------------------------------------
// 選択セルのキーフレームを解除する
//------------------------------------------------
void IwTimeLineKeySelection::removeKey() {
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // シェイプ選択が無ければreturn
  if (!m_shape.shapePairP) return;
  // セル選択範囲が無ければreturn
  if (m_selectedFrames.isEmpty()) return;

  // 何かキーを追加した場合のフラグ
  bool somethingChanged = false;

  // Undoに格納するデータ
  UnsetTimeLineKeyUndo::UnsetKeyData unsetKeyData;
  unsetKeyData.shape = m_shape;

  // 各セルを見る
  for (int f = 0; f < m_selectedFrames.size(); f++) {
    int tmpFrame = m_selectedFrames.at(f);

    // 形状キー
    //  キーのときのみ追加
    if (m_isFormKeySelected &&
        m_shape.shapePairP->isFormKey(tmpFrame, m_shape.fromTo)) {
      unsetKeyData.formKeyFrames[tmpFrame] =
          m_shape.shapePairP->getBezierPointList(tmpFrame, m_shape.fromTo);
      somethingChanged = true;
    }

    // 対応点キー
    //  キーのときのみ追加
    if (!m_isFormKeySelected &&
        m_shape.shapePairP->isCorrKey(tmpFrame, m_shape.fromTo)) {
      unsetKeyData.corrKeyFrames[tmpFrame] =
          m_shape.shapePairP->getCorrPointList(tmpFrame, m_shape.fromTo);
      somethingChanged = true;
    }
  }

  if (!somethingChanged) return;

  // Undo追加 redoが呼ばれ、キーがセットされる
  IwUndoManager::instance()->push(
      new UnsetTimeLineKeyUndo(unsetKeyData, project));

  // 更新シグナルをエミット
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
// 補間を0.5(線形)に戻す
//---------------------------------------------------
void IwTimeLineKeySelection::resetInterpolation() {
  if (m_selectedFrames.isEmpty()) return;
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  QMap<int, double> interps;
  QList<int> frames = m_selectedFrames;
  std::sort(frames.begin(), frames.end());
  for (auto frame : frames) {
    // 形状キーの場合
    if (m_isFormKeySelected &&
        m_shape.shapePairP->isFormKey(frame, m_shape.fromTo)) {
      double interp =
          m_shape.shapePairP->getBezierInterpolation(frame, m_shape.fromTo);
      if (interp != 0.5) interps.insert(frame, interp);
    }
    if (!m_isFormKeySelected &&
        m_shape.shapePairP->isCorrKey(frame, m_shape.fromTo)) {
      double interp =
          m_shape.shapePairP->getCorrInterpolation(frame, m_shape.fromTo);
      if (interp != 0.5) interps.insert(frame, interp);
    }
  }
  // 最初のフレームがキーフレームでなかった場合、そのセグメントのキーについても判定する
  int firstFrame = frames.at(0);
  if (m_isFormKeySelected &&
      !m_shape.shapePairP->isFormKey(firstFrame, m_shape.fromTo)) {
    int belongingKey =
        m_shape.shapePairP->belongingFormKey(firstFrame, m_shape.fromTo);
    if (belongingKey != -1) {
      double interp = m_shape.shapePairP->getBezierInterpolation(
          belongingKey, m_shape.fromTo);
      if (interp != 0.5) interps.insert(belongingKey, interp);
    }
  } else if (!m_isFormKeySelected &&
             !m_shape.shapePairP->isCorrKey(firstFrame, m_shape.fromTo)) {
    int belongingKey =
        m_shape.shapePairP->belongingCorrKey(firstFrame, m_shape.fromTo);
    if (belongingKey != -1) {
      double interp = m_shape.shapePairP->getCorrInterpolation(belongingKey,
                                                               m_shape.fromTo);
      if (interp != 0.5) interps.insert(belongingKey, interp);
    }
  }

  if (interps.isEmpty()) return;

  // Undo追加 redoが呼ばれ、補間値が0.5にリセットされる
  IwUndoManager::instance()->push(new ResetInterpolationUndo(
      m_shape, m_isFormKeySelected, interps, project));

  // 更新シグナルをエミット
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
// 以下、Undoコマンド
//---------------------------------------------------

TimeLineKeyEditUndo::TimeLineKeyEditUndo(OneShape& shape, bool isForm,
                                         IwProject* project)
    : m_project(project)
    , m_shape(shape)
    , m_isFormKeyEdited(isForm)
    , m_firstFlag(true) {
  // 操作前のキー情報を格納
  storeKeyData(true);
}

//---------------------------------------------------

void TimeLineKeyEditUndo::storeKeyData(bool before) {
  KeyData keyData;

  // 形状キー
  if (m_isFormKeyEdited)
    keyData.formData = m_shape.shapePairP->getFormData(m_shape.fromTo);

  // 対応点キー
  else
    keyData.corrData = m_shape.shapePairP->getCorrData(m_shape.fromTo);

  // キー情報を格納
  if (before)
    m_beforeKeys = keyData;
  else
    m_afterKeys = keyData;
}

//---------------------------------------------------

void TimeLineKeyEditUndo::undo() {
  // 形状キー
  if (m_isFormKeyEdited)
    m_shape.shapePairP->setFormData(m_beforeKeys.formData, m_shape.fromTo);

  // 対応点キー
  else
    m_shape.shapePairP->setCorrData(m_beforeKeys.corrData, m_shape.fromTo);
  //}

  // カレントプロジェクトなら
  if (m_project->isCurrent()) {
    // 影響したシェイプを選択する
    IwShapePairSelection::instance()->selectOneShape(m_shape);
    IwShapePairSelection::instance()->makeCurrent();

    // 更新シグナルをエミット
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

//---------------------------------------------------

void TimeLineKeyEditUndo::redo() {
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  // 形状キー
  if (m_isFormKeyEdited)
    m_shape.shapePairP->setFormData(m_afterKeys.formData, m_shape.fromTo);
  // 対応点キー
  else
    m_shape.shapePairP->setCorrData(m_afterKeys.corrData, m_shape.fromTo);

  // カレントプロジェクトなら
  if (m_project->isCurrent()) {
    // 影響したシェイプを選択する
    IwShapePairSelection::instance()->selectOneShape(m_shape);
    IwShapePairSelection::instance()->makeCurrent();
    // 更新シグナルをエミット
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

//------------------------------------------------
// 選択セルをキーフレームにする のUndo
//------------------------------------------------
SetTimeLineKeyUndo::SetTimeLineKeyUndo(SetKeyData& setKeyData,
                                       IwProject* project)
    : m_setKeyData(setKeyData), m_project(project) {}

// キーフレームを解除する
void SetTimeLineKeyUndo::undo() {
  // シェイプ
  OneShape shape = m_setKeyData.shape;
  // 形状キーを入れたフレーム
  for (auto formKey : m_setKeyData.formKeyFrames.keys())
    shape.shapePairP->removeFormKey(formKey, shape.fromTo);
  // 対応点キーを入れたフレーム
  for (auto corrKey : m_setKeyData.corrKeyFrames.keys())
    shape.shapePairP->removeCorrKey(corrKey, shape.fromTo);

  // このプロジェクトがカレントなら更新シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(shape.shapePairP));
  }
}

// キーフレームをセットする
void SetTimeLineKeyUndo::redo() {
  // シェイプ
  OneShape shape = m_setKeyData.shape;
  // 形状キーを入れたフレーム
  QMap<int, BezierPointList>::const_iterator i =
      m_setKeyData.formKeyFrames.constBegin();
  while (i != m_setKeyData.formKeyFrames.constEnd()) {
    shape.shapePairP->setFormKey(i.key(), shape.fromTo, i.value());
    i++;
  }
  // 対応点キーを入れたフレーム
  QMap<int, CorrPointList>::const_iterator j =
      m_setKeyData.corrKeyFrames.constBegin();
  while (j != m_setKeyData.corrKeyFrames.constEnd()) {
    shape.shapePairP->setCorrKey(j.key(), shape.fromTo, j.value());
    j++;
  }

  // このプロジェクトがカレントなら更新シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(shape.shapePairP));
  }
}

//------------------------------------------------
// 選択セルのキーフレームを解除する のUndo
//------------------------------------------------
UnsetTimeLineKeyUndo::UnsetTimeLineKeyUndo(UnsetKeyData& unsetKeyData,
                                           IwProject* project)
    : m_unsetKeyData(unsetKeyData), m_project(project) {}

// キーフレームをセットする
void UnsetTimeLineKeyUndo::undo() {
  // シェイプ
  OneShape shape = m_unsetKeyData.shape;
  // 形状キーを消したフレーム
  QMap<int, BezierPointList> formKey = m_unsetKeyData.formKeyFrames;
  if (!formKey.isEmpty()) {
    QMap<int, BezierPointList>::iterator i = formKey.begin();
    while (i != formKey.end()) {
      shape.shapePairP->setFormKey(i.key(), shape.fromTo, i.value());
      ++i;
    }
  }
  // 対応点キーを消したフレーム
  QMap<int, CorrPointList> corrKey = m_unsetKeyData.corrKeyFrames;
  if (!corrKey.isEmpty()) {
    QMap<int, CorrPointList>::iterator i = corrKey.begin();
    while (i != corrKey.end()) {
      shape.shapePairP->setCorrKey(i.key(), shape.fromTo, i.value());
      ++i;
    }
  }

  // このプロジェクトがカレントなら更新シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(shape.shapePairP));
  }
}

// キーフレームを解除する (最後の1つは消さない)
void UnsetTimeLineKeyUndo::redo() {
  // シェイプ
  OneShape shape = m_unsetKeyData.shape;
  // 形状キーを消したフレーム
  QMap<int, BezierPointList> formKey = m_unsetKeyData.formKeyFrames;
  if (!formKey.isEmpty()) {
    QList<int> formKeyFrames = formKey.keys();
    for (int f = 0; f < formKeyFrames.size(); f++)
      shape.shapePairP->removeFormKey(formKeyFrames.at(f),
                                      shape.fromTo);  // (最後の1つは消さない)
  }
  // 対応点キーを消したフレーム
  QMap<int, CorrPointList> corrKey = m_unsetKeyData.corrKeyFrames;
  if (!corrKey.isEmpty()) {
    QList<int> corrKeyFrames = corrKey.keys();
    for (int f = 0; f < corrKeyFrames.size(); f++)
      shape.shapePairP->removeCorrKey(corrKeyFrames.at(f),
                                      shape.fromTo);  // (最後の1つは消さない)
  }

  // このプロジェクトがカレントなら更新シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(shape.shapePairP));
  }
}

//------------------------------------------------
// 補間値を0.5にリセット のUndo
//------------------------------------------------
ResetInterpolationUndo::ResetInterpolationUndo(OneShape& shape, bool isForm,
                                               QMap<int, double> interps,
                                               IwProject* project)
    : m_shape(shape)
    , m_isForm(isForm)
    , m_interp_before(interps)
    , m_project(project) {}

// beforeの値にする
void ResetInterpolationUndo::undo() {
  QMap<int, double>::const_iterator i = m_interp_before.constBegin();
  while (i != m_interp_before.constEnd()) {
    if (m_isForm)
      m_shape.shapePairP->setBezierInterpolation(i.key(), m_shape.fromTo,
                                                 i.value());
    else
      m_shape.shapePairP->setCorrInterpolation(i.key(), m_shape.fromTo,
                                               i.value());
    ++i;
  }
  // このプロジェクトがカレントなら更新シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

// 全て0.5にする
void ResetInterpolationUndo::redo() {
  for (auto key : m_interp_before.keys()) {
    if (m_isForm)
      m_shape.shapePairP->setBezierInterpolation(key, m_shape.fromTo, 0.5);
    else
      m_shape.shapePairP->setCorrInterpolation(key, m_shape.fromTo, 0.5);
  }
  // このプロジェクトがカレントなら更新シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}
