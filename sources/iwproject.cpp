//---------------------------------------------------
// IwProject
// �v���W�F�N�g���̃N���X�B
//---------------------------------------------------

#include "iwproject.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwlayerhandle.h"

#include "viewsettings.h"
#include "preferences.h"

#include "rendersettings.h"
#include "outputsettings.h"
#include "shapetagsettings.h"

#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QStringRef>
#include <QMessageBox>

#include "iwlayer.h"

//---------------------------------------------------
IwProject::IwProject()
    : m_settings(new ViewSettings())
    , m_workAreaSize(0, 0)
    , m_renderSettings(new RenderSettings())
    , m_renderQueue(new RenderQueue())
    , m_shapeTagSettings(new ShapeTagSettings())
    , m_path("Untitled")
    , m_prevFrom(-1)
    , m_prevTo(-1) {
  static int id = 0;
  m_id          = id;
  id++;
}

//---------------------------------------------------
// ���[���������
// �V�F�C�v�����
// ViewSettings* m_settings�����
//---------------------------------------------------
IwProject::~IwProject() {
  // ViewSettings* m_settings�����
  delete m_settings;
  m_settings = 0;

  // ���C�������
  for (int lay = 0; lay < m_layers.size(); lay++) delete m_layers.at(lay);
  m_layers.clear();
}

//---------------------------------------------------
/// �e���C���ň�Ԓ����t���[������Ԃ��i=�v���W�F�N�g�̃t���[�����j
int IwProject::getProjectFrameLength() {
  int maxLen = 0;

  if (m_layers.isEmpty()) return 0;

  for (int s = 0; s < m_layers.size(); s++) {
    if (maxLen < m_layers.at(s)->getFrameLength())
      maxLen = m_layers.at(s)->getFrameLength();
  }

  return maxLen;
}

//---------------------------------------------------
// �v���W�F�N�g�̌��݂̃t���[����Ԃ�
//---------------------------------------------------
int IwProject::getViewFrame() { return m_settings->getFrame(); }

//---------------------------------------------------
// ���݂̃t���[����ύX����
//---------------------------------------------------
void IwProject::setViewFrame(int frame) {
  // �t���[���ɕύX���������return
  if (m_settings->getFrame() == frame) return;

  m_settings->setFrame(frame);

  IwApp::instance()->getCurrentProject()->notifyViewFrameChanged();
}

//---------------------------------------------------
// ���̃v���W�F�N�g���J�����g���ǂ�����Ԃ�
//---------------------------------------------------
bool IwProject::isCurrent() {
  return (IwApp::instance()->getCurrentProject()->getProject() == this);
}

//---------------------------------------------------
// �ۑ�/���[�h
//---------------------------------------------------
void IwProject::saveData(QXmlStreamWriter& writer) {
  // ImageSize (���[�N�G���A�̃T�C�Y)
  writer.writeComment("Work Area Size");
  writer.writeStartElement("ImageSize");
  writer.writeTextElement("width", QString::number(m_workAreaSize.width()));
  writer.writeTextElement("height", QString::number(m_workAreaSize.height()));
  writer.writeEndElement();

  // Guides
  if (!m_hGuides.empty() || !m_vGuides.empty()) {
    writer.writeComment("Guides Positions");
    writer.writeStartElement("Guides");
    if (!m_hGuides.empty()) {
      QString hGuidesStr;
      for (auto g : m_hGuides) hGuidesStr.append(QString::number(g) + " ");
      writer.writeTextElement("Horizontal", hGuidesStr);
    }
    if (!m_vGuides.empty()) {
      QString vGuidesStr;
      for (auto g : m_vGuides) vGuidesStr.append(QString::number(g) + " ");
      writer.writeTextElement("Vertical", vGuidesStr);
    }
    writer.writeEndElement();
  }

  // MorphOptions
  writer.writeComment("Render Settings");
  writer.writeStartElement("MorphOptions");
  m_renderSettings->saveData(writer);
  writer.writeEndElement();

  // OutputOptions
  writer.writeComment("Output Settings");
  writer.writeStartElement("RenderQueue");
  m_renderQueue->saveData(writer);
  writer.writeEndElement();

  // ShapeTagSettings
  if (m_shapeTagSettings->tagCount() > 0) {
    writer.writeComment("ShapeTag Settings");
    writer.writeStartElement("ShapeTags");
    m_shapeTagSettings->saveData(writer);
    writer.writeEndElement();
  }

  // Layers
  writer.writeComment("Layers");
  writer.writeStartElement("Layers");
  for (int lay = 0; lay < m_layers.size(); lay++) {
    if (!m_layers.at(lay)) continue;

    writer.writeStartElement("Layer");

    m_layers.at(lay)->saveData(writer);

    writer.writeEndElement();
  }
  writer.writeEndElement();
  writer.writeEndElement();
}

