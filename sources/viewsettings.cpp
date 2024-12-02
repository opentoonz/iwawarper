//---------------------------------------------------
// ViewSettings
// プロジェクト毎に持つ、現在の表示状態を管理するクラス
// プロジェクトファイルにはたぶん保存されない
//---------------------------------------------------

#include "viewsettings.h"

#include <QOpenGLTexture>

ViewSettings::ViewSettings()
    : m_frame(0)
    , m_imageMode(ImageMode_Edit)
    , m_zoomStep(0)
    , m_showMesh(false)
    , m_applyMatte(false)
    , m_fromToVisible({true, true})
    , m_onionRange(0) {}

//---------------------------------------------------
// プレビュー結果のテクスチャIDを操作
//---------------------------------------------------
unsigned int ViewSettings::getPreviewTextureId(int frame) {
  if (!m_previewTextureId.contains(frame))
    return 0;  // 0でいいのかな？テクスチャIDは常に非０？
  return m_previewTextureId.value(frame);
}

void ViewSettings::setPreviewTextureId(int frame, unsigned int texId) {
  m_previewTextureId[frame] = texId;
}

//---------------------------------------------------
// リセット
//---------------------------------------------------
void ViewSettings::reset(int rollId) {
  if (rollId >= 2) return;
  m_rolls[rollId].size      = QSize();
  m_rolls[rollId].textureId = 0;
}

void ViewSettings::resetAll() { m_layerImages.clear(); }

//---------------------------------------------------
//--- テクスチャ管理
//---------------------------------------------------
// テクスチャが格納されていればtrue
//---------------------------------------------------
bool ViewSettings::hasTexture(QString& path) {
  return m_layerImages.contains(path);
}
//---------------------------------------------------
// テクスチャIDの取得 無ければ０を返す
//---------------------------------------------------
ViewSettings::LAYERIMAGE ViewSettings::getTextureId(QString& path) {
  return m_layerImages.value(path, LAYERIMAGE());
}
//---------------------------------------------------
// テクスチャの登録
//---------------------------------------------------
void ViewSettings::storeTextureId(QString& path, LAYERIMAGE textureId) {
  m_layerImages[path] = textureId;
}
//---------------------------------------------------
// テクスチャリストを返す
//---------------------------------------------------
QList<ViewSettings::LAYERIMAGE> ViewSettings::getTextureIdList() {
  return m_layerImages.values();
}
// テクスチャリストをクリア
void ViewSettings::clearAllTextures() {
  for (auto layerImage : m_layerImages.values()) {
    layerImage.texture->release();
    delete layerImage.texture;
  }
  m_layerImages.clear();
}

void ViewSettings::releaseTexture(const QString path) {
  if (!m_layerImages.contains(path)) return;
  m_layerImages[path].texture->release();
  delete m_layerImages[path].texture;
  m_layerImages.remove(path);
}