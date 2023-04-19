//----------------------------------------------
// TimeLineWindow
// タイムライン
//----------------------------------------------

#include "timelinewindow.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwlayerhandle.h"
#include "iwselectionhandle.h"
#include "iwproject.h"
#include "iwlayer.h"
#include "menubarcommand.h"
#include "menubarcommandids.h"
#include "iocommand.h"
#include "viewsettings.h"
#include "projectutils.h"

#include "iwtimelineselection.h"
#include "iwtimelinekeyselection.h"

#include "iwshapepairselection.h"

#include "iwundomanager.h"
#include "iwtrianglecache.h"
#include "keydragtool.h"
#include "outputsettings.h"
#include "preferences.h"

#include <iostream>

#include <QLineEdit>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QCheckBox>
#include <QPainterPath>
#include <QPushButton>

#include <QMenu>
#include <QTooltip>

#include <QComboBox>

#define RowHeight 21
#define FrameWidth 30
#define FrameWidth_Large 88
#define HeaderHeight 30

//----------------------------------------------

namespace {
int posToRow(const QPoint& pos) { return pos.y() / RowHeight; }
int rowToVPos(int vpos) { return vpos * RowHeight; }
//----------------------------------------------
// はずれている場合は-1を返す
//----------------------------------------------
int rowToLayerIndex(int row) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return -1;

  int layerCount = prj->getLayerCount();
  if (layerCount == 0) return -1;

  int currentRow = 0;
  for (int lay = 0; lay < layerCount; lay++) {
    IwLayer* layer = prj->getLayer(lay);
    if (layer) {
      if (currentRow == row) return lay;
      currentRow += layer->getRowCount();
      if (currentRow > row) return lay;
    }
  }

  return -1;
}
int layerIndexToRow(int index) {
  if (index < 0) return -1;
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return -1;

  int layerCount = prj->getLayerCount();

  if (index >= layerCount) return -1;

  int currentRow = 0;
  for (int i = 0; i < index; i++) {
    IwLayer* layer = prj->getLayer(i);
    if (layer) currentRow += layer->getRowCount();
  }
  return currentRow;
}

//----------------------------------------------
int posToFrame(const QPoint& pos, int frameWidth) {
  return (pos.x() < 0) ? -1 : pos.x() / frameWidth;
}
double posToFrameD(const QPoint& pos, int frameWidth) {
  return (pos.x() < 0) ? -1.0 : (double)pos.x() / (double)frameWidth;
}
// int frameToHPos(int frame) { return frame * RowHeight; }

inline QColor gray(int g) { return QColor(g, g, g); }

static QPainterPath prevFromMarker;
static QPainterPath prevToMarker;

void buildMarkerShapes() {
  prevFromMarker.moveTo(0, 0);
  prevFromMarker.lineTo(0, HeaderHeight);
  prevFromMarker.lineTo(7, HeaderHeight);
  prevFromMarker.lineTo(4, HeaderHeight - 3);
  prevFromMarker.lineTo(4, 3);
  prevFromMarker.lineTo(7, 0);
  prevFromMarker.lineTo(0, 0);

  prevToMarker.moveTo(0, 0);
  prevToMarker.lineTo(3, 3);
  prevToMarker.lineTo(3, HeaderHeight - 3);
  prevToMarker.lineTo(0, HeaderHeight);
  prevToMarker.lineTo(7, HeaderHeight);
  prevToMarker.lineTo(7, 0);
  prevToMarker.lineTo(0, 0);
}

};  // namespace

//----------------------------------------------
// TimeLineExtender
//----------------------------------------------

TimeLineExtenderUndo::TimeLineExtenderUndo(IwLayer* layer, int startFrame)
    : m_layer(layer), m_startFrame(startFrame) {}

void TimeLineExtenderUndo::undo() {
  int df = m_endFrame - m_startFrame;
  // undoing extension to right
  if (df > 0) {
    // 画像消去
    m_layer->deletePaths(m_startFrame, df, true);
  }
  // undoing shrink to left
  else {
    QPair<int, QString> pathData = m_layer->getPathData(m_endFrame - 1);
    // 画像挿入
    for (int f = 0; f < -df; f++) m_layer->insertPath(m_endFrame, pathData);
  }
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
}

void TimeLineExtenderUndo::redo() {
  // skip redo on register
  if (m_first) {
    m_first = false;
    return;
  }

  int df = m_endFrame - m_startFrame;
  // extend to right
  if (df > 0) {
    QPair<int, QString> pathData = m_layer->getPathData(m_startFrame - 1);
    // 画像挿入
    for (int f = 0; f < df; f++) m_layer->insertPath(m_startFrame, pathData);
  }
  // shrink to left
  else {
    // 画像消去
    m_layer->deletePaths(m_endFrame, -df, true);
  }
  // シグナル
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
}

//----------------------------------------------

TimeLineExtender::TimeLineExtender(IwLayer* layer, int frameIndex)
    : m_layer(layer), m_currentFrame(frameIndex) {
  assert(m_currentFrame > 0);
  m_pathData = m_layer->getPathData(m_currentFrame - 1);
}

void TimeLineExtender::onClick() {
  // Undoつくる
  m_undo = new TimeLineExtenderUndo(m_layer, m_currentFrame);
}

void TimeLineExtender::onDrag(int toFrame) {
  // フレーム移動していなければreturn
  if (m_currentFrame == toFrame) return;

  int df = toFrame - m_currentFrame;
  // extend to right
  if (df > 0) {
    // 画像挿入
    for (int f = 0; f < df; f++)
      m_layer->insertPath(m_currentFrame, m_pathData);
  }
  // shrink to left
  else {
    // 左が空コマの境界を左に移動する場合
    if (m_pathData.second.isEmpty()) {
      // 何か入っていたら break
      // フレームをさかのぼり、toFrameまで同じ素材が連続しているかチェック
      for (int tmpF = m_currentFrame; tmpF >= toFrame; tmpF--) {
        if (tmpF - 1 < 0 || m_layer->getPathData(tmpF - 1) != m_pathData) {
          toFrame = tmpF;
          break;
        }
      }
    }
    // 左に何か入っているコマの境界を左に移動する場合
    else {
      // フレームをさかのぼり、toFrameまで同じ素材が連続しているかチェック
      for (int tmpF = m_currentFrame; tmpF >= toFrame; tmpF--) {
        if (tmpF - 2 < 0 || m_layer->getPathData(tmpF - 2) != m_pathData) {
          toFrame = tmpF;
          break;
        }
      }
    }
    // 修正後、フレーム移動していなければreturn
    if (m_currentFrame == toFrame) return;
    df = toFrame - m_currentFrame;

    // 画像消去
    m_layer->deletePaths(toFrame, -df, true);
  }
  m_currentFrame = toFrame;
}

void TimeLineExtender::onRelease() {
  if (m_currentFrame == m_undo->startFrame()) {
    delete m_undo;
    return;
  }
  m_undo->setEndFrame(m_currentFrame);

  IwUndoManager::instance()->push(m_undo);
  // signal
  IwApp::instance()->getCurrentLayer()->notifyLayerChanged(m_layer);
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}
//----------------------------------------------

PreviewRangeDragTool::PreviewRangeDragTool(StartEnd startEnd)
    : m_startEnd(startEnd) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  m_undo         = new PreviewRangeUndo(prj);
}

void PreviewRangeDragTool::onClick(int frame) { m_currentFrame = frame; }

