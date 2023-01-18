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

//[�t�@�C����]#0000 �Ƃ����`���ɂ��ĕԂ�
QString getFileNameWithFrameNumber(QString fileName, int frameNumber) {
  return QString("%1#%2").arg(fileName).arg(
      QString::number(frameNumber).rightJustified(4, '0'));
}

//[�t�@�C����]#0000�Ƃ����`���̕��������́B�t���[���ԍ�����菜���A�Ԃ��B
int separateFileNameAndFrameNumber(QString& fileName) {
  int frameNumber = fileName.right(4).toInt();
  fileName.chop(5);
  return frameNumber;
}

// ���[�U����Ԃ�
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
  // �ǂݍ��߂�t�@�C���̃��X�g�����
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

  // �l�ݒ�ɕۑ�����Ă��鏉���t�H���_���擾
  QString initialFolder = PersonalSettingsManager::instance()
                              ->getValue(PS_G_ProjEditPrefs, PS_ImageDir)
                              .toString();

  // �t�@�C���_�C�A���O���J��
  return QFileDialog::getOpenFileNames(IwApp::instance()->getMainWindow(),
                                       QString("Insert Images"), initialFolder,
                                       filterString);
}

// �t�@�C���p�X����t���[���ԍ��𒊏o����B���Ȃ����-1��Ԃ�
int getFrameNumberFromName(QString fileName) {
  // �Ō�̃s���I�h�ȍ~�����
  int extIndex = fileName.lastIndexOf(".");
  fileName.truncate(extIndex);
  // ����4���𒊏o
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
// �V�K�v���W�F�N�g
//-------------------------------
void IoCmd::newProject() {
  // ��̃v���W�F�N�g�����
  IwProject* project = new IwProject();

  // �v���W�F�N�g��IwApp�̃��[�h����Ă���v���W�F�N�g���X�g�ɓo�^
  IwApp::instance()->insertLoadedProject(project);

  // �J�����g�v���W�F�N�g�����̃v���W�F�N�g�ɂ���
  IwApp::instance()->getCurrentProject()->setProject(project);
}

//-------------------------------
// �V�[�N�G���X�̑I��͈͂ɉ摜��}��
//-------------------------------
// new done
void IoCmd::insertImagesToSequence(int layerIndex) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  // ���C�����Ȃ��ꍇ�́A���݂̃��C�����擾
  IwLayer* layer = nullptr;
  if (layerIndex >= 0 && layerIndex < prj->getLayerCount())
    layer = prj->getLayer(layerIndex);
  // �V�K���C�������ꍇ
  if (!layer) layer = prj->appendLayer();

  QStringList pathList = getOpenFilePathList();

  // �P���t�@�C�����I������Ă��Ȃ����return
  if (pathList.empty()) {
    // �󃌃C����������ꍇ���v���W�F�N�g�ύX�V�O�i�����G�~�b�g����B
    IwApp::instance()->getCurrentLayer()->notifyLayerChanged(layer);
    return;
  }

  // �t�@�C�������\�[�g
  pathList.sort();

  // �}������t���[�� �Ƃ肠����������ɕt����
  // ���{���͑I���V�[�N�G���X�͈͂̐擪�ɂ��킹��
  int f = layer->getFrameLength();
  // int f = 0;

  // ���݂̃V�[�N�G���X�Ƀt�H���_�p�X��ǉ����A�C���f�b�N�X�𓾂�
  QDir dir(pathList.at(0));
  dir.cdUp();
  QString folderPath = dir.path();
  // �����ŁA�J�����g�V�[�N�G���X����t�H���_�̃C���f�b�N�X�𓾂�
  int folderIndex = layer->getFolderPathIndex(folderPath);

  // �I���t�@�C���P���Ƃ�
  for (int p = 0; p < pathList.size(); p++) {
    QString path = pathList.at(p);

    TFilePath fp(path.toStdWString());
    std::string ext = fp.getUndottedType();  // �g���q

    // �t�@�C����
    QString fileName =
        QString::fromStdWString(fp.withoutParentDir().getWideString());

    // ���[�r�[�t�@�C���̏�
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

      // ���݂̑I���V�[�N�G���X�͈͂̑���ɘA�Ԃ�}��
      std::vector<TFrameId> fids;
      for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
        // �t���[���ԍ�
        int frameNum = it->first.getNumber();
        if (fromFrame <= frameNum && frameNum <= toFrame) {
          QString fileNameWithFrame =
              getFileNameWithFrameNumber(fileName, frameNum);
          for (int s = 0; s < step; s++) {
            layer->insertPath(
                f, QPair<int, QString>(folderIndex, fileNameWithFrame));
            // �}���|�C���g���P�E�ɐi�߂�
            f++;
          }
        }
      }

    }
    // �P�i�摜�̏ꍇ
    else {
      // �I���V�[�N�G���X�͈͂ɉ摜��}��
      layer->insertPath(f, QPair<int, QString>(folderIndex, fileName));
      // �}���|�C���g���P�E�ɐi�߂�
      f++;
    }
    // �ЂƂł��ǂ߂��ꍇ�̓t���O���Ă�
  }

  // �J�����g���C�����ړ�
  IwApp::instance()->getCurrentLayer()->setLayer(layer);

  // �ЂƂł��ǂ߂��ꍇ�́A�v���W�F�N�g�ύX�V�O�i�����G�~�b�g����B
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(layer);

  // ���[�h�����t�H���_���l�ݒ�t�@�C���Ɋi�[����
  PersonalSettingsManager::instance()->setValue(PS_G_ProjEditPrefs, PS_ImageDir,
                                                QVariant(folderPath));

  //---�����ŁA���݂�Project�̃��[�N�G���A�����ݒ�̂Ƃ��i�ŏ��ɉ摜��ǂݍ��񂾂Ƃ��j
  // �P�t���[���ڂ̉摜�T�C�Y�����[�N�G���A�̃T�C�Y�Ƃ��ėp����
  // ���݂̃v���W�F�N�g���擾
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
      // �`��������Ȃ����return
      else
        return;

      project->setWorkAreaSize(QSize(ras->getWrap(), ras->getLy()));
      // �V�O�i�����G�~�b�g���āASequenceEditor�̃��[�N�G���A�T�C�Y�̕\�����������񂷂�
      IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
    }
  }
}

