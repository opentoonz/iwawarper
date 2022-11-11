//----------------------------------------------------
// TimeLineData
// タイムライン内のコピー／ペーストで使うデータ
//----------------------------------------------------

#include "timelinedata.h"

#include "iwlayer.h"

TimeLineData::TimeLineData() {}

//----------------------------------------------------

TimeLineData::TimeLineData(const TimeLineData *src) {
  m_frameData = src->m_frameData;
}

//----------------------------------------------------
// Sequenceからデータを格納する (コピー)
//----------------------------------------------------
void TimeLineData::setFrameData(IwLayer *layer, int start, int end,
                                bool rightFrameIncluded) {
  // ロールが得られなかったらreturn
  if (!layer) return;

  // 念のため、データが空でなかったらクリア
  if (!m_frameData.isEmpty()) m_frameData.clear();

  // 右のフレームが含まれる場合はlastFrameを１たす
  int lastFrame = (rightFrameIncluded) ? end + 1 : end;
  // 各フレームを格納する
  for (int f = start; f < lastFrame; f++) {
    m_frameData.append(QPair<QString, QString>(layer->getParentFolderName(f),
                                               layer->getImageFileName(f)));
  }

  setText("TimeLineData");
}

//----------------------------------------------------
// データからSequenceの指定されたindexに入れる (挿入ペースト)
//----------------------------------------------------
void TimeLineData::getFrameData(IwLayer *layer, int frameIndex) const {
  // ロールが得られなかったらreturn
  if (!layer) return;

  // データが空ならreturn
  if (m_frameData.isEmpty()) return;

  // データの各フレームをロールに挿入する
  for (int f = 0; f < m_frameData.size(); f++) {
    // フォルダパスを得る
    QString folderPath = m_frameData.at(f).first;
    // ファイルパスを得る
    QString filePath = m_frameData.at(f).second;

    // フォルダインデックスを得る
    int folderIndex = layer->getFolderPathIndex(folderPath);

    layer->insertPath(frameIndex, QPair<int, QString>(folderIndex, filePath));

    // 挿入する境界インデックスを1つ進める
    frameIndex++;
  }
}