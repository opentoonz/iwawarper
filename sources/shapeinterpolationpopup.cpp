#include "shapeinterpolationpopup.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwundomanager.h"
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <assert.h>

#include "shapepair.h"
#include "iwproject.h"
#include "iwtrianglecache.h"

ShapeInterpolationPopup::ShapeInterpolationPopup(QWidget* parent)
    : QWidget(parent, Qt::Popup), m_shape(nullptr), m_undo(nullptr) {
  setFixedWidth(150);

  m_slider = new QSlider(Qt::Horizontal, this);
  m_slider->setMinimum(0);
  m_slider->setMaximum(10);
  m_slider->setFixedWidth(100);

  m_label = new QLabel(this);

  QHBoxLayout* layout = new QHBoxLayout();
  layout->setMargin(10);
  layout->setSpacing(5);
  {
    layout->addWidget(m_slider, 0);
    layout->addWidget(m_label, 1);
  }
  setLayout(layout);

  connect(m_slider, SIGNAL(sliderPressed()), this, SLOT(onSliderPressed()));
  connect(m_slider, SIGNAL(sliderMoved(int)), this, SLOT(onSliderMoved(int)));
  connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));
}

void ShapeInterpolationPopup::setShape(ShapePair* shape) {
  m_shape    = shape;
  double val = shape->getAnimationSmoothness();
  m_slider->setValue((int)(std::round(val * 10.0)));
  m_label->setText(QString::number(val));
}

void ShapeInterpolationPopup::onSliderPressed() {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  assert(m_undo == nullptr);
  m_undo = new ChangeShapeInterpolationUndo(prj, m_shape);
}

void ShapeInterpolationPopup::onSliderMoved(int v) {
  if (!m_shape) return;
  double val = (double)v * 0.1;
  m_shape->setAnimationSmoothness(val);
  m_label->setText(QString::number(val));
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void ShapeInterpolationPopup::onSliderReleased() {
  if (m_undo->storeAfterData())
    delete m_undo;
  else
    IwUndoManager::instance()->push(m_undo);
  m_undo = nullptr;
}

//================================================================

ChangeShapeInterpolationUndo::ChangeShapeInterpolationUndo(IwProject* prj,
                                                           ShapePair* shape)
    : m_project(prj), m_shape(shape), m_firstFlag(true) {
  m_before = shape->getAnimationSmoothness();
}

bool ChangeShapeInterpolationUndo::storeAfterData() {
  m_after = m_shape->getAnimationSmoothness();
  return m_before == m_after;
}

void ChangeShapeInterpolationUndo::undo() {
  m_shape->setAnimationSmoothness(m_before);
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // invalidate
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape));
  }
}

void ChangeShapeInterpolationUndo::redo() {
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }
  m_shape->setAnimationSmoothness(m_after);
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // invalidate
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape));
  }
}