//---------------------------------------------------

void IwProject::loadData(QXmlStreamReader& reader) {
  while (reader.readNextStartElement()) {
    // ImageSize (���[�N�G���A�̃T�C�Y)
    if (reader.name() == "ImageSize") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "width") {
          int w = reader.readElementText().toInt();
          m_workAreaSize.setWidth(w);
        } else if (reader.name() == "height") {
          int h = reader.readElementText().toInt();
          m_workAreaSize.setHeight(h);
        }
      }
    }

    // Guides
    else if (reader.name() == "Guides") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "Horizontal") {
          m_hGuides.clear();
          QStringList guides =
              reader.readElementText().split(" ", Qt::SkipEmptyParts);
          for (auto guide : guides) m_hGuides.push_back(guide.toDouble());
        } else if (reader.name() == "Vertical") {
          m_vGuides.clear();
          QStringList guides =
              reader.readElementText().split(" ", Qt::SkipEmptyParts);
          for (auto guide : guides) m_vGuides.push_back(guide.toDouble());
        }
      }
    }

    // MorphOptions
    else if (reader.name() == "MorphOptions")
      m_renderSettings->loadData(reader);

    // OutputOptions : old version data
    else if (reader.name() == "OutputOptions")
      m_renderQueue->loadPrevVersionData(reader);
    // Render Queue
    else if (reader.name() == "RenderQueue")
      m_renderQueue->loadData(reader);

    // ShapeTag Settings
    else if (reader.name() == "ShapeTags")
      m_shapeTagSettings->loadData(reader);

    // Layers
    else if (reader.name() == "Layers") {
      // ���[�h���āA���܂܂ł̃��[���ƍ����ւ���
      while (reader.readNextStartElement()) {
        if (reader.name() == "Layer") {
          // ���[�h
          IwLayer* layer = appendLayer();
          layer->loadData(reader);
        } else
          reader.skipCurrentElement();
      }
    } else
      reader.skipCurrentElement();
  }

  std::cout << "loaded size = " << m_workAreaSize.width() << ", "
            << m_workAreaSize.height() << std::endl;
}

//---------------------------------------------------

void IwProject::versionCheck(const Version& loadedVersion) {
  // 0.1.0 �ȑO�ŁA�o�͐ݒ聨�����_�����O�͈�Start��InitialFrameNumber,
  // increment��
  // ���ׂ�1�łȂ��V�[�����J�����ꍇ�A�l�𒲐�������Ōx���_�C�A���O���o��
  QString msg = m_renderQueue->versionCheck(loadedVersion);
  if (msg.isEmpty()) return;

  QMessageBox::warning(nullptr, tr("Warning"),
                       tr("An older version of the project file was loaded. "
                          "Please check the following:") +
                           QString("\n\n%1").arg(msg));
}

//---------------------------------------------------
// �p�X����v���W�F�N�g���𔲂��o���ĕԂ�
//---------------------------------------------------
QString IwProject::getProjectName() { return QFileInfo(m_path).baseName(); }

//---------------------------------------------------
// frame�ɑ΂���ۑ��p�X��Ԃ�
//---------------------------------------------------
QString IwProject::getOutputPath(int frame, int outputFrame, QString formatStr,
                                 int queueId) {
  return m_renderQueue->getPath(frame, outputFrame, getProjectName(), formatStr,
                                queueId);
}

//---------------------------------------------------
// MultiFrame�֌W
//---------------------------------------------------
// �V�[�������ς�����Ƃ��AMultiFrame������������
//---------------------------------------------------
void IwProject::initMultiFrames() {
  int frameLength = getProjectFrameLength();

  // �t���[�������ς���Ă��Ȃ��Ƃ��Areturn
  if (m_multiFrames.size() == frameLength) return;

  // MultiFrame��S��True�ɃZ�b�g
  m_multiFrames.clear();
  for (int f = 0; f < frameLength; f++) m_multiFrames.append(true);
}

