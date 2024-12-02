//---------------------------------------------------
// ViewSettings
// �v���W�F�N�g���Ɏ��A���݂̕\����Ԃ��Ǘ�����N���X
// �v���W�F�N�g�t�@�C���ɂ͕ۑ�����Ȃ�
//---------------------------------------------------

#ifndef VIEWSETTINGS_H
#define VIEWSETTINGS_H

#include <QSize>
#include <QMap>
#include <QList>

#include <array>
#include <QMap>

class QOpenGLTexture;

// �摜�\�����[�h�i�ҏW/�v���r���[�j
enum IMAGEMODE { ImageMode_Edit = 0, ImageMode_Half, ImageMode_Preview };

class ViewSettings {
  // ���݂̃t���[��
  int m_frame;

  struct ROLLS {
    QSize size;
    unsigned int textureId;
  } m_rolls[2];

  // �\�����
  std::array<bool, 2> m_fromToVisible;
  // ���b�V���̕\��
  bool m_showMesh;
  // �}�b�g��K�p�\��
  bool m_applyMatte;

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

  // �摜�\�����[�h
  IMAGEMODE m_imageMode;

  // �v���r���[���ʂ̃e�N�X�`��ID�����߂�Ƃ��� (Key:�t���[��/Val:�e�N�X�`��ID)
  QMap<int, unsigned int> m_previewTextureId;

  // �Y�[���{���̒i�K
  int m_zoomStep;

  int m_onionRange;

public:
  ViewSettings();

  // ���݂̃t���[���𑀍삷��
  int getFrame() { return m_frame; }
  void setFrame(int f) { m_frame = f; }

  // �ۑ�����Ă���LAYERIMAGE�f�[�^�̐���Ԃ�
  int getLayerImageCount() { return m_layerImages.size(); }

  // �v���r���[���ʂ̃e�N�X�`��ID�𑀍�
  unsigned int getPreviewTextureId(int frame);
  void setPreviewTextureId(int frame, unsigned int texId);

  // ���Z�b�g
  void reset(int rollId);
  void resetAll();

  // �摜�\�����[�h�𑀍�
  IMAGEMODE getImageMode() { return m_imageMode; }
  void setImageMode(IMAGEMODE mode) { m_imageMode = mode; }

  // �Y�[���{���̒i�K�𑀍�
  int getZoomStep() { return m_zoomStep; }
  void setZoomStep(int zoomStep) { m_zoomStep = zoomStep; }

  //--- �e�N�X�`���Ǘ�
  // �e�N�X�`�����i�[����Ă����true
  bool hasTexture(QString& path);
  // �e�N�X�`��ID�^�T�C�Y�̎擾
  LAYERIMAGE getTextureId(QString& path);
  // �e�N�X�`���̓o�^
  void storeTextureId(QString& path, LAYERIMAGE texture);
  // �e�N�X�`�����X�g��Ԃ�
  QList<LAYERIMAGE> getTextureIdList();
  // �e�N�X�`�����X�g���N���A
  void clearAllTextures();
  void releaseTexture(const QString path);

  //---- �\�����
  bool isFromToVisible(int fromTo) { return m_fromToVisible[fromTo]; }
  void setIsFromToVisible(int fromTo, bool lock) {
    m_fromToVisible[fromTo] = lock;
  }
  //----
  // ���b�V���̕\��
  bool isMeshVisible() { return m_showMesh; }
  void setMeshVisible(bool visible) { m_showMesh = visible; }

  // �}�b�g��K�p�\��
  bool isMatteApplied() { return m_applyMatte; }
  void setMatteApplied(bool on) { m_applyMatte = on; }

  int getOnionRange() { return m_onionRange; }
  void setOnionRange(int onionRange) { m_onionRange = onionRange; }
};

#endif