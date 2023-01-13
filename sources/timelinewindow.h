#ifndef TIMELINEWINDOW_H
#define TIMELINEWINDOW_H
//----------------------------------------------
// TimeLineWindow
// �^�C�����C��
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
// �����̃A�C�e�����X�g
//----------------------------------------------

// �w�b�_
class ItemListHead : public QWidget {
  Q_OBJECT
  // �g��^�k���\��
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

// ���l�[���p��LineEdit
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
  // ���{�^���h���b�O�Ńp�������ǂ����B
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

// �p�l��
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

  // ���C���A�V�F�C�v���_�u���N���b�N�Ŗ��O�̕ύX
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
// �E���̃^�C�����C��
//----------------------------------------------

// ��̃t���[���\���p�l��
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

// �p�l��
class TimeLinePanel : public ScrollPanel {
  Q_OBJECT

  QPoint m_mousePos;
  // �I��͈�
  IwTimeLineSelection *m_timeLineSelection;

  // �N���b�N���ꂽ�t���[���C���f�b�N�X�B�����ꍇ��-1
  int m_clickedFrameIndex;
  // �N���b�N���ꂽ�Ƃ��낪���E���A�t���[����
  bool m_isFrameClicked;

  // �}�E�X�N���b�N�p�F���ʒu����t���[���C���f�b�N�X��Ԃ��B
  // ���E�̋߂��̏ꍇ��isBorder�t���O��true�ɂȂ�B
  int xPosToFrameIndexAndBorder(int xPos, bool &isBorder);

  TimeLineExtender *m_timeLineExtender = nullptr;

public:
  TimeLinePanel(QWidget *parent);

protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;

  void mouseMoveEvent(QMouseEvent *) override;
  // �}�E�X�N���b�N�őI������肭��
  void mousePressEvent(QMouseEvent *e) override;
  // �O��̃N���b�N�ʒu�����N���A����
  void mouseReleaseEvent(QMouseEvent *e) override;

  void leaveEvent(QEvent *e) override;
  // �L�[�̐ݒ�A����
  void contextMenuEvent(QContextMenuEvent *) override;

  // �c�[���`�b�v�Ńt�@�C�����̕\���Ȃ�
  bool event(QEvent *e) override;

public slots:
  void computeSize();
signals:
  void updateUIs();
};

//----------------------------------------------
// �{��
//----------------------------------------------

class TimeLineWindow : public QDockWidget {
  Q_OBJECT

  // �����̃A�C�e�����X�g
  ItemListHead *m_itemListHead;
  ItemListPanel *m_itemListPanel;

  // �E���̃^�C�����C��
  TimeLineHead *m_timeLineHead;
  TimeLinePanel *m_timeLinePanel;

public:
  TimeLineWindow(QWidget *parent);

protected:
  void resizeEvent(QResizeEvent *) final override;

protected slots:
  void onUpdateUIs();
  // �t���[�������T�C�Y���ω������Ƃ�
  void onViewSizeChanged(int);
  void onSelectionChanged(IwSelection *);
  void onCurrentFrameChanged();
};

#endif
