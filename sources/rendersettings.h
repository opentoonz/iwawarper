//---------------------------------------------------
// RenderSettings
// モーフィング、レンダリングの設定
//---------------------------------------------------
#ifndef RENDERSETTINGS_H
#define RENDERSETTINGS_H

class QXmlStreamWriter;
class QXmlStreamReader;

enum AlphaMode { SourceAlpha = 0, ShapeAlpha };
enum ResampleMode { AreaAverage = 0, NearestNeighbor };

class RenderSettings {
  // メッシュ再帰分割の回数
  int m_warpPrecision;
  double m_faceSizeThreshold;

  // アルファを、素材で抜くか、シェイプの形に抜くか
  AlphaMode m_alphaMode;

  // リサンプル
  ResampleMode m_resampleMode;

  // ラスター画像の縮小読み込み
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

  // 保存/ロード
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);
};

#endif  // RENDERSETTINGS_H