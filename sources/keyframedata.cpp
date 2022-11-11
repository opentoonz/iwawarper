//----------------------------------------------------
// KeyFrameData
// KeyFrameEditor内のコピー/ペーストで扱うデータ
//----------------------------------------------------

#include "keyframedata.h"
#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"

#include "shapepair.h"
#include "iwtrianglecache.h"

KeyFrameData::KeyFrameData() {}

//----------------------------------------------------

KeyFrameData::KeyFrameData(const KeyFrameData* src) { m_data = src->m_data; }

//----------------------------------------------------
// 選択範囲からデータを格納する
//----------------------------------------------------

void KeyFrameData::setKeyFrameData(OneShape& shape, QList<int>& selectedFrames,
                                   KeyFrameEditor::KEYMODE keymode) {
  // 現在の選択範囲から コピー基準点（一番左のフレーム）を得る
  int baseFrame = 10000;
  for (int f = 0; f < selectedFrames.size(); f++) {
    // 最小値の更新
    if (selectedFrames.at(f) < baseFrame) baseFrame = selectedFrames.at(f);
  }

  for (int f = 0; f < selectedFrames.size(); f++) {
    CellData cellData;
    int frame        = selectedFrames.at(f);
    cellData.pos     = QPoint(frame - baseFrame, 0);
    cellData.keymode = keymode;

    cellData.shapeWasClosed = shape.shapePairP->isClosed();
    cellData.bpCount        = shape.shapePairP->getBezierPointAmount();
    cellData.cpCount        = shape.shapePairP->getCorrPointAmount();

    // 形状キーフレームで、かつモードがForm/Bothの場合、キー情報を格納
    if (shape.shapePairP->isFormKey(frame, shape.fromTo) &&
        (keymode == KeyFrameEditor::KeyMode_Form ||
         keymode == KeyFrameEditor::KeyMode_Both)) {
      cellData.bpList =
          shape.shapePairP->getBezierPointList(frame, shape.fromTo);
      cellData.b_interp =
          shape.shapePairP->getBezierInterpolation(frame, shape.fromTo);
    }

    // 対応点キーフレームで、かつモードがCorr/Bothの場合、キー情報を格納
    if (shape.shapePairP->isCorrKey(frame, shape.fromTo) &&
        (keymode == KeyFrameEditor::KeyMode_Corr ||
         keymode == KeyFrameEditor::KeyMode_Both)) {
      cellData.cpList = shape.shapePairP->getCorrPointList(frame, shape.fromTo);
      cellData.c_interp =
          shape.shapePairP->getCorrInterpolation(frame, shape.fromTo);
    }

    m_data.push_back(cellData);
  }
}

//----------------------------------------------------
// 指定セルにデータを上書きペーストする
//----------------------------------------------------

