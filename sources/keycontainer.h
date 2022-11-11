//-----------------------------------------------------------------------------
// KeyContainer クラス
//  キーフレームを持つデータを扱うテンプレートクラス
//	‐シェイプ情報を扱う FormKey
//	‐Correspondence（対応点）情報を扱う CorrKey
//-----------------------------------------------------------------------------

#ifndef KEYCONTAINER_H
#define KEYCONTAINER_H

#include <QMap>
#include <QList>
#include <QPointF>

class QXmlStreamWriter;
class QXmlStreamReader;

// ベジエ曲線の各点の座標
typedef struct BezierPoint {
  // コントロールポイント座標
  QPointF pos;
  // ハンドル＃１座標
  QPointF firstHandle;
  // ハンドル＃２座標
  QPointF secondHandle;
} BezierPoint;

// シェイプ形状のデータ構造（データ数＝コントロールポイント数）
typedef QList<BezierPoint> BezierPointList;
// 対応点情報のデータ構造（データ数＝Corrポイント数）
// 値は 0～m_bezierPointCount(Closed)
//      0～m_bezierPointCount-1(Open)
typedef QList<double> CorrPointList;

template <class T>
class KeyContainer {
  QMap<int, T> m_data;

  // キーフレーム間の割り方を指定。均等割り＝0.5の場合は登録しない
  // keyはセグメントの前側のキーフレーム。常に上のm_dataのkeyに含まれる必要。
  QMap<int, double> m_interpolation;

public:
  KeyContainer() {}

  // キーフレーム情報を追加する。以前にデータがある場合、消去して置き換える
  void setKeyData(int frame, T data);
  // 指定されたフレームがキーフレームかどうかを返す
  bool isKey(int frame);
  // frame より後で（frameを含まない）一番近いキーフレームを返す。
  // 無ければ -1を返す
  int nextKey(int frame);
  // frameがKeyならframeを返し、そうでなければframeより前で一番近いキーフレームを返す。
  // 無ければ -1を返す
  int belongingKey(int frame);

  // 指定されたフレームの値を返す。(特殊化する)
  // キーフレームが無い場合は初期値。
  // キーフレームの場合はその値。
  // キーフレームではない場合は補間して返す。
  T getData(int frame, int maxValue = 0, double smoothness = 0.);
  // T getData(int frame);

  // キーフレームの数を返す
  int getKeyCount() { return m_data.size(); }
  // 指定したフレームのキーフレーム情報を消す
  void removeKeyData(int frame);

  // データを得る
  QMap<int, T> getData() { return m_data; }
  QMap<int, double>& getInterpolation() { return m_interpolation; }
  // データをセットする
  void setData(QMap<int, T> data) { m_data = data; }
  void setInterpolation(QMap<int, double> interpolation) {
    m_interpolation = interpolation;
  }

  // セーブ/ロード(特殊化する)
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);
};

// 特殊化
template <>
BezierPointList KeyContainer<BezierPointList>::getData(int frame, int maxValue,
                                                       double smoothness);
template <>
CorrPointList KeyContainer<CorrPointList>::getData(int frame, int maxValue,
                                                   double smoothness);

#endif