void PreviewRangeDragTool::onDrag(int frame) {
  if (m_currentFrame == frame) return;
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (m_startEnd == START)
    prj->setPreviewFrom(frame);
  else  // END
    prj->setPreviewTo(frame);
}

void PreviewRangeDragTool::onRelease() {
  bool changed = m_undo->registerAfter();
  if (changed)
    IwUndoManager::instance()->push(m_undo);
  else
    delete m_undo;
}

//----------------------------------------------
// 左側のアイテムリスト
//----------------------------------------------
// ヘッダ
//----------------------------------------------
ItemListHead::ItemListHead(QWidget* parent) : QWidget(parent) {
  setFixedHeight(HeaderHeight);
  setFixedWidth(200);

  // オブジェクトの宣言
  m_viewSizeCombo = new QComboBox(this);
  // m_syncExpandBtn = new QPushButton(this);
  m_onionRangeCombo = new QComboBox(this);

  m_shapeDisplayCombo = new QComboBox(this);

  // プロパティの設定
  // 表示モード
  QStringList views;
  views << tr("Small") << tr("Large");
  m_viewSizeCombo->addItems(views);

  m_onionRangeCombo->setToolTip(tr("Relative Onion Skin Frame Range"));
  m_onionRangeCombo->addItem(tr("None"), 0);
  m_onionRangeCombo->addItem("5", 5);
  m_onionRangeCombo->addItem("10", 10);
  m_onionRangeCombo->addItem("20", 20);
  m_onionRangeCombo->addItem(tr("50 (step2)"), 50);

  m_shapeDisplayCombo->setToolTip(tr("Shape Display Mode"));
  m_shapeDisplayCombo->addItem(tr("All"), false);
  m_shapeDisplayCombo->addItem(tr("Selected"), true);
  m_shapeDisplayCombo->setCurrentIndex(m_shapeDisplayCombo->findData(
      Preferences::instance()->isShowSelectedShapesOnly()));

  // レイアウト
  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setMargin(0);
  mainLay->setSpacing(0);
  {
    mainLay->addWidget(m_viewSizeCombo, 0);
    mainLay->addWidget(m_shapeDisplayCombo, 0);
    mainLay->addStretch(1);
    mainLay->addWidget(m_onionRangeCombo, 0);
  }
  setLayout(mainLay);

  // シグナル／スロット接続
  // カレントフレームが変わったらカレントフレーム表示を更新

  // サイズのComboBoxが切り替わったらサイズ変更シグナルを出す
  connect(m_viewSizeCombo, SIGNAL(activated(int)), this,
          SIGNAL(viewSizeChanged(int)));
  // connect(m_syncExpandBtn, SIGNAL(clicked(bool)), this,
  //   SLOT(onSyncExpandBtnChecked(bool)));
  connect(m_onionRangeCombo, SIGNAL(activated(int)), this,
          SLOT(onOnionRangeChanged()));
  connect(m_shapeDisplayCombo, SIGNAL(activated(int)), this,
          SLOT(onShapeDisplayChanged()));
}

void ItemListHead::setViewSize(int sizeIndex) {
  m_viewSizeCombo->setCurrentIndex(sizeIndex);
}

void ItemListHead::onOnionRangeChanged() {
  ViewSettings* settings =
      IwApp::instance()->getCurrentProject()->getProject()->getViewSettings();
  settings->setOnionRange(m_onionRangeCombo->currentData().toInt());
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void ItemListHead::onShapeDisplayChanged() {
  Preferences::instance()->setShowSelectedShapesOnly(
      m_shapeDisplayCombo->currentData().toBool());
  // update timeline display
  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//----------------------------------------------
// リネーム用のLineEdit
//----------------------------------------------
ItemRenameField::ItemRenameField(QWidget* parent)
    : QLineEdit(parent), m_layer(0), m_shapePair(0) {
  setFixedSize(100, RowHeight - 4);
  connect(this, SIGNAL(editingFinished()), this, SLOT(renameItem()));
  setContextMenuPolicy(Qt::PreventContextMenu);
}
//----------------------------------------------
void ItemRenameField::showField(IwLayer* layer, ShapePair* shape,
                                const QPoint& pos) {
  // どっちかに入力する
  if (!layer && !shape) return;
  m_layer     = layer;
  m_shapePair = shape;

  move(pos);
  static QFont font("Helvetica", 12, QFont::Normal);
  setFont(font);

  QString name;
  if (m_layer)
    name = m_layer->getName();
  else if (m_shapePair)
    name = m_shapePair->getName();

  setText(name);
  selectAll();
  show();
  raise();
  setFocus();
}
//----------------------------------------------
void ItemRenameField::renameItem() {
  // どっちかに入力する
  if (!m_layer && !m_shapePair) return;

  QString newName = text();

  if (!newName.isEmpty()) {
    if (m_layer)
      ProjectUtils::RenameLayer(m_layer, newName);
    else if (m_shapePair)
      ProjectUtils::RenameShapePair(m_shapePair, newName);
  }

  m_layer     = 0;
  m_shapePair = 0;
  hide();
}

//----------------------------------------------
// パネル
//----------------------------------------------

ScrollPanel::ScrollPanel(QWidget* parent)
    : QFrame(parent), m_isPanning(false), m_viewer(nullptr) {}
int ScrollPanel::m_frameWidth = FrameWidth;

void ScrollPanel::doPan(QPoint delta) {
  if (!m_viewer) return;

  int valueH = m_viewer->horizontalScrollBar()->value() + delta.x();
  int valueV = m_viewer->verticalScrollBar()->value() + delta.y();

  m_viewer->horizontalScrollBar()->setValue(valueH);
  m_viewer->verticalScrollBar()->setValue(valueV);
}

void ScrollPanel::setViewSize(int sizeIndex) {
  m_frameWidth = (sizeIndex == 0) ? FrameWidth : FrameWidth_Large;
}

void ScrollPanel::scrollToFrame(int currentFrame) {
  int hPos = currentFrame * m_frameWidth + m_frameWidth / 2;
  int vPos =
      visibleRegion().boundingRect().center().y();  // not to scroll vertically
  m_viewer->ensureVisible(hPos, vPos, m_frameWidth / 2, 1);
}

//----------------------------------------------

ItemListPanel::ItemListPanel(QWidget* parent)
    : ScrollPanel(parent)
    , m_mousePos(-1, -1)
    , m_renameField(new ItemRenameField(this)) {
  setFrameStyle(QFrame::StyledPanel);
  setMouseTracking(true);

  m_renameField->hide();
}

//----------------------------------------------

void ItemListPanel::paintEvent(QPaintEvent* e) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  QPainter p(this);

  QRect visibleRect = e->rect().adjusted(0, 0, -1, -1);

  if (visibleRect.isEmpty()) return;

  QPen defaultPen(p.pen());

  p.setPen(Qt::black);
  p.setBrush(gray(55));
  p.drawRect(visibleRect);

  p.setPen(defaultPen);

  int layerCount = prj->getLayerCount();
  if (layerCount == 0) {
    p.drawText(QPoint(5, 25), tr("- No Layers -"));
    return;
  }

  int r0 = posToRow(visibleRect.topLeft());
  int r1 = posToRow(visibleRect.bottomLeft());

  int lay0 = rowToLayerIndex(r0);
  int lay1 = rowToLayerIndex(r1);
  // lay1が-1のとき、最後までレイヤを描画
  if (lay1 == -1) {
    if (lay0 == -1)
      return;
    else
      lay1 = layerCount - 1;
  }

  // 描画を始める行
  int currentRow = layerIndexToRow(lay0);
  // マウスがある行
  int mouseOverRow = posToRow(m_mousePos);
  // マウスの横位置
  int mouseHPos = m_mousePos.x();

  // std::cout<<"mouseOverRow = "<<mouseOverRow<<std::endl;

  for (int lay = lay0; lay <= lay1; lay++) {
    IwLayer* layer = prj->getLayer(lay);
    if (layer) {
      // 各レイヤの描画
      layer->drawTimeLineHead(p, rowToVPos(layerIndexToRow(lay)),
                              visibleRect.width(), RowHeight, currentRow,
                              mouseOverRow, mouseHPos);
    }
  }
}

//----------------------------------------------

void ItemListPanel::resizeEvent(QResizeEvent*) { computeSize(); }

//----------------------------------------------

void ItemListPanel::mousePressEvent(QMouseEvent* e) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  m_mousePos = e->pos();

  if (e->button() == Qt::MiddleButton) {
    m_isPanning = true;
    return;
  }

  // マウスがある行
  int mousePressedRow = posToRow(m_mousePos);
  // マウスの横位置
  int mouseHPos = m_mousePos.x();

  int layerIndex = rowToLayerIndex(mousePressedRow);
  if (layerIndex == -1) return;

  // レイヤを取得
  IwLayer* layer = prj->getLayer(layerIndex);
  if (!layer) return;

  bool doUpdate = layer->onLeftClick(
      mousePressedRow - layerIndexToRow(layerIndex), mouseHPos, e);

  // 自分と、隣のパネルも更新
  if (doUpdate) emit updateUIs();
}

