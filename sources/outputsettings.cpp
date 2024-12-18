//---------------------------------------------
// OutputSettings
// ���݂̃t���[���ԍ��ɑΉ�����t�@�C���̕ۑ���p�X��Ԃ��B
//---------------------------------------------

#include "outputsettings.h"

#include "tiio.h"
#include "mypropertygroup.h"
#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwlayer.h"
#include "shapepair.h"

#include <iostream>

#include <QStringList>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QList>
#include <QRegExp>

OutputSettings::OutputSettings()
    : m_saver(Saver_PNG)
    , m_directory("")
    , m_format("[dir]/[base]_iwp.[num].[ext]")
    , m_initialFrameNumber(1)
    , m_increment(1)
    , m_numberOfDigits(4)
    , m_shapeTagId(-1)
    , m_shapeImageFileName("shape")
    , m_shapeImageSizeId(-1)  // work area size
    , m_renderState(On) {
  m_saveRange.startFrame = 0;
  m_saveRange.endFrame   = -1;  // end��-1�i�����l�̏ꍇ�́A�V�[�����ɍ��킹��j
  m_saveRange.stepFrame  = 1;

  // TIFF 16bpc���f�t�H���g�ɂ���
  // TEnumProperty* bitPerPixelProp =
  // (TEnumProperty*)(getFileFormatProperties(Saver_TIFF)->getProperty("Bits Per
  // Pixel")); bitPerPixelProp->setValue(L"64(RGBM)");
}

OutputSettings::OutputSettings(const OutputSettings &os)
    : m_saver(os.m_saver)
    , m_directory(os.m_directory)
    , m_format(os.m_format)
    , m_initialFrameNumber(os.m_initialFrameNumber)
    , m_increment(os.m_increment)
    , m_numberOfDigits(os.m_numberOfDigits)
    , m_saveRange(os.m_saveRange)
    , m_shapeTagId(os.m_shapeTagId)
    , m_shapeImageFileName(os.m_shapeImageFileName)
    , m_shapeImageSizeId(os.m_shapeImageSizeId)
    , m_renderState(On) {
  // m_formatProperties�̓N���[������
  for (auto key : os.m_formatProperties.keys()) {
    MyPropertyGroup *pg      = os.m_formatProperties.value(key);
    MyPropertyGroup *clonePg = new MyPropertyGroup(pg->getProp()->clone());
    m_formatProperties.insert(key, clonePg);
  }
}

//---------------------------------------------------
// frame�ɑ΂���ۑ��p�X��Ԃ�
//---------------------------------------------------
QString OutputSettings::getPath(int frame, QString projectName,
                                QString formatStr) {
  QString str = (formatStr.isEmpty()) ? m_format : formatStr;

  int modFrame = m_initialFrameNumber + frame * m_increment;
  QString numStr =
      QString::number(modFrame).rightJustified(m_numberOfDigits, '0');

  // DateTime���i�[����Ă��邩�m�F
  QRegExp rx("(.+)_\\d{6}-\\d{6}$");
  int pos = rx.indexIn(projectName);
  QString projectNameCore;
  if (pos > -1) {
    projectNameCore = rx.cap(1);
  } else
    projectNameCore = projectName;

  str = str.replace("[ext]", OutputSettings::getStandardExtension(m_saver))
            .replace("[num]", numStr)
            .replace("[base]", projectNameCore)
            .replace("[dir]", m_directory);

  if (str.contains("[mattename]") || str.contains("[mattenum]")) {
    if (m_tmp_matteLayerNames.isEmpty()) {
      str = str.replace("[mattename]", "nomatte").replace("[mattenum]", numStr);
    } else {
      // �Ƃ肠�������X�g1�߂̃��C���[��K���g��
      QString matteNumStr    = "----";
      QString matteLayerName = m_tmp_matteLayerNames[0];
      IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
      assert(project != nullptr);
      IwLayer *matteLayer = project->getLayerByName(matteLayerName);
      assert(matteLayer != nullptr);
      QString fileName = matteLayer->getImageFileName(frame);
      if (!fileName.isEmpty()) {
        // [�������x����] [�s���I�h�܂��̓A���_�[�o�[]
        // [any�����̐���][5�����܂ł̐ڔ���] [�s���I�h] [3�`4�����̊g���q]
        //                                             ��������������������������������
        //                                             �������L���v�`������
        QRegExp rx("^.+[._]([0-9]+\\S{,5})\\..{3,4}$");
        if (rx.indexIn(fileName, 0) != -1) {
          matteNumStr = rx.cap(1);
        }
      }

      str = str.replace("[mattename]", matteLayerName)
                .replace("[mattenum]", matteNumStr);
    }
  }

  return str;
}

