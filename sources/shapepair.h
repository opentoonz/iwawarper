#ifndef SHAPEPAIR_H
#define SHAPEPAIR_H

//---------------------------------------
//  ShapePair
//  From-Toの形状をペアで持つ
//---------------------------------------

#include "keycontainer.h"

#include "keycontainer.h"
#include "colorsettings.h"

#include <QRectF>
#include <array>
#include <QMutex>
#include <QObject>

class IwProject;
class QPainter;
class QXmlStreamWriter;
class QXmlStreamReader;
class QMouseEvent;

class ShapePair : public QObject {
  Q_OBJECT

  QString m_name;
  // Correnspondence(対応点)間を何分割するか
  int m_precision;
  // シェイプが閉じているかどうか
  bool m_isClosed;
  // ベジエのポイント数
  int m_bezierPointAmount;
  // Correspondence(対応点)の数
  int m_corrPointAmount;
  // 形状データ - from, to の２つ
  KeyContainer<BezierPointList> m_formKeys[2];
  // Correspondence(対応点)データ - from, to の２つ
  KeyContainer<CorrPointList> m_corrKeys[2];

  // 以下タイムライン用
  // 表示が開いているかどうか = 選択されているかどうか
  std::array<bool, 2> m_isExpanded;
  // ロックされているかどうか
  std::array<bool, 2> m_isLocked;
  // ピン（表示固定）されているかどうか
  std::array<bool, 2> m_isPinned;

  // タグの一覧
  std::array<QList<int>, 2> m_shapeTag;

  // シェイプが親かどうか
  // レンダリングは、親シェイプと、その下に（もしあれば）
  // くっついている子シェイプを一かたまりにして計算される。
  // 初期値は、Closedの場合はtrue、Openedの場合はfalse
  bool m_isParent;

  // シェイプが表示されているかどうか。非表示のシェイプは選択できない。
  // （ロックされているのと同じ）レンダリングには用いられる？
  bool m_isVisible;

  // シェイプのキー間の補間の度合い。
  // Smoothness = 0 (初期値)：リニア補間
  // Smoothness = 1 : 3次スプライン補間
  double m_animationSmoothness;

  QMutex mutex_;

public:
  ShapePair(int frame, bool isClosed, BezierPointList bPointList,
            CorrPointList cPointList, double dstShapeOffset = 0.0);

  //(ファイルのロード時のみ使う)空のシェイプのコンストラクタ
  ShapePair();

  // コピーコンストラクタ
  ShapePair(ShapePair* src);

  ShapePair* clone();

  // あるフレームでのベジエ形状を返す
  BezierPointList getBezierPointList(int frame, int fromTo);
  // あるフレームでの対応点を返す
  CorrPointList getCorrPointList(int frame, int fromTo);

  double getBezierInterpolation(int frame, int fromTo);
  double getCorrInterpolation(int frame, int fromTo);
  void setBezierInterpolation(int frame, int fromTo, double interp);
  void setCorrInterpolation(int frame, int fromTo, double interp);

  // 指定したフレームのベジエ頂点情報を返す
  //  isFromShape = true ならFROM, falseならTO の シェイプの頂点を得る
  double* getVertexArray(int frame, int fromTo, IwProject* project);

  // 閉じているかどうかを返す
  bool isClosed() { return m_isClosed; }
  // シェイプを閉じる。
  void setIsClosed(bool close) { m_isClosed = close; }

  // 表示用の頂点数を求める。線が閉じているか空いているかで異なる
  int getVertexAmount(IwProject* project);

  // シェイプのバウンディングボックスを返す
  //  fromTo = 0 ならFROM, 1ならTO の バウンディングボックスを得る
  QRectF getBBox(int frame, int fromTo);

  // ベジエのポイント数を返す
  int getBezierPointAmount() { return m_bezierPointAmount; }
  // Correspondence(対応点)の数を返す
  int getCorrPointAmount() { return m_corrPointAmount; }

  // 引数のフレームが、形状のキーフレームかどうかを返す
  bool isFormKey(int frame, int fromTo) {
    return m_formKeys[fromTo].isKey(frame);
  }
  // 引数のフレームが、対応点のキーフレームかどうかを返す
  bool isCorrKey(int frame, int fromTo) {
    return m_corrKeys[fromTo].isKey(frame);
  }

