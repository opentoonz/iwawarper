#include "iocommand.h"

#include <QFileDialog>
#include <QStringList>
#include <QMessageBox>

#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QDateTime>
#include <QProcess>
#include <QTextCodec>
#include <QLineEdit>
#include <QIntValidator>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

#include "tfilepath.h"
#include "tlevel_io.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
#include "timageinfo.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"
#include "iwapp.h"
#include "iwproject.h"
#include "iwprojecthandle.h"
#include "iwselectionhandle.h"
#include "iwtimelineselection.h"

#include "personalsettingsmanager.h"
#include "mainwindow.h"

#include "iwlayerhandle.h"
#include "iwlayer.h"
#include "tlevel.h"
#include "projectutils.h"
#include "shapepair.h"

#include "logger.h"

#define PRINT_LOG(message)                                                     \
  { Logger::Write(message); }

namespace {
const int majorVersion = 0;
const int minorVersion = 1;
const int patchVersion = 0;

//[ファイル名]#0000 という形式にして返す
QString getFileNameWithFrameNumber(QString fileName, int frameNumber) {
  return QString("%1#%2").arg(fileName).arg(
      QString::number(frameNumber).rightJustified(4, '0'));
}

//[ファイル名]#0000という形式の文字列を入力。フレーム番号を取り除き、返す。
int separateFileNameAndFrameNumber(QString& fileName) {
  int frameNumber = fileName.right(4).toInt();
  fileName.chop(5);
  return frameNumber;
}

// ユーザ名を返す
QString getUserName() {
  QStringList list = QProcess::systemEnvironment();
  int j;
  for (j = 0; j < list.size(); j++) {
    QString value = list.at(j);
    QString user;
#ifdef WIN32
    if (value.startsWith("USERNAME=")) user = value.right(value.size() - 9);
#else
    if (value.startsWith("USER=")) user = value.right(value.size() - 5);
#endif
    if (!user.isEmpty()) return user;
  }
  return QString("none");
}

QStringList getOpenFilePathList() {
  // 読み込めるファイルのリストを作る
  QStringList formatList;
  formatList << "bmp"
             << "jpg"
             << "pic"
             << "pct"
             << "png"
             << "rgb"
             << "sgi"
             << "tga"
             << "tif"
             << "tiff"
             << "tlv"
             << "pli"
             << "mov"
             << "avi";

  QString filterString = QString("Images (");
  for (int f = 0; f < formatList.size(); f++) {
    filterString.append(QString(" *.%1").arg(formatList.at(f)));
  }
  filterString.append(" )");

  // 個人設定に保存されている初期フォルダを取得
  QString initialFolder = PersonalSettingsManager::instance()
                              ->getValue(PS_G_ProjEditPrefs, PS_ImageDir)
                              .toString();

  // ファイルダイアログを開く
  return QFileDialog::getOpenFileNames(IwApp::instance()->getMainWindow(),
                                       QString("Insert Images"), initialFolder,
                                       filterString);
}

// ファイルパスからフレーム番号を抽出する。取れなければ-1を返す
int getFrameNumberFromName(QString fileName) {
  // 最後のピリオド以降を削る
  int extIndex = fileName.lastIndexOf(".");
  fileName.truncate(extIndex);
  // 後ろの4桁を抽出
  QString numStr = fileName.right(4);
  bool ok;
  int ret = numStr.toInt(&ok, 10);
  if (!ok)
    return -1;
  else
    return ret;
}
};  // namespace

//-------------------------------
// 新規プロジェクト
//-------------------------------
void IoCmd::newProject() {
  // 空のプロジェクトを作る
  IwProject* project = new IwProject();

  // プロジェクトをIwAppのロードされているプロジェクトリストに登録
  IwApp::instance()->insertLoadedProject(project);

  // カレントプロジェクトをこのプロジェクトにする
  IwApp::instance()->getCurrentProject()->setProject(project);
}

