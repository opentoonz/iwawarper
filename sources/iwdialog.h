//---------------------------------------------------
// IwDialog
// 個人設定データにジオメトリデータを
// 読み書きするようにする
//---------------------------------------------------

#ifndef IWDIALOG_H
#define IWDIALOG_H

#include <QDialog>
#include <QRect>

#include "personalsettingsmanager.h"

class IwDialog : public QDialog {
  bool m_isResizable;
  SettingsId m_dialogName;

  QRect m_geom;

public:
  // コンストラクタで、位置/サイズをロード
  IwDialog(QWidget* parent, SettingsId dialogName, bool isResizable);
  // デストラクタで、位置/サイズを保存
  ~IwDialog();
  // ダイアログ名を返す
  SettingsId getDialogName() { return m_dialogName; }

protected:
  // 開くとき、位置/サイズをジオメトリに適用
  void showEvent(QShowEvent*);
  // 閉じるとき、ジオメトリをキープ
  void hideEvent(QHideEvent*);
};

#endif
