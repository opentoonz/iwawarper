//----------------------------------------------------
// TimeLineData
// タイムライン内のコピー／ペーストで使うデータ
//----------------------------------------------------
#ifndef TIMELINEDATA_H
#define TIMELINEDATA_H

#include "iwmimedata.h"

#include <QList>
#include <QString>

class IwLayer;

class TimeLineData : public IwMimeData {
public:
  QList<QPair<QString, QString>> m_frameData;

  TimeLineData();
  TimeLineData(const TimeLineData *src);

  TimeLineData *clone() const { return new TimeLineData(this); }

  // データのフレーム長を返す
  int getFrameLength() { return m_frameData.size(); }

  // レイヤからデータを格納する (コピー)
  void setFrameData(IwLayer *layer, int start, int end,
                    bool rightFrameIncluded);
  // データからレイヤの指定されたindexに入れる (挿入ペースト)
  void getFrameData(IwLayer *layer, int frameIndex) const;
};

#endif