//-------------------------------
// シークエンスの選択範囲に画像を挿入
//-------------------------------
// new done
void IoCmd::insertImagesToSequence(int layerIndex) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  // レイヤ作らない場合は、現在のレイヤを取得
  IwLayer* layer = nullptr;
  if (layerIndex >= 0 && layerIndex < prj->getLayerCount())
    layer = prj->getLayer(layerIndex);
  // 新規レイヤを作る場合
  if (!layer) layer = prj->appendLayer();

  QStringList pathList = getOpenFilePathList();

  // １つもファイルが選択されていなければreturn
  if (pathList.empty()) {
    // 空レイヤを作った場合もプロジェクト変更シグナルをエミットする。
    IwApp::instance()->getCurrentLayer()->notifyLayerChanged(layer);
    return;
  }

  // ファイル名をソート
  pathList.sort();

  // 挿入するフレーム とりあえずおしりに付ける
  // ★本当は選択シークエンス範囲の先頭にあわせる
  int f = layer->getFrameLength();
  // int f = 0;

  // 現在のシークエンスにフォルダパスを追加し、インデックスを得る
  QDir dir(pathList.at(0));
  dir.cdUp();
  QString folderPath = dir.path();
  // ここで、カレントシークエンスからフォルダのインデックスを得る
  int folderIndex = layer->getFolderPathIndex(folderPath);

  // 選択ファイル１つごとに
  for (int p = 0; p < pathList.size(); p++) {
    QString path = pathList.at(p);

    TFilePath fp(path.toStdWString());
    std::string ext = fp.getUndottedType();  // 拡張子

    // ファイル名
    QString fileName =
        QString::fromStdWString(fp.withoutParentDir().getWideString());

    // ムービーファイルの場
    if (ext == "tlv" || ext == "pli" || ext == "mov" || ext == "avi") {
      TLevelReaderP lr(fp);
      TLevelP level = lr->loadInfo();

      int fromFrame = level->begin()->first.getNumber();
      int toFrame   = level->getTable()->rbegin()->first.getNumber();

      LoadFrameRangeDialog* dialog = new LoadFrameRangeDialog(
          IwApp::instance()->getMainWindow(), fileName, fromFrame, toFrame);
      dialog->exec();
      int step;
      dialog->getValues(fromFrame, toFrame, step);
      delete dialog;

      // 現在の選択シークエンス範囲の代わりに連番を挿入
      std::vector<TFrameId> fids;
      for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
        // フレーム番号
        int frameNum = it->first.getNumber();
        if (fromFrame <= frameNum && frameNum <= toFrame) {
          QString fileNameWithFrame =
              getFileNameWithFrameNumber(fileName, frameNum);
          for (int s = 0; s < step; s++) {
            layer->insertPath(
                f, QPair<int, QString>(folderIndex, fileNameWithFrame));
            // 挿入ポイントを１つ右に進める
            f++;
          }
        }
      }

    }
    // 単品画像の場合
    else {
      // 選択シークエンス範囲に画像を挿入
      layer->insertPath(f, QPair<int, QString>(folderIndex, fileName));
      // 挿入ポイントを１つ右に進める
      f++;
    }
    // ひとつでも読めた場合はフラグ立てる
  }

  // カレントレイヤを移動
  IwApp::instance()->getCurrentLayer()->setLayer(layer);

  // ひとつでも読めた場合は、プロジェクト変更シグナルをエミットする。
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(layer);

  // ロードしたフォルダを個人設定ファイルに格納する
  PersonalSettingsManager::instance()->setValue(PS_G_ProjEditPrefs, PS_ImageDir,
                                                QVariant(folderPath));

  //---ここで、現在のProjectのワークエリアが未設定のとき（最初に画像を読み込んだとき）
  // １フレーム目の画像サイズをワークエリアのサイズとして用いる
  // 現在のプロジェクトを取得
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (project->getWorkAreaSize().isEmpty()) {
    TImageP img = layer->getImage(0);
    if (img.getPointer()) {
      TRasterP ras;
      TRasterImageP ri = (TRasterImageP)img;
      TToonzImageP ti  = (TToonzImageP)img;
      if (ri)
        ras = ri->getRaster();
      else if (ti)
        ras = ti->getRaster();
      // 形式が合わなければreturn
      else
        return;

      project->setWorkAreaSize(QSize(ras->getWrap(), ras->getLy()));
      // シグナルをエミットして、SequenceEditorのワークエリアサイズの表示をこうしんする
      IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    }
  }
}