//---------------------------------------------------
// �ۑ��`���ɑ΂���W���̊g���q��Ԃ� static
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
// ����ۑ��p�̕���������
//---------------------------------------------------
QString OutputSettings::getNumberFormatString() {
  return QString("%1/%2")
      .arg(QString::number(m_initialFrameNumber)
               .rightJustified(m_numberOfDigits, '0'))
      .arg(m_increment);
}

//---------------------------------------------------
// ���̋t�B�����񂩂�p�����[�^�𓾂�
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

// ���݂̏o�͐ݒ�Ń����_�����O�ΏۂƂȂ�V�F�C�v���p���Ă���
// �A���t�@�}�b�g���C���[�̃��C���[���ꗗ���ꎞ�I�ɕێ�����B�����_�����O�J�n���ɍĎ擾����
void OutputSettings::obtainMatteLayerNames() {
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  if (!m_format.contains("[mattename]") && m_format.contains("[mattenum]"))
    return;

  m_tmp_matteLayerNames.clear();

  // ������A�e���C���ɂ���
  for (int lay = project->getLayerCount() - 1; lay >= 0; lay--) {
    IwLayer *layer = project->getLayer(lay);
    if (!layer) continue;
    // ���C���ɃV�F�C�v��������΃X�L�b�v
    if (layer->getShapePairCount() == 0) continue;
    // ���C���������_�����O��\���Ȃ�X�L�b�v
    if (!layer->isVisibleInRender()) continue;
    // �V�F�C�v�������������Ă���
    for (int s = layer->getShapePairCount() - 1; s >= 0; s--) {
      ShapePair *shape = layer->getShapePair(s);
      if (!shape) continue;
      // �q�V�F�C�v�̂Ƃ��A���̃V�F�C�v��
      if (!shape->isParent()) continue;
      // �e�V�F�C�v���^�[�Q�b�g�ɂȂ��Ă��Ȃ��ꍇ�A����
      if (!shape->isRenderTarget(m_shapeTagId)) continue;
      // �A���t�@�}�b�g���t���Ă��邩
      QString matteLayerName = shape->matteInfo().layerName;
      if (matteLayerName.isEmpty()) continue;

      if (!m_tmp_matteLayerNames.contains(matteLayerName))
        m_tmp_matteLayerNames.append(matteLayerName);
    }
  }
}

//---------------------------------------------------
// �v���p�e�B�𓾂�
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
  // start, end�͕ۑ����ɂP�����āA���[�h���ɂP����
  writer.writeTextElement("startFrame",
                          QString::number(m_saveRange.startFrame + 1));
  writer.writeTextElement("endFrame",
                          QString::number(m_saveRange.endFrame + 1));
  writer.writeTextElement("stepFrame", QString::number(m_saveRange.stepFrame));
  writer.writeEndElement();

  writer.writeTextElement("saver", m_saver);
  writer.writeTextElement("directory", m_directory);
  writer.writeTextElement("format", m_format);
  writer.writeTextElement("number", getNumberFormatString());

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

  writer.writeTextElement("renderState", QString::number((int)m_renderState));
}

