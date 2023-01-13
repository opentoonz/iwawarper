//------------------------------
// PersonalSettingsManager
// ���[�U���Ƃ̐ݒ���Ǘ�����
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

  // �ݒ�t�@�C���ւ̃p�X��Ԃ�
  QString getSettingsPath();
  // �l���i�[����
  void setValue(SettingsId mainGroupId, SettingsId subGroupId,
                SettingsId valueId, const QVariant& value);
  void setValue(SettingsId groupId, SettingsId valueId, const QVariant& value);
  void setValue(SettingsId valueId, const QVariant& value);
  // �l���擾����
  QVariant getValue(SettingsId mainGroupId, SettingsId subGroupId,
                    SettingsId valueId);
  QVariant getValue(SettingsId groupId, SettingsId valueId);
  QVariant getValue(SettingsId valueId);
};

#endif