//---------------------------------------------------
// フレーム範囲を指定して差し替え
//---------------------------------------------------
void IoCmd::doReplaceImages(IwLayer* layer, int from, int to) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  QStringList pathList = getOpenFilePathList();
  // １つもファイルが選択されていなければreturn
  if (pathList.empty()) return;
  // ファイル名をソート
  pathList.sort();

  // 現在のシークエンスにフォルダパスを追加し、インデックスを得る
  QDir dir(pathList.at(0));
  dir.cdUp();
  QString folderPath = dir.path();
  // ここで、カレントシークエンスからフォルダのインデックスを得る
  int folderIndex = layer->getFolderPathIndex(folderPath);

  // TLVならほぐしてパスのリストをば作る
  QList<QString> timelinePathList;

  // 選択ファイル１つごとに
  for (int p = 0; p < pathList.size(); p++) {
    QString path = pathList.at(p);
    TFilePath fp(path.toStdWString());
    std::string ext = fp.getUndottedType();  // 拡張子
    // ファイル名
    QString fileName =
        QString::fromStdWString(fp.withoutParentDir().getWideString());
    // ムービーファイルの場合
    if (ext == "tlv" || ext == "pli" || ext == "mov" || ext == "avi") {
      TLevelReaderP lr(fp);
      TLevelP level = lr->loadInfo();

      int fromFrame = level->begin()->first.getNumber();
      int toFrame   = level->getTable()->rbegin()->first.getNumber();

      LoadFrameRangeDialog* dialog = new LoadFrameRangeDialog(
          IwApp::instance()->getMainWindow(), fileName, fromFrame, toFrame);
      dialog->exec();
      int step;
      dialog->getValues(fromFrame, toFrame, step);
      delete dialog;

      // 現在の選択シークエンス範囲の代わりに連番を挿入
      std::vector<TFrameId> fids;
      for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
        // フレーム番号
        int frameNum = it->first.getNumber();
        if (fromFrame <= frameNum && frameNum <= toFrame) {
          QString fileNameWithFrame =
              getFileNameWithFrameNumber(fileName, frameNum);
          for (int s = 0; s < step; s++)
            // ステップ数ぶん登録する
            timelinePathList.append(fileNameWithFrame);
        }
      }
    }
    // 単品画像の場合
    else
      // 登録する
      timelinePathList.append(fileName);
  }

  // このリストを使って差し替える
  // 選択範囲の各フレームについて
  int i = 0;
  int f = from;

  // パスが１つだけの場合は、選択範囲をすべてそれで置き換える
  if (timelinePathList.size() == 1) {
    for (; f <= to; f++, i++) {
      layer->replacePath(
          f, QPair<int, QString>(folderIndex, timelinePathList.at(0)));
    }
  } else {
    for (; f <= to; f++, i++) {
      // リストがまだ足りていればファイルを差し替え
      if (i < timelinePathList.size())
        layer->replacePath(
            f, QPair<int, QString>(folderIndex, timelinePathList.at(i)));
      // リストをオーバーしていたら、空きセルにする
      else
        layer->replacePath(f, QPair<int, QString>(0, QString()));
    }
    // 選択範囲よりロード画像が多い場合、後ろに追加する
    if (i < timelinePathList.size())
      for (; i < timelinePathList.size(); i++, f++) {
        layer->insertPath(
            f, QPair<int, QString>(folderIndex, timelinePathList.at(i)));
      }
  }

  // ひとつでも読めた場合は、プロジェクト変更シグナルをエミットする。
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(layer);

  // ロードしたフォルダを個人設定ファイルに格納する
  PersonalSettingsManager::instance()->setValue(PS_G_ProjEditPrefs, PS_ImageDir,
                                                QVariant(folderPath));

  IwTimeLineSelection::instance()->selectFrames(from, f - 1, true);
  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//---------------------------------------------------
