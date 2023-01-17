// based on ruler implementation of OpenToonz

#include "ruler.h"
#include "sceneviewer.h"
#include "iwapp.h"
#include "iwprojecthandle.h"

#include <QPainter>
#include <QMouseEvent>

#include <cassert>

namespace {

inline int stepceil(int x, int step) {
  return step * (x >= 0 ? ((x + step - 1) / step) : -((-x) / step));
}

}  // namespace

//=============================================================================
// Ruler
//-----------------------------------------------------------------------------

Ruler::Ruler(QWidget* parent, SceneViewer* viewer, bool vertical)
    : QWidget(parent)
    , m_viewer(viewer)
    , m_vertical(vertical)
    , m_moving(false)
    , m_hiding(false)
    , m_highlightId(-1) {
  if (vertical) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFixedWidth(12);
    setToolTip(tr("Click to create an horizontal guide"));
  } else {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(12);
    setToolTip(tr("Click to create a vertical guide"));
  }
  setMouseTracking(true);
}

//-----------------------------------------------------------------------------

Ruler::Guides& Ruler::getGuides() const {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  return m_vertical ? project->getVGuides() : project->getHGuides();
}

//-----------------------------------------------------------------------------

int Ruler::getGuideCount() const { return (int)getGuides().size(); }

//-----------------------------------------------------------------------------

double Ruler::getGuide(int index) const {
  Guides& guides = getGuides();
  assert(0 <= index && index < (int)guides.size());
  return guides[index];
}

//-----------------------------------------------------------------------------

void Ruler::getIndices(double origin, double iunit, int size, int& i0, int& i1,
                       int& ic) const {
  i0 = 0;
  i1 = -1;
  ic = 0;
  if (origin >= 0) {
    i0    = -std::floor(origin * iunit);
    i1    = i0 + std::floor(size * iunit);
    int d = stepceil(-i0, 10);
    i0 += d;
    i1 += d;
    ic += d;
  } else {
    i0 = std::ceil(-origin * iunit);
    i1 = i0 + std::floor(size * iunit);
    ic = 0;
  }
}

//-----------------------------------------------------------------------------

double Ruler::getPan() const {
  QTransform aff = m_viewer->getAffine();
  if (m_vertical)
    return aff.dy();
  else
    return aff.dx();  // Horizontal
}

//-----------------------------------------------------------------------------

void Ruler::drawVertical(QPainter& p) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  QSize workAreaSize = project->getWorkAreaSize();
  // ワークエリアサイズが空ならreturn
  if (workAreaSize.isEmpty()) return;

  int w = width();

  Guides& guides = getGuides();

  double minV = posToValue(QPoint(0, height()));
  double maxV = posToValue(QPoint(0, 0));

  // 0.01単位を描画するかどうか
  bool drawSmallScale = (valueToPos(0.0) - valueToPos(0.01)) >= 10.0;

  int x0 = 0, x1 = w - 1;

  if (drawSmallScale) {
    double v = 0.01 * std::ceil(minV * 100.0);
    while (v <= maxV) {
      bool isLarge = (int)std::round(v * 100.0) % 10 == 0;

      double vPos = valueToPos(v);
      p.setPen((isLarge) ? Qt::lightGray : Qt::gray);

      p.drawLine((isLarge) ? x0 : (x0 + x1) / 2, vPos, x1, vPos);

      v += 0.01;
    }
  } else {
    double v = 0.1 * std::ceil(minV * 10.0);
    while (v <= maxV) {
      double vPos = valueToPos(v);
      p.setPen(Qt::lightGray);
      p.drawLine(x0, vPos, x1, vPos);
      v += 0.1;
    }
  }

  int i;
  int count = (int)guides.size();
  if (m_hiding) count--;
  for (i = 0; i < count; i++) {
    double v = guides[i];
    if (v < minV || v > maxV) continue;
    double vPos  = valueToPos(v);
    QColor color = (m_highlightId == i)
                       ? (m_moving ? QColor(Qt::yellow) : QColor(150, 220, 100))
                       : QColor(100, 180, 255);
    QPen pen(color);
    pen.setWidth(2);
    p.setPen(pen);
    p.drawLine(x0, vPos, x1, vPos);
  }
}

//-----------------------------------------------------------------------------

