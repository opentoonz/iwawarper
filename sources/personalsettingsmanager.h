//------------------------------
// PersonalSettingsManager
// ユーザごとの設定を管理する
//------------------------------
#ifndef PERSONALSETTINGSMANAGER
#define PERSONALSETTINGSMANAGER

#include "personalsettingsids.h"

#include <QString>

class QVariant;

typedef const char* SettingsId;

class PersonalSettingsManager  // singleton
{
  QString m_userName;

  PersonalSettingsManager();

public:
  static PersonalSettingsManager* instance();

  // 設定ファイルへのパスを返す
  QString getSettingsPath();
  // 値を格納する
  void setValue(SettingsId mainGroupId, SettingsId subGroupId,
                SettingsId valueId, QVariant& value);
  void setValue(SettingsId groupId, SettingsId valueId, QVariant& value);
  void setValue(SettingsId valueId, QVariant& value);
  // 値を取得する
  QVariant getValue(SettingsId mainGroupId, SettingsId subGroupId,
                    SettingsId valueId);
  QVariant getValue(SettingsId groupId, SettingsId valueId);
  QVariant getValue(SettingsId valueId);
};

#endif