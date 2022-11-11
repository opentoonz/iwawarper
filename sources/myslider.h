//---------------------------------
// MySider
// スライダとLineEditの組み合わせ
//---------------------------------
#ifndef MYSLIDER_H
#define MYSLIDER_H

#include <QWidget>

class QSlider;
class QLineEdit;

class MyIntSlider : public QWidget {
  Q_OBJECT

  QSlider* m_slider;
  QLineEdit* m_edit;

  bool m_isDragging;

public:
  MyIntSlider(int min, int max, QWidget* parent = 0);

  int value();

  void setValue(int val);

protected slots:
  void onSliderValueChanged(int);
  void onSliderPressed();
  void onSliderReleased();
  void onEditEditingFinished();

signals:
  void valueChanged(bool isDragging);
};

#endif