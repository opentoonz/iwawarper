//---------------------------------------------------
// InfoDialog
// �t�@�C������\������
//---------------------------------------------------

#ifndef INFODIALOG_H
#define INFODIALOG_H

#include <QLabel>
#include <QMap>

#include "iwdialog.h"

class QString;
class QGridLayout;
class QSlider;

enum FormatItem {
  eFullpath = 0,
  eFileType,
  eOwner,
  eSize,
  eCreated,
  eModified,
  eLastAccess,
  eImageSize,
  eSaveBox,
  eBitsSample,
  eSamplePixel,
  eDpi
};

class InfoDialog : public IwDialog {
  Q_OBJECT

  // �\�����ڌQ
  QMap<FormatItem, QLabel*> m_items;
  // ���݂̃��C���[
  QLabel* m_layerLabel;
  // ���݂̃t���[���̃��x��
  QLabel* m_frameLabel;
  QSlider* m_frameSlider;

  QString m_currentPath;

  void createItem(FormatItem id, const QString& name, QGridLayout* layout);
  void updateFields(QString& path);
  void setVal(FormatItem id, const QString& str);
  QString fileSizeString(qint64 size, int precision = 2);
  QString getTypeString(QString& ext);

public:
  InfoDialog();

protected:
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);

protected slots:
  // �t���[���I�����ς������\����؂�ւ���
  void onSelectionChanged();
  void onSliderMoved(int frame);
};

#endif
