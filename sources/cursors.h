#ifndef CURSORS_INCLUDED
#define CURSORS_INCLUDED

namespace ToolCursor {

enum {
  ArrowCursor,
  //---------
  // Transform Tool
  SizeVerCursor,
  SizeHorCursor,
  SizeBDiagCursor,
  SizeFDiagCursor,
  SizeAllCursor,
  RotationCursor,
  TrapezoidCursor,
  ShearCursor,
  //---------
  // Reshape Tool
  BlackArrowCursor,
  BlackArrowAddCursor,
  //---------
  // Square Tool
  SquareCursor,
  //---------
  // Circle Tool
  CircleCursor,
  //---------
  // Pen Tool
  BlackArrowCloseShapeCursor,
  //---------
  // KeyFrameEditor
  MoveKeyFrame,
  CopyKeyFrame,
  //---------
  // Zoom Tool
  ZoomInCursor,
  ZoomOutCursor,
  //----------
  ForbiddenCursor
};

}  // namespace ToolCursor

#endif
