//---------------------------------------------------
// RenderSettings
// ���[�t�B���O�A�����_�����O�̐ݒ�
//---------------------------------------------------
#ifndef RENDERSETTINGS_H
#define RENDERSETTINGS_H

class QXmlStreamWriter;
class QXmlStreamReader;

enum AlphaMode { SourceAlpha = 0, ShapeAlpha };

class RenderSettings {
  // ���b�V���ċA�����̉�
  int m_warpPrecision;
  double m_faceSizeThreshold;

  // �A���t�@���A�f�ނŔ������A�V�F�C�v�̌`�ɔ�����
  AlphaMode m_alphaMode;

  // ���X�^�[�摜�̏k���ǂݍ���
  int m_imageShrink;

  bool m_antialias;

public:
  RenderSettings();

  int getWarpPrecision() { return m_warpPrecision; }
  void setWarpPrecision(int prec);

  double getFaceSizeThreshold() { return m_faceSizeThreshold; }
  void setFaceSizeThreshold(double thres) { m_faceSizeThreshold = thres; }

  AlphaMode getAlphaMode() { return m_alphaMode; }
  void setAlphaMode(AlphaMode mode) { m_alphaMode = mode; }

  int getImageShrink() { return m_imageShrink; }
  void setImageShrink(int imageShrink) { m_imageShrink = imageShrink; }

  bool getAntialias() { return m_antialias; }
  void setAntialias(bool on) { m_antialias = on; }

  // �ۑ�/���[�h
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);
};

#endif  // RENDERSETTINGS_H