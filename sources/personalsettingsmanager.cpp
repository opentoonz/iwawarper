//------------------------------
// PersonalSettingsManager
// ユーザごとの設定を管理する
//------------------------------
#include "personalsettingsmanager.h"

#include "Iwfolders.h"
#include <QProcess>
#include <QString>
#include <QSettings>
#include <QVariant>

PersonalSettingsManager::PersonalSettingsManager() {
  // ユーザ名を取得する
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
    if (!user.isEmpty()) {
      m_userName = user;
      return;
    }
  }
  m_userName = "";
}

//-----------------------------------------------------------------------------

PersonalSettingsManager* PersonalSettingsManager::instance() {
  static PersonalSettingsManager _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

// 設定ファイルへのパスを返す
QString PersonalSettingsManager::getSettingsPath() {
  return IwFolders::getMyProfileFolderPath() + QString("/env.ini");
}

//-----------------------------------------------------------------------------

// 値を格納する
void PersonalSettingsManager::setValue(SettingsId mainGroupId,
                                       SettingsId subGroupId,
                                       SettingsId valueId,
                                       const QVariant& value) {
  QSettings settings(getSettingsPath(), QSettings::IniFormat);
  if (mainGroupId != "") settings.beginGroup(mainGroupId);
  if (subGroupId != "") settings.beginGroup(subGroupId);

  settings.setValue(valueId, value);
}

//-----------------------------------------------------------------------------

void PersonalSettingsManager::setValue(SettingsId groupId, SettingsId valueId,
                                       const QVariant& value) {
  setValue(groupId, "", valueId, value);
}

//-----------------------------------------------------------------------------

void PersonalSettingsManager::setValue(SettingsId valueId,
                                       const QVariant& value) {
  setValue("", "", valueId, value);
}

//-----------------------------------------------------------------------------

// 値を取得する
QVariant PersonalSettingsManager::getValue(SettingsId mainGroupId,
                                           SettingsId subGroupId,
                                           SettingsId valueId) {
  QSettings settings(getSettingsPath(), QSettings::IniFormat);
  if (mainGroupId != "") settings.beginGroup(mainGroupId);
  if (subGroupId != "") settings.beginGroup(subGroupId);

  return settings.value(valueId);
}

//-----------------------------------------------------------------------------

QVariant PersonalSettingsManager::getValue(SettingsId groupId,
                                           SettingsId valueId) {
  return getValue(groupId, "", valueId);
}

//-----------------------------------------------------------------------------

QVariant PersonalSettingsManager::getValue(SettingsId valueId) {
  return getValue("", "", valueId);
}