// ファイルパスから画像を一枚ロードする
//---------------------------------------------------
TImageP IoCmd::loadImage(QString path) {
  PRINT_LOG("loadImage")
  // TLVなどムービーファイルのとき
  if (path.lastIndexOf('#') == (path.length() - 5)) {
    //[ファイル名]#0000という形式の文字列を入力。フレーム番号を取り除き、返す。
    int frame = separateFileNameAndFrameNumber(path);
    TFilePath fp(path.toStdWString());
    TLevelReaderP lr(fp);

    TImageP img;

    TImageReaderP ir = lr->getFrameReader(frame);
    ir->enable16BitRead(true);
    img = ir->load();
    // img = lr->getFrameReader(frame)->load();

    return img;
  }
  // 単品画像のとき
  else {
    TFilePath fp(path.toStdWString());
    // multiFileLevelEnabledをOFFにすること
    TLevelReaderP lr(fp);
    TImageReaderP ir(fp);
    // lr->disableMultiFileLevel();

    TImageP img;

    // TImageReaderP ir = lr->getFrameReader(0);
    ir->enable16BitRead(true);
    img = ir->load();
    // img = lr->getFrameReader(0)->load();

    return img;
  }
  return TImageP();
}

//---------------------------------------------------
// プロジェクトを保存
// ファイルパスを指定していない場合はダイアログを開く
//---------------------------------------------------
void IoCmd::saveProject(QString path) {
  // 現在のプロジェクトが無ければreturn
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  bool addToRecentFiles = false;
  // 名称未設定ならファイルダイアログでパスを指定
  if (path.isEmpty() || path == "Untitled") {
    path = QFileDialog::getSaveFileName(IwApp::instance()->getMainWindow(),
                                        "Save Project File", prj->getPath(),
                                        "IwaWarper Project (*.iwp)");
    addToRecentFiles = true;
  }

  if (path.isEmpty()) return;

  QFile fp(path);
  if (!fp.open(QIODevice::WriteOnly)) return;

  QXmlStreamWriter writer(&fp);

  writer.setAutoFormatting(true);

  writer.writeStartDocument();

  writer.writeStartElement("IwaWarper");
  // バージョン
  writer.writeAttribute("version", QString("%1.%2.%3")
                                       .arg(majorVersion)
                                       .arg(minorVersion)
                                       .arg(patchVersion));

  // 情報を書く
  writer.writeStartElement("Info");
  // 作成日時
  writer.writeTextElement("date",
                          QDateTime::currentDateTime().toString(Qt::ISODate));
  // 作成者
  writer.writeTextElement("author", getUserName());
  writer.writeEndElement();

  // プロジェクト情報を保存
  writer.writeStartElement("Project");
  prj->saveData(writer);
  writer.writeEndElement();

  writer.writeEndDocument();

  writer.writeEndElement();
  fp.close();

  prj->setPath(path);

  if (addToRecentFiles) RecentFiles::instance()->addPath(path);

  // DirtyFlag解除
  IwApp::instance()->getCurrentProject()->setDirty(false);

  // タイトルバー更新
  IwApp::instance()->getMainWindow()->updateTitle();

  // ファイル名をタイトルバーに入れる
  IwApp::instance()->showMessageOnStatusBar(
      QObject::tr("Saved to %1 .").arg(path));
}

void IoCmd::saveProjectWithDateTime(QString path) {
  // 現在のプロジェクトが無ければreturn
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  // 名称未設定ならファイルダイアログでパスを指定
  if (path.isEmpty() || path == "Untitled") {
    path = QFileDialog::getSaveFileName(IwApp::instance()->getMainWindow(),
                                        "Save Project File", "",
                                        "IwaWarper Project (*.iwp)");
  }

  if (path.isEmpty()) return;

  QString fileBaseName = QFileInfo(path).baseName();
  // すでにDateTimeが格納されているか確認
  QRegExp rx("(.+)_\\d{6}-\\d{6}$");

  int pos = rx.indexIn(fileBaseName);
  QString fileNameCore;
  if (pos > -1) {
    fileNameCore = rx.cap(1);
  } else
    fileNameCore = fileBaseName;

  QString dt = QDateTime::currentDateTime().toString("yyMMdd-hhmmss");
  QString savePath =
      QFileInfo(path).dir().filePath(fileNameCore + "_" + dt + ".iwp");

  saveProject(savePath);

  RecentFiles::instance()->addPath(savePath);
}