//---------------------------------------------------
// MultiFrame�̃��X�g��Ԃ�
//---------------------------------------------------
QList<int> IwProject::getMultiFrames() {
  QList<int> list;
  for (int f = 0; f < m_multiFrames.size(); f++) {
    if (m_multiFrames.at(f)) list.append(f);
  }
  return list;
}

//---------------------------------------------------
// MultiFrame�Ƀt���[����ǉ�/�폜����
//---------------------------------------------------
void IwProject::setMultiFrames(int frame, bool on) {
  if (0 <= frame && frame < m_multiFrames.size()) m_multiFrames[frame] = on;
}

//---------------------------------------------------
// ���C���֌W
//---------------------------------------------------
IwLayer* IwProject::getLayerByName(const QString& name) {
  for (int lay = 0; lay < getLayerCount(); lay++) {
    IwLayer* layer = getLayer(lay);
    if (!layer) continue;
    if (layer->getName() == name) return layer;
  }
  return nullptr;
}

IwLayer* IwProject::appendLayer() {
  IwLayer* newLayer = new IwLayer();
  m_layers.append(newLayer);
  return newLayer;
}

//---------------------------------------------------
// ���C���̂ǂꂩ�ɃV�F�C�v�������Ă����true
//---------------------------------------------------
bool IwProject::contains(ShapePair* shape) {
  for (int lay = 0; lay < getLayerCount(); lay++) {
    IwLayer* layer = getLayer(lay);
    if (!layer) continue;
    if (layer->contains(shape)) return true;
  }
  return false;
}
//---------------------------------------------------
// �V�F�C�v�����Ԗڂ̃��C���[�̉��Ԗڂɓ����Ă��邩�Ԃ�
//---------------------------------------------------
bool IwProject::getShapeIndex(const ShapePair* shape, int& layerIndex,
                              int& shapeIndex) {
  for (int lay = 0; lay < getLayerCount(); lay++) {
    IwLayer* layer = getLayer(lay);
    if (!layer) continue;
    int sIndex = layer->getIndexFromShapePair(shape);
    if (sIndex < 0) continue;
    layerIndex = lay;
    shapeIndex = sIndex;
    return true;
  }
  return false;
}

//---------------------------------------------------
// �V�F�C�v���烌�C����Ԃ�
//---------------------------------------------------
IwLayer* IwProject::getLayerFromShapePair(ShapePair* shape) {
  for (int lay = 0; lay < getLayerCount(); lay++) {
    IwLayer* layer = getLayer(lay);
    if (!layer) continue;
    if (layer->contains(shape)) return layer;
  }
  return 0;
}

ShapePair* IwProject::getParentShape(ShapePair* shape) {
  if (!shape) return nullptr;
  IwLayer* layer = getLayerFromShapePair(shape);
  if (!layer) return nullptr;
  return layer->getParentShape(shape);
}

bool IwProject::getPreviewRange(int& from, int& to) {
  if (m_prevFrom < 0) {
    from = 0;
    to   = getProjectFrameLength() - 1;
    return false;
  }
  assert(m_prevTo >= 0 && m_prevFrom <= m_prevTo);
  from = m_prevFrom;
  to   = m_prevTo;
  return true;
}

void IwProject::setPreviewFrom(int from) {
  if (from < 0) from = 0;
  if (m_prevTo < 0) {
    m_prevTo = getProjectFrameLength() - 1;
  }
  m_prevFrom = std::min(from, m_prevTo);
}

void IwProject::setPreviewTo(int to) {
  if (to > getProjectFrameLength() - 1) to = getProjectFrameLength() - 1;
  if (m_prevFrom < 0) m_prevFrom = 0;

  m_prevTo = std::max(to, m_prevFrom);
}

void IwProject::clearPreviewRange() {
  m_prevFrom = -1;
  m_prevTo   = -1;
}

void IwProject::toggleOnionFrame(int frame) {
  if (m_onionFrames.contains(frame))
    m_onionFrames.remove(frame);
  else if (frame >= 0 && frame < getProjectFrameLength()) {
    m_onionFrames.insert(frame);
  }
}
