//---------------------------------------------------
// IwDialog
// 個人設定データにジオメトリデータを
// 読み書きするようにする
//---------------------------------------------------

#include "iwdialog.h"

#include <QVariant>
#include <iostream>

//---------------------------------------------------
// コンストラクタで、位置/サイズをロード
//---------------------------------------------------
IwDialog::IwDialog(QWidget* parent, SettingsId dialogName, bool isResizable)
    : QDialog(parent), m_dialogName(dialogName), m_isResizable(isResizable) {
  // 位置情報を取得
  bool ok;
  int leftEdge = PersonalSettingsManager::instance()
                     ->getValue(PS_G_DialogPositions, m_dialogName, PS_LeftEdge)
                     .toInt(&ok);
  // ダイアログ情報が入っていない場合はreturn
  if (!ok) {
    m_geom = QRect();
    return;
  }

  int topEdge = PersonalSettingsManager::instance()
                    ->getValue(PS_G_DialogPositions, m_dialogName, PS_TopEdge)
                    .toInt();

  // リサイズ可なら、サイズを取得
  if (m_isResizable) {
    int width = PersonalSettingsManager::instance()
                    ->getValue(PS_G_DialogPositions, m_dialogName, PS_Width)
                    .toInt();
    int height = PersonalSettingsManager::instance()
                     ->getValue(PS_G_DialogPositions, m_dialogName, PS_Height)
                     .toInt();

    m_geom = QRect(leftEdge, topEdge, width, height);
  }
  // リサイズ不可なら移動だけ
  else {
    m_geom = QRect(leftEdge, topEdge, 1, 1);
  }
}

//---------------------------------------------------
// デストラクタで、位置/サイズを保存
//---------------------------------------------------
IwDialog::~IwDialog() {
  // 左端
  PersonalSettingsManager::instance()->setValue(PS_G_DialogPositions,
                                                m_dialogName, PS_LeftEdge,
                                                QVariant(geometry().x()));
  // 上端
  PersonalSettingsManager::instance()->setValue(
      PS_G_DialogPositions, m_dialogName, PS_TopEdge, QVariant(geometry().y()));

  // リサイズ可なら、サイズも保存
  if (m_isResizable) {
    // 横幅
    PersonalSettingsManager::instance()->setValue(PS_G_DialogPositions,
                                                  m_dialogName, PS_Width,
                                                  QVariant(geometry().width()));
    // 高さ
    PersonalSettingsManager::instance()->setValue(
        PS_G_DialogPositions, m_dialogName, PS_Height,
        QVariant(geometry().height()));
  }
}

//---------------------------------------------------
// 開くとき、位置/サイズをジオメトリに適用
//---------------------------------------------------
void IwDialog::showEvent(QShowEvent*) {
  if (m_geom.isEmpty()) return;

  // リサイズ可なら、サイズを取得
  if (m_isResizable) {
    setGeometry(m_geom);
  }
  // リサイズ不可なら移動だけ
  else {
    move(m_geom.left(), m_geom.top());
  }
}

//---------------------------------------------------
// 閉じるとき、ジオメトリをキープ
//---------------------------------------------------
void IwDialog::hideEvent(QHideEvent*) { m_geom = geometry(); }