//---------------------------------------------------
// �t���[���͈͂��w�肵�č����ւ�
//---------------------------------------------------
void IoCmd::doReplaceImages(IwLayer* layer, int from, int to) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  QStringList pathList = getOpenFilePathList();
  // �P���t�@�C�����I������Ă��Ȃ����return
  if (pathList.empty()) return;
  // �t�@�C�������\�[�g
  pathList.sort();

  // ���݂̃V�[�N�G���X�Ƀt�H���_�p�X��ǉ����A�C���f�b�N�X�𓾂�
  QDir dir(pathList.at(0));
  dir.cdUp();
  QString folderPath = dir.path();
  // �����ŁA�J�����g�V�[�N�G���X����t�H���_�̃C���f�b�N�X�𓾂�
  int folderIndex = layer->getFolderPathIndex(folderPath);

  // TLV�Ȃ�ق����ăp�X�̃��X�g���΍��
  QList<QString> timelinePathList;

  // �I���t�@�C���P���Ƃ�
  for (int p = 0; p < pathList.size(); p++) {
    QString path = pathList.at(p);
    TFilePath fp(path.toStdWString());
    std::string ext = fp.getUndottedType();  // �g���q
    // �t�@�C����
    QString fileName =
        QString::fromStdWString(fp.withoutParentDir().getWideString());
    // ���[�r�[�t�@�C���̏ꍇ
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

      // ���݂̑I���V�[�N�G���X�͈͂̑���ɘA�Ԃ�}��
      std::vector<TFrameId> fids;
      for (TLevel::Iterator it = level->begin(); it != level->end(); ++it) {
        // �t���[���ԍ�
        int frameNum = it->first.getNumber();
        if (fromFrame <= frameNum && frameNum <= toFrame) {
          QString fileNameWithFrame =
              getFileNameWithFrameNumber(fileName, frameNum);
          for (int s = 0; s < step; s++)
            // �X�e�b�v���Ԃ�o�^����
            timelinePathList.append(fileNameWithFrame);
        }
      }
    }
    // �P�i�摜�̏ꍇ
    else
      // �o�^����
      timelinePathList.append(fileName);
  }

  // ���̃��X�g���g���č����ւ���
  // �I��͈͂̊e�t���[���ɂ���
  int i = 0;
  int f = from;

  // �p�X���P�����̏ꍇ�́A�I��͈͂����ׂĂ���Œu��������
  if (timelinePathList.size() == 1) {
    for (; f <= to; f++, i++) {
      layer->replacePath(
          f, QPair<int, QString>(folderIndex, timelinePathList.at(0)));
    }
  } else {
    for (; f <= to; f++, i++) {
      // ���X�g���܂�����Ă���΃t�@�C���������ւ�
      if (i < timelinePathList.size())
        layer->replacePath(
            f, QPair<int, QString>(folderIndex, timelinePathList.at(i)));
      // ���X�g���I�[�o�[���Ă�����A�󂫃Z���ɂ���
      else
        layer->replacePath(f, QPair<int, QString>(0, QString()));
    }
    // �I��͈͂�胍�[�h�摜�������ꍇ�A���ɒǉ�����
    if (i < timelinePathList.size())
      for (; i < timelinePathList.size(); i++, f++) {
        layer->insertPath(
            f, QPair<int, QString>(folderIndex, timelinePathList.at(i)));
      }
  }

  // �ЂƂł��ǂ߂��ꍇ�́A�v���W�F�N�g�ύX�V�O�i�����G�~�b�g����B
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(layer);

  // ���[�h�����t�H���_���l�ݒ�t�@�C���Ɋi�[����
  PersonalSettingsManager::instance()->setValue(PS_G_ProjEditPrefs, PS_ImageDir,
                                                QVariant(folderPath));

  IwTimeLineSelection::instance()->selectFrames(from, f - 1, true);
  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//---------------------------------------------------
