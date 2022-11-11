#include "iwfolders.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <iostream>

namespace {
QString getUserName() {
  QString name = qgetenv("USER");
  if (name.isEmpty()) name = qgetenv("USERNAME");
  return name;
}

QString getStuffPath() {
  static QString IwStuffPath = QString();
  if (IwStuffPath.isEmpty()) {
    QString confPath = qApp->applicationDirPath() + "\\conf.ini";
    QSettings settings(confPath, QSettings::IniFormat);
    IwStuffPath = settings.value("IWSTUFFROOT", "").toString();
    if (QDir::isRelativePath(IwStuffPath))
      IwStuffPath = QDir(IwStuffPath).absolutePath();
  }
  return IwStuffPath;
}

QString getProfilesPath() {
  QString stuffPath = getStuffPath();
  if (stuffPath.isEmpty()) return QString();
  QDir stuffDir(stuffPath);
  if (!stuffDir.mkpath("profiles")) return QString();
  return stuffPath + "\\profiles";
}

}  // namespace

QString IwFolders::getStuffFolderPath() { return getStuffPath(); }

QString IwFolders::getMyProfileFolderPath() {
  QString profPath = getProfilesPath();
  QString userName = getUserName();
  if (profPath.isEmpty()) return QString();
  QDir profDir(profPath);
  if (!profDir.mkpath(userName)) return QString();
  return profPath + "\\" + userName;
}

QString IwFolders::getLicenseFolderPath() {
  return getStuffPath() + "\\doc\\LICENSE";
}

QString IwFolders::getLocalizationFolderPath() {
  return getStuffPath() + "\\loc";
}