//----------------------------------------------

void ItemListPanel::mouseMoveEvent(QMouseEvent* e) {
  QPoint pos = e->pos();
  if (m_isPanning) {
    doPan(QPoint(0, m_mousePos.y() - pos.y()));
    return;
  }
  m_mousePos = pos;
  update();
}

//----------------------------------------------

void ItemListPanel::contextMenuEvent(QContextMenuEvent* e) {
  QMenu menu(this);  // マウスがある行

  //--- レイヤ上で右クリックした場合は、画像の追加もできるようにする
  int mousePressedRow = posToRow(m_mousePos);
  int layerIndex      = rowToLayerIndex(mousePressedRow);

  IwLayer* layer = nullptr;
  if (layerIndex != -1) {
    layer = IwApp::instance()->getCurrentProject()->getProject()->getLayer(
        layerIndex);
  }

  if (!layer || layer->isLocked()) {
    menu.addAction(CommandManager::instance()->getAction(MI_InsertImages));
    menu.exec(e->globalPos());
    return;
  }

  QAction* appendImgAction = menu.addAction(tr("Append Images..."));
  appendImgAction->setData(layerIndex);
  connect(appendImgAction, SIGNAL(triggered()), this, SLOT(onAppend()));

  //---
  menu.addAction(CommandManager::instance()->getAction(MI_InsertImages));

  auto addLayerAction = [&](const QString& title) {
    QAction* action = menu.addAction(title);
    action->setData(layerIndex);
    return action;
  };

  // マウスがある行
  int row = mousePressedRow - layerIndexToRow(layerIndex);
  if (row != 0) {
    layer->onRightClick(row, menu);
  }
  // レイヤ部分右クリックの場合
  else {
    // レイヤ上のフレーム選択範囲が有効なとき、差し替えコマンドを表示
    if (IwSelection::getCurrent() == IwTimeLineSelection::instance() &&
        !IwTimeLineSelection::instance()->isEmpty())
      menu.addAction(CommandManager::instance()->getAction(MI_ReplaceImages));

    // レイヤの削除
    connect(addLayerAction(tr("Delete Layer")), SIGNAL(triggered()), this,
            SLOT(onDeleteLayer()));

    // レイヤの複製
    connect(addLayerAction(tr("Clone Layer")), SIGNAL(triggered()), this,
            SLOT(onCloneLayer()));
  }

  menu.addSeparator();
  if (layerIndex > 0) {
    connect(addLayerAction(tr("Move Layer Upward")), SIGNAL(triggered()), this,
            SLOT(onMoveLayerUpward()));
  }
  if (layerIndex <
      IwApp::instance()->getCurrentProject()->getProject()->getLayerCount() -
          1) {
    connect(addLayerAction(tr("Move Layer Downward")), SIGNAL(triggered()),
            this, SLOT(onMoveLayerDownward()));
  }

  menu.exec(e->globalPos());
}

//----------------------------------------------
// レイヤ、シェイプ名ダブルクリックで名前の変更
//----------------------------------------------
void ItemListPanel::mouseDoubleClickEvent(QMouseEvent* e) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  m_mousePos = e->pos();
  // マウスがある行
  int mousePressedRow = posToRow(m_mousePos);
  // マウスの横位置
  int mouseHPos  = m_mousePos.x();
  int layerIndex = rowToLayerIndex(mousePressedRow);
  if (layerIndex == -1) return;

  // レイヤを取得
  IwLayer* layer = prj->getLayer(layerIndex);
  if (!layer) return;

  ShapePair* shape            = 0;
  bool nameFieldDoubleClicked = layer->onDoubleClick(
      mousePressedRow - layerIndexToRow(layerIndex), mouseHPos, &shape);

  if (nameFieldDoubleClicked) {
    if (shape) {
      int indent = (shape->isParent()) ? 10 : 30;
      m_renameField->showField(
          0, shape, QPoint(45 + indent, rowToVPos(mousePressedRow) + 2));
    } else {
      m_renameField->showField(layer, 0,
                               QPoint(80, rowToVPos(mousePressedRow) + 2));
    }
  }
}

//----------------------------------------------

void ItemListPanel::mouseReleaseEvent(QMouseEvent* /*e*/) {
  m_isPanning = false;
}

//----------------------------------------------

void ItemListPanel::leaveEvent(QEvent*) {
  m_mousePos  = QPoint(-1, -1);
  m_isPanning = false;
  update();
}

//----------------------------------------------

void ItemListPanel::onAppend() {
  int layerIndex = qobject_cast<QAction*>(sender())->data().toInt();
  IoCmd::insertImagesToSequence(layerIndex);
}

//----------------------------------------------

void ItemListPanel::computeSize() {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  int rowCount = 0;
  for (int lay = 0; lay < prj->getLayerCount(); lay++) {
    IwLayer* layer = prj->getLayer(lay);
    if (layer) rowCount += layer->getRowCount();
  }
  int w = parentWidget()->width();

  int h = std::max(rowCount * RowHeight + 10, parentWidget()->height());

  setFixedSize(w, h);
}
//----------------------------------------------
// レイヤ上に移動
void ItemListPanel::onMoveLayerUpward() {
  int layerIndex = qobject_cast<QAction*>(sender())->data().toInt();
  ProjectUtils::SwapLayers(layerIndex - 1, layerIndex);
}
//----------------------------------------------
// レイヤ下に移動
void ItemListPanel::onMoveLayerDownward() {
  int layerIndex = qobject_cast<QAction*>(sender())->data().toInt();
  ProjectUtils::SwapLayers(layerIndex, layerIndex + 1);
}
//----------------------------------------------
// レイヤ削除
void ItemListPanel::onDeleteLayer() {
  int layerIndex = qobject_cast<QAction*>(sender())->data().toInt();
  ProjectUtils::DeleteLayer(layerIndex);
}
//----------------------------------------------
// レイヤ複製
void ItemListPanel::onCloneLayer() {
  int layerIndex = qobject_cast<QAction*>(sender())->data().toInt();
  ProjectUtils::CloneLayer(layerIndex);
}