  // frameがKeyならframeを返し、そうでなければframeより前で一番近いキーフレームを返す。
  // 無ければ -1を返す
  int belongingFormKey(int frame, int fromTo) {
    return m_formKeys[fromTo].belongingKey(frame);
  }
  int belongingCorrKey(int frame, int fromTo) {
    return m_corrKeys[fromTo].belongingKey(frame);
  }

  // 形状キーフレームのセット
  void setFormKey(int frame, int fromTo, BezierPointList bPointList);
  // 形状キーフレームを消去する
  void removeFormKey(int frame, int fromTo, bool enableRemoveLastKey = false);

  // 対応点キーフレームのセット
  void setCorrKey(int frame, int fromTo, CorrPointList cPointList);
  // 対応点キーフレームを消去する
  void removeCorrKey(int frame, int fromTo, bool enableRemoveLastKey = false);

  // 指定位置にコントロールポイントを追加する　コントロールポイントのインデックスを返す
  int addControlPoint(const QPointF& pos, int frame, int fromTo);
  // 指定位置に対応点を追加する うまくいったらtrueを返す
  bool addCorrPoint(const QPointF& pos, int frame, int fromTo);

  // 指定インデックスの対応点を消去する うまくいったらtrueを返す
  bool deleteCorrPoint(int index);

  // マウス位置に最近傍のベジエ座標を得る
  double getNearestBezierPos(const QPointF& pos, int frame, int fromTo);
  double getNearestBezierPos(const QPointF& pos, int frame, int fromTo,
                             double& dist);
  // マウス位置に最近傍のベジエ座標を得る(範囲つき版)
  double getNearestBezierPos(const QPointF& pos, int frame, int fromTo,
                             const double rangeBefore, const double rangeAfter,
                             double& dist);

  // ワークエリアのリサイズに合わせ逆数でスケールしてシェイプのサイズを維持する
  void counterResize(QSizeF workAreaScale);

  // 形状データを返す
  QMap<int, BezierPointList> getFormData(int fromTo);
  QMap<int, double> getFormInterpolation(int fromTo);
  // 形状データをセット
  void setFormData(QMap<int, BezierPointList> data, int fromTo);
  // 対応点データを返す
  QMap<int, CorrPointList> getCorrData(int fromTo);
  QMap<int, double> getCorrInterpolation(int fromTo);
  // 対応点データをセット
  void setCorrData(QMap<int, CorrPointList> data, int fromTo);

  // 対応点の座標リストを得る
  QList<QPointF> getCorrPointPositions(int frame, int fromTo);
  // 対応点のウェイトのリストを得る
  QList<double> getCorrPointWeights(int frame, int fromTo);

  // 現在のフレームのCorrenspondence(対応点)間を分割した点の分割値を得る
  QList<double> getDiscreteCorrValues(const QPointF& onePix, int frame,
                                      int fromTo);

  // 現在のフレームのCorrenspondence(対応点)間を分割した点のウェイト値を得る
  QList<double> getDiscreteCorrWeights(int frame, int fromTo);

  // ベジエ値から座標値を計算する
  QPointF getBezierPosFromValue(int frame, int fromTo, double bezierValue);

  // 指定ベジエ値での１ベジエ値あたりの座標値の変化量を返す
  double getBezierSpeedAt(int frame, int fromTo, double bezierValue,
                          double distance);

  // 指定したベジエ値間のピクセル距離を返す
  double getBezierLengthFromValueRange(const QPointF& onePix, int frame,
                                       int fromTo, double startVal,
                                       double endVal);

  // Correnspondence(対応点)間を何分割するか、のI/O
  int getEdgeDensity() { return m_precision; }
  void setEdgeDensity(int prec);

  // シェイプのキー間の補間の度合い、のI/O
  double getAnimationSmoothness() { return m_animationSmoothness; }
  void setAnimationSmoothness(double smoothness);

  // 形状キーフレーム数を返す
  int getFormKeyFrameAmount(int fromTo);
  // 対応点キーフレーム数を返す
  int getCorrKeyFrameAmount(int fromTo);

