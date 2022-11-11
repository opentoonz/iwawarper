// based on ruler implementation of OpenToonz

#pragma once

#ifndef RULER_INCLUDED
#define RULER_INCLUDED

#include <QWidget>
#include "iwproject.h"

// forward declaration
class SceneViewer;

//=============================================================================
// Ruler
//-----------------------------------------------------------------------------

class Ruler final : public QWidget {
  Q_OBJECT
  SceneViewer* m_viewer;
  bool m_vertical;
  bool m_moving;
  bool m_hiding;
  int m_highlightId;

  typedef IwProject::Guides Guides;

public:
  Ruler(QWidget* parent, SceneViewer* viewer, bool vertical);
  Guides& getGuides() const;

  int getGuideCount() const;
  double getGuide(int index) const;

  void getIndices(double origin, double iunit, int size, int& i0, int& i1,
                  int& ic) const;

  double getPan() const;

  void drawVertical(QPainter&);
  void drawHorizontal(QPainter&);
  void paintEvent(QPaintEvent*) override;

  double posToValue(const QPoint& p) const;
  double valueToPos(const double& v) const;

  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;

  void leaveEvent(QEvent* event) override;
};

#endif
