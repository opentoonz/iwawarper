//-----------------------------------------------------------------------------
// Preferences
// 各種設定のクラス
//-----------------------------------------------------------------------------
#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QString>

class Preferences {  // singleton
public:
  // ベジエの表示上の分割数
  enum BezierActivePrecision {
    SUPERHIGH = 100,
    HIGH      = 50,
    MEDIUM    = 20,
    LOW       = 10
  };

private:
  //----------------
  // GUI preferences
  //----------------
  // ベジエの表示上の分割数
  BezierActivePrecision m_bezierActivePrecision;

  // Reshape Tool の右クリックコマンド「Lock Positions」
  // （クリックしたシェイプのActiveなコントロールポイントを、
  // 　最寄の 他のシェイプのActiveなコントロールポイントに
  //   フィットするように移動させるコマンド）で用いる。
  // このコマンドが発動するのに、Viewerでの見た目上のピクセル距離が
  // この値よりも近い必要がある
  int m_lockThreshold;

  // ONのとき、選択されたシェイプだけがタイムラインに表示される
  // OFFのとき、全てのシェイプが表示される
  bool m_showSelectedShapesOnly;

  QString m_language;

  Preferences();

  void loadSettings();
  void saveSettings();

public:
  ~Preferences();

  static Preferences* instance();

  // ベジエの表示上の分割数
  BezierActivePrecision getBezierActivePrecision() {
    return m_bezierActivePrecision;
  }
  void setBezierActivePrecision(BezierActivePrecision prec) {
    m_bezierActivePrecision = prec;
  }
  // Reshape Tool の右クリックコマンド「Lock Positions」の閾値
  int getLockThreshold() { return m_lockThreshold; }
  void setLockThreshold(int thres) { m_lockThreshold = thres; }

  bool isShowSelectedShapesOnly() { return m_showSelectedShapesOnly; }
  void setShowSelectedShapesOnly(bool on) { m_showSelectedShapesOnly = on; }

  QString language() const { return m_language; }
  void setLanguage(QString lang) { m_language = lang; }
};

#endif