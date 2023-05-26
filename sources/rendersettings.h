//---------------------------------------------------
// RenderSettings
// ���[�t�B���O�A�����_�����O�̐ݒ�
//---------------------------------------------------
#ifndef RENDERSETTINGS_H
#define RENDERSETTINGS_H

class QXmlStreamWriter;
class QXmlStreamReader;

enum AlphaMode { SourceAlpha = 0, ShapeAlpha };
enum ResampleMode { AreaAverage = 0, NearestNeighbor };

class RenderSettings {
  // ���b�V���ċA�����̉�
  int m_warpPrecision;
  double m_faceSizeThreshold;

  // �A���t�@���A�f�ނŔ������A�V�F�C�v�̌`�ɔ�����
  AlphaMode m_alphaMode;

  // ���T���v��
  ResampleMode m_resampleMode;

  // ���X�^�[�摜�̏k���ǂݍ���
  int m_imageShrink;

  bool m_antialias;

public:
  RenderSettings();

  int getWarpPrecision() const { return m_warpPrecision; }
  void setWarpPrecision(const int prec);

  double getFaceSizeThreshold() const { return m_faceSizeThreshold; }
  void setFaceSizeThreshold(const double thres) { m_faceSizeThreshold = thres; }

  AlphaMode getAlphaMode() const { return m_alphaMode; }
  void setAlphaMode(const AlphaMode mode) { m_alphaMode = mode; }

  ResampleMode getResampleMode() const { return m_resampleMode; }
  void setResampleMode(const ResampleMode mode) { m_resampleMode = mode; }

  int getImageShrink() const { return m_imageShrink; }
  void setImageShrink(const int imageShrink) { m_imageShrink = imageShrink; }

  bool getAntialias() const { return m_antialias; }
  void setAntialias(const bool on) { m_antialias = on; }

  // �ۑ�/���[�h
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);
};

#endif  // RENDERSETTINGS_H