//----------------------------------------------
// 右側のタイムライン
//----------------------------------------------
// 上のフレーム表示パネル
TimeLineHead::TimeLineHead(QWidget* parent)
    : ScrollPanel(parent), m_mousePos(-1, -1), m_prevDragTool(nullptr) {
  buildMarkerShapes();
  setFixedHeight(HeaderHeight);
  setMouseTracking(true);
  //  プレビュー情報が変わったら表示を更新する
  connect(IwApp::instance()->getCurrentProject(),
          SIGNAL(previewRenderFinished(int)), this, SLOT(update()));
  connect(IwApp::instance()->getCurrentProject(),
          SIGNAL(previewRenderStarted(int)), this, SLOT(update()));
  // キャッシュがInvalidateされたら表示を更新する
  connect(IwApp::instance()->getCurrentProject(),
          SIGNAL(previewCacheInvalidated()), this, SLOT(update()));
}

//----------------------------------------------

void TimeLineHead::paintEvent(QPaintEvent* e) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  OutputSettings* settings = prj->getOutputSettings();
  if (!settings) return;

  QSet<int> onionFrames = prj->getOnionFrames();
  int onionRange        = prj->getViewSettings()->getOnionRange();

  QPainter p(this);

  QRect visibleRect = e->rect();
  p.setClipRect(visibleRect);

  if (visibleRect.isEmpty()) return;

  QPen defaultPen(p.pen());

  p.setPen(Qt::black);
  p.setBrush(gray(55));
  p.drawRect(visibleRect);

  p.setPen(defaultPen);

  int mouseOverFrame = posToFrame(m_mousePos, m_frameWidth);

  int f0 = posToFrame(visibleRect.topLeft(), m_frameWidth);
  int f1 = posToFrame(visibleRect.topRight(), m_frameWidth);

  int frameLength  = prj->getProjectFrameLength();
  int currentFrame = prj->getViewFrame();

  QRect frameRect(0, 0, m_frameWidth, HeaderHeight - 1);
  QPoint onionTag[4] = {QPoint(m_frameWidth / 2 - 10, 0.0),
                        QPoint(m_frameWidth / 2 + 10, 0.0),
                        QPoint(m_frameWidth / 2, 5.0)};

  int prevFrom, prevTo;
  bool prevRangeSpecified = prj->getPreviewRange(prevFrom, prevTo);
  QColor rangeColor       = (prevRangeSpecified) ? Qt::white : Qt::darkGray;

  int initialFrame = settings->getInitialFrameNumber();
  int increment    = settings->getIncrement();

  p.translate(f0 * m_frameWidth, 0);
  for (int f = f0; f <= f1; f++) {
    if (f >= frameLength) p.setBrush(Qt::NoBrush);
    // 下を塗りつぶす
    else if (f == currentFrame)
      p.setBrush(QColor(100, 110, 255, 128));
    else if (f == mouseOverFrame)
      p.setBrush(QColor(255, 255, 220, 50));
    else
      p.setBrush(gray(64));
    p.setPen(Qt::black);
    p.drawRect(frameRect);

    // onion frame tag
    if (f < frameLength) {
      if (onionFrames.contains(f)) {
        p.setBrush(QColor(200, 200, 0, 128));
        p.setPen(Qt::NoPen);
        p.drawPolygon(onionTag, 3);
      } else if (onionRange > 0 && std::abs(f - currentFrame) <= onionRange &&
                 (onionRange < 50 || std::abs(f - currentFrame) % 2 == 0)) {
        double ratio =
            1. - (double)(std::abs(f - currentFrame)) / (double)onionRange;
        p.setBrush(QColor(200, 200, 0, 128));
        p.setPen(Qt::NoPen);
        p.drawRect(QRectF(0, 0, m_frameWidth, 5. * ratio));
      }
    }

    QFont font = p.font();
    // フレーム番号を書く
    if (f >= frameLength) {
      p.setPen(Qt::darkGray);
      font.setBold(false);
    } else {
      p.setPen(defaultPen);
      font.setBold(true);
    }
    p.setFont(font);

    p.drawText(frameRect, Qt::AlignCenter,
               QString::number(f * increment + initialFrame));

    // プレビュー完了のバーを表示
    if (f < frameLength) {
      bool isCached = IwTriangleCache::instance()->isFrameValid(f);
      bool isRunning =
          IwApp::instance()->getCurrentProject()->isRunningPreview(f);
      QRect barRect = frameRect;
      barRect.setTop(barRect.bottom() - 4);
      barRect.setBottom(barRect.bottom() - 1);
      p.fillRect(barRect, (isCached)    ? Qt::darkGreen
                          : (isRunning) ? Qt::darkYellow
                                        : Qt::darkRed);

      if (f == prevFrom) {
        bool onMarker =
            (f == mouseOverFrame && m_mousePos.x() % m_frameWidth < 8);
        p.setPen(Qt::black);
        p.setBrush((onMarker) ? QColor(200, 200, 128) : rangeColor);
        p.drawPath(prevFromMarker);
      }
      if (f == prevTo) {
        bool onMarker = (f == mouseOverFrame &&
                         m_mousePos.x() % m_frameWidth > m_frameWidth - 8);
        p.save();
        p.translate(frameRect.width() - 7, 0);
        p.setPen(Qt::black);
        p.setBrush((onMarker) ? QColor(200, 200, 128) : rangeColor);
        p.drawPath(prevToMarker);
        p.restore();
      }
    }

    p.translate(m_frameWidth, 0);
  }
}

//----------------------------------------------

void TimeLineHead::resizeEvent(QResizeEvent* /*e*/) { computeSize(); }

//----------------------------------------------

void TimeLineHead::mouseMoveEvent(QMouseEvent* e) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  QPoint pos = e->pos();

  if (m_isPanning) {
    doPan(QPoint(m_mousePos.x() - pos.x(), 0));
    return;
  }

  m_mousePos = pos;

  // 左ドラッグでカレントフレームを移動
  if (e->buttons() & Qt::LeftButton) {
    int mousePressedFrame = posToFrame(m_mousePos, m_frameWidth);
    // フレームのクランプ
    int frameLength = project->getProjectFrameLength();
    if (mousePressedFrame > frameLength - 1)
      mousePressedFrame = frameLength - 1;
    if (mousePressedFrame < 0) mousePressedFrame = 0;
    // プレビュー範囲の移動
    if (m_prevDragTool)
      m_prevDragTool->onDrag(mousePressedFrame);
    else
      project->setViewFrame(mousePressedFrame);
  }

  update();
}

//----------------------------------------------

