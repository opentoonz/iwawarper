//----------------------------------------------------
// KeyFrameData
// KeyFrameEditor内のコピー/ペーストで扱うデータ
//----------------------------------------------------
#ifndef KEYFRAMEDATA_H
#define KEYFRAMEDATA_H

#include "iwmimedata.h"
#include "keyframeeditor.h"
#include "keycontainer.h"

#include <QPoint>
#include <QList>

struct OneShape;

class KeyFrameData : public IwMimeData {
  struct CellData {
    QPoint pos;                       // 選択範囲バウンディングの左上からの座標
    KeyFrameEditor::KEYMODE keymode;  // コピー時のキーモード
    bool shapeWasClosed;              // コピー時、シェイプが閉じていたかどうか
    int bpCount;                      // コピー時のコントロールポイント数
    int cpCount;                      // コピー時の対応点数
    BezierPointList bpList;           // 形状キーフレーム情報
    CorrPointList cpList;             // 対応点キーフレーム情報
    double b_interp;                  // 形状キーフレームの補間情報
    double c_interp;                  // 対応点キーフレームの補間情報
  };

public:
  QList<CellData> m_data;

  KeyFrameData();
  KeyFrameData(const KeyFrameData* src);

  KeyFrameData* clone() const { return new KeyFrameData(this); }

  // 選択範囲からデータを格納する
  void setKeyFrameData(OneShape& shape, QList<int>& selectedFrames,
                       KeyFrameEditor::KEYMODE keymode);

  // 指定セルにデータを上書きペーストする
  void getKeyFrameData(OneShape& shape, QList<int>& selectedCells,
                       KeyFrameEditor::KEYMODE keymode) const;
};

#endif