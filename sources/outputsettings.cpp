//---------------------------------------------
// OutputSettings
// 現在のフレーム番号に対応するファイルの保存先パスを返す。
//---------------------------------------------

#include "outputsettings.h"

#include "tiio.h"
#include "mypropertygroup.h"

#include <iostream>

#include <QStringList>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

OutputSettings::OutputSettings()
    : m_saver(Saver_PNG)
    , m_directory("")
    , m_extension("png")
    , m_format("[dir]/[base]_iwp.[num].[ext]")
    , m_initialFrameNumber(1)
    , m_increment(1)
    , m_numberOfDigits(4)
    , m_useSource(true)
    , m_addNumber(true)
    , m_replaceExt(true)
    , m_shapeTagId(-1)
    , m_shapeImageFileName("shape")
    , m_shapeImageSizeId(-1)  // work area size
{
  m_saveRange.startFrame = 0;
  m_saveRange.endFrame   = -1;  // endが-1（初期値の場合は、シーン長に合わせる）
  m_saveRange.stepFrame  = 1;

  // TIFF 16bpcをデフォルトにする
  // TEnumProperty* bitPerPixelProp =
  // (TEnumProperty*)(getFileFormatProperties(Saver_TIFF)->getProperty("Bits Per
  // Pixel")); bitPerPixelProp->setValue(L"64(RGBM)");
}

//---------------------------------------------------
// frameに対する保存パスを返す
//---------------------------------------------------
QString OutputSettings::getPath(int frame, QString projectName,
                                QString formatStr) {
  QString str = (formatStr.isEmpty()) ? m_format : formatStr;

  int modFrame = m_initialFrameNumber + frame * m_increment;
  QString numStr =
      QString::number(modFrame).rightJustified(m_numberOfDigits, '0');

  // DateTimeが格納されているか確認
  QRegExp rx("(.+)_\\d{6}-\\d{6}$");
  int pos = rx.indexIn(projectName);
  QString projectNameCore;
  if (pos > -1) {
    projectNameCore = rx.cap(1);
  } else
    projectNameCore = projectName;

  str = str.replace("[ext]", m_extension)
            .replace("[num]", numStr)
            .replace("[base]", projectNameCore)
            .replace("[dir]", m_directory);

  return str;
}

//---------------------------------------------------
// 保存形式に対する標準の拡張子を返す static
//---------------------------------------------------
QString OutputSettings::getStandardExtension(QString saver) {
  if (saver == Saver_PNG)
    return QString("png");
  else if (saver == Saver_TIFF)
    return QString("tif");
  else if (saver == Saver_SGI)
    return QString("sgi");
  else if (saver == Saver_TGA)
    return QString("tga");

  else
    return QString("");
}

//---------------------------------------------------
// m_initialFrameNumber, m_increment, m_numberOfDigits
// から保存用の文字列を作る
//---------------------------------------------------
QString OutputSettings::getNumberFormatString() {
  return QString("%1/%2")
      .arg(QString::number(m_initialFrameNumber)
               .rightJustified(m_numberOfDigits, '0'))
      .arg(m_increment);
}

//---------------------------------------------------
// ↑の逆。文字列からパラメータを得る
//---------------------------------------------------
void OutputSettings::setNumberFormat(QString str) {
  QStringList list     = str.split("/");
  m_numberOfDigits     = list.at(0).size();
  m_initialFrameNumber = list.at(0).toInt();
  m_increment          = list.at(1).toInt();

  std::cout << "str = " << str.toStdString()
            << "  m_numberOfDigits:" << m_numberOfDigits
            << "  m_initialFrameNumber:" << m_initialFrameNumber
            << "  m_increment:" << m_increment << std::endl;
}

//---------------------------------------------------
// プロパティを得る
//---------------------------------------------------
TPropertyGroup *OutputSettings::getFileFormatProperties(QString saver) {
  QMap<QString, MyPropertyGroup *>::const_iterator it;
  it = m_formatProperties.find(saver);

  if (it == m_formatProperties.end()) {
    QString ext               = OutputSettings::getStandardExtension(saver);
    TPropertyGroup *ret       = Tiio::makeWriterProperties(ext.toStdString());
    MyPropertyGroup *prop     = new MyPropertyGroup(ret);
    m_formatProperties[saver] = prop;
    return ret;
  } else
    return it.value()->getProp();
}

