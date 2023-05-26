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

private:
  // �ۑ��`��
  QString m_saver;
  // �ۑ��t�H���_
  QString m_directory;
  // �g���q
  QString m_extension;
  // �ۑ��p�X�̃t�H�[�}�b�g
  QString m_format;

  // Initial frame number �����_�����O1�t���[���ڂɂ��t���[���ԍ�
  int m_initialFrameNumber;
  // Increment �t���[���ԍ��������i�߂邩
  int m_increment;
  // Number of digits �t���[���ԍ��̌���
  int m_numberOfDigits;
  // Use project name [base]�^�O���g�����ǂ����B���N���b�N���̂݁Am_format���X�V
  bool m_useSource;
  // Add frame number [num]�^�O���g�����ǂ����B���N���b�N���̂݁Am_format���X�V
  bool m_addNumber;
  // Add Extension [ext]�^�O���g�����ǂ����B���N���b�N���̂݁Am_format���X�V
  bool m_replaceExt;

  // �ۑ��t���[���͈�
  SaveRange m_saveRange;

  // �t�@�C���`�����̃v���p�e�B
  QMap<QString, MyPropertyGroup *> m_formatProperties;

  // FROM�܂���TO�̐e�V�F�C�v�ɁA����̃^�O�̂������̂��������_�����O����
  // -1�̏ꍇ�̓^�O�𖳎�����B�q�V�F�C�v�̃^�O�͖�������B
  int m_shapeTagId;

  QString m_shapeImageFileName;
  int m_shapeImageSizeId;

public:
  OutputSettings();

  QString getSaver() { return m_saver; }
  void setSaver(QString saver) { m_saver = saver; }

  QString getDirectory() { return m_directory; }
  void setDirectory(QString directory) { m_directory = directory; }

  QString getExtension() { return m_extension; }
  void setExtension(QString extension) { m_extension = extension; }

  QString getFormat() { return m_format; }
  void setFormat(QString format) { m_format = format; }

  int getInitialFrameNumber() { return m_initialFrameNumber; }
  void setInitiaFrameNumber(int ifn) { m_initialFrameNumber = ifn; }

  int getIncrement() { return m_increment; }
  void setIncrement(int incr) { m_increment = incr; }

  int getNumberOfDigits() { return m_numberOfDigits; }
  void setNumberOfDigits(int nod) { m_numberOfDigits = nod; }

  bool isUseSource() { return m_useSource; }
  void setIsUseSource(bool use) { m_useSource = use; }

  bool isAddNumber() { return m_addNumber; }
  void setIsAddNumber(bool add) { m_addNumber = add; }

  bool isReplaceExt() { return m_replaceExt; }
  void setIsReplaceExt(bool replace) { m_replaceExt = replace; }

  SaveRange getSaveRange() { return m_saveRange; }
  void setSaveRange(SaveRange saveRange) { m_saveRange = saveRange; }

  int getShapeTagId() { return m_shapeTagId; }
  void setShapeTagId(int tag) { m_shapeTagId = tag; }

  QString getShapeImageFileName() { return m_shapeImageFileName; }
  void setShapeImageFileName(QString name) { m_shapeImageFileName = name; }
  int getShapeImageSizeId() { return m_shapeImageSizeId; }
  void setShapeImageSizeId(int id) { m_shapeImageSizeId = id; }

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

  // �ۑ�/���[�h
  void saveData(QXmlStreamWriter &writer);
  void loadData(QXmlStreamReader &reader);
};

#endif