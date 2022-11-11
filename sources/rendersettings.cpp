//---------------------------------------------------
// RenderSettings
// モーフィング、レンダリングの設定
//---------------------------------------------------

#include "rendersettings.h"

#include <QStringList>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

namespace {
QStringList precisionStrList = (QStringList() << "Linear"
                                              << "Low"
                                              << "Medium"
                                              << "High"
                                              << "Very High"
                                              << "Super High");

};

RenderSettings::RenderSettings()
    : m_warpPrecision(4)
    , m_alphaMode(SourceAlpha)
    , m_faceSizeThreshold(25.0)
    , m_imageShrink(1)
    , m_antialias(true) {}

//---------------------------------------------------

void RenderSettings::setWarpPrecision(int prec) {
  if (prec < 0)
    prec = 0;
  else if (prec > 5)
    prec = 5;
  m_warpPrecision = prec;
}

//---------------------------------------------------
// 保存/ロード
void RenderSettings::saveData(QXmlStreamWriter& writer) {
  writer.writeStartElement("WarpOptions");
  // メッシュ再帰分割の回数
  writer.writeTextElement("precision", precisionStrList.at(m_warpPrecision));
  writer.writeTextElement("faceSizeThreshold",
                          QString::number(m_faceSizeThreshold));
  writer.writeTextElement("alphaMode", QString::number((int)m_alphaMode));
  writer.writeTextElement("imageShrink", QString::number((int)m_imageShrink));
  writer.writeTextElement("antialias", (m_antialias) ? "1" : "0");
  writer.writeEndElement();
}

//---------------------------------------------------
void RenderSettings::loadData(QXmlStreamReader& reader) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "WarpOptions") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "precision")
          m_warpPrecision = precisionStrList.indexOf(reader.readElementText());
        else if (reader.name() == "faceSizeThreshold")
          m_faceSizeThreshold = reader.readElementText().toDouble();
        else if (reader.name() == "alphaMode")
          m_alphaMode = (AlphaMode)(reader.readElementText().toInt());
        else if (reader.name() == "imageShrink")
          m_imageShrink = (AlphaMode)(reader.readElementText().toInt());
        else if (reader.name() == "antialias")
          m_antialias = (reader.readElementText().toInt() != 0);
        else
          reader.skipCurrentElement();
      }
    } else
      reader.skipCurrentElement();
  }
}