#include "cursormanager.h"
#include "iwtool.h"
#include "cursors.h"

#include <QWidget>
#include <QPixmap>
#include <QCursor>
#include <assert.h>
#include <map>
#include <QDebug>

namespace {

const struct {
  int cursorType;
  const char *pixmapFilename;
  int x, y;
} cursorInfo[] = {
    //---------
    // Transform Tool
    {ToolCursor::RotationCursor, "rotation", 8, 1},
    {ToolCursor::TrapezoidCursor, "trapezoid", 8, 2},
    {ToolCursor::ShearCursor, "shear", 8, 2},
    //---------
    // Reshape Tool
    {ToolCursor::BlackArrowCursor, "blackarrow", 1, 1},
    {ToolCursor::BlackArrowAddCursor, "blackarrowadd", 1, 1},
    //---------
    // Square Tool
    {ToolCursor::SquareCursor, "square", 8, 2},
    //---------
    // Circle Tool
    {ToolCursor::CircleCursor, "circle", 8, 2},
    //---------
    // Pen Tool
    {ToolCursor::BlackArrowCloseShapeCursor, "blackarrowcloseshape", 1, 1},
    //---------
    // KeyFrameEditor
    {ToolCursor::MoveKeyFrame, "movekeyframe", 8, 2},
    {ToolCursor::CopyKeyFrame, "copykeyframe", 8, 2},
    //---------
    // Zoom Tool
    {ToolCursor::ZoomInCursor, "zoomin", 8, 8},
    {ToolCursor::ZoomOutCursor, "zoomout", 8, 8},

    {0, 0, 0, 0}};

struct CursorData {
  QPixmap pixmap;
  int x, y;
};
};  // namespace

//=============================================================================
// CursorManager
//-----------------------------------------------------------------------------

class CursorManager {  // singleton

  std::map<int, CursorData> m_cursors;

  CursorManager() {}

public:
  static CursorManager *instance() {
    static CursorManager _instance;
    return &_instance;
  }

  const CursorData &getCursorData(int cursorType) {
    std::map<int, CursorData>::iterator it;
    it = m_cursors.find(cursorType);
    if (it != m_cursors.end()) return it->second;

    int i;
    for (i = 0; cursorInfo[i].pixmapFilename; i++)
      if (cursorType == cursorInfo[i].cursorType) {
        QString path = QString(":Resources/cursors/") +
                       cursorInfo[i].pixmapFilename + ".png";
        CursorData data;
        data.pixmap = QPixmap(path);
        data.x      = cursorInfo[i].x;
        data.y      = cursorInfo[i].y;
        it          = m_cursors.insert(std::make_pair(cursorType, data)).first;
        return it->second;
      }

    CursorData data;
    static const QPixmap standardCursorPixmap("cursors/hook.png");
    data.pixmap = standardCursorPixmap;
    data.x = data.y = 0;
    it              = m_cursors.insert(std::make_pair(cursorType, data)).first;
    return it->second;
  }

  void setCursor(QWidget *viewer, int cursorType) {
    QCursor cursor;
    // Qtのデフォルトのやつを使う場合は例外処理する
    switch (cursorType) {
    case ToolCursor::ArrowCursor:
      cursor = Qt::ArrowCursor;
      break;
    case ToolCursor::SizeVerCursor:
      cursor = Qt::SizeVerCursor;
      break;
    case ToolCursor::SizeHorCursor:
      cursor = Qt::SizeHorCursor;
      break;
    case ToolCursor::SizeBDiagCursor:
      cursor = Qt::SizeBDiagCursor;
      break;
    case ToolCursor::SizeFDiagCursor:
      cursor = Qt::SizeFDiagCursor;
      break;
    case ToolCursor::SizeAllCursor:
      cursor = Qt::SizeAllCursor;
      break;
    case ToolCursor::ForbiddenCursor:
      cursor = Qt::ForbiddenCursor;
      break;
    default:
      const CursorData &data = getCursorData(cursorType);
      cursor                 = QCursor(data.pixmap, data.x, data.y);
      break;
    }

    viewer->setCursor(cursor);
  }
};

//-----------------------------------------------------------------------------

void setToolCursor(QWidget *viewer, int cursorType) {
  CursorManager::instance()->setCursor(viewer, cursorType);
}
