#include "layeroptionpanel.h"
#include "myslider.h"
#include "iwapp.h"
#include "iwlayerhandle.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwlayer.h"
#include "iwundomanager.h"

#include <QVBoxLayout>
#include <QLabel>

int LayerOptionUndo::getValue() {
  switch (m_valueType) {
  case Brightness:
    return m_layer->brightness();
    break;
  case Contrast:
    return m_layer->contrast();
    break;
  };
  return 0;
}

void LayerOptionUndo::setValue(int value) {
  switch (m_valueType) {
  case Brightness:
    m_layer->setBrightness(value);
    break;
  case Contrast:
    m_layer->setContrast(value);
    break;
  };
}

LayerOptionUndo::LayerOptionUndo(IwProject* project, IwLayer* layer,
                                 bool isBrightness)
    : m_project(project)
    , m_layer(layer)
    , m_valueType((isBrightness) ? Brightness : Contrast)
    , m_firstFlag(true) {
  m_before = getValue();
}

void LayerOptionUndo::undo() {
  setValue(m_before);

  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void LayerOptionUndo::redo() {
  if (m_firstFlag) {
    m_after     = getValue();
    m_firstFlag = false;
    return;
  }
  setValue(m_after);

  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------

LayerOptionPanel::LayerOptionPanel(QWidget* parent)
    : QDockWidget(tr("Layer Options"), parent), m_undo(nullptr) {
  // The dock widget cannot be closed, moved, or floated.
  setFeatures(QDockWidget::NoDockWidgetFeatures);
  m_brightness = new MyIntSlider(-255, 255, this);
  m_contrast   = new MyIntSlider(-255, 255, this);

  QWidget* widget = new QWidget(this);

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setSpacing(3);
  mainLay->setMargin(10);
  {
    mainLay->addWidget(new QLabel(tr("Brightness:")), 0);
    mainLay->addWidget(m_brightness, 0);
    mainLay->addWidget(new QLabel(tr("Contrast:")), 0);
    mainLay->addWidget(m_contrast, 0);
    mainLay->addStretch(1);
  }
  widget->setLayout(mainLay);
  setWidget(widget);

  connect(m_brightness, SIGNAL(valueChanged(bool)), this,
          SLOT(onBrightnessChanged(bool)));
  connect(m_contrast, SIGNAL(valueChanged(bool)), this,
          SLOT(onContrastChanged(bool)));
}

void LayerOptionPanel::showEvent(QShowEvent* /*event*/) {
  connect(IwApp::instance()->getCurrentLayer(), SIGNAL(layerSwitched()), this,
          SLOT(onLayerSwitched()));
  onLayerSwitched();
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(projectChanged()),
          this, SLOT(onProjectChanged()));
  onProjectChanged();
}

void LayerOptionPanel::hideEvent(QHideEvent* /*event*/) {
  disconnect(IwApp::instance()->getCurrentLayer(), SIGNAL(layerSwitched()),
             this, SLOT(onLayerSwitched()));
  disconnect(IwApp::instance()->getCurrentProject(), SIGNAL(projectChanged()),
             this, SLOT(onProjectChanged()));
}

void LayerOptionPanel::onLayerSwitched() {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) {
    m_brightness->setValue(0);
    m_brightness->setEnabled(false);
    m_contrast->setValue(0);
    m_contrast->setEnabled(false);
    return;
  }
  m_brightness->setValue(layer->brightness());
  m_brightness->setEnabled(true);
  m_contrast->setValue(layer->contrast());
  m_contrast->setEnabled(true);
}

void LayerOptionPanel::onProjectChanged() {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  if (m_brightness->value() != layer->brightness())
    m_brightness->setValue(layer->brightness());

  if (m_contrast->value() != layer->contrast())
    m_contrast->setValue(layer->contrast());
}

void LayerOptionPanel::onBrightnessChanged(bool isDragging) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  if (!m_undo)
    m_undo = new LayerOptionUndo(
        IwApp::instance()->getCurrentProject()->getProject(), layer, true);

  layer->setBrightness(m_brightness->value());

  if (!isDragging) {
    if (layer->brightness() != m_undo->beforeValue())
      IwUndoManager::instance()->push(m_undo);
    else
      delete m_undo;
    m_undo = nullptr;
  }

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void LayerOptionPanel::onContrastChanged(bool isDragging) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  if (!m_undo)
    m_undo = new LayerOptionUndo(
        IwApp::instance()->getCurrentProject()->getProject(), layer, false);

  layer->setContrast(m_contrast->value());

  if (!isDragging) {
    if (layer->contrast() != m_undo->beforeValue())
      IwUndoManager::instance()->push(m_undo);
    else
      delete m_undo;
    m_undo = nullptr;
  }

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}