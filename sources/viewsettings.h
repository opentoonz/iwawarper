//---------------------------------------------------
// ViewSettings
// プロジェクト毎に持つ、現在の表示状態を管理するクラス
// プロジェクトファイルには保存されない
//---------------------------------------------------

#ifndef VIEWSETTINGS_H
#define VIEWSETTINGS_H

#include <QSize>
#include <QMap>
#include <QList>

#include <array>
#include <QMap>

class QOpenGLTexture;

// 画像表示モード（編集/プレビュー）
enum IMAGEMODE { ImageMode_Edit = 0, ImageMode_Half, ImageMode_Preview };

class ViewSettings {
  // 現在のフレーム
  int m_frame;

  struct ROLLS {
    QSize size;
    unsigned int textureId;
  } m_rolls[2];

  // 表示情報
  std::array<bool, 2> m_fromToVisible;
  // メッシュの表示
  bool m_showMesh;

public:
  struct LAYERIMAGE {
    QSize size;
    int brightness;
    int contrast;
    int shrink;
    QOpenGLTexture* texture;
    LAYERIMAGE() {
      size       = QSize(0, 0);
      brightness = 0;
      contrast   = 0;
      shrink     = 1;
      texture    = nullptr;
    }
  };

private:
  QMap<QString, LAYERIMAGE> m_layerImages;

  // 画像表示モード
  IMAGEMODE m_imageMode;

  // プレビュー結果のテクスチャIDを収めるところ (Key:フレーム/Val:テクスチャID)
  QMap<int, unsigned int> m_previewTextureId;

  // ズーム倍率の段階
  int m_zoomStep;

  int m_onionRange;

public:
  ViewSettings();

  // 現在のフレームを操作する
  int getFrame() { return m_frame; }
  void setFrame(int f) { m_frame = f; }

  // 保存されているLAYERIMAGEデータの数を返す
  int getLayerImageCount() { return m_layerImages.size(); }

  // プレビュー結果のテクスチャIDを操作
  unsigned int getPreviewTextureId(int frame);
  void setPreviewTextureId(int frame, unsigned int texId);

  // リセット
  void reset(int rollId);
  void resetAll();

  // 画像表示モードを操作
  IMAGEMODE getImageMode() { return m_imageMode; }
  void setImageMode(IMAGEMODE mode) { m_imageMode = mode; }

  // ズーム倍率の段階を操作
  int getZoomStep() { return m_zoomStep; }
  void setZoomStep(int zoomStep) { m_zoomStep = zoomStep; }

  //--- テクスチャ管理
  // テクスチャが格納されていればtrue
  bool hasTexture(QString& path);
  // テクスチャID／サイズの取得
  LAYERIMAGE getTextureId(QString& path);
  // テクスチャの登録
  void storeTextureId(QString& path, LAYERIMAGE texture);
  // テクスチャリストを返す
  QList<LAYERIMAGE> getTextureIdList();
  // テクスチャリストをクリア
  void clearAllTextures();
  void releaseTexture(const QString path);

  //---- 表示情報
  bool isFromToVisible(int fromTo) { return m_fromToVisible[fromTo]; }
  void setIsFromToVisible(int fromTo, bool lock) {
    m_fromToVisible[fromTo] = lock;
  }
  //----
  // メッシュの表示
  bool isMeshVisible() { return m_showMesh; }
  void setMeshVisible(bool visible) { m_showMesh = visible; }

  int getOnionRange() { return m_onionRange; }
  void setOnionRange(int onionRange) { m_onionRange = onionRange; }
};

#endif