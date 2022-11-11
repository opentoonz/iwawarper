#include "preferences.h"

#include "iwfolders.h"
#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

Preferences::Preferences()
    : m_bezierActivePrecision(MEDIUM)
    , m_lockThreshold(50)
    , m_showSelectedShapesOnly(false)
    , m_language("English") {
  loadSettings();
}

Preferences::~Preferences() { saveSettings(); }

Preferences* Preferences::instance() {
  static Preferences _instance;
  return &_instance;
}

void Preferences::loadSettings() {
  QString profPath = IwFolders::getMyProfileFolderPath();
  if (profPath.isEmpty()) return;

  QSettings settings(profPath + "/preferences.ini", QSettings::IniFormat);

  settings.setIniCodec("UTF-8");

  if (!settings.childGroups().contains("Preferences")) return;

  settings.beginGroup("Preferences");

  m_bezierActivePrecision =
      (BezierActivePrecision)(settings
                                  .value("BezierPrecision",
                                         (int)m_bezierActivePrecision)
                                  .toInt());
  m_lockThreshold = settings.value("LockThreshold", m_lockThreshold).toInt();
  m_showSelectedShapesOnly =
      settings.value("ShowSelectedShapesOnly", m_showSelectedShapesOnly)
          .toBool();
  m_language = settings.value("Language", m_language).toString();
  settings.endGroup();
}

void Preferences::saveSettings() {
  QString profPath = IwFolders::getMyProfileFolderPath();
  if (profPath.isEmpty()) return;

  QSettings settings(profPath + "/preferences.ini", QSettings::IniFormat);

  settings.setIniCodec("UTF-8");

  settings.beginGroup("Preferences");

  settings.setValue("BezierPrecision", (int)m_bezierActivePrecision);
  settings.setValue("LockThreshold", (int)m_lockThreshold);
  settings.setValue("ShowSelectedShapesOnly", m_showSelectedShapesOnly);
  settings.setValue("Language", m_language);

  settings.endGroup();
}