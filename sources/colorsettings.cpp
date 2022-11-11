//---------------------------------------------------
// ColorSettings
// manage interface colors インタフェースの色情報を格納する
//---------------------------------------------------
#include "colorsettings.h"

#include "iwfolders.h"
#include <QSettings>

ColorSettings::ColorSettings() {
  /*
  m_colors.insert(QString(Color_ASource), QColor(191, 0, 0));
  m_colors.insert(QString(Color_ASelSource), QColor(255, 0, 0));
  m_colors.insert(QString(Color_ADest), QColor(191, 102, 102));
  m_colors.insert(QString(Color_ASelDest), QColor(255, 102, 102));
  m_colors.insert(QString(Color_AEdge), QColor(191, 191, 0));
  m_colors.insert(QString(Color_BSource), QColor(0, 0, 191));
  m_colors.insert(QString(Color_BSelSource), QColor(0, 0, 255));
  m_colors.insert(QString(Color_BDest), QColor(102, 102, 191));
  m_colors.insert(QString(Color_BSelDest), QColor(102, 102, 255));
  m_colors.insert(QString(Color_BEdge), QColor(0, 0, 191));
  m_colors.insert(QString(Color_Tangent), QColor(0, 255, 0));
  m_colors.insert(QString(Color_BoundBox), QColor(255, 255, 255));
  m_colors.insert(QString(Color_Wireframe), QColor(0, 0, 0));
  m_colors.insert(QString(Color_ActiveGroup), QColor(0, 0, 255));
  m_colors.insert(QString(Color_ABSource), QColor(191, 0, 191));
  m_colors.insert(QString(Color_ABSelSource), QColor(255, 0, 255));
  m_colors.insert(QString(Color_ABEdge), QColor(191, 0, 191));
  m_colors.insert(QString(Color_Grid), QColor(0, 0, 0));
  m_colors.insert(QString(Color_Path), QColor(0, 191, 0));
  m_colors.insert(QString(Color_SelPath), QColor(0, 255, 0));
  m_colors.insert(QString(Color_NoImage), QColor(0, 0, 32));
  m_colors.insert(QString(Color_NoAlpha), QColor(32, 0, 0));
  m_colors.insert(QString(Color_AsstedTraceFixedPoly), QColor(0, 255, 32));
  m_colors.insert(QString(Color_AsstedTraceFreePoly), QColor(191, 191, 0));
  m_colors.insert(QString(Color_AsstedTraceControlPoint), QColor(0, 255, 32));
  m_colors.insert(QString(Color_AsstedTraceSelControlPoint),
                  QColor(255, 255, 255));
  m_colors.insert(QString(Color_SelectBox), QColor(255, 255, 255));
  m_colors.insert(QString(Color_ABDest), QColor(191, 102, 191));
  m_colors.insert(QString(Color_ABSelDest), QColor(255, 102, 255));
  */
  m_colors.insert(QString(Color_CorrPoint), QColor(255, 255, 0));
  m_colors.insert(QString(Color_ActiveCorr), QColor(255, 0, 255));
  m_colors.insert(QString(Color_CtrlPoint), QColor(255, 255, 255));
  m_colors.insert(QString(Color_ActiveCtrl), QColor(0, 255, 0));
  m_colors.insert(QString(Color_CorrNumber), QColor(255, 255, 255));
  m_colors.insert(QString(Color_Background), QColor(102, 102, 102));
  m_colors.insert(QString(Color_FreeHandToolTracedLine), QColor(0, 255, 0));
  m_colors.insert(QString(Color_InbetweenCtrl), QColor(255, 0, 0));
  m_colors.insert(QString(Color_InbetweenCorr), QColor(255, 0, 0));

  loadData();
}

//---------------------------------------------------
ColorSettings::~ColorSettings() { saveData(); }

//---------------------------------------------------
ColorSettings* ColorSettings::instance() {
  static ColorSettings _instance;
  return &_instance;
}

//---------------------------------------------------
// returns color vector ( 0.0 - 1.0 )
// 指定されたColorIdに対して色を取得（0～1.0）
void ColorSettings::getColor(double* colorVec, ColorId colorId) {
  QColor col = m_colors.value(QString(colorId), QColor(Qt::red));
  col.getRgbF(&colorVec[0], &colorVec[1], &colorVec[2]);
}

//---------------------------------------------------
// returns QColor
// 色をQColorで返す
QColor ColorSettings::getQColor(ColorId colorId) {
  return m_colors.value(QString(colorId), QColor(Qt::red));
}

//---------------------------------------------------
void ColorSettings::saveData() {
  QString profPath = IwFolders::getMyProfileFolderPath();
  if (profPath.isEmpty()) return;

  QSettings settings(profPath + "/preferences.ini", QSettings::IniFormat);

  settings.setIniCodec("UTF-8");

  settings.beginGroup("Color Settings");
  QMapIterator<QString, QColor> i(m_colors);
  while (i.hasNext()) {
    i.next();
    QString str = QString("%1,%2,%3")
                      .arg(i.value().red())
                      .arg(i.value().green())
                      .arg(i.value().blue());
    settings.setValue(i.key(), str);
  }

  settings.endGroup();
}

void ColorSettings::loadData() {
  QString profPath = IwFolders::getMyProfileFolderPath();
  if (profPath.isEmpty()) return;

  QSettings settings(profPath + "/preferences.ini", QSettings::IniFormat);

  settings.setIniCodec("UTF-8");

  settings.beginGroup("Color Settings");
  for (auto key : settings.childKeys()) {
    QStringList list = settings.value(key).toString().split(",");
    if (list.size() < 3) continue;
    m_colors[key] = QColor(list[0].toInt(), list[1].toInt(), list[2].toInt());
  }
  settings.endGroup();
}