// �t�@�C���p�X����摜���ꖇ���[�h����
//---------------------------------------------------
TImageP IoCmd::loadImage(QString path) {
  PRINT_LOG("loadImage")
  // TLV�Ȃǃ��[�r�[�t�@�C���̂Ƃ�
  if (path.lastIndexOf('#') == (path.length() - 5)) {
    //[�t�@�C����]#0000�Ƃ����`���̕��������́B�t���[���ԍ�����菜���A�Ԃ��B
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
  // �P�i�摜�̂Ƃ�
  else {
    TFilePath fp(path.toStdWString());
    // multiFileLevelEnabled��OFF�ɂ��邱��
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
// �v���W�F�N�g��ۑ�
// �t�@�C���p�X���w�肵�Ă��Ȃ��ꍇ�̓_�C�A���O���J��
//---------------------------------------------------
void IoCmd::saveProject(QString path) {
  // ���݂̃v���W�F�N�g���������return
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  bool addToRecentFiles = false;
  // ���̖��ݒ�Ȃ�t�@�C���_�C�A���O�Ńp�X���w��
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
  // �o�[�W����
  writer.writeAttribute("version", QString("%1.%2.%3")
                                       .arg(majorVersion)
                                       .arg(minorVersion)
                                       .arg(patchVersion));

  // ��������
  writer.writeStartElement("Info");
  // �쐬����
  writer.writeTextElement("date",
                          QDateTime::currentDateTime().toString(Qt::ISODate));
  // �쐬��
  writer.writeTextElement("author", getUserName());
  writer.writeEndElement();

  // �v���W�F�N�g����ۑ�
  writer.writeStartElement("Project");
  prj->saveData(writer);
  writer.writeEndElement();

  writer.writeEndDocument();

  writer.writeEndElement();
  fp.close();

  prj->setPath(path);

  if (addToRecentFiles) RecentFiles::instance()->addPath(path);

  // DirtyFlag����
  IwApp::instance()->getCurrentProject()->setDirty(false);

  // �^�C�g���o�[�X�V
  IwApp::instance()->getMainWindow()->updateTitle();

  // �t�@�C�������^�C�g���o�[�ɓ����
  IwApp::instance()->showMessageOnStatusBar(
      QObject::tr("Saved to %1 .").arg(path));
}

void IoCmd::saveProjectWithDateTime(QString path) {
  // ���݂̃v���W�F�N�g���������return
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  // ���̖��ݒ�Ȃ�t�@�C���_�C�A���O�Ńp�X���w��
  if (path.isEmpty() || path == "Untitled") {
    path = QFileDialog::getSaveFileName(IwApp::instance()->getMainWindow(),
                                        "Save Project File", "",
                                        "IwaWarper Project (*.iwp)");
  }

  if (path.isEmpty()) return;

  QString fileBaseName = QFileInfo(path).baseName();
  // ���ł�DateTime���i�[����Ă��邩�m�F
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
// �v���W�F�N�g�����[�h
//---------------------------------------------------

void IoCmd::loadProject(QString path, bool addToRecentFiles) {
  // ���̖��ݒ�Ȃ�t�@�C���_�C�A���O�Ńp�X���w��
  if (path.isEmpty()) {
    path = QFileDialog::getOpenFileName(IwApp::instance()->getMainWindow(),
                                        "Open Project File", "",
                                        "IwaWarper Project (*.iwp)");
  }

  if (path.isEmpty()) return;

  QFile fp(path);
  if (!fp.open(QIODevice::ReadOnly)) return;

  //---------------------------
  // �܂��A"IwaMorph"�w�b�_�����邱�Ƃ��m�F����
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
          // ��X�A�����Ƀo�[�W�����`�F�b�N������
          find = true;
          break;
        }
      }
    }
    if (!find) return;
  }

  IwApp::instance()->getMainWindow()->setEnabled(false);
  fp.reset();

  // ��̃v���W�F�N�g�����
  IwProject* project = new IwProject();

  // �v���W�F�N�g��IwApp�̃��[�h����Ă���v���W�F�N�g���X�g�ɓo�^
  IwApp::instance()->insertLoadedProject(project);

  // �J�����g�v���W�F�N�g�����̃v���W�F�N�g�ɂ���
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

  // �^�C�g���o�[�X�V
  IwApp::instance()->getMainWindow()->updateTitle();

  IwApp::instance()->getMainWindow()->setEnabled(true);

  // �v���W�F�N�g�������ɂ��ĐV����ViewerWindow���J���B���łɗL��ꍇ�͂����Active�ɂ���
  IwApp::instance()->getCurrentProject()->notifyProjectSwitched();
  IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  IwApp::instance()->getCurrentProject()->notifyViewFrameChanged();
  if (project->getLayerCount() > 0)
    IwApp::instance()->getCurrentLayer()->setLayer(project->getLayer(0));
  // DirtyFlag����
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
  // �܂��A"IwaMorph"�w�b�_�����邱�Ƃ��m�F����
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
          // ��X�A�����Ƀo�[�W�����`�F�b�N������
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

  // �v���W�F�N�g�������ɂ��ĐV����ViewerWindow���J���B���łɗL��ꍇ�͂����Active�ɂ���
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
  // �o�[�W����
  writer.writeAttribute("version", QString("%1.%2.%3")
                                       .arg(majorVersion)
                                       .arg(minorVersion)
                                       .arg(patchVersion));

  // ��������
  writer.writeStartElement("Info");
  // �쐬����
  writer.writeTextElement("date",
                          QDateTime::currentDateTime().toString(Qt::ISODate));
  // �쐬��
  writer.writeTextElement("author", getUserName());
  writer.writeEndElement();

  // �V�F�C�v��ۑ�
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
// TLV�̃��[�h���ɊJ��
//----------------------------------------------------------
LoadFrameRangeDialog::LoadFrameRangeDialog(QWidget* parent, QString levelName,
                                           int from, int to)
    : QDialog(parent) {
  // �I�u�W�F�N�g�쐬
  m_from = new QLineEdit(QString::number(from), this);
  m_to   = new QLineEdit(QString::number(to), this);
  m_step = new QLineEdit(QString::number(1), this);

  // �v���p�`�[
  QString text        = QString("Loading %1").arg(levelName);
  QIntValidator* vali = new QIntValidator(from, to);
  m_from->setValidator(vali);
  m_to->setValidator(vali);
  m_step->setValidator(new QIntValidator(1, 1000));

  QPushButton* loadBtn = new QPushButton("Load", this);

  // ���C�A�E�c
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

  // �V�O�X���ڑ�
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