void TimeLineHead::mousePressEvent(QMouseEvent* e) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  m_mousePos = e->pos();

  if (e->button() == Qt::MiddleButton) {
    m_isPanning = true;
    return;
  }

  int mousePressedFrame = posToFrame(m_mousePos, m_frameWidth);
  int prevFrom, prevTo;
  project->getPreviewRange(prevFrom, prevTo);

  // プレビュー範囲の移動
  if (e->button() == Qt::LeftButton) {
    assert(m_prevDragTool == nullptr);
    if (mousePressedFrame == prevFrom && m_mousePos.x() % m_frameWidth < 8)
      m_prevDragTool = new PreviewRangeDragTool(PreviewRangeDragTool::START);
    else if (mousePressedFrame == prevTo &&
             m_mousePos.x() % m_frameWidth > m_frameWidth - 8)
      m_prevDragTool = new PreviewRangeDragTool(PreviewRangeDragTool::END);
    if (m_prevDragTool) {
      // 中でundo作成
      m_prevDragTool->onClick(mousePressedFrame);
      return;
    }
  }

  // フレームのクランプ
  int frameLength = project->getProjectFrameLength();
  if (mousePressedFrame > frameLength - 1) mousePressedFrame = frameLength - 1;

  project->setViewFrame(mousePressedFrame);

  update();
}

//----------------------------------------------

void TimeLineHead::mouseReleaseEvent(QMouseEvent* e) {
  if (m_prevDragTool && e->button() == Qt::LeftButton) {
    m_prevDragTool->onRelease();
    delete m_prevDragTool;
    m_prevDragTool = nullptr;
  }
  m_isPanning = false;
}

//----------------------------------------------

void TimeLineHead::leaveEvent(QEvent*) {
  m_mousePos  = QPoint(-1, -1);
  m_isPanning = false;
  update();
}

//----------------------------------------------

void TimeLineHead::contextMenuEvent(QContextMenuEvent* e) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  int mousePressedFrame = posToFrame(m_mousePos, m_frameWidth);

  if (mousePressedFrame < 0 ||
      mousePressedFrame >= prj->getProjectFrameLength())
    return;

  int prevFrom, prevTo;
  bool isSpecified = prj->getPreviewRange(prevFrom, prevTo);

  QMenu menu(this);

  if (mousePressedFrame != prevFrom && mousePressedFrame <= prevTo) {
    QAction* action = menu.addAction(tr("Set Start Marker"));
    action->setData(QPoint(mousePressedFrame, -1));
    connect(action, SIGNAL(triggered()), SLOT(onPreviewMarkerCommand()));
  }
  if (mousePressedFrame != prevTo && mousePressedFrame >= prevFrom) {
    QAction* action = menu.addAction(tr("Set Stop Marker"));
    action->setData(QPoint(-1, mousePressedFrame));
    connect(action, SIGNAL(triggered()), SLOT(onPreviewMarkerCommand()));
  }
  if (isSpecified) {
    QAction* action = menu.addAction(tr("Remove Markers"));
    action->setData(QPoint(-1, -1));
    connect(action, SIGNAL(triggered()), SLOT(onPreviewMarkerCommand()));
  }
  if (mousePressedFrame != prevTo || mousePressedFrame != prevFrom) {
    QAction* action = menu.addAction(tr("Preview This"));
    action->setData(QPoint(mousePressedFrame, mousePressedFrame));
    connect(action, SIGNAL(triggered()), SLOT(onPreviewMarkerCommand()));
  }

  if (!menu.isEmpty()) menu.addSeparator();
  QAction* action = menu.addAction(tr("Toggle Onion Skin Frame"));
  action->setData(mousePressedFrame);
  connect(action, SIGNAL(triggered()), SLOT(onToggleShapeReferenceFrame()));

  if (menu.actions().isEmpty()) return;

  menu.exec(e->globalPos());
}

//----------------------------------------------
void TimeLineHead::computeSize() {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  int w = std::max((prj->getProjectFrameLength() + 1) * m_frameWidth,
                   parentWidget()->width());

  int h = parentWidget()->height();

  setFixedSize(w, h);
}

//----------------------------------------------
void TimeLineHead::onPreviewMarkerCommand() {
  QAction* action = qobject_cast<QAction*>(sender());
  if (!action) return;
  QPoint p = action->data().toPoint();
  ProjectUtils::setPreviewRange(p.x(), p.y());
  update();
}

//----------------------------------------------
void TimeLineHead::onToggleShapeReferenceFrame() {
  QAction* action = qobject_cast<QAction*>(sender());
  if (!action) return;
  int f = action->data().toInt();

  ProjectUtils::toggleOnionSkinFrame(f);
  update();
}

//----------------------------------------------
void TimeLineHead::wheelEvent(QWheelEvent* event) {
  // とりあえずマウスホイールだけ実装
  if (event->source() != Qt::MouseEventNotSynthesized) return;
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  int frameDelta  = event->angleDelta().y() < 0 ? 1 : -1;
  int newFrame    = prj->getViewFrame() + frameDelta;
  int frameLength = prj->getProjectFrameLength();
  // clamp
  if (newFrame < 0)
    newFrame = 0;
  else if (newFrame >= frameLength)
    newFrame = frameLength - 1;

  prj->setViewFrame(newFrame);

  event->accept();
}

//----------------------------------------------
//----------------------------------------------
//----------------------------------------------
// パネル
TimeLinePanel::TimeLinePanel(QWidget* parent)
    : ScrollPanel(parent)
    , m_mousePos(-1, -1)
    , m_timeLineSelection(IwTimeLineSelection::instance())
    , m_clickedFrameIndex(-1)
    , m_isFrameClicked(false) {
  setFrameStyle(QFrame::StyledPanel);
  setMouseTracking(true);

  // プレビュー情報が変わったら表示を更新する
  connect(IwApp::instance()->getCurrentProject(),
          SIGNAL(previewRenderFinished(int)), this, SLOT(update()));
  connect(IwApp::instance()->getCurrentProject(),
          SIGNAL(previewRenderStarted(int)), this, SLOT(update()));
  // キャッシュがInvalidateされたら表示を更新する
  connect(IwApp::instance()->getCurrentProject(),
          SIGNAL(previewCacheInvalidated()), this, SLOT(update()));
}

//----------------------------------------------
void TimeLinePanel::paintEvent(QPaintEvent* e) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  QPainter p(this);
  int layerCount = prj->getLayerCount();
  if (layerCount == 0) {
    p.drawText(QPoint(5, 25), tr("- No Layers -"));
    return;
  }

  QRect visibleRect = e->rect();
  p.setClipRect(visibleRect);

  if (visibleRect.isEmpty()) return;

  p.fillRect(visibleRect, QColor(64, 64, 64, 255));

  int r0 = posToRow(visibleRect.topLeft());
  int r1 = posToRow(visibleRect.bottomLeft());

  int lay0 = rowToLayerIndex(r0);
  int lay1 = rowToLayerIndex(r1);
  // lay1が-1のとき、最後までレイヤを描画
  if (lay1 == -1) {
    if (lay0 == -1)
      return;
    else
      lay1 = layerCount - 1;
  }

  int f0 = posToFrame(visibleRect.topLeft(), m_frameWidth);
  int f1 = posToFrame(visibleRect.topRight(), m_frameWidth);

  // 描画を始める行
  int currentRow = layerIndexToRow(lay0);
  // マウスがある行
  int mouseOverRow = posToRow(m_mousePos);
  // マウスの横位置
  double mouseOverFrameD = posToFrameD(m_mousePos, m_frameWidth);

  // std::cout<<"mouseOverRow = "<<mouseOverRow<<std::endl;

  for (int lay = lay0; lay <= lay1; lay++) {
    IwLayer* layer = prj->getLayer(lay);
    if (layer) {
      // 各レイヤの描画
      layer->drawTimeLine(p, rowToVPos(layerIndexToRow(lay)),
                          visibleRect.width(), f0, f1, m_frameWidth, RowHeight,
                          currentRow, mouseOverRow, mouseOverFrameD,
                          m_timeLineSelection);
    }
  }
}

