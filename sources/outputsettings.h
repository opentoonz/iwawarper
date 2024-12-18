//---------------------------------------------
// OutputSettings
// ���݂̃t���[���ԍ��ɑΉ�����t�@�C���̕ۑ���p�X��Ԃ��B
//---------------------------------------------

#ifndef OUTPUTSETTINGS_H
#define OUTPUTSETTINGS_H

//---------------------------------------------
// �Ή��ۑ��`���̃��X�g
//---------------------------------------------
#define Saver_TIFF "TIFF"
#define Saver_SGI "SGI"
#define Saver_PNG "PNG"
#define Saver_TGA "TGA"

#include <QString>
#include <QStringList>
#include <QMap>

class TPropertyGroup;
class MyPropertyGroup;
class QXmlStreamWriter;
class QXmlStreamReader;

class OutputSettings {
public:
  struct SaveRange {
    int startFrame;
    int endFrame;
    int stepFrame;
  };

  enum RenderState { Off = 0, On, Done };

private:
  // �ۑ��`��
  QString m_saver;
  // �ۑ��t�H���_
  QString m_directory;
  // �ۑ��p�X�̃t�H�[�}�b�g
  QString m_format;

  // Initial frame number �����_�����O1�t���[���ڂɂ��t���[���ԍ�
  int m_initialFrameNumber;
  // Increment �t���[���ԍ��������i�߂邩
  int m_increment;
  // Number of digits �t���[���ԍ��̌���
  int m_numberOfDigits;

  // �ۑ��t���[���͈�
  SaveRange m_saveRange;

  // �t�@�C���`�����̃v���p�e�B
  QMap<QString, MyPropertyGroup *> m_formatProperties;

  // FROM�܂���TO�̐e�V�F�C�v�ɁA����̃^�O�̂������̂��������_�����O����
  // -1�̏ꍇ�̓^�O�𖳎�����B�q�V�F�C�v�̃^�O�͖�������B
  int m_shapeTagId;

  QString m_shapeImageFileName;
  int m_shapeImageSizeId;

  RenderState m_renderState;

  // ���݂̏o�͐ݒ�Ń����_�����O�ΏۂƂȂ�V�F�C�v���p���Ă���
  // �A���t�@�}�b�g���C���[�̃��C���[���ꗗ���ꎞ�I�ɕێ�����B�����_�����O�J�n���ɍĎ擾����
  QStringList m_tmp_matteLayerNames;

public:
  OutputSettings();
  OutputSettings(const OutputSettings &);

  QString getSaver() { return m_saver; }
  void setSaver(QString saver) { m_saver = saver; }

  QString getDirectory() { return m_directory; }
  void setDirectory(QString directory) { m_directory = directory; }

  QString getFormat() { return m_format; }
  void setFormat(QString format) { m_format = format; }

  int getInitialFrameNumber() { return m_initialFrameNumber; }
  void setInitiaFrameNumber(int ifn) { m_initialFrameNumber = ifn; }

  int getIncrement() { return m_increment; }
  void setIncrement(int incr) { m_increment = incr; }

  int getNumberOfDigits() { return m_numberOfDigits; }
  void setNumberOfDigits(int nod) { m_numberOfDigits = nod; }

  SaveRange getSaveRange() { return m_saveRange; }
  void setSaveRange(SaveRange saveRange) { m_saveRange = saveRange; }

  int getShapeTagId() { return m_shapeTagId; }
  void setShapeTagId(int tag) { m_shapeTagId = tag; }

  QString getShapeImageFileName() { return m_shapeImageFileName; }
  void setShapeImageFileName(QString name) { m_shapeImageFileName = name; }
  int getShapeImageSizeId() { return m_shapeImageSizeId; }
  void setShapeImageSizeId(int id) { m_shapeImageSizeId = id; }

  RenderState renderState() { return m_renderState; }
  void setRenderState(RenderState state) { m_renderState = state; }

  // frame�ɑ΂���ۑ��p�X��Ԃ�
  QString getPath(int frame, QString projectName,
                  QString formatStr = QString());

  // �ۑ��`���ɑ΂���W���̊g���q��Ԃ�
  static QString getStandardExtension(QString saver);

  // m_initialFrameNumber, m_increment, m_numberOfDigits
  // ����ۑ��p�̕���������
  QString getNumberFormatString();
  // ���̋t�B�����񂩂�p�����[�^�𓾂�
  void setNumberFormat(QString str);

  // �v���p�e�B�𓾂�
  TPropertyGroup *getFileFormatProperties(QString saver);

  void obtainMatteLayerNames();

  // �ۑ�/���[�h
  void saveData(QXmlStreamWriter &writer);
  void loadData(QXmlStreamReader &reader);
};

class RenderQueue {
  QList<OutputSettings *> m_outputs;
  int m_currentSettingsId;

public:
  RenderQueue();

  // �ۑ�/���[�h
  void saveData(QXmlStreamWriter &writer);
  void loadData(QXmlStreamReader &reader);
  void loadPrevVersionData(QXmlStreamReader &reader);

  // frame�ɑ΂���ۑ��p�X��Ԃ�
  QString getPath(int frame, QString projectName, QString formatStr = QString(),
                  int queueId = -1);

  // On�ɂȂ��Ă���A�C�e����Ԃ�
  QList<OutputSettings *> activeItems();
  QList<OutputSettings *> allItems() { return m_outputs; }
  // ���݂̃A�C�e����Ԃ�
  OutputSettings *currentOutputSettings();
  OutputSettings *outputSettings(int index) {
    assert(index >= 0 && index < m_outputs.size());
    return m_outputs[index];
  }

  int currentSettingsId() { return m_currentSettingsId; }
  void setCurrentSettingsId(int id) { m_currentSettingsId = id; }
  void cloneCurrentItem();
  void removeCurrentItem();
};

#endif