//---------------------------------------------------
// プロジェクトをロード
//---------------------------------------------------

void IoCmd::loadProject(QString path, bool addToRecentFiles) {
  // 名称未設定ならファイルダイアログでパスを指定
  if (path.isEmpty()) {
    path = QFileDialog::getOpenFileName(IwApp::instance()->getMainWindow(),
                                        "Open Project File", "",
                                        "IwaWarper Project (*.iwp)");
  }

  if (path.isEmpty()) return;

  QFile fp(path);
  if (!fp.open(QIODevice::ReadOnly)) return;

  //---------------------------
  // まず、"IwaMorph"ヘッダがあることを確認する
  //---------------------------
  {
    bool find = false;
    QXmlStreamReader headerFinder(&fp);
    while (!headerFinder.atEnd()) {
      if (headerFinder.readNextStartElement()) {
        if (headerFinder.name().toString() == "IwaWarper") {
          std::cout << headerFinder.attributes()
                           .value("version")
                           .toString()
                           .toStdString()
                    << std::endl;
          // 後々、ここにバージョンチェックを入れる
          find = true;
          break;
        }
      }
    }
    if (!find) return;
  }

  IwApp::instance()->getMainWindow()->setEnabled(false);
  fp.reset();

  // 空のプロジェクトを作る
  IwProject* project = new IwProject();

  // プロジェクトをIwAppのロードされているプロジェクトリストに登録
  IwApp::instance()->insertLoadedProject(project);

  // カレントプロジェクトをこのプロジェクトにする
  IwApp::instance()->getCurrentProject()->setProject(project);

  QXmlStreamReader reader(&fp);

  while (reader.readNextStartElement()) {
    if (reader.name() == "IwaWarper") {
    } else if (reader.name() == "Info") {
      std::cout << "Info detected" << std::endl;
      reader.skipCurrentElement();
    } else if (reader.name() == "Project") {
      std::cout << "Project detected" << std::endl;
      project->loadData(reader);
    } else {
      std::cout << "Unexpected tag : " << reader.name().toString().toStdString()
                << " detected" << std::endl;
      reader.skipCurrentElement();
    }

    if (reader.hasError()) {
      std::cout << "XML Error Line:" << reader.lineNumber()
                << " Offset:" << reader.characterOffset()
                << " Token:" << reader.tokenString().toStdString()
                << "  Name:" << reader.name().toString().toStdString() << "  "
                << reader.errorString().toStdString() << std::endl;
    }
  }

  fp.close();

  project->setPath(path);

  if (addToRecentFiles) RecentFiles::instance()->addPath(path);

  // タイトルバー更新
  IwApp::instance()->getMainWindow()->updateTitle();

  IwApp::instance()->getMainWindow()->setEnabled(true);

  // プロジェクトを引数にして新たなViewerWindowを開く。すでに有る場合はそれをActiveにする
  IwApp::instance()->getCurrentProject()->notifyProjectSwitched();
  IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  IwApp::instance()->getCurrentProject()->notifyViewFrameChanged();
  if (project->getLayerCount() > 0)
    IwApp::instance()->getCurrentLayer()->setLayer(project->getLayer(0));
  // DirtyFlag解除
  IwApp::instance()->getCurrentProject()->setDirty(false);
}