void TimeLinePanel::resizeEvent(QResizeEvent*) { computeSize(); }

//----------------------------------------------

void TimeLinePanel::mouseMoveEvent(QMouseEvent* e) {
  QPoint pos = e->pos();
  setCursor(Qt::ArrowCursor);
  update();

  // 現在のプロジェクトを取得
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  if (m_isPanning) {
    doPan(m_mousePos - pos);
    return;
  }

  m_mousePos = pos;

  int row        = posToRow(m_mousePos);
  int layerIndex = rowToLayerIndex(row);
  if (layerIndex < 0) return;

  IwLayer* layer = project->getLayer(layerIndex);
  if (!layer) return;

  bool isBorder;
  int frameIndex = xPosToFrameIndexAndBorder(e->pos().x(), isBorder);

  // マウスオーバーの場合
  if (e->buttons() == Qt::NoButton) {
    // レイヤ以外の行をマウスオーバーした場合
    if (row != layerIndexToRow(layerIndex)) return;
    if (e->modifiers() & Qt::ShiftModifier) return;
    if (layer->onMouseMove(frameIndex, isBorder)) setCursor(Qt::SplitHCursor);

    QString path = layer->getImageFilePath(frameIndex);
    // ここで、pathに空のStringが入ることもあるが、かまわずにstatusBarに投げる
    IwApp::instance()->showMessageOnStatusBar(path);
    return;
  }

  // ドラッグの場合
  if ((e->buttons() & Qt::LeftButton) != 0) {
    // キーフレームのドラッグ
    if (KeyDragTool::instance()->isDragging()) {
      KeyDragTool::instance()->onMove(frameIndex);
      return;
    }

    // 補間ハンドルのドラッグ
    if (InterpHandleDragTool::instance()->isDragging()) {
      // +Ctrlで 0.5フレーム毎にスナップする
      bool doSnap = e->modifiers() & Qt::ControlModifier;
      InterpHandleDragTool::instance()->onMove(
          posToFrameD(m_mousePos, m_frameWidth), doSnap);
      return;
    }

    // もし、ロール選択がされていない場合はreturn
    if (IwSelection::getCurrent() != m_timeLineSelection) return;

    // もし、前回クリック位置が無かった場合(-1の場合)はreturn
    if (m_clickedFrameIndex < 0) return;

    if (m_timeLineExtender) {
      m_timeLineExtender->onDrag(frameIndex);
      return;
    }

    // もしフレーム長より先までドラッグした場合、frameIndexを補正
    if (frameIndex >= layer->getFrameLength()) {
      frameIndex = layer->getFrameLength();
      isBorder   = true;
    }

    bool selectionChanged;

    // 同じフレームをドラッグ中なら、!isBorderとm_isFrameClickedのORを取って選択
    if (frameIndex == m_clickedFrameIndex)
      selectionChanged = m_timeLineSelection->selectFrames(
          frameIndex, frameIndex, (!isBorder | m_isFrameClicked));
    // 左にドラッグした場合
    else if (frameIndex < m_clickedFrameIndex)
      selectionChanged = m_timeLineSelection->selectFrames(
          frameIndex, m_clickedFrameIndex, m_isFrameClicked);
    // 右にドラッグした場合
    else  //(frameIndex > m_clickedFrameIndex)
      selectionChanged = m_timeLineSelection->selectFrames(
          m_clickedFrameIndex, frameIndex, !isBorder);

    // もし選択範囲が変わったら更新する
    if (selectionChanged) {
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      update();
    }
  }
}

//----------------------------------
// マウスクリックで選択をやりくり
//----------------------------------
void TimeLinePanel::mousePressEvent(QMouseEvent* e) {
  // std::cout << "mousePress" << std::endl;
  // 現在のプロジェクトを取得
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // 左ボタンの場合、ロール選択
  if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton) {
    // クリックしたレイヤはどこかしら
    int row        = posToRow(m_mousePos);
    int layerIndex = rowToLayerIndex(row);
    if (layerIndex < 0) return;

    IwLayer* layer = project->getLayer(layerIndex);
    if (!layer) return;

    // カレントレイヤを切り替え。すでにカレントの場合はfalseが返る
    bool currentLayerChanged =
        IwApp::instance()->getCurrentLayer()->setLayer(layer);

    // クリックしたフレームはどれかしら
    bool isBorder;
    int frameIndex = xPosToFrameIndexAndBorder(e->pos().x(), isBorder);
    double frameD  = posToFrameD(e->pos(), m_frameWidth);

    // ここで、レイヤのRowをクリックしたか、その下のシェイプのRowをクリックしたかで処理を分ける
    // レイヤ以外の行をクリックした場合
    if (row != layerIndexToRow(layerIndex)) {
      bool doUpdate = layer->onTimeLineClick(
          row - layerIndexToRow(layerIndex), frameD,
          (e->modifiers() & Qt::ControlModifier) != 0,
          (e->modifiers() & Qt::ShiftModifier) != 0, e->button());
      if (doUpdate) update();
      return;
    }

    // 以下、レイヤの行をクリックした場合

    if (layer->isLocked()) {
      m_timeLineSelection->selectNone();
      return;
    }  // 何かポップアップ出す？

    // IwTimeLineSelectionをカレントに
    m_timeLineSelection->makeCurrent();

    // frameIndexがsequence長を超えた場合は、端っこの境界を選択したことに補正
    if (frameIndex >= layer->getFrameLength()) {
      frameIndex = layer->getFrameLength();
      isBorder   = true;
    }

    bool selectionChanged;

    //---
    // Shift+クリック かつ、前にクリックしたのと同じロールをクリックした かつ、
    // 選択が空じゃない場合、選択範囲を伸ばす
    if (e->modifiers() & Qt::ShiftModifier && !currentLayerChanged &&
        !m_timeLineSelection->isEmpty()) {
      // 選択範囲を得る
      int start, end;
      bool rightFrameIncluded;

      m_timeLineSelection->getRange(start, end, rightFrameIncluded);

      // 既存の範囲の左側をShift+クリック
      if (frameIndex < start) start = frameIndex;
      // 既存の範囲のお尻をクリック
      else if (frameIndex == end)
        rightFrameIncluded = (rightFrameIncluded | !isBorder);
      // 既存の範囲の右をクリック
      else if (frameIndex > end) {
        end                = frameIndex;
        rightFrameIncluded = !isBorder;
      }
      // それ以外の場合は、既存の範囲の中をクリック(範囲の変更なし)
      //  else{}

      // 選択範囲を更新
      selectionChanged =
          m_timeLineSelection->selectFrames(start, end, rightFrameIncluded);

    }
    //--- シングルクリックの場合
    else {
      // 右クリックで、選択範囲内のクリックの場合は何もしない
      if (!isBorder && m_timeLineSelection->isFrameSelected(frameIndex))
        // if ((isBorder && m_timeLineSelection->isBorderSelected(frameIndex))
        // ||
        //   (!isBorder && m_timeLineSelection->isFrameSelected(frameIndex)))
        return;
      // IwSequenceSelectionをいったん解除
      m_timeLineSelection->selectNone();
      // 境界クリックの場合
      if (isBorder) {
        if (frameIndex >= 0 && layer->getImageFileName(frameIndex) !=
                                   layer->getImageFileName(frameIndex - 1)) {
          if (m_timeLineExtender) delete m_timeLineExtender;
          m_timeLineExtender = new TimeLineExtender(layer, frameIndex);
          m_timeLineExtender->onClick();
        } else
          m_timeLineSelection->selectBorder(frameIndex);
      }
      // フレームクリックの場合
      else
        m_timeLineSelection->selectFrame(frameIndex);

      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();

      selectionChanged = true;
    }

    // ドラッグに備え、選択位置を保存
    m_clickedFrameIndex = frameIndex;
    m_isFrameClicked    = !isBorder;

    // 選択範囲が更新されていたら、更新
    if (selectionChanged) update();
  } else if (e->button() == Qt::MiddleButton) {
    m_mousePos  = e->pos();
    m_isPanning = true;
  }
}

