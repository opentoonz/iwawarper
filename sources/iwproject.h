//---------------------------------------------------
// IwProject
// �v���W�F�N�g���̃N���X�B
//---------------------------------------------------
#ifndef IWPROJECT_H
#define IWPROJECT_H

#include "colorsettings.h"

#include <QList>
#include <QMap>
#include <QSet>
#include <QSize>
#include <QString>

class ViewSettings;
class Preferences;
class QXmlStreamWriter;
class QXmlStreamReader;
class RenderSettings;
class OutputSettings;
class ShapeTagSettings;

class IwLayer;
struct OneShape;
class ShapePair;

class IwProject {
  // �v���W�F�N�g���ƂɐU����ʂ��ԍ�
  int m_id;

  // �v���W�F�N�g���Ɏ��A���݂̕\����Ԃ��Ǘ�����N���X
  // �v���W�F�N�g�t�@�C���ɂ͂��Ԃ�ۑ�����Ȃ�
  ViewSettings* m_settings;

  // ���[�N�G���A�i�J�����j�̃T�C�Y
  QSize m_workAreaSize;

  // �t�@�C���p�X
  QString m_path;

  // �����_�����O�ݒ�
  RenderSettings* m_renderSettings;

  // �o�͐ݒ�
  OutputSettings* m_outputSettings;

  // MultiFrame���[�h��ON�̂Ƃ��ɕҏW�����t���[����true
  // �v���W�F�N�g�t�@�C���ɂ͕ۑ�����Ȃ�
  QList<bool> m_multiFrames;

  QList<IwLayer*> m_layers;

  // �v���r���[�͈�
  int m_prevFrom, m_prevTo;

  // �V�F�C�v�^�O�ݒ�. id �� ShapeTagInfo(���O�A�F�A�`)
  ShapeTagSettings* m_shapeTagSettings;

  // �I�j�I���X�L���t���[��
  QSet<int> m_onionFrames;

public:
  typedef std::vector<double> Guides;

private:
  Guides m_hGuides, m_vGuides;

public:
  IwProject();

  // ���[���������
  // �V�F�C�v�����
  //  ViewSettings* m_settings�����
  //  Preferences * m_preferences�����
  //  ColorSettings * m_colorSettings�����
  //  �O���[�v��� m_groups�����
  ~IwProject();

  // �ʂ��ԍ���Ԃ�
  int getId() { return m_id; }

  // �e���[���ň�Ԓ����t���[������Ԃ��i=�v���W�F�N�g�̃t���[�����j
  int getProjectFrameLength();
  // �v���W�F�N�g�̌��݂̃t���[����Ԃ�
  int getViewFrame();
  // ���݂̃t���[����ύX����
  void setViewFrame(int frame);

  ViewSettings* getViewSettings() { return m_settings; }
  ShapeTagSettings* getShapeTags() { return m_shapeTagSettings; }

  // ���[�N�G���A�i�J�����j�̃T�C�Y
  QSize getWorkAreaSize() { return m_workAreaSize; }
  void setWorkAreaSize(QSize size) { m_workAreaSize = size; }

  // ���̃v���W�F�N�g���J�����g���ǂ�����Ԃ�
  bool isCurrent();

  // �t�@�C���p�X
  QString getPath() { return m_path; }
  void setPath(QString path) { m_path = path; }

  // �ۑ�/���[�h
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);

  // �����_�����O�ݒ�
  RenderSettings* getRenderSettings() { return m_renderSettings; }

  // �o�͐ݒ�
  OutputSettings* getOutputSettings() { return m_outputSettings; }

  // �p�X����v���W�F�N�g���𔲂��o���ĕԂ�
  QString getProjectName();

  // frame�ɑ΂���ۑ��p�X��Ԃ�
  QString getOutputPath(int frame, QString formatStr = QString());

  //-------------
  // MultiFrame�֌W
  // �V�[�������ς�����Ƃ��AMultiFrame������������
  void initMultiFrames();
  // MultiFrame�̃��X�g��Ԃ�
  QList<int> getMultiFrames();
  // MultiFrame�Ƀt���[����ǉ�/�폜����
  void setMultiFrames(int frame, bool on);
  // ����t���[����MultiFrame�őI������Ă��邩�ǂ�����Ԃ�
  bool isMultiFrame(int frame) {
    if (frame < 0 || frame >= m_multiFrames.size()) return false;
    return m_multiFrames.at(frame);
  }
  //-------------

  //-------------
  // ���C���֌W
  int getLayerCount() { return m_layers.size(); }
  IwLayer* getLayer(int index) { return m_layers.at(index); }
  IwLayer* appendLayer();
  void swapLayers(int index1, int index2) {
    if (index1 < 0 || index1 >= m_layers.size() || index2 < 0 ||
        index2 >= m_layers.size())
      return;
    m_layers.swapItemsAt(index1, index2);
  }
  // ���C������
  void deleteLayer(int index) {
    if (index < 0 || index >= m_layers.size()) return;
    m_layers.removeAt(index);
  }
  void deleteLayer(IwLayer* layer) { m_layers.removeOne(layer); }
  // ���C���}��
  void insertLayer(IwLayer* layer, int index) {
    if (index < 0 || index > m_layers.size()) return;
    m_layers.insert(index, layer);
  }
  // ���C�����בւ�
  void reorderLayers(QList<IwLayer*> layers) {
    if (m_layers.count() != layers.count() ||
        QSet<IwLayer*>(m_layers.begin(), m_layers.end()) !=
            QSet<IwLayer*>(layers.begin(), layers.end()))
      return;
    m_layers = layers;
  }

  //-------------
  // ���C���̂ǂꂩ�ɃV�F�C�v�������Ă����true
  bool contains(ShapePair*);
  // �V�F�C�v�����Ԗڂ̃��C���[�̉��Ԗڂɓ����Ă��邩�Ԃ�
  bool getShapeIndex(const ShapePair*, int& layerIndex, int& shapeIndex);
  // �V�F�C�v���烌�C����Ԃ�
  IwLayer* getLayerFromShapePair(ShapePair*);
  // �ȒP�̂��ߒǉ��B�e�V�F�C�v��Ԃ�
  ShapePair* getParentShape(ShapePair*);

  // ���C���������Ă����true
  bool contains(IwLayer* lay) { return m_layers.contains(lay); }

  bool getPreviewRange(int& from, int& to);
  void setPreviewFrom(int from);
  void setPreviewTo(int to);
  void clearPreviewRange();

  /*!
          Returns horizontal Guides; a vector of double values that shows
     y-value of
          horizontal lines.
  */
  Guides& getHGuides() { return m_hGuides; }
  /*!
          Returns vertical Guides; a vector of double values that shows x-value
     of
          vertical lines.
  */
  Guides& getVGuides() { return m_vGuides; }

  void toggleOnionFrame(int frame);
  const QSet<int> getOnionFrames() { return m_onionFrames; };
};

#endif