void IoCmd::importProject() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();

  QString path = QFileDialog::getOpenFileName(
      IwApp::instance()->getMainWindow(),
      QObject::tr("Import Project/Shapes File"), project->getPath(),
      "IwaWarper Data (*.iwp *.iws);;IwaWarper Project (*.iwp);;IwaWarper "
      "Shapes (*.iws)");

  if (path.isEmpty()) return;

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (path.endsWith(".iws") && !layer) {
    QMessageBox::warning(IwApp::instance()->getMainWindow(),
                         QObject::tr("Error"), QObject::tr("No current layer"));
    return;
  }

  QFile fp(path);
  if (!fp.open(QIODevice::ReadOnly)) return;

  //---------------------------
  // まず、"IwaMorph"ヘッダがあることを確認する
  //---------------------------
  bool isShapeData = false;
  {
    bool find = false;
    QXmlStreamReader headerFinder(&fp);
    while (!headerFinder.atEnd()) {
      if (headerFinder.readNextStartElement()) {
        if (headerFinder.name().toString() == "IwaWarper" ||
            headerFinder.name().toString() == "IwaWarperShapes") {
          std::cout << headerFinder.attributes()
                           .value("version")
                           .toString()
                           .toStdString()
                    << std::endl;
          // 後々、ここにバージョンチェックを入れる
          find = true;
          if (headerFinder.name().toString() == "IwaWarperShapes")
            isShapeData = true;
          break;
        }
      }
    }
    if (!find) return;
  }

  IwApp::instance()->getMainWindow()->setEnabled(false);
  fp.reset();

  QXmlStreamReader reader(&fp);

  while (reader.readNextStartElement()) {
    if (reader.name() == "IwaWarper" || reader.name() == "IwaWarperShapes") {
      // std::cout<<"IwaMorph detected"<<std::endl;
    } else if (reader.name() == "Info") {
      std::cout << "Info detected" << std::endl;
      reader.skipCurrentElement();
    } else if (reader.name() == "Project") {
      std::cout << "Project detected" << std::endl;
      ProjectUtils::importProjectData(reader);
    } else if (reader.name() == "ShapePairs" && isShapeData) {
      std::cout << "Shapes detected" << std::endl;
      ProjectUtils::importShapesData(reader);
    } else {
      std::cout << "Unexpected tag : " << reader.name().toString().toStdString()
                << " detected" << std::endl;
      reader.skipCurrentElement();
    }

    if (reader.hasError()) {
      std::cout << "XML Error Line:" << reader.lineNumber()
                << " Offset:" << reader.characterOffset()
                << " Token:" << reader.tokenString().toStdString()
                << "  Name:" << reader.name().toString().toStdString() << "  "
                << reader.errorString().toStdString() << std::endl;
    }
  }

  fp.close();

  IwApp::instance()->getMainWindow()->setEnabled(true);

  // プロジェクトを引数にして新たなViewerWindowを開く。すでに有る場合はそれをActiveにする
  IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  IwApp::instance()->getCurrentProject()->setDirty(true);
}
//----------------------------------------------------------

bool IoCmd::saveProjectIfNeeded(const QString commandName) {
  if (!IwApp::instance()->getCurrentProject()->isDirty()) return true;

  QMessageBox::StandardButton ret = QMessageBox::question(
      IwApp::instance()->getMainWindow(), commandName,
      QObject::tr("%1 : The current project has been modified.\n"
                  "What would you like to do?")
          .arg(commandName),
      QMessageBox::StandardButtons(QMessageBox::Save | QMessageBox::Discard |
                                   QMessageBox::Cancel),
      QMessageBox::NoButton);

  if (ret == QMessageBox::Save) {
    IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
    saveProject(prj->getPath());
    return true;
  } else if (ret == QMessageBox::Discard)
    return true;
  else
    return false;
}

//----------------------------------------------------------

