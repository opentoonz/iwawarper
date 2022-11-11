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
// 新規プロジェクト
void newProject();
// シークエンスの選択範囲に画像を挿入
void insertImagesToSequence(int layerIndex = -1);
// ファイルパスから画像を一枚ロードする
TImageP loadImage(QString path);

// フレーム範囲を指定して差し替え
void doReplaceImages(IwLayer* layer, int from, int to);

// プロジェクトを保存
// ファイルパスを指定していない場合はダイアログを開く
void saveProject(QString path = QString());
void saveProjectWithDateTime(QString path);
// プロジェクトをロード
void loadProject(QString path = QString(), bool addToRecentFiles = true);
void importProject();

bool saveProjectIfNeeded(const QString commandName);

bool exportShapes(const QSet<ShapePair*>);
}  // namespace IoCmd

// TLVのロード時に開く
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