//----------------------------------
// 前回のクリック位置情報をクリアする
//----------------------------------
void TimeLinePanel::mouseReleaseEvent(QMouseEvent* /*e*/) {
  m_clickedFrameIndex = -1;
  m_isFrameClicked    = false;
  m_isPanning         = false;
  if (m_timeLineExtender) {
    m_timeLineExtender->onRelease();
    delete m_timeLineExtender;
    m_timeLineExtender = nullptr;
  }
  if (KeyDragTool::instance()->isDragging())
    KeyDragTool::instance()->onRelease();
  else if (InterpHandleDragTool::instance()->isDragging())
    InterpHandleDragTool::instance()->onRelease();
}

void TimeLinePanel::leaveEvent(QEvent* /*e*/) {
  m_clickedFrameIndex = -1;
  m_isFrameClicked    = false;
  m_isPanning         = false;
  m_mousePos          = QPoint(-1, -1);
  update();
  IwApp::instance()->showMessageOnStatusBar("");
}
//----------------------------------
// キーの設定、解除
//----------------------------------
void TimeLinePanel::contextMenuEvent(QContextMenuEvent* e) {
  // std::cout << "contextMenu" << std::endl;
  // 現在のプロジェクトを取得
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  IwTimeLineKeySelection* keySelection = dynamic_cast<IwTimeLineKeySelection*>(
      IwApp::instance()->getCurrentSelection()->getSelection());
  IwTimeLineSelection* frameSelection = dynamic_cast<IwTimeLineSelection*>(
      IwApp::instance()->getCurrentSelection()->getSelection());

  if (!keySelection && !frameSelection) return;
  if (keySelection && keySelection->isEmpty()) return;
  if (frameSelection && frameSelection->isEmpty()) return;

  int row        = posToRow(m_mousePos);
  int layerIndex = rowToLayerIndex(row);
  IwLayer* layer = project->getLayer(layerIndex);
  if (layer && layer->isLocked()) {
    QMenu menu(this);
    menu.addAction(CommandManager::instance()->getAction(MI_FileInfo));
    menu.exec(e->globalPos());
    return;
  }

  QMenu menu(this);  // マウスがある行

  if (keySelection) {
    menu.addAction(CommandManager::instance()->getAction(MI_Key));
    menu.addAction(CommandManager::instance()->getAction(MI_Unkey));
    menu.addAction(
        CommandManager::instance()->getAction(MI_ResetInterpolation));
  } else if (frameSelection) {
    menu.addAction(CommandManager::instance()->getAction(MI_InsertEmpty));
    if (!frameSelection->isEmpty()) {
      menu.addAction(CommandManager::instance()->getAction(MI_ReplaceImages));
      menu.addAction(CommandManager::instance()->getAction(MI_ReloadImages));
    }
    menu.addSeparator();
    menu.addAction(CommandManager::instance()->getAction(MI_FileInfo));
  }

  menu.exec(e->globalPos());
}

//----------------------------------------------
// ツールチップでファイル名の表示など
//----------------------------------------------

bool TimeLinePanel::event(QEvent* e) {
  if (e->type() == QEvent::ToolTip) {
    IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
    if (!prj) return QFrame::event(e);
    if (prj->getLayerCount() == 0) return QFrame::event(e);

    QHelpEvent* helpEvent = dynamic_cast<QHelpEvent*>(e);
    QString toolTip;
    QPoint pos = helpEvent->pos();

    int row = posToRow(pos);
    int lay = rowToLayerIndex(row);

    if (lay == -1) return QFrame::event(e);

    int frame = posToFrame(pos, m_frameWidth);

    IwLayer* layer = prj->getLayer(lay);

    if (!layer) return QFrame::event(e);

    toolTip = layer->getImageFileName(frame);
    if (toolTip != "")
      QToolTip::showText(helpEvent->globalPos(), toolTip);
    else
      QToolTip::hideText();
    e->accept();
  }
  return QFrame::event(e);
}

//----------------------------------------------

void TimeLinePanel::computeSize() {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  int rowCount = 0;
  for (int lay = 0; lay < prj->getLayerCount(); lay++) {
    IwLayer* layer = prj->getLayer(lay);
    if (layer) rowCount += layer->getRowCount();
  }

  int w = std::max((prj->getProjectFrameLength() + 1) * m_frameWidth,
                   parentWidget()->width());

  int h = std::max(rowCount * RowHeight + 10, parentWidget()->height());

  setFixedSize(w, h);
}

//----------------------------------------------
// マウスクリック用：横位置からフレームインデックスを返す。
// 境界の近くの場合はisBorderフラグがtrueになる。
//----------------------------------------------
int TimeLinePanel::xPosToFrameIndexAndBorder(int xPos, bool& isBorder) {
  int margin     = 3;  // 境界から3ピクセル左右は境界をクリック扱い
  int frameIndex = xPos / m_frameWidth;
  int posInFrame = xPos % m_frameWidth;
  // 3+margin以下は境界をクリック扱い
  if (posInFrame <= 3 + margin) isBorder = true;
  // 88-margin以上は次のフレームの境界をクリック扱い
  else if (posInFrame >= m_frameWidth - margin) {
    isBorder = true;
    frameIndex++;
  }
  // それ以外はフレームをクリック扱い
  else
    isBorder = false;

  return frameIndex;
}

//----------------------------------------------
// タイムラインウィンドウ
//----------------------------------------------

