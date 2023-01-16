#ifndef TIMELINEWINDOW_H
#define TIMELINEWINDOW_H
//----------------------------------------------
// TimeLineWindow
// タイムライン
//----------------------------------------------

#include <QDockWidget>

#include <QWidget>
#include <QScrollArea>
#include <QLineEdit>
#include <QUndoCommand>

class QComboBox;
class QCheckBox;
class QPushButton;

class IwLayer;
class ShapePair;

class IwTimeLineSelection;
class PreviewRangeUndo;
class IwSelection;

class TimeLineExtenderUndo : public QUndoCommand {
  IwLayer *m_layer;
  int m_startFrame;
  int m_endFrame;
  bool m_first = true;

public:
  TimeLineExtenderUndo(IwLayer *layer, int startFrame);
  int startFrame() { return m_startFrame; }
  void setEndFrame(int endFrame) { m_endFrame = endFrame; }
  void undo();
  void redo();
};

class TimeLineExtender {
  IwLayer *m_layer;
  int m_currentFrame;
  QPair<int, QString> m_pathData;
  TimeLineExtenderUndo *m_undo = nullptr;

public:
  TimeLineExtender(IwLayer *layer, int frameIndex);
  void onClick();
  void onDrag(int toFrame);
  void onRelease();
};

//----------------------------------------------

class PreviewRangeDragTool {
public:
  enum StartEnd { START = 0, END };

private:
  StartEnd m_startEnd;
  PreviewRangeUndo *m_undo;
  int m_currentFrame;

public:
  PreviewRangeDragTool(StartEnd startEnd);
  void onClick(int frame);
  void onDrag(int frame);
  void onRelease();
};

//----------------------------------------------
// 左側のアイテムリスト
//----------------------------------------------

// ヘッダ
class ItemListHead : public QWidget {
  Q_OBJECT
  // 拡大／縮小表示
  QComboBox *m_viewSizeCombo;
  QComboBox *m_onionRangeCombo;
  QComboBox *m_shapeDisplayCombo;

public:
  ItemListHead(QWidget *parent);
  void setViewSize(int sizeIndex);
signals:
  void viewSizeChanged(int);
protected slots:
  void onOnionRangeChanged();
  void onShapeDisplayChanged();
};

// リネーム用のLineEdit
class ItemRenameField : public QLineEdit {
  Q_OBJECT
  IwLayer *m_layer;
  ShapePair *m_shapePair;

public:
  ItemRenameField(QWidget *parent);
  void showField(IwLayer *, ShapePair *, const QPoint &);

public slots:
  void renameItem();
};

class ScrollPanel : public QFrame {
  Q_OBJECT
protected:
  // 中ボタンドラッグでパン中かどうか。
  bool m_isPanning;
  QScrollArea *m_viewer;
  static int m_frameWidth;

public:
  ScrollPanel(QWidget *parent);
  void doPan(QPoint delta);
  void setViewer(QScrollArea *viewer) { m_viewer = viewer; }
  static void setViewSize(int);
  void scrollToFrame(int currentFrame);
};

// パネル
class ItemListPanel : public ScrollPanel {
  Q_OBJECT

  QPoint m_mousePos;
  ItemRenameField *m_renameField;

public:
  ItemListPanel(QWidget *parent);

protected:
  // event handlers
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;

  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void contextMenuEvent(QContextMenuEvent *) override;

  // レイヤ、シェイプ名ダブルクリックで名前の変更
  void mouseDoubleClickEvent(QMouseEvent *) override;
  void leaveEvent(QEvent *) override;

public slots:
  void computeSize();
  void onAppend();

  void onMoveLayerUpward();
  void onMoveLayerDownward();

  void onDeleteLayer();
  void onCloneLayer();

signals:
  void updateUIs();
};

//----------------------------------------------
// 右側のタイムライン
//----------------------------------------------

// 上のフレーム表示パネル
class TimeLineHead : public ScrollPanel {
  Q_OBJECT

  QPoint m_mousePos;
  PreviewRangeDragTool *m_prevDragTool;

public:
  TimeLineHead(QWidget *parent);

protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;

  void mouseMoveEvent(QMouseEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;

  void leaveEvent(QEvent *) override;

  void contextMenuEvent(QContextMenuEvent *) override;
  void wheelEvent(QWheelEvent *e) final override;

public slots:
  void computeSize();
  void onPreviewMarkerCommand();
  void onToggleShapeReferenceFrame();
};

// パネル
class TimeLinePanel : public ScrollPanel {
  Q_OBJECT

  QPoint m_mousePos;
  // 選択範囲
  IwTimeLineSelection *m_timeLineSelection;

  // クリックされたフレームインデックス。無い場合は-1
  int m_clickedFrameIndex;
  // クリックされたところが境界か、フレームか
  bool m_isFrameClicked;

  // マウスクリック用：横位置からフレームインデックスを返す。
  // 境界の近くの場合はisBorderフラグがtrueになる。
  int xPosToFrameIndexAndBorder(int xPos, bool &isBorder);

  TimeLineExtender *m_timeLineExtender = nullptr;

public:
  TimeLinePanel(QWidget *parent);

protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;

  void mouseMoveEvent(QMouseEvent *) override;
  // マウスクリックで選択をやりくり
  void mousePressEvent(QMouseEvent *e) override;
  // 前回のクリック位置情報をクリアする
  void mouseReleaseEvent(QMouseEvent *e) override;

  void leaveEvent(QEvent *e) override;
  // キーの設定、解除
  void contextMenuEvent(QContextMenuEvent *) override;

  // ツールチップでファイル名の表示など
  bool event(QEvent *e) override;

public slots:
  void computeSize();
signals:
  void updateUIs();
};

//----------------------------------------------
// 本体
//----------------------------------------------

class TimeLineWindow : public QDockWidget {
  Q_OBJECT

  // 左側のアイテムリスト
  ItemListHead *m_itemListHead;
  ItemListPanel *m_itemListPanel;

  // 右側のタイムライン
  TimeLineHead *m_timeLineHead;
  TimeLinePanel *m_timeLinePanel;

public:
  TimeLineWindow(QWidget *parent);

protected:
  void resizeEvent(QResizeEvent *) final override;

protected slots:
  void onUpdateUIs();
  // フレーム横幅サイズが変化したとき
  void onViewSizeChanged(int);
  void onSelectionChanged(IwSelection *);
  void onCurrentFrameChanged();
};

#endif
