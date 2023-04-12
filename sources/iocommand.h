#ifndef IOCOMMAND_H
#define IOCOMMAND_H

#include "timage.h"

#include <iostream>
#include <QString>

#include <QDialog>
#include <QSet>

class IwLayer;
class QStringList;

class QLineEdit;
class ShapePair;

namespace IoCmd {
// �V�K�v���W�F�N�g
void newProject();
// �V�[�N�G���X�̑I��͈͂ɉ摜��}��
void insertImagesToSequence(int layerIndex = -1);
// �t�@�C���p�X����摜���ꖇ���[�h����
TImageP loadImage(QString path);

// �t���[���͈͂��w�肵�č����ւ�
void doReplaceImages(IwLayer* layer, int from, int to);

// �v���W�F�N�g��ۑ�
// �t�@�C���p�X���w�肵�Ă��Ȃ��ꍇ�̓_�C�A���O���J��
void saveProject(QString path = QString());
void saveProjectWithDateTime(QString path);
// �v���W�F�N�g�����[�h
void loadProject(QString path = QString(), bool addToRecentFiles = true);
void importProject();

bool saveProjectIfNeeded(const QString commandName);

bool exportShapes(const QList<ShapePair*>);
}  // namespace IoCmd

// TLV�̃��[�h���ɊJ��
class LoadFrameRangeDialog : public QDialog {
  Q_OBJECT

  QLineEdit* m_from;
  QLineEdit* m_to;
  QLineEdit* m_step;

public:
  LoadFrameRangeDialog(QWidget* parent, QString levelName, int from, int to);
  void getValues(int& from, int& to, int& step);
};

#endif