TimeLineWindow::TimeLineWindow(QWidget* parent)
    : QDockWidget(tr("Timeline"), parent) {
  setObjectName("TimeLineWindow");
  // The dock widget cannot be closed
  setFeatures(QDockWidget::DockWidgetMovable |
              QDockWidget::DockWidgetFloatable);
  // setTitleBarWidget(new QWidget(this));
  // オブジェクトの宣言
  m_itemListHead            = new ItemListHead(this);
  QScrollArea* itemListArea = new QScrollArea(this);
  m_itemListPanel           = new ItemListPanel(this);

  QScrollArea* timeLineHeadArea = new QScrollArea(this);
  m_timeLineHead                = new TimeLineHead(this);
  QScrollArea* timeLineArea     = new QScrollArea(this);
  m_timeLinePanel               = new TimeLinePanel(this);
  QFrame* frame                 = new QFrame(this);

  // プロパティの設定
  m_itemListHead->setViewSize(0);  // 0:小 1:大

  itemListArea->setWidget(m_itemListPanel);
  itemListArea->setFixedWidth(200);
  itemListArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  itemListArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_itemListPanel->setViewer(itemListArea);

  timeLineHeadArea->setFixedHeight(HeaderHeight);
  timeLineHeadArea->setWidget(m_timeLineHead);
  timeLineHeadArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  timeLineHeadArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_timeLineHead->setViewer(timeLineHeadArea);

  timeLineArea->setWidget(m_timeLinePanel);
  timeLineArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  timeLineArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_timeLinePanel->setViewer(timeLineArea);

  // レイアウト
  QGridLayout* mainLay = new QGridLayout();
  mainLay->setMargin(1);
  mainLay->setHorizontalSpacing(2);
  mainLay->setVerticalSpacing(1);
  {
    mainLay->addWidget(m_itemListHead, 0, 0);

    QVBoxLayout* itemListLay = new QVBoxLayout();
    itemListLay->setSpacing(0);
    itemListLay->setMargin(0);
    {
      itemListLay->addWidget(itemListArea, 1);
      itemListLay->addSpacing(17);  // 下のスクロールバー分
    }
    mainLay->addLayout(itemListLay, 1, 0);

    QHBoxLayout* timeLineHeadLay = new QHBoxLayout();
    timeLineHeadLay->setSpacing(0);
    timeLineHeadLay->setMargin(0);
    {
      timeLineHeadLay->addWidget(timeLineHeadArea, 1);
      timeLineHeadLay->addSpacing(17);  // 右のスクロールバー分
    }
    mainLay->addLayout(timeLineHeadLay, 0, 1);

    mainLay->addWidget(timeLineArea, 1, 1);
  }
  mainLay->setColumnStretch(0, 0);
  mainLay->setColumnStretch(1, 1);
  mainLay->setRowStretch(0, 0);
  mainLay->setRowStretch(1, 1);

  frame->setLayout(mainLay);

  setWidget(frame);

  // シグナル／スロット接続
  // 全てのパネルのアップデート
  connect(m_itemListPanel, SIGNAL(updateUIs()), this, SLOT(onUpdateUIs()));
  connect(m_timeLinePanel, SIGNAL(updateUIs()), this, SLOT(onUpdateUIs()));
  // スクロールバーの同期
  // 縦方向
  connect(itemListArea->verticalScrollBar(), SIGNAL(valueChanged(int)),
          timeLineArea->verticalScrollBar(), SLOT(setValue(int)));
  connect(timeLineArea->verticalScrollBar(), SIGNAL(valueChanged(int)),
          itemListArea->verticalScrollBar(), SLOT(setValue(int)));
  // 横方向
  connect(timeLineHeadArea->horizontalScrollBar(), SIGNAL(valueChanged(int)),
          timeLineArea->horizontalScrollBar(), SLOT(setValue(int)));
  connect(timeLineArea->horizontalScrollBar(), SIGNAL(valueChanged(int)),
          timeLineHeadArea->horizontalScrollBar(), SLOT(setValue(int)));
  // もろもろシーンが変更されたら更新
  IwApp* app = IwApp::instance();
  connect(app->getCurrentProject(), SIGNAL(projectSwitched()), this,
          SLOT(onUpdateUIs()));
  connect(app->getCurrentProject(), SIGNAL(projectChanged()), this,
          SLOT(onUpdateUIs()));
  connect(app->getCurrentProject(), SIGNAL(viewFrameChanged()), this,
          SLOT(onCurrentFrameChanged()));
  connect(app->getCurrentLayer(), SIGNAL(layerSwitched()), this,
          SLOT(onUpdateUIs()));
  connect(app->getCurrentLayer(), SIGNAL(layerChanged(IwLayer*)), this,
          SLOT(onUpdateUIs()));
  connect(app->getCurrentSelection(), SIGNAL(selectionChanged(IwSelection*)),
          this, SLOT(onSelectionChanged(IwSelection*)));

  // itemListHeadでサイズが変更されたら更新する
  connect(m_itemListHead, SIGNAL(viewSizeChanged(int)), this,
          SLOT(onViewSizeChanged(int)));
}

void TimeLineWindow::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (m_itemListPanel) m_itemListPanel->computeSize();
  if (m_timeLinePanel) m_timeLinePanel->computeSize();
  if (m_timeLineHead) m_timeLineHead->computeSize();
}

// 全てのパネルのアップデート
void TimeLineWindow::onUpdateUIs() {
  if (m_itemListPanel) {
    m_itemListPanel->computeSize();
    m_itemListPanel->repaint();
  }
  if (m_timeLinePanel) {
    m_timeLinePanel->computeSize();
    m_timeLinePanel->repaint();
  }
  if (m_timeLineHead) {
    m_timeLineHead->computeSize();
    m_timeLineHead->repaint();
  }
}

// フレーム横幅サイズが変化したとき
void TimeLineWindow::onViewSizeChanged(int sizeIndex) {
  if (m_timeLinePanel) {
    m_timeLinePanel->setViewSize(sizeIndex);
    m_timeLinePanel->computeSize();
    m_timeLinePanel->repaint();
  }
  if (m_timeLineHead) {
    m_timeLineHead->setViewSize(sizeIndex);
    m_timeLineHead->computeSize();
    m_timeLineHead->repaint();
  }
}

// 選択されている、またはピンされているシェイプを開く

void TimeLineWindow::onSelectionChanged(IwSelection* selection) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  //  shapePairSelectionのときのみ操作
  IwShapePairSelection* shapePairSelection =
      dynamic_cast<IwShapePairSelection*>(selection);
  if (!shapePairSelection) {
    onUpdateUIs();
    return;
  }

  // Selectedのとき：Pinnedまたは選択されたシェイプを表示。表示シェイプの有無によってレイヤを展開切り替え
  if (Preferences::instance()->isShowSelectedShapesOnly()) {
    // すべてのシェイプを走査
    for (int lay = 0; lay < project->getLayerCount(); lay++) {
      IwLayer* layer  = project->getLayer(lay);
      bool shapeFound = false;
      for (int s = 0; s < layer->getShapePairCount(); s++) {
        ShapePair* shapePair = layer->getShapePair(s);
        if (!shapePair) continue;
        for (int fromTo = 0; fromTo < 2; fromTo++) {
          if (shapePair->isPinned(fromTo) ||
              shapePairSelection->isSelected(OneShape(shapePair, fromTo))) {
            shapePair->setExpanded(fromTo, true);
            shapeFound = true;
          } else
            shapePair->setExpanded(fromTo, false);
        }
      }
      layer->setExpanded(shapeFound);
    }
  }
  // Allのとき：選択されたシェイプがあるレイヤを展開。レイヤを閉じない
  else {
    for (int lay = 0; lay < project->getLayerCount(); lay++) {
      IwLayer* layer = project->getLayer(lay);
      // すでにExpandしているレイヤは飛ばす
      if (layer->isExpanded()) continue;
      bool shapeFound = false;
      for (int s = 0; s < layer->getShapePairCount(); s++) {
        ShapePair* shapePair = layer->getShapePair(s);
        if (!shapePair) continue;
        for (int fromTo = 0; fromTo < 2; fromTo++) {
          if (shapePairSelection->isSelected(OneShape(shapePair, fromTo))) {
            shapeFound = true;
            break;
          }
        }
        if (shapeFound) break;
      }
      if (shapeFound) layer->setExpanded(true);
    }
  }

  onUpdateUIs();
}

void TimeLineWindow::onCurrentFrameChanged() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  int currentFrame = project->getViewFrame();
  m_timeLineHead->scrollToFrame(currentFrame);
  m_timeLinePanel->scrollToFrame(currentFrame);
  m_timeLineHead->update();
}
