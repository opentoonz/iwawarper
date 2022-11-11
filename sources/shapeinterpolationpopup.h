#pragma once
#ifndef SHAPEINTERPOLATIONPOPUP_H
#define SHAPEINTERPOLATIONPOPUP_H

#include <QWidget>
#include <QUndoCommand>

class QSlider;
class QLabel;
class ShapePair;
class IwProject;
class ChangeShapeInterpolationUndo;

class ShapeInterpolationPopup : public QWidget {
  Q_OBJECT

  ShapePair* m_shape;
  QSlider* m_slider;
  QLabel* m_label;
  ChangeShapeInterpolationUndo* m_undo;

public:
  ShapeInterpolationPopup(QWidget* parent);

  void setShape(ShapePair* shape);

public slots:
  void onSliderPressed();
  void onSliderMoved(int v);
  void onSliderReleased();
};

// Undo

class ChangeShapeInterpolationUndo : public QUndoCommand {
  IwProject* m_project;
  ShapePair* m_shape;
  double m_before, m_after;

  // redo‚ð‚³‚ê‚È‚¢‚æ‚¤‚É‚·‚éƒtƒ‰ƒO
  int m_firstFlag;

public:
  ChangeShapeInterpolationUndo(IwProject*, ShapePair*);
  bool storeAfterData();
  void undo();
  void redo();
};

#endif