//---------------------------------------------------
void OutputSettings::saveData(QXmlStreamWriter &writer) {
  writer.writeStartElement("SaveRange");
  // start, endは保存時に１足して、ロード時に１引く
  writer.writeTextElement("startFrame",
                          QString::number(m_saveRange.startFrame + 1));
  writer.writeTextElement("endFrame",
                          QString::number(m_saveRange.endFrame + 1));
  writer.writeTextElement("stepFrame", QString::number(m_saveRange.stepFrame));
  writer.writeEndElement();

  writer.writeTextElement("saver", m_saver);
  writer.writeTextElement("directory", m_directory);
  writer.writeTextElement("extension", m_extension);
  writer.writeTextElement("format", m_format);
  writer.writeTextElement("number", getNumberFormatString());
  writer.writeTextElement("useSource", (m_useSource) ? "True" : "False");
  writer.writeTextElement("addNumber", (m_addNumber) ? "True" : "False");
  writer.writeTextElement("replaceExt", (m_replaceExt) ? "True" : "False");

  if (!m_formatProperties.isEmpty()) {
    writer.writeStartElement("FormatsProperties");
    QMap<QString, MyPropertyGroup *>::const_iterator i =
        m_formatProperties.constBegin();
    while (i != m_formatProperties.constEnd()) {
      writer.writeStartElement("FormatProperties");
      writer.writeAttribute("saver", i.key());
      i.value()->saveData(writer);
      writer.writeEndElement();
      ++i;
    }
    writer.writeEndElement();
  }

  if (m_shapeTagId != -1)
    writer.writeTextElement("shapeTagId", QString::number(m_shapeTagId));

  writer.writeTextElement("shapeImageFileName", m_shapeImageFileName);
  writer.writeTextElement("shapeImageSizeId",
                          QString::number(m_shapeImageSizeId));
}

//---------------------------------------------------
void OutputSettings::loadData(QXmlStreamReader &reader) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "SaveRange") {
      while (reader.readNextStartElement()) {
        // start, endは保存時に１足しているので、ロード時に１引く
        if (reader.name() == "startFrame")
          m_saveRange.startFrame = reader.readElementText().toInt() - 1;
        else if (reader.name() == "endFrame")
          m_saveRange.endFrame = reader.readElementText().toInt() - 1;
        else if (reader.name() == "stepFrame")
          m_saveRange.stepFrame = reader.readElementText().toInt();
        else
          reader.skipCurrentElement();
      }
    } else if (reader.name() == "saver")
      m_saver = reader.readElementText();
    else if (reader.name() == "directory")
      m_directory = reader.readElementText();
    else if (reader.name() == "extension")
      m_extension = reader.readElementText();
    else if (reader.name() == "format")
      m_format = reader.readElementText();
    else if (reader.name() == "number")
      setNumberFormat(reader.readElementText());
    else if (reader.name() == "useSource")
      m_useSource = (reader.readElementText() == "True") ? true : false;
    else if (reader.name() == "addNumber")
      m_addNumber = (reader.readElementText() == "True") ? true : false;
    else if (reader.name() == "replaceExt")
      m_replaceExt = (reader.readElementText() == "True") ? true : false;

    else if (reader.name() == "FormatsProperties") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "FormatProperties") {
          QString saverStr = reader.attributes().value("saver").toString();
          // 一度呼び出すことで作られる
          getFileFormatProperties(saverStr);
          m_formatProperties[saverStr]->loadData(reader);
        } else
          reader.skipCurrentElement();
      }
    } else if (reader.name() == "shapeTagId")
      m_shapeTagId = reader.readElementText().toInt();

    else if (reader.name() == "shapeImageFileName")
      m_shapeImageFileName = reader.readElementText();
    else if (reader.name() == "shapeImageSizeId")
      m_shapeImageSizeId = reader.readElementText().toInt();
    else
      reader.skipCurrentElement();
  }
}
