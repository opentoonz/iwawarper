#pragma once
#ifndef LAYEROPTIONPANEL_H
#define LAYEROPTIONPANEL_H

#include <QDockWidget>
#include <QUndoCommand>

class MyIntSlider;
class IwProject;
class IwLayer;

class LayerOptionUndo : public QUndoCommand {
  IwProject* m_project;
  IwLayer* m_layer;

  enum ValueType { Brightness = 0, Contrast } m_valueType;

  int m_before, m_after;

  // redo‚ð‚³‚ê‚È‚¢‚æ‚¤‚É‚·‚éƒtƒ‰ƒO
  bool m_firstFlag;

  int getValue();
  void setValue(int val);

public:
  LayerOptionUndo(IwProject* project, IwLayer* layer, bool isBrightness);

  void undo() override;
  void redo() override;
  int beforeValue() { return m_before; }
};

class LayerOptionPanel : public QDockWidget {
  Q_OBJECT

  MyIntSlider *m_brightness, *m_contrast;

  LayerOptionUndo* m_undo;

public:
  LayerOptionPanel(QWidget* parent);
  ~LayerOptionPanel() {
    if (m_undo) delete m_undo;
  };

protected:
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;

protected slots:
  void onLayerSwitched();
  void onProjectChanged();
  void onBrightnessChanged(bool);
  void onContrastChanged(bool);
};

#endif
