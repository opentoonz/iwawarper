//---------------------------------------------------
// ViewSettings
// �v���W�F�N�g���Ɏ��A���݂̕\����Ԃ��Ǘ�����N���X
// �v���W�F�N�g�t�@�C���ɂ͂��Ԃ�ۑ�����Ȃ�
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
// �v���r���[���ʂ̃e�N�X�`��ID�𑀍�
//---------------------------------------------------
unsigned int ViewSettings::getPreviewTextureId(int frame) {
  if (!m_previewTextureId.contains(frame))
    return 0;  // 0�ł����̂��ȁH�e�N�X�`��ID�͏�ɔ�O�H
  return m_previewTextureId.value(frame);
}

void ViewSettings::setPreviewTextureId(int frame, unsigned int texId) {
  m_previewTextureId[frame] = texId;
}

//---------------------------------------------------
// ���Z�b�g
//---------------------------------------------------
void ViewSettings::reset(int rollId) {
  if (rollId >= 2) return;
  m_rolls[rollId].size      = QSize();
  m_rolls[rollId].textureId = 0;
}

void ViewSettings::resetAll() { m_layerImages.clear(); }

//---------------------------------------------------
//--- �e�N�X�`���Ǘ�
//---------------------------------------------------
// �e�N�X�`�����i�[����Ă����true
//---------------------------------------------------
bool ViewSettings::hasTexture(QString& path) {
  return m_layerImages.contains(path);
}
//---------------------------------------------------
// �e�N�X�`��ID�̎擾 ������΂O��Ԃ�
//---------------------------------------------------
ViewSettings::LAYERIMAGE ViewSettings::getTextureId(QString& path) {
  return m_layerImages.value(path, LAYERIMAGE());
}
//---------------------------------------------------
// �e�N�X�`���̓o�^
//---------------------------------------------------
void ViewSettings::storeTextureId(QString& path, LAYERIMAGE textureId) {
  m_layerImages[path] = textureId;
}
//---------------------------------------------------
// �e�N�X�`�����X�g��Ԃ�
//---------------------------------------------------
QList<ViewSettings::LAYERIMAGE> ViewSettings::getTextureIdList() {
  return m_layerImages.values();
}
// �e�N�X�`�����X�g���N���A
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