bool IoCmd::exportShapes(const QSet<ShapePair*> shapePairs) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return false;

  QString defaultPath(prj->getPath());
  if (defaultPath.endsWith(".iwp")) {
    defaultPath.chop(4);
    defaultPath += "_shape.iws";
  }

  QString path = QFileDialog::getSaveFileName(
      IwApp::instance()->getMainWindow(), "Export Shapes", defaultPath,
      "IwaWarper Shapes (*.iws)");

  if (path.isEmpty()) return false;

  QFile fp(path);
  if (!fp.open(QIODevice::WriteOnly)) return false;

  QXmlStreamWriter writer(&fp);

  writer.setAutoFormatting(true);

  writer.writeStartDocument();

  writer.writeStartElement("IwaWarperShapes");
  // バージョン
  writer.writeAttribute("version", QString("%1.%2.%3")
                                       .arg(majorVersion)
                                       .arg(minorVersion)
                                       .arg(patchVersion));

  // 情報を書く
  writer.writeStartElement("Info");
  // 作成日時
  writer.writeTextElement("date",
                          QDateTime::currentDateTime().toString(Qt::ISODate));
  // 作成者
  writer.writeTextElement("author", getUserName());
  writer.writeEndElement();

  // シェイプを保存
  writer.writeComment("ShapePairs");
  writer.writeStartElement("ShapePairs");

  QSet<ShapePair*>::const_iterator sIt = shapePairs.constBegin();
  while (sIt != shapePairs.constEnd()) {
    if (!*sIt) continue;
    writer.writeStartElement("ShapePair");
    (*sIt)->saveData(writer);
    writer.writeEndElement();
    ++sIt;
  }
  writer.writeEndElement();

  writer.writeEndDocument();

  writer.writeEndElement();
  fp.close();

  return true;
}

//----------------------------------------------------------
// TLVのロード時に開く
//----------------------------------------------------------
LoadFrameRangeDialog::LoadFrameRangeDialog(QWidget* parent, QString levelName,
                                           int from, int to)
    : QDialog(parent) {
  // オブジェクト作成
  m_from = new QLineEdit(QString::number(from), this);
  m_to   = new QLineEdit(QString::number(to), this);
  m_step = new QLineEdit(QString::number(1), this);

  // プロパチー
  QString text        = QString("Loading %1").arg(levelName);
  QIntValidator* vali = new QIntValidator(from, to);
  m_from->setValidator(vali);
  m_to->setValidator(vali);
  m_step->setValidator(new QIntValidator(1, 1000));

  QPushButton* loadBtn = new QPushButton("Load", this);

  // レイアウツ
  QGridLayout* mainLay = new QGridLayout();
  mainLay->setMargin(5);
  mainLay->setHorizontalSpacing(5);
  mainLay->setVerticalSpacing(8);
  {
    mainLay->addWidget(new QLabel(text, this), 0, 0, 1, 2);

    mainLay->addWidget(new QLabel("From : ", this), 1, 0);
    mainLay->addWidget(m_from, 1, 1);

    mainLay->addWidget(new QLabel("To : ", this), 2, 0);
    mainLay->addWidget(m_to, 2, 1);

    mainLay->addWidget(new QLabel("Step : ", this), 3, 0);
    mainLay->addWidget(m_step, 3, 1);

    mainLay->addWidget(loadBtn, 4, 0, 1, 2);
  }
  setLayout(mainLay);

  // シグスロ接続
  connect(loadBtn, SIGNAL(clicked()), this, SLOT(close()));
}

void LoadFrameRangeDialog::getValues(int& from, int& to, int& step) {
  from = m_from->text().toInt();
  to   = m_to->text().toInt();
  step = m_step->text().toInt();
}

//-----------------------------------------------------------------------------

class OpenRecentProjectFileCommandHandler final : public MenuItemHandler {
public:
  OpenRecentProjectFileCommandHandler()
      : MenuItemHandler(MI_OpenRecentProject) {}
  void execute() override {
    QAction* act = CommandManager::instance()->getAction(MI_OpenRecentProject);
    int index    = act->data().toInt();
    QString path = RecentFiles::instance()->getPath(index);
    IoCmd::loadProject(path, false);
    RecentFiles::instance()->movePath(index, 0);
  }
} openRecentProjectFileCommandHandler;

//-----------------------------------------------------------------------------

class ClearRecentProjectFileListCommandHandler final : public MenuItemHandler {
public:
  ClearRecentProjectFileListCommandHandler()
      : MenuItemHandler(MI_ClearRecentProject) {}
  void execute() override { RecentFiles::instance()->clearRecentFilesList(); }
} clearRecentProjectFileListCommandHandler;
