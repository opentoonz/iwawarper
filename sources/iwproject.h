//---------------------------------------------------
// IwProject
// プロジェクト情報のクラス。
//---------------------------------------------------
#ifndef IWPROJECT_H
#define IWPROJECT_H

#include "colorsettings.h"

#include <QList>
#include <QMap>
#include <QSet>
#include <QSize>
#include <QString>

class ViewSettings;
class Preferences;
class QXmlStreamWriter;
class QXmlStreamReader;
class RenderSettings;
class OutputSettings;
class ShapeTagSettings;

class IwLayer;
struct OneShape;
class ShapePair;

class IwProject {
  // プロジェクトごとに振られる通し番号
  int m_id;

  // プロジェクト毎に持つ、現在の表示状態を管理するクラス
  // プロジェクトファイルにはたぶん保存されない
  ViewSettings* m_settings;

  // ワークエリア（カメラ）のサイズ
  QSize m_workAreaSize;

  // ファイルパス
  QString m_path;

  // レンダリング設定
  RenderSettings* m_renderSettings;

  // 出力設定
  OutputSettings* m_outputSettings;

  // MultiFrameモードがONのときに編集されるフレームはtrue
  // プロジェクトファイルには保存されない
  QList<bool> m_multiFrames;

  QList<IwLayer*> m_layers;

  // プレビュー範囲
  int m_prevFrom, m_prevTo;

  // シェイプタグ設定. id と ShapeTagInfo(名前、色、形)
  ShapeTagSettings* m_shapeTagSettings;

  // オニオンスキンフレーム
  QSet<int> m_onionFrames;

public:
  typedef std::vector<double> Guides;

private:
  Guides m_hGuides, m_vGuides;

public:
  IwProject();

  // ロール情報を解放
  // シェイプを解放
  //  ViewSettings* m_settingsを解放
  //  Preferences * m_preferencesを解放
  //  ColorSettings * m_colorSettingsを解放
  //  グループ情報 m_groupsを解放
  ~IwProject();

  // 通し番号を返す
  int getId() { return m_id; }

  // 各ロールで一番長いフレーム長を返す（=プロジェクトのフレーム長）
  int getProjectFrameLength();
  // プロジェクトの現在のフレームを返す
  int getViewFrame();
  // 現在のフレームを変更する
  void setViewFrame(int frame);

  ViewSettings* getViewSettings() { return m_settings; }
  ShapeTagSettings* getShapeTags() { return m_shapeTagSettings; }

  // ワークエリア（カメラ）のサイズ
  QSize getWorkAreaSize() { return m_workAreaSize; }
  void setWorkAreaSize(QSize size) { m_workAreaSize = size; }

  // このプロジェクトがカレントかどうかを返す
  bool isCurrent();

  // ファイルパス
  QString getPath() { return m_path; }
  void setPath(QString path) { m_path = path; }

  // 保存/ロード
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);

  // レンダリング設定
  RenderSettings* getRenderSettings() { return m_renderSettings; }

  // 出力設定
  OutputSettings* getOutputSettings() { return m_outputSettings; }

  // パスからプロジェクト名を抜き出して返す
  QString getProjectName();

  // frameに対する保存パスを返す
  QString getOutputPath(int frame, QString formatStr = QString());

  //-------------
  // MultiFrame関係
  // シーン長が変わったとき、MultiFrameを初期化する
  void initMultiFrames();
  // MultiFrameのリストを返す
  QList<int> getMultiFrames();
  // MultiFrameにフレームを追加/削除する
  void setMultiFrames(int frame, bool on);
  // あるフレームがMultiFrameで選択されているかどうかを返す
  bool isMultiFrame(int frame) {
    if (frame < 0 || frame >= m_multiFrames.size()) return false;
    return m_multiFrames.at(frame);
  }
  //-------------

  //-------------
  // レイヤ関係
  int getLayerCount() { return m_layers.size(); }
  IwLayer* getLayer(int index) { return m_layers.at(index); }
  IwLayer* appendLayer();
  void swapLayers(int index1, int index2) {
    if (index1 < 0 || index1 >= m_layers.size() || index2 < 0 ||
        index2 >= m_layers.size())
      return;
    m_layers.swapItemsAt(index1, index2);
  }
  // レイヤ消去
  void deleteLayer(int index) {
    if (index < 0 || index >= m_layers.size()) return;
    m_layers.removeAt(index);
  }
  void deleteLayer(IwLayer* layer) { m_layers.removeOne(layer); }
  // レイヤ挿入
  void insertLayer(IwLayer* layer, int index) {
    if (index < 0 || index > m_layers.size()) return;
    m_layers.insert(index, layer);
  }
  // レイヤ並べ替え
  void reorderLayers(QList<IwLayer*> layers) {
    if (m_layers.count() != layers.count() ||
        QSet<IwLayer*>(m_layers.begin(), m_layers.end()) !=
            QSet<IwLayer*>(layers.begin(), layers.end()))
      return;
    m_layers = layers;
  }

  //-------------
  // レイヤのどれかにシェイプが入っていればtrue
  bool contains(ShapePair*);
  // シェイプが何番目のレイヤーの何番目に入っているか返す
  bool getShapeIndex(ShapePair*, int& layerIndex, int& shapeIndex);
  // シェイプからレイヤを返す
  IwLayer* getLayerFromShapePair(ShapePair*);
  // 簡単のため追加。親シェイプを返す
  ShapePair* getParentShape(ShapePair*);

  // レイヤが入っていればtrue
  bool contains(IwLayer* lay) { return m_layers.contains(lay); }

  bool getPreviewRange(int& from, int& to);
  void setPreviewFrom(int from);
  void setPreviewTo(int to);
  void clearPreviewRange();

  /*!
          Returns horizontal Guides; a vector of double values that shows
     y-value of
          horizontal lines.
  */
  Guides& getHGuides() { return m_hGuides; }
  /*!
          Returns vertical Guides; a vector of double values that shows x-value
     of
          vertical lines.
  */
  Guides& getVGuides() { return m_vGuides; }

  void toggleOnionFrame(int frame);
  const QSet<int> getOnionFrames() { return m_onionFrames; };
};

#endif