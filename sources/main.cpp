/*
 IwaWarper
 2012-04-20 Project started
*/

#include <QApplication>
#include <QTranslator>
#include <QFile>
#include <QDir>

#include "tiio_std.h"
#include "tnzimage.h"
#include "mainwindow.h"
#include "menubarcommand.h"
#include "menubarcommandids.h"
#include "logger.h"
#include "shapetagsettings.h"
#include "iwfolders.h"
#include "preferences.h"

std::string Logger::fileName = "";
FILE* Logger::file           = nullptr;

int main(int argc, char* argv[]) {
  Logger::Initialize("Log.txt");
  QApplication a(argc, argv);
  a.setApplicationName("IwaWarper");
  a.setApplicationVersion("1.0.1");

  QDir::setCurrent(qApp->applicationDirPath());

  QTranslator tra;
  QString currentLang = Preferences::instance()->language();
  if (!currentLang.isEmpty() && currentLang != "English") {
    QDir locDir(IwFolders::getLocalizationFolderPath());
    if (locDir.cd(currentLang) && locDir.exists("IwaWarper.qm")) {
      tra.load("IwaWarper.qm", locDir.absolutePath());
      a.installTranslator(&tra);
    }
  }

  QFont font("Segoe UI", -1);
  font.setPixelSize(13);
  font.setWeight(50);
  a.setFont(font);

  Tiio::defineStd();
  initImageIo(false);

  // スタイルシート
  QString qssPath("file:///" + IwFolders::getStuffFolderPath() +
                  QString("/qss/Dark/Dark.qss"));
  a.setStyleSheet(qssPath);

  ShapeTagSettings::initTagIcons();

  MainWindow w;

  CommandManager::instance()->execute(MI_NewProject);

  w.show();

  return a.exec();
}