  // ソートされた形状キーフレームのリストを返す
  QList<int> getFormKeyFrameList(int fromTo);
  // ソートされた対応点キーフレームのリストを返す
  QList<int> getCorrKeyFrameList(int fromTo);

  void getFormKeyRange(int& start, int& end, int frame, int fromTo);
  void getCorrKeyRange(int& start, int& end, int frame, int fromTo);
  //----------------------------------
  // 以下、タイムラインでの表示用
  //----------------------------------
  // 展開しているかどうかを踏まえて表示行数を返す
  int getRowCount();

  // 描画(ヘッダ部分)
  void drawTimeLineHead(QPainter& p, int& vpos, int width, int rowHeight,
                        int& currentRow, int mouseOverRow, int mouseHPos,
                        ShapePair* parentShape,
                        ShapePair* nextShape,  // ← 親子関係の線を引くため
                        bool layerIsLocked);
  // 描画(本体)
  void drawTimeLine(QPainter& p, int& vpos, int width, int fromFrame,
                    int toFrame, int frameWidth, int rowHeight, int& currentRow,
                    int mouseOverRow, double mouseOverFrameD,
                    bool layerIsLocked, bool layerIsVisibleInRender);

  // タイムラインクリック時
  bool onTimeLineClick(int rowInShape, double frameD, bool ctrlPressed,
                       bool shiftPressed, Qt::MouseButton button);

  QString getName() { return m_name; }
  void setName(QString name) { m_name = name; }

  // 左クリック時の処理 rowはこのレイヤ内で上から０で始まる行数。
  // 再描画が必要ならtrueを返す
  bool onLeftClick(int row, int mouseHPos, QMouseEvent* e);

  // FromシェイプをToシェイプにコピー
  void copyFromShapeToToShape(double offset = 0.0);

  bool isExpanded(int fromTo) { return m_isExpanded[fromTo]; }
  bool isAtLeastOneShapeExpanded();

  void setExpanded(int fromTo, bool expand) { m_isExpanded[fromTo] = expand; }
  bool isLocked(int fromTo) { return m_isLocked[fromTo]; }
  void setLocked(int fromTo, bool lock) { m_isLocked[fromTo] = lock; }

  bool isPinned(int fromTo) { return m_isPinned[fromTo]; }
  void setPinned(int fromTo, bool pin) { m_isPinned[fromTo] = pin; }

  bool isParent() { return m_isParent; }
  void setIsParent(bool parent) { m_isParent = parent; }

  // シェイプタグ

  QList<int>& getShapeTags(int fromTo) { return m_shapeTag[fromTo]; }
  void addShapeTag(int fromTo, int shapeTagId) {
    if (m_shapeTag[fromTo].contains(shapeTagId)) return;
    m_shapeTag[fromTo].append(shapeTagId);
    std::sort(m_shapeTag[fromTo].begin(), m_shapeTag[fromTo].end());
  }
  void removeShapeTag(int fromTo, int shapeTagId) {
    m_shapeTag[fromTo].removeAll(shapeTagId);
  }
  bool containsShapeTag(int fromTo, int shapeTagId) {
    return m_shapeTag[fromTo].contains(shapeTagId);
  }
  int shapeTagCount(int fromTo) { return m_shapeTag[fromTo].count(); }
  bool isRenderTarget(int shapeTagId) {
    if (shapeTagId == -1) return true;
    return containsShapeTag(0, shapeTagId) || containsShapeTag(1, shapeTagId);
  }

  bool isVisible() { return m_isVisible; }
  void setVisible(bool on) { m_isVisible = on; }

  // セーブ/ロード
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);
};

struct OneShape {
  ShapePair* shapePairP;
  int fromTo;

  OneShape(ShapePair* shape = 0, int ft = 0) {
    shapePairP = shape;
    fromTo     = ft;
  }
  bool operator==(const OneShape& right) const {
    return shapePairP == right.shapePairP && fromTo == right.fromTo;
  }
  bool operator!=(const OneShape& right) const {
    return shapePairP != right.shapePairP || fromTo != right.fromTo;
  }

  BezierPointList getBezierPointList(int frame) {
    return shapePairP->getBezierPointList(frame, fromTo);
  }

  OneShape getPartner() { return OneShape(shapePairP, (fromTo + 1) % 2); }
};

#endif