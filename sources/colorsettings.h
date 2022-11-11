//---------------------------------------------------
// ColorSettings
// インタフェースの色情報を格納する
//---------------------------------------------------
#ifndef COLORSETTINGS_H
#define COLORSETTINGS_H

#include <iostream>

//----------------------------------------------------
// 色のリスト
//----------------------------------------------------
#define Color_CorrPoint "CorrPoint"
#define Color_ActiveCorr "ActiveCorr"
#define Color_CtrlPoint "CtrlPoint"
#define Color_ActiveCtrl "ActiveCtrl"
#define Color_CorrNumber "CorrNumber"
#define Color_Background "Background"
#define Color_FreeHandToolTracedLine "FreeHandToolTracedLine"
#define Color_InbetweenCtrl "InbetweenCtrl"
#define Color_InbetweenCorr "InbetweenCorr"
//----------------------------------------------------

#include <QMap>
#include <QColor>

typedef const char* ColorId;

class ColorSettings {  // singleton
  QMap<QString, QColor> m_colors;

  ColorSettings();

public:
  ~ColorSettings();
  static ColorSettings* instance();

  // 指定されたColorIdに対して色を取得（0～1.0）、後でglColor3dvで使えるか？
  void getColor(double* colorVec, ColorId colorId);
  // 色をQColorで返す
  QColor getQColor(ColorId colorId);

  // 保存/ロード
  void saveData();
  void loadData();
};

#endif