//---------------------------------------------------
void OutputSettings::loadData(QXmlStreamReader &reader) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "SaveRange") {
      while (reader.readNextStartElement()) {
        // start, end�͕ۑ����ɂP�����Ă���̂ŁA���[�h���ɂP����
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
    // else if (reader.name() == "extension")
    //   m_extension = reader.readElementText();
    else if (reader.name() == "format")
      m_format = reader.readElementText();
    else if (reader.name() == "number")
      setNumberFormat(reader.readElementText());
    // obsoleted
    // else if (reader.name() == "useSource")
    //  m_useSource = (reader.readElementText() == "True") ? true : false;
    // else if (reader.name() == "addNumber")
    //  m_addNumber = (reader.readElementText() == "True") ? true : false;
    // else if (reader.name() == "replaceExt")
    //  m_replaceExt = (reader.readElementText() == "True") ? true : false;

    else if (reader.name() == "FormatsProperties") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "FormatProperties") {
          QString saverStr = reader.attributes().value("saver").toString();
          // ��x�Ăяo�����Ƃō����
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
    else if (reader.name() == "renderState")
      m_renderState = (RenderState)reader.readElementText().toInt();
    else
      reader.skipCurrentElement();
  }
}

//------------------------------

RenderQueue::RenderQueue() {
  m_outputs.append(new OutputSettings());
  m_currentSettingsId = 0;
}

// �ۑ�/���[�h
void RenderQueue::saveData(QXmlStreamWriter &writer) {
  writer.writeStartElement("RenderQueueItems");
  for (auto os : m_outputs) {
    writer.writeStartElement("OutputOptions");
    os->saveData(writer);
    writer.writeEndElement();
  }
  writer.writeEndElement();

  writer.writeTextElement("currentSettingsId",
                          QString::number(m_currentSettingsId));
}

void RenderQueue::loadData(QXmlStreamReader &reader) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "RenderQueueItems") {
      m_outputs.clear();
      while (reader.readNextStartElement()) {
        if (reader.name() == "OutputOptions") {
          OutputSettings *os = new OutputSettings();
          os->loadData(reader);
          m_outputs.append(os);
        } else
          reader.skipCurrentElement();
      }
    } else if (reader.name() == "currentSettingsId")
      m_currentSettingsId = reader.readElementText().toInt();
    else
      reader.skipCurrentElement();
  }
}

void RenderQueue::loadPrevVersionData(QXmlStreamReader &reader) {
  m_outputs.clear();
  OutputSettings *os = new OutputSettings();
  os->loadData(reader);
  m_outputs.append(os);
  m_currentSettingsId = 0;
}

// frame�ɑ΂���ۑ��p�X��Ԃ�
QString RenderQueue::getPath(int frame, QString projectName, QString formatStr,
                             int queueId) {
  if (queueId == -1) queueId = m_currentSettingsId;
  return m_outputs.at(queueId)->getPath(frame, projectName, formatStr);
}

// On�ɂȂ��Ă���A�C�e����Ԃ�
QList<OutputSettings *> RenderQueue::activeItems() {
  QList<OutputSettings *> ret;
  for (auto os : m_outputs) {
    if (os->renderState() == OutputSettings::On) ret.append(os);
  }
  return ret;
}

// ���݂̃A�C�e����Ԃ�
OutputSettings *RenderQueue::currentOutputSettings() {
  return m_outputs[m_currentSettingsId];
}

void RenderQueue::cloneCurrentItem() {
  m_outputs.append(new OutputSettings(*currentOutputSettings()));

  m_currentSettingsId = m_outputs.size() - 1;
}

void RenderQueue::removeCurrentItem() {
  if (m_outputs.size() == 1) return;
  OutputSettings *toBeDeleted = currentOutputSettings();
  m_outputs.removeAt(m_currentSettingsId);

  if (m_currentSettingsId == m_outputs.size()) m_currentSettingsId -= 1;
}