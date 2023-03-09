//-----------------------------------------------------------------------------
// IwTool
// ツールのクラス
//-----------------------------------------------------------------------------
#ifndef IWTOOL_H
#define IWTOOL_H

#include "cursors.h"

#include "shapepair.h"

#include <QString>
#include <QObject>

class QPointF;
class QMouseEvent;

class SceneViewer;
class IwShape;

class QMenu;

class IwTool : public QObject {
  QString m_name;

protected:
  // 現在の親のSceneViewerを登録
  SceneViewer *m_viewer;

public:
  IwTool(QString toolName);

  // ツール名からツールを取得
  static IwTool *getTool(QString toolName);

  // 名前を返す
  QString getName() const { return m_name; }

  // Viewerをupdateしたい場合はtrueを返す
  virtual bool leftButtonDown(const QPointF &, const QMouseEvent *) {
    return false;
  }
  virtual bool leftButtonDrag(const QPointF &, const QMouseEvent *) {
    return false;
  }
  virtual bool leftButtonUp(const QPointF &, const QMouseEvent *) {
    return false;
  }

  virtual void leftButtonDoubleClick(const QPointF &, const QMouseEvent *) {}

  virtual bool rightButtonDown(const QPointF &, const QMouseEvent *,
                               bool & /*canOpenMenu*/, QMenu & /*menu*/) {
    return false;
  }

  virtual bool mouseMove(const QPointF &, const QMouseEvent *) { return false; }

  // 何かショートカットキーが押されたときの解決。
  //  Toolでショートカットを処理した場合はtrueが返る
  virtual bool keyDown(int /*key*/, bool /*ctrl*/, bool /*shift*/,
                       bool /*alt*/) {
    return false;
  }

  virtual int getCursorId(const QMouseEvent *) {
    return ToolCursor::ArrowCursor;
  };

  virtual void draw() {}

  virtual void onDeactivate();
  virtual void onActivate() {}

  // SceneViewerを登録
  void setViewer(SceneViewer *viewer) { m_viewer = viewer; }
  // SceneViewerを取り出す
  SceneViewer *getViewer() { return m_viewer; }

  // 指定シェイプの対応点を描画する
  void drawCorrLine(OneShape);

  // Joinしているシェイプを繋ぐ対応線を描く
  void drawJoinLine(ShapePair *);

  virtual bool setSpecialShapeColor(OneShape) { return false; }
  virtual bool setSpecialGridColor(int /*gId*/, bool /*isVertical*/) {
    return false;
  }
};

#endif