void KeyFrameData::getKeyFrameData(OneShape& shape, QList<int>& selectedFrames,
                                   KeyFrameEditor::KEYMODE keymode) const {
  // セル選択範囲が無ければreturn
  if (selectedFrames.isEmpty()) return;

  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // 形状キー
  bool doFormPaste = (keymode == KeyFrameEditor::KeyMode_Form ||
                      keymode == KeyFrameEditor::KeyMode_Both);
  // 対応点キー
  bool doCorrPaste = (keymode == KeyFrameEditor::KeyMode_Corr ||
                      keymode == KeyFrameEditor::KeyMode_Both);

  // 現在の選択範囲から、ペースト基準点（一番左のフレーム）を得る
  int baseFrame = 10000;
  for (int f = 0; f < selectedFrames.size(); f++) {
    // 最小値の更新
    if (selectedFrames.at(f) < baseFrame) baseFrame = selectedFrames.at(f);
  }

  // コピーされた各セルについて、対象セルにペーストが可能かどうかの判断を行う
  QList<bool> pasteAvailability;
  for (int d = 0; d < m_data.size(); d++) {
    CellData cellData = m_data.at(d);

    // ペースト対象フレーム座標
    int pasteFrame = baseFrame + cellData.pos.x();

    // 条件① Open/Closeが違ったらそもそもペーストできない。return
    if (cellData.shapeWasClosed != shape.shapePairP->isClosed()) return;

    // 座標が現在のセル表示をはみ出していたらfalseにしてcontinue
    if (pasteFrame < 0 || pasteFrame >= project->getProjectFrameLength()) {
      pasteAvailability.push_back(false);
      continue;
    }

    // モード毎に判定を分ける
    //  形状キーの操作
    //  条件② コントロールポイント数が同じ
    if (doFormPaste &&
        (cellData.keymode == KeyFrameEditor::KeyMode_Form ||
         cellData.keymode == KeyFrameEditor::KeyMode_Both) &&
        shape.shapePairP->getBezierPointAmount() != cellData.bpCount) {
      return;
    }

    // 対応点キーの操作
    //  条件② Corr点数が同じ
    if (doCorrPaste &&
        (cellData.keymode == KeyFrameEditor::KeyMode_Corr ||
         cellData.keymode == KeyFrameEditor::KeyMode_Both) &&
        shape.shapePairP->getCorrPointAmount() != cellData.cpCount) {
      return;
    }

    pasteAvailability.push_back(true);
  }

  // ★ここで、「ペースト後にキーフレームを無くさない」条件を考慮する
  // ペースト可のリストを探し
  // 先頭のキーフレームだけペースト禁止にする条件をチェック
  //  ① 選択フレーム内にキーが無い
  //  ② Dst範囲にそのシェイプの全てのキーが入っている

  for (int d = 0; d < m_data.size();) {
    // ペースト可のセルを発見
    if (pasteAvailability.at(d)) {
      QList<int> dstFrames;
      QList<int> srcIndices;
      while (1) {
        if (d >= m_data.size()) break;
        if (!pasteAvailability.at(d))  // フレーム範囲の端を越える条件
          break;
        dstFrames.push_back(m_data.at(d).pos.x() + baseFrame);
        srcIndices.push_back(d);
        d++;
      }

      // ●形状キー
      if (doFormPaste) {
        bool keyFound = false;
        // ① 選択フレーム内に形状キーがあれば この行は問題なし
        // 次のCorrのチェックへ
        for (int s = 0; s < srcIndices.size(); s++) {
          if (!m_data.at(s).bpList.isEmpty()) {
            keyFound = true;
            break;
          }
        }
        // 次の条件へ
        if (!keyFound) {
          int keyAmount     = 0;
          int firstKeyFrame = -1;
          // ② Dst範囲にそのシェイプの全てのキーが入っている
          for (int d = 0; d < dstFrames.size(); d++) {
            // キー数をカウント
            if (shape.shapePairP->isFormKey(dstFrames.at(d), shape.fromTo)) {
              keyAmount++;
              // 最初のフレームを保持
              if (firstKeyFrame < 0) firstKeyFrame = dstFrames.at(d);
            }
          }
          // 総数が一致したら、これはイカン！FormKeyの最初のフレームをペースト不可にして残す
          if (shape.shapePairP->getFormKeyFrameAmount(shape.fromTo) ==
              keyAmount) {
            // QPoint(firstKeyFrame,
            // currentDstRow)のセルのコピー可能フラグをfalseにする
            int frameToBeKept  = firstKeyFrame;
            int copiedFramePos = frameToBeKept - baseFrame;
            for (int i = 0; i < m_data.size(); i++) {
              if (m_data.at(i).pos.x() == copiedFramePos) {
                pasteAvailability.replace(i, false);
                break;
              }
            }
          }
        }
      }

      // ●対応点キー
      if (doCorrPaste) {
        bool keyFound = false;
        // ① 選択フレーム内に対応点キーがあれば この行は問題なし
        // 次のCorrのチェックへ
        for (int s = 0; s < srcIndices.size(); s++) {
          if (!m_data.at(s).cpList.isEmpty()) {
            keyFound = true;
            break;
          }
        }
        // 次の条件へ
        if (!keyFound) {
          int keyAmount     = 0;
          int firstKeyFrame = -1;
          // ② Dst範囲にそのシェイプの全てのキーが入っている
          for (int d = 0; d < dstFrames.size(); d++) {
            // キー数をカウント
            if (shape.shapePairP->isCorrKey(dstFrames.at(d), shape.fromTo)) {
              keyAmount++;
              // 最初のフレームを保持
              if (firstKeyFrame < 0) firstKeyFrame = dstFrames.at(d);
            }
          }
          // 総数が一致したら、これはイカン！FormKeyの最初のフレームをペースト不可にして残す
          if (shape.shapePairP->getCorrKeyFrameAmount(shape.fromTo) ==
              keyAmount) {
            // QPoint(firstKeyFrame,
            // currentDstRow)のセルのコピー可能フラグをfalseにする
            int frameToBeKept  = firstKeyFrame;
            int copiedFramePos = frameToBeKept - baseFrame;
            for (int i = 0; i < m_data.size(); i++) {
              if (m_data.at(i).pos.x() == copiedFramePos) {
                pasteAvailability.replace(i, false);
                break;
              }
            }
          }
        }
        //-----
      }
    } else
      d++;
  }

  // ペースト操作
  for (int p = 0; p < pasteAvailability.size(); p++) {
    if (!pasteAvailability.at(p)) continue;

    int dstFrame = m_data.at(p).pos.x() + baseFrame;

    if (doFormPaste) {
      if (!m_data.at(p).bpList.isEmpty())
        shape.shapePairP->setFormKey(
            dstFrame, shape.fromTo,
            m_data.at(p).bpList);  // 既にキーがある場合はreplaceされる
      else
        shape.shapePairP->removeFormKey(
            dstFrame, shape.fromTo);  // キーじゃない場合は無視される

      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, dstFrame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, project->getParentShape(shape.shapePairP));
    }
    if (doCorrPaste) {
      if (!m_data.at(p).cpList.isEmpty())
        shape.shapePairP->setCorrKey(
            dstFrame, shape.fromTo,
            m_data.at(p).cpList);  // 既にキーがある場合はreplaceされる
      else
        shape.shapePairP->removeCorrKey(
            dstFrame, shape.fromTo);  // キーじゃない場合は無視される

      int start, end;
      shape.shapePairP->getCorrKeyRange(start, end, dstFrame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, project->getParentShape(shape.shapePairP));
    }
  }
}