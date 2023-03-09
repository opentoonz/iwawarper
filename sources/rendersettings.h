//---------------------------------------------------
// RenderSettings
// モーフィング、レンダリングの設定
//---------------------------------------------------
#ifndef RENDERSETTINGS_H
#define RENDERSETTINGS_H

class QXmlStreamWriter;
class QXmlStreamReader;

enum AlphaMode { SourceAlpha = 0, ShapeAlpha };

class RenderSettings {
  // メッシュ再帰分割の回数
  int m_warpPrecision;
  double m_faceSizeThreshold;

  // アルファを、素材で抜くか、シェイプの形に抜くか
  AlphaMode m_alphaMode;

  // ラスター画像の縮小読み込み
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

  // 保存/ロード
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);
};

#endif  // RENDERSETTINGS_H