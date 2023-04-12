#ifndef IWLAYER_H
#define IWLAYER_H
//---------------------------------------
// IwLayer
//---------------------------------------

#include "timage.h"

#include <QString>
#include <QList>
#include <QPair>
#include <QDir>
#include <QObject>

// class IwShape;
class ShapePair;

class QPainter;

struct OneShape;
class IwTimeLineSelection;
class QXmlStreamWriter;
class QXmlStreamReader;

class QMenu;
class QMouseEvent;

class IwLayer : public QObject {
  Q_OBJECT
public:
  enum LayerVisibility { Invisible = 0, HalfVisible, Full };

private:
  QString m_name;

  // Viewer上で見えるかどうかのスイッチ
  // 0:不可視 1:半分 2: 100％見える
  LayerVisibility m_isVisibleInViewer;

  // Renderingされるかどうか
  bool m_isVisibleInRender;

  // タイムライン上で展開しているかどうかのスイッチ
  bool m_isExpanded;

  // ロックされているか
  bool m_isLocked;

  // フォルダのリスト
  QList<QDir> m_paths;
  // フレーム数分のリスト
  // フォルダインデックス、ファイル名が格納されている。
  QList<QPair<int, QString>> m_images;

  // シェイプ
  QList<ShapePair*> m_shapepairs;

  int m_brightness, m_contrast;

public:
  IwLayer();
  IwLayer(const QString& name, LayerVisibility visibleInViewer, bool isExpanded,
          bool isVisibleInRender, bool isLocked, int brightness, int contrast);
  // IwLayer(const IwLayer&) {};
  IwLayer* clone();

  // フレーム番号から格納されているファイルパスを返す
  //(フレームは０スタート。無い場合は空Stringを返す)
  QString getImageFilePath(int frame);

  // フレーム番号から格納されているファイル名を返す
  //  Parentフォルダ無し。
  //(フレームは０スタート。無い場合は空Stringを返す)
  QString getImageFileName(int frame);

  // フレーム番号から格納されているparentフォルダ名を返す
  //(フレームは０スタート。無い場合は空Stringを返す)
  QString getParentFolderName(int frame);

  // ディレクトリパスを入力。
  // まだリストに無い場合は追加してインデックスを返す。
  int getFolderPathIndex(const QString& folderPath);

  QString getName() { return m_name; }
  void setName(QString& name) { m_name = name; }

  // フレーム長を返す
  int getFrameLength() { return m_images.size(); }

  // 画像パスの挿入 ※ シグナルは外で各自エミットすること
  void insertPath(int index, QPair<int, QString> image);
  // 画像パスを消去する。 ※ シグナルは外で各自エミットすること
  void deletePaths(int startIndex, int amount, bool skipPathCheck = false);
  // 画像パスの差し替え ※ シグナルは外で各自エミットすること
  void replacePath(int index, QPair<int, QString> image);

  // キャッシュがあればそれを返す。
  // 無ければ画像をロードしてキャッシュに格納する
  TImageP getImage(int index);

  // シェイプ
  int getShapePairCount(bool skipDeletedShape = false);
  ShapePair* getShapePair(int shapePairIndex);
  // シェイプの追加。indexを指定していなければ新たに追加
  void addShapePair(ShapePair* shapePair, int index = -1);
  void deleteShapePair(int shapePairIndex, bool doRemove = false);
  void deleteShapePair(ShapePair* shapePair, bool doRemove = false);

  // シェイプリストの丸々差し替え
  void replaceShapePairs(QList<ShapePair*>);

  // Viewer上で見えるかどうかのスイッチを返す
  LayerVisibility isVisibleInViewer() { return m_isVisibleInViewer; }
  void setVisibilityInViewer(LayerVisibility visiblity) {
    m_isVisibleInViewer = visiblity;
  }
  bool isVisibleInRender() { return m_isVisibleInRender; }
  void setVisibleInRender(bool on) { m_isVisibleInRender = on; }

  //----------------------------------
  // 以下、タイムラインでの表示用
  //----------------------------------
  // 展開しているかどうかを踏まえて中のシェイプの展開情報を取得して表示行数を返す
  int getRowCount();
  // 描画(ヘッダ部分)
  void drawTimeLineHead(QPainter& p, int vpos, int width, int rowHeight,
                        int& currentRow, int mouseOverRow, int mouseHPos);
  // 描画(本体)
  void drawTimeLine(QPainter& p, int vpos, int width, int fromFrame,
                    int toFrame, int frameWidth, int rowHeight, int& currentRow,
                    int mouseOverRow, double mouseOverFrameD,
                    IwTimeLineSelection* selection);
  // 左のアイテムリスト左クリック時の処理
  // rowはこのレイヤ内で上から０で始まる行数。 再描画が必要ならtrueを返す
  bool onLeftClick(int row, int mouseHPos, QMouseEvent* e);
  // ダブルクリック時、名前部分だったらtrue。シェイプかどうかも判定
  bool onDoubleClick(int row, int mouseHPos, ShapePair** shapeP);
  // 左のアイテムリスト右クリック時の操作
  void onRightClick(int row, QMenu& menu);

  // タイムライン上左クリック時の処理
  bool onTimeLineClick(int row, double frameD, bool controlPressed,
                       bool shiftPressed, Qt::MouseButton button);

  // マウスホバー時の処理。ドラッグ可能なボーダーのときtrueを返す
  bool onMouseMove(int frameIndex, bool isBorder);

  // セーブ/ロード
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);

  // 名前を返す
  int getNameFromShapePair(OneShape shape);
  // シェイプを返す
  OneShape getShapePairFromName(int name);
  // このレイヤに引数のシェイプペアが属していたらtrue
  bool contains(ShapePair*);
  // シェイプからインデックスを返す 無ければ-1
  int getIndexFromShapePair(const ShapePair*) const;
  // シェイプを入れ替える
  void swapShapes(int index1, int index2) {
    if (index1 < 0 || index1 >= m_shapepairs.size() || index2 < 0 ||
        index2 >= m_shapepairs.size() || index1 == index2)
      return;
    m_shapepairs.swapItemsAt(index1, index2);
  }
  // シェイプを移動する
  void moveShape(int from, int to) {
    if (from < 0 || from >= m_shapepairs.size() || to < 0 ||
        to >= m_shapepairs.size() || from == to)
      return;
    m_shapepairs.move(from, to);
  }

  // Undo操作用。フレームのデータをそのまま返す
  QPair<int, QString> getPathData(int frame);

  // 自分がParentなら自分を返す。子なら親Shapeのポインタを返す
  ShapePair* getParentShape(ShapePair* shape);

  int brightness() { return m_brightness; }
  void setBrightness(int brightness) { m_brightness = brightness; }
  int contrast() { return m_contrast; }
  void setContrast(int contrast) { m_contrast = contrast; }

  void setExpanded(bool lock) { m_isExpanded = lock; }
  bool isExpanded() { return m_isExpanded; }

  void setLocked(bool Lock) { m_isLocked = Lock; }
  bool isLocked() { return m_isLocked; }

protected slots:
  void onMoveShapeUpward();
  void onMoveShapeDownward();
  void onMoveAboveUpperShape();
  void onMoveBelowLowerShape();

  void onSwitchParentChild();
};

#endif