void Ruler::drawHorizontal(QPainter& p) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  QSize workAreaSize = project->getWorkAreaSize();
  // ワークエリアサイズが空ならreturn
  if (workAreaSize.isEmpty()) return;

  int h = height();

  Guides& guides = getGuides();

  double maxV = posToValue(QPoint(width(), 0));
  double minV = posToValue(QPoint(0, 0));

  // 0.01単位を描画するかどうか
  bool drawSmallScale = (valueToPos(0.01) - valueToPos(0.0)) >= 10.0;

  int y0 = 0, y1 = h - 1;

  if (drawSmallScale) {
    double v = 0.01 * std::ceil(minV * 100.0);
    while (v <= maxV) {
      bool isLarge = (int)std::round(v * 100.0) % 10 == 0;

      double vPos = valueToPos(v);
      p.setPen((isLarge) ? Qt::lightGray : Qt::gray);

      p.drawLine(vPos, (isLarge) ? y0 : (y0 + y1) / 2, vPos, y1);

      v += 0.01;
    }
  } else {
    double v = 0.1 * std::ceil(minV * 10.0);
    while (v <= maxV) {
      double vPos = valueToPos(v);
      p.setPen(Qt::lightGray);
      p.drawLine(vPos, y0, vPos, y1);
      v += 0.1;
    }
  }

  int i;
  int count = (int)guides.size();
  if (m_hiding) count--;
  for (i = 0; i < count; i++) {
    double v = guides[i];
    if (v < minV || v > maxV) continue;
    double vPos  = valueToPos(v);
    QColor color = (m_highlightId == i)
                       ? (m_moving ? QColor(Qt::yellow) : QColor(150, 220, 100))
                       : QColor(100, 180, 255);
    QPen pen(color);
    pen.setWidth(2);
    p.setPen(pen);
    p.drawLine(vPos, y0, vPos, y1);
  }
}

//-----------------------------------------------------------------------------

void Ruler::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(QRect(0, 0, width(), height()), QBrush(QColor(64, 64, 64)));

  if (m_vertical)
    drawVertical(p);
  else
    drawHorizontal(p);
}

//-----------------------------------------------------------------------------

double Ruler::posToValue(const QPoint& p) const {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  QSize workAreaSize = project->getWorkAreaSize();
  // ワークエリアサイズが空ならreturn
  if (workAreaSize.isEmpty()) return 0.0;

  if (m_vertical)
    return ((-p.y() + height() / 2) / m_viewer->getAffine().m22() -
            m_viewer->getAffine().dy()) /
               (double)workAreaSize.height() +
           0.5;
  else
    return ((p.x() - width() / 2) / m_viewer->getAffine().m11() -
            m_viewer->getAffine().dx()) /
               (double)workAreaSize.width() +
           0.5;
}

//-----------------------------------------------------------------------------

double Ruler::valueToPos(const double& v) const {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  QSize workAreaSize = project->getWorkAreaSize();
  // ワークエリアサイズが空ならreturn
  if (workAreaSize.isEmpty()) return 0.0;
  if (m_vertical)
    return -((((v - 0.5) * (double)workAreaSize.height()) +
              m_viewer->getAffine().dy()) *
                 m_viewer->getAffine().m22() -
             (double)height() / 2.0);
  else
    return (((v - 0.5) * (double)workAreaSize.width()) +
            m_viewer->getAffine().dx()) *
               m_viewer->getAffine().m11() +
           (double)width() / 2.0;
}

//-----------------------------------------------------------------------------

void Ruler::mousePressEvent(QMouseEvent* e) {
  if (e->button() != Qt::LeftButton && e->button() != Qt::RightButton) return;
  Guides& guides = getGuides();
  double v       = posToValue(e->pos());
  m_hiding       = false;
  m_moving       = false;

  if (e->button() == Qt::LeftButton) {
    if (m_highlightId < 0) {
      guides.push_back(v);
      m_viewer->update();
    } else
      std::swap(guides[m_highlightId], guides.back());

    m_highlightId = (int)guides.size() - 1;
    m_moving      = true;
    update();
    assert(guides.size() > 0);
  } else if (e->button() == Qt::RightButton) {
    if (!guides.empty() && m_highlightId >= 0) {
      guides.erase(guides.begin() + m_highlightId);
      m_highlightId = -1;
      update();
      m_viewer->update();
      return;
    }
  }
}

//-----------------------------------------------------------------------------

void Ruler::mouseMoveEvent(QMouseEvent* e) {
  Guides& guides = getGuides();

  if (m_moving) {
    m_hiding = m_vertical ? (e->pos().x() < 0) : (e->pos().y() < 0);
    guides[m_highlightId] = posToValue(e->pos());
    // aggiorna sprop!!!!
    update();
    m_viewer->update();
    return;
  }

  double v = posToValue(e->pos());
  m_hiding = false;
  m_moving = false;

  int oldHighlightId = m_highlightId;
  m_highlightId      = -1;
  double minDist     = 0.001;
  for (int i = 0; i < guides.size(); i++) {
    double g    = guides[i];
    double dist = std::abs(g - v);
    if (dist < minDist) {
      minDist       = dist;
      m_highlightId = i;
    }
  }
  if (m_highlightId >= 0) {
    setToolTip(
        tr("Left-click and drag to move guide, right-click to delete guide."));
  }
  // in case no guides are found near the cursor
  else if (m_vertical)
    setToolTip(tr("Click to create a horizontal guide"));
  else
    setToolTip(tr("Click to create a vertical guide"));

  if (m_highlightId != oldHighlightId) update();
}

//-----------------------------------------------------------------------------

void Ruler::mouseReleaseEvent(QMouseEvent* /*e*/) {
  if (!m_moving) return;
  if (m_hiding) {
    assert(!getGuides().empty());
    getGuides().pop_back();
  }
  m_moving = m_hiding = false;
  update();
  m_viewer->update();
}
//-----------------------------------------------------------------------------

void Ruler::leaveEvent(QEvent* /*event*/) {
  m_highlightId = -1;
  update();
}
