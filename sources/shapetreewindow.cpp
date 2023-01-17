#include "shapetreewindow.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwlayerhandle.h"
#include "iwselectionhandle.h"
#include "iwproject.h"
#include "viewsettings.h"

#include "iwlayer.h"
#include "iwshapepairselection.h"
#include "projectutils.h"
#include "iwundomanager.h"

#include <QHeaderView>
#include <QTreeWidgetItem>
#include <QPainter>
#include <QItemSelectionModel>
#include <QItemSelection>
#include <QMouseEvent>
#include <QProxyStyle>
#include <QMenu>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QColorDialog>
#include <QScrollBar>
#include <QApplication>

namespace {

const QColor layerBgColor(83, 99, 119);
const QColor lockedLayerBgColor(80, 80, 80);
const QColor parentShapeBgColor(50, 81, 51);
const QColor childShapeBgColor(69, 93, 84);
const QColor fromBgColor(100, 60, 50);
const QColor toBgColor(50, 60, 110);
const QColor selectedColor(128, 128, 250, 100);
const QColor hoverColor(220, 220, 128, 75);

}  // namespace

//=============================================================================
// ShapeTagAction
//-----------------------------------------------------------------------------

class ShapeTagActionWidget : public QWidget {
  QCheckBox* m_chkBox;

public:
  ShapeTagActionWidget(QCheckBox* chkBox, const QIcon& icon,
                       const QString& name, QWidget* parent = nullptr)
      : QWidget(parent), m_chkBox(chkBox) {
    QLabel* iconLabel = new QLabel();
    iconLabel->setPixmap(icon.pixmap(12, 12));
    iconLabel->setFixedSize(12, 12);

    setObjectName("ShapeTagActionWidget");
    QHBoxLayout* lay = new QHBoxLayout();
    lay->setMargin(1);
    lay->setSpacing(3);
    {
      lay->addWidget(chkBox, 0);
      lay->addWidget(iconLabel, 0);
      lay->addWidget(new QLabel(name), 0);
      lay->addSpacing(10);
      lay->addStretch(1);
    }
    setLayout(lay);
  }

protected:
  void mousePressEvent(QMouseEvent* e) override {
    if (e->button() == Qt::LeftButton) {
      m_chkBox->click();
      e->accept();
      return;
    } else
      QWidget::mousePressEvent(e);
  }
};

// Tristateなチェックボックス＋アイコン＋タグ名
ShapeTagAction::ShapeTagAction(const QIcon& icon, const QString& name,
                               const Qt::CheckState checkState, QWidget* parent)
    : QWidgetAction(parent) {
  m_chkBox = new QCheckBox();
  m_chkBox->setTristate(true);
  m_chkBox->setCheckState(checkState);

  setDefaultWidget(new ShapeTagActionWidget(m_chkBox, icon, name));

  connect(m_chkBox, SIGNAL(clicked(bool)), this, SLOT(onCheckBoxClicked()));
}

void ShapeTagAction::onCheckBoxClicked() {
  // skip the intermediate state
  if (m_chkBox->checkState() == Qt::PartiallyChecked)
    m_chkBox->setCheckState(Qt::Checked);

  emit stateChanged(m_chkBox->checkState());
}

//=============================================================================
// MyProxyStyle
//-----------------------------------------------------------------------------

class MyProxyStyle : public QProxyStyle {
public:
  MyProxyStyle(QStyle* baseStyle = nullptr) : QProxyStyle(baseStyle) {}

  void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                     QPainter* painter, const QWidget* widget) const {
    if (element == QStyle::PE_IndicatorItemViewItemDrop) {
      painter->setRenderHint(QPainter::Antialiasing, true);

      QPalette palette;
      QColor c(palette.highlightedText().color());
      QPen pen(c);
      pen.setWidth(2);
      c.setAlpha(50);
      QBrush brush(c);

      painter->setPen(pen);
      painter->setBrush(brush);

      QRect rowRect = option->rect;
      rowRect.setLeft(0);
      rowRect.setRight(widget->rect().right());

      if (rowRect.height() == 0) {
        painter->drawEllipse(rowRect.topLeft(), 3, 3);
        painter->drawLine(
            QPoint(rowRect.topLeft().x() + 3, rowRect.topLeft().y()),
            rowRect.topRight());
      } else {
        painter->drawRoundedRect(rowRect, 5, 5);
      }
    } else {
      QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
  }
};

//=============================================================================
// ShapeTreeDelegate
//-----------------------------------------------------------------------------

void ShapeTreeDelegate::paint(QPainter* painter,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
  const ShapeTree* tree = qobject_cast<const ShapeTree*>(option.widget);
  if (!tree) return;
  OneShape shape = tree->shapeFromIndex(index);
  IwLayer* layer = tree->layerFromIndex(index);
  if (!layer) return;
  bool layerIsLocked = layer->isLocked();

  // レイヤーの場合
  if (!shape.shapePairP) {
    painter->setPen(Qt::black);
    painter->setBrush(layerIsLocked ? lockedLayerBgColor : layerBgColor);
    painter->drawRect(option.rect);

    if (option.state & QStyle::State_MouseOver) {
      // only the unlock button is enabled when locked
      if (!layerIsLocked || m_hoverOn == HoverOnLayerLockButton) {
        if (m_hoverOn >= HoverOnLayerVisibleButton &&
            m_hoverOn <= HoverOnLayerLockButton) {
          int offset = (m_hoverOn - HoverOnLayerVisibleButton) * 18;
          QRect hoverRect(option.rect.left() + offset, option.rect.top(), 18,
                          18);
          painter->fillRect(hoverRect, hoverColor);
        } else
          painter->fillRect(option.rect, hoverColor);
      }
    }

    // 表示／非表示ボタン
    IwLayer::LayerVisibility visibleInViewer = layer->isVisibleInViewer();
    painter->drawPixmap(option.rect.left() + 2, option.rect.top() + 2, 14, 14,
                        (visibleInViewer == IwLayer::Invisible)
                            ? QPixmap(":Resources/visible_off.svg")
                            : ((visibleInViewer == IwLayer::HalfVisible)
                                   ? QPixmap(":Resources/visible_half.svg")
                                   : QPixmap(":Resources/visible_on.svg")));
    if (layerIsLocked) {
      painter->fillRect(QRect(option.rect.left(), option.rect.top(), 18, 18),
                        QColor(80, 80, 80, 128));
    }

    bool isVisibleInRender = layer->isVisibleInRender();
    painter->drawPixmap(
        option.rect.left() + 18 + 2, option.rect.top() + 2, 14, 14,
        (isVisibleInRender) ? QPixmap(":Resources/render_on.svg")
                            : QPixmap(":Resources/render_off.svg"));
    if (layerIsLocked) {
      painter->fillRect(
          QRect(option.rect.left() + 18, option.rect.top(), 18, 18),
          QColor(80, 80, 80, 128));
    }

    painter->drawPixmap(option.rect.left() + 36, option.rect.top(), 18, 18,
                        (layerIsLocked) ? QPixmap(":Resources/lock_on.svg")
                                        : QPixmap(":Resources/lock.svg"));

    QRect textRect = option.rect;
    textRect.setLeft(textRect.left() + 56);

    bool isCurrent = IwApp::instance()->getCurrentLayer()->getLayer() == layer;
    painter->setPen((isCurrent)       ? Qt::yellow
                    : (layerIsLocked) ? Qt::lightGray
                                      : Qt::white);
    painter->drawText(textRect, layer->getName());
    return;
  }

  QColor bgColor;
  switch (index.column()) {
  case 0:
    bgColor =
        shape.shapePairP->isParent() ? parentShapeBgColor : childShapeBgColor;
    break;
  case 1:
    bgColor = fromBgColor;
    break;
  case 2:
    bgColor = toBgColor;
    break;
  default:
    break;
  }

  painter->setPen(Qt::black);
  painter->setBrush(bgColor);
  painter->drawRect(option.rect);
  if (layerIsLocked) {
    painter->fillRect(option.rect,
                      QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
  }

  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect, selectedColor);
    if (index.column() != 0) {
      if (m_hoverOn == HoverOnLockButton_SelectedShapes) {
        QRect lockRect(option.rect.left(), option.rect.top(), 18, 18);
        painter->fillRect(lockRect, hoverColor);
      } else if (m_hoverOn == HoverOnPinButton_SelectedShapes) {
        QRect pinRect(option.rect.left() + 18, option.rect.top(), 18, 18);
        painter->fillRect(pinRect, hoverColor);
      }
    } else {  // drawing column
      if (m_hoverOn == HoverOnVisibleButton_SelectedShapes) {
        QRect visibleRect(option.rect.left() + 18, option.rect.top(), 18, 18);
        painter->fillRect(visibleRect, hoverColor);
      }
    }
  }
  if (option.state & QStyle::State_MouseOver) {
    if (index.column() != 0) {
      if (m_hoverOn == HoverOnLockButton) {
        QRect lockRect(option.rect.left(), option.rect.top(), 18, 18);
        painter->fillRect(lockRect, hoverColor);
      } else if (m_hoverOn == HoverOnPinButton) {
        QRect pinRect(option.rect.left() + 18, option.rect.top(), 18, 18);
        painter->fillRect(pinRect, hoverColor);
      } else if (m_hoverOn != HoverOnLockButton_SelectedShapes &&
                 m_hoverOn != HoverOnPinButton_SelectedShapes && !layerIsLocked)
        painter->fillRect(option.rect, hoverColor);
    } else {  // drawing column
      if (m_hoverOn == HoverOnVisibleButton) {
        QRect visibleRect(option.rect.left() + 18, option.rect.top(), 18, 18);
        painter->fillRect(visibleRect, hoverColor);
      } else if (m_hoverOn != HoverOnVisibleButton_SelectedShapes &&
                 !layerIsLocked)
        painter->fillRect(option.rect, hoverColor);
    }
  }

  // シェイプ名の前に閉曲線／開曲線のアイコンを入れる
  if (index.column() == 0) {
    bool isClosed = shape.shapePairP->isClosed();
    painter->drawPixmap(option.rect.left(), option.rect.top(), 18, 18,
                        isClosed ? QPixmap(":Resources/shape_close.svg")
                                 : QPixmap(":Resources/shape_open.svg"));

    bool isVisible = shape.shapePairP->isVisible();
    painter->drawPixmap(option.rect.left() + 18 + 2, option.rect.top() + 2, 14,
                        14,
                        isVisible ? QPixmap(":Resources/visible_on.svg")
                                  : QPixmap(":Resources/visible_off.svg"));

    QRect textRect = option.rect;
    textRect.setLeft(textRect.left() + 38);

    painter->setPen(Qt::white);
    painter->drawText(textRect, shape.shapePairP->getName());
    return;
  }

  bool isLocked = shape.shapePairP->isLocked(shape.fromTo);
  if (isLocked) {
    painter->fillRect(option.rect,
                      QBrush(QColor(128, 128, 128, 128), Qt::FDiagPattern));
  }

  painter->drawPixmap(option.rect.left(), option.rect.top(), 18, 18,
                      isLocked ? QPixmap(":Resources/lock_on.svg")
                               : QPixmap(":Resources/lock.svg"));

  bool isPinned = shape.shapePairP->isPinned(shape.fromTo);

  painter->drawPixmap(option.rect.left() + 18, option.rect.top(), 18, 18,
                      isPinned ? QPixmap(":Resources/pin_on.svg")
                               : QPixmap(":Resources/pin.svg"));

  // シェイプタグの描画
  int shapeTagCount = shape.shapePairP->shapeTagCount(shape.fromTo);
  if (shapeTagCount == 0) return;

  ShapeTagSettings* shapeTags =
      IwApp::instance()->getCurrentProject()->getProject()->getShapeTags();
  QRect tagAreaRect = option.rect;
  tagAreaRect.setLeft(tagAreaRect.left() + 36);
  int posOffset =
      std::min(12, (int)std::ceil((double)(tagAreaRect.width() - 12) /
                                  (double)shapeTagCount));

  QRect tagIconRect(tagAreaRect.left(), tagAreaRect.top() + 3, 12, 12);

  for (auto tagId : shape.shapePairP->getShapeTags(shape.fromTo)) {
    QIcon icon = shapeTags->getTagFromId(tagId).icon;
    painter->drawPixmap(tagIconRect, icon.pixmap(12, 12));
    tagIconRect.translate(posOffset, 0);
  }
}

#define IGNORERETURN                                                           \
  return QStyledItemDelegate::editorEvent(event, model, option, index);

bool ShapeTreeDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index) {
  if (event->type() == QEvent::MouseButtonPress && index.isValid()) {
    auto me = static_cast<QMouseEvent*>(event);
    if (me->button() == Qt::LeftButton) {
      const ShapeTree* tree = qobject_cast<const ShapeTree*>(option.widget);
      if (!tree) IGNORERETURN

      OneShape shape = tree->shapeFromIndex(index);
      // レイヤークリック時の操作
      if (!shape.shapePairP) {
        // check if clicking on layer
        IwLayer* layer = tree->layerFromIndex(index);
        if (!layer) IGNORERETURN

        if (m_hoverOn == HoverOnNone)
          return IwApp::instance()->getCurrentLayer()->setLayer(layer);

        ProjectUtils::LayerPropertyType type =
            (m_hoverOn == HoverOnLayerVisibleButton)  ? ProjectUtils::Visibility
            : (m_hoverOn == HoverOnLayerRenderButton) ? ProjectUtils::Render
                                                      : ProjectUtils::Lock;

        if (layer->isLocked() && type != ProjectUtils::Lock) IGNORERETURN

        ProjectUtils::toggleLayerProperty(layer, type);

        return true;
      }

      if (m_hoverOn == HoverOnNone) IGNORERETURN

      if (m_hoverOn == HoverOnLockButton)
        ProjectUtils::switchLock(shape);
      else if (m_hoverOn == HoverOnLockButton_SelectedShapes) {
        IwShapePairSelection::instance()->toggleLocks();
      } else if (m_hoverOn == HoverOnPinButton) {
        ProjectUtils::switchPin(shape);
        // timelineを更新するため、シグナルを出す
        IwApp::instance()->getCurrentSelection()->setSelection(
            IwShapePairSelection::instance());
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      } else if (m_hoverOn == HoverOnPinButton_SelectedShapes)
        IwShapePairSelection::instance()->togglePins();
      else if (m_hoverOn == HoverOnVisibleButton) {
        ProjectUtils::switchShapeVisibility(shape.shapePairP);
      } else {  // HoverOnVisibleButton_SelectedShapes
        IwShapePairSelection::instance()->toggleVisibility();
      }

      return true;
    }
  } else if (event->type() == QEvent::MouseMove && index.isValid()) {
    HoverOn prevState = m_hoverOn;
    auto me           = static_cast<QMouseEvent*>(event);
    if (me->buttons() == Qt::NoButton) {
      auto findHoverOn = [&]() {
        const ShapeTree* tree = qobject_cast<const ShapeTree*>(option.widget);
        if (!tree) return HoverOnNone;
        OneShape shape = tree->shapeFromIndex(index);

        QPoint pos = me->pos() - option.rect.topLeft();
        // レイヤーの場合
        if (!shape.shapePairP) {
          if (pos.x() < 18)
            return HoverOnLayerVisibleButton;
          else if (pos.x() < 36)
            return HoverOnLayerRenderButton;
          else if (pos.x() < 54)
            return HoverOnLayerLockButton;
          return HoverOnNone;
        }

        // ロック中のレイヤ内のシェイプにHoverしても反応させない
        IwLayer* layer = tree->layerFromIndex(index);
        if (!layer || layer->isLocked()) return HoverOnNone;

        // shape の列
        if (index.column() == 0) {
          if (pos.x() < 18 || 36 < pos.x()) return HoverOnNone;
          return (IwShapePairSelection::instance()->isSelected(
                     shape.shapePairP))
                     ? HoverOnVisibleButton_SelectedShapes
                     : HoverOnVisibleButton;
        }

        // from to の列
        if (pos.x() > 36) return HoverOnNone;

        // 選択シェイプ上でロック／ピンを操作した場合、選択シェイプ全て変換する
        if (IwShapePairSelection::instance()->isSelected(shape))
          return (pos.x() < 18) ? HoverOnLockButton_SelectedShapes
                                : HoverOnPinButton_SelectedShapes;
        else
          return (pos.x() < 18) ? HoverOnLockButton : HoverOnPinButton;
      };

      m_hoverOn = findHoverOn();
    } else
      m_hoverOn = HoverOnNone;

    if (m_hoverOn != prevState) {
      emit repaintView();
      return true;
    }
  }
  IGNORERETURN
}

QSize ShapeTreeDelegate::sizeHint(const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const {
  QSize ret = QStyledItemDelegate::sizeHint(option, index);
  ret.setHeight(18);
  return ret;
}

QWidget* ShapeTreeDelegate::createEditor(QWidget* parent,
                                         const QStyleOptionViewItem& option,
                                         const QModelIndex& index) const {
  if (index.column() != 0) return nullptr;
  if (m_hoverOn == HoverOnVisibleButton ||
      m_hoverOn == HoverOnVisibleButton_SelectedShapes)
    return nullptr;
  return QStyledItemDelegate::createEditor(parent, option, index);
}

void ShapeTreeDelegate::updateEditorGeometry(QWidget* editor,
                                             const QStyleOptionViewItem& option,
                                             const QModelIndex& index) const {
  if (index.column() != 0) return;

  const ShapeTree* tree = qobject_cast<const ShapeTree*>(option.widget);
  if (!tree) return;
  OneShape shape = tree->shapeFromIndex(index);
  QRect rect     = option.rect;
  if (shape.shapePairP)
    rect.adjust(38, 0, 0, 0);
  else
    rect.adjust(54, 0, 0, 0);
  editor->setGeometry(rect);
}

void ShapeTreeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                     const QModelIndex& index) const {
  QStyledItemDelegate::setModelData(editor, model, index);
  if (index.column() == 0) emit editingFinished(index);
}

//=============================================================================
// LayerItem
//-----------------------------------------------------------------------------

class LayerItem final : public QTreeWidgetItem {
  IwLayer* m_layer;

public:
  LayerItem(QTreeWidgetItem* parent, IwLayer* layer)
      : QTreeWidgetItem(parent, UserType), m_layer(layer) {
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
             Qt::ItemIsDropEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    setText(0, m_layer->getName());

    setFirstColumnSpanned(true);
    setBackground(0, layerBgColor);
  }
  IwLayer* getLayer() const { return m_layer; }
};

//=============================================================================
// ShapePairItem
//-----------------------------------------------------------------------------

class ShapePairItem final : public QTreeWidgetItem {
  ShapePair* m_shapePair;

public:
  ShapePairItem(QTreeWidgetItem* parent, ShapePair* shapePair)
      : QTreeWidgetItem(parent, UserType), m_shapePair(shapePair) {
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
             Qt::ItemIsDropEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    setText(0, m_shapePair->getName());
    // setToolTip(0, QObject::tr("[Drag] to move position"));
    setBackground(
        0, shapePair->isParent() ? parentShapeBgColor : childShapeBgColor);
    setBackground(1, fromBgColor);
    setBackground(2, toBgColor);
    setExpanded(true);
  }

  ShapePair* shapePair() const { return m_shapePair; }
};

//-----------------------------------------------------------------------------

ShapeTree::ShapeTree(QWidget* parent)
    : QTreeWidget(parent), m_isSyncronizing(false) {
  ShapeTreeDelegate* delegate = new ShapeTreeDelegate(this);
  setItemDelegate(delegate);
  setDragEnabled(true);
  setDropIndicatorShown(true);
  setDefaultDropAction(Qt::MoveAction);
  setDragDropMode(QAbstractItemView::InternalMove);
  setColumnCount(3);
  setIndentation(10);
  setHeaderLabels({tr("Shapes"), tr("From"), tr("To")});
  setSelectionBehavior(SelectItems);
  setSelectionMode(ExtendedSelection);
  setDropIndicatorShown(true);
  setMouseTracking(true);

  header()->hide();

  setStyle(new MyProxyStyle(style()));

  header()->setSectionResizeMode(0, QHeaderView::Stretch);
  header()->setSectionResizeMode(1, QHeaderView::Fixed);
  header()->setSectionResizeMode(2, QHeaderView::Fixed);
  header()->resizeSection(1, 80);
  header()->resizeSection(2, 80);
  header()->setStretchLastSection(false);

  connect(IwApp::instance()->getCurrentProject(), SIGNAL(shapeTreeChanged()),
          this, SLOT(updateTree()));
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(projectChanged()),
          this, SLOT(update()));
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(projectSwitched()),
          this, SLOT(updateTree()));
  connect(IwApp::instance()->getCurrentProject(),
          SIGNAL(shapeTagSettingsChanged()), this, SLOT(update()));

  connect(IwApp::instance()->getCurrentSelection(),
          SIGNAL(selectionChanged(IwSelection*)), this,
          SLOT(onCurrentSelectionChanged(IwSelection*)));

  connect(IwApp::instance()->getCurrentLayer(), SIGNAL(layerSwitched()), this,
          SLOT(update()));
  connect(IwApp::instance()->getCurrentLayer(), SIGNAL(layerChanged(IwLayer*)),
          this, SLOT(onLayerChanged(IwLayer*)));

  connect(
      selectionModel(),
      SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
      this, SLOT(onModelSelectionChanged()));

  connect(delegate, SIGNAL(repaintView()), this, SLOT(update()));

  connect(delegate, SIGNAL(editingFinished(const QModelIndex&)), this,
          SLOT(onItemEdited(const QModelIndex&)));

  updateTree();
}

OneShape ShapeTree::shapeFromIndex(const QModelIndex& index) const {
  QTreeWidgetItem* item        = itemFromIndex(index);
  ShapePairItem* shapePairItem = dynamic_cast<ShapePairItem*>(item);
  if (!shapePairItem) return OneShape();

  int fromTo = (index.column() == 1) ? 0 : 1;  // column 0 のときはToになる
  return OneShape(shapePairItem->shapePair(), fromTo);
}

namespace {
LayerItem* getLayerItem(QTreeWidgetItem* item, QTreeWidgetItem* root) {
  if (item == root || item == nullptr) return nullptr;
  LayerItem* layerItem = dynamic_cast<LayerItem*>(item);
  if (layerItem) return layerItem;
  return getLayerItem(item->parent(), root);
};
}  // namespace

IwLayer* ShapeTree::layerFromIndex(const QModelIndex& index) const {
  QTreeWidgetItem* item = itemFromIndex(index);
  LayerItem* layerItem  = getLayerItem(item, invisibleRootItem());
  if (!layerItem) return nullptr;
  return layerItem->getLayer();
}

void ShapeTree::updateTree() {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  // 畳まれているアイテムを記憶しておく
  QList<IwLayer*> shrinkedLayerItems;
  QMap<IwLayer*, LayerItem*>::const_iterator i_lay = m_layerItems.constBegin();
  while (i_lay != m_layerItems.constEnd()) {
    if (!i_lay.value()->isExpanded()) shrinkedLayerItems.append(i_lay.key());
    ++i_lay;
  }
  QList<ShapePair*> shrinkedShapePairItems;
  QMap<ShapePair*, ShapePairItem*>::const_iterator i_sp =
      m_shapePairItems.constBegin();
  while (i_sp != m_shapePairItems.constEnd()) {
    if (i_sp.value()->childCount() > 0 && !i_sp.value()->isExpanded() &&
        i_sp.key()->isParent())
      shrinkedShapePairItems.append(i_sp.key());
    ++i_sp;
  }
  // スクロール位置を記憶
  int verticalMaximum = verticalScrollBar()->maximum();
  int verticalScroll  = verticalScrollBar()->value();

  clear();
  m_layerItems.clear();
  m_shapePairItems.clear();

  for (int lay = 0; lay < prj->getLayerCount(); lay++) {
    IwLayer* layer = prj->getLayer(lay);
    if (!layer) continue;
    // 各レイヤの描画
    LayerItem* layItem = new LayerItem(0, layer);
    addTopLevelItem(layItem);
    layItem->setFirstColumnSpanned(true);
    // 畳んだ状態を再現
    layItem->setExpanded(!shrinkedLayerItems.contains(layer));
    // ロックされていたらフラグの初期値を変更
    if (layer->isLocked()) layItem->setFlags(Qt::ItemIsEnabled);
    m_layerItems[layer] = layItem;

    QTreeWidgetItem* parentItem = layItem;

    // シェイプ
    for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
      ShapePair* shapePair = layer->getShapePair(sp);
      if (!shapePair) continue;

      ShapePairItem* child;
      if (shapePair->isParent()) {
        child      = new ShapePairItem(layItem, shapePair);
        parentItem = child;
        child->setExpanded(true);
      } else {
        child = new ShapePairItem(parentItem, shapePair);
        child->setExpanded(true);

        // 一つ目の子シェイプ登録時に畳んだ状態を再現
        ShapePairItem* parentShapePairItem =
            dynamic_cast<ShapePairItem*>(parentItem);
        if (parentShapePairItem &&
            shrinkedShapePairItems.contains(parentShapePairItem->shapePair()))
          parentShapePairItem->setExpanded(false);
      }
      // ロックされていたらフラグの初期値を変更
      if (layer->isLocked()) child->setFlags(Qt::ItemIsEnabled);

      m_shapePairItems[shapePair] = child;
    }
  }
  // スクロール位置を再現
  verticalScrollBar()->setMaximum(verticalMaximum);
  verticalScrollBar()->setValue(verticalScroll);

  // expandAll();
}

void ShapeTree::onCurrentSelectionChanged(IwSelection* sel) {
  if (m_isSyncronizing) return;

  if (dynamic_cast<IwShapePairSelection*>(sel) == nullptr) return;

  m_isSyncronizing = true;

  // reflect to the model selection
  IwShapePairSelection* selection = IwShapePairSelection::instance();

  QItemSelectionModel* modelSelection = selectionModel();
  modelSelection->clear();

  for (int s = 0; s < selection->getShapesCount(); s++) {
    OneShape oneShape = selection->getShape(s);
    if (!oneShape.shapePairP) continue;
    if (!m_shapePairItems.contains(oneShape.shapePairP)) continue;
    QModelIndex index =
        indexFromItem(m_shapePairItems.value(oneShape.shapePairP),
                      (oneShape.fromTo == 0) ? 1 : 2);
    modelSelection->select(index, QItemSelectionModel::Select);
    if (s == 0)
      scrollToItem(itemFromIndex(index), QAbstractItemView::PositionAtCenter);
  }

  // setSelectionModel(modelSelection);
  // update();

  m_isSyncronizing = false;
}

void ShapeTree::onModelSelectionChanged() {
  if (m_isSyncronizing) return;
  m_isSyncronizing = true;

  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  IwShapePairSelection* selection     = IwShapePairSelection::instance();
  QItemSelectionModel* modelSelection = selectionModel();

  selection->selectNone();

  QList<QModelIndex> itemsToBeSelected;
  QList<QModelIndex> layerItems;
  bool hasShapeItems = false;

  QModelIndexList selectedIndexes = modelSelection->selectedIndexes();
  if (!selectedIndexes.contains(modelSelection->currentIndex()))
    selectedIndexes.append(modelSelection->currentIndex());

  IwLayer* selectedLayer = nullptr;

  for (auto index : selectedIndexes) {
    QTreeWidgetItem* item = itemFromIndex(index);
    // レイヤーがロックされている場合は抜かす
    IwLayer* layer = layerFromIndex(index);
    if (!layer) continue;
    selectedLayer = layer;
    if (layer->isLocked()) continue;

    ShapePairItem* shapePairItem = dynamic_cast<ShapePairItem*>(item);
    // レイヤーのアイテムは抜かす
    if (!shapePairItem) {
      layerItems.append(index);
      continue;
    }

    ShapePair* shapePair = shapePairItem->shapePair();

    // col0 の場合 fromTo両方選択
    // Lockされておらず、かつFromToが表示されている必要
    if (index.column() == 0) {
      if (!shapePair->isLocked(0) &&
          prj->getViewSettings()->isFromToVisible(0)) {
        selection->addShape(OneShape(shapePair, 0));
        itemsToBeSelected.append(indexFromItem(item, 1));
      }
      if (!shapePair->isLocked(1) &&
          prj->getViewSettings()->isFromToVisible(1)) {
        selection->addShape(OneShape(shapePair, 1));
        itemsToBeSelected.append(indexFromItem(item, 2));
      }
    } else {
      int fromTo = (index.column() == 1) ? 0 : 1;
      if (!shapePair->isLocked(fromTo))
        selection->addShape(OneShape(shapePair, fromTo));
      else
        modelSelection->select(index, QItemSelectionModel::Deselect);
    }

    hasShapeItems = true;
  }

  for (auto index : itemsToBeSelected) {
    modelSelection->select(index, QItemSelectionModel::Select);
  }

  // 選択インデックスにレイヤとシェイプの両方が含まれていたら、シェイプに統一
  // シェイプを選ぶときはシェイプだけ、レイヤーを選ぶときはレイヤーだけにする
  if (hasShapeItems) {
    for (auto layerItemIndex : layerItems) {
      modelSelection->select(layerItemIndex, QItemSelectionModel::Deselect);
    }
  }

  IwApp::instance()->getCurrentSelection()->setSelection(selection);
  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();

  // アイテム１つだけを選択した場合、カレントレイヤの切り替え
  if (selectedIndexes.size() == 1 && selectedLayer) {
    IwApp::instance()->getCurrentLayer()->setLayer(selectedLayer);
  }

  // if (!itemsToBeDoubleSelected.isEmpty())
  //   update();
  update();

  m_isSyncronizing = false;
}

void ShapeTree::onItemEdited(const QModelIndex& index) {
  if (index.column() != 0) return;

  QTreeWidgetItem* item = itemFromIndex(index);

  // アイテムのリネーム
  ShapePairItem* shapePairItem = dynamic_cast<ShapePairItem*>(item);
  if (shapePairItem) {
    QString inputName = shapePairItem->text(index.column());
    if (inputName.isEmpty())
      shapePairItem->setText(index.column(),
                             shapePairItem->shapePair()->getName());
    else
      ProjectUtils::RenameShapePair(shapePairItem->shapePair(), inputName);
    return;
  }

  LayerItem* layerItem = dynamic_cast<LayerItem*>(item);
  if (layerItem) {
    QString inputName = layerItem->text(index.column());
    if (inputName.isEmpty())
      layerItem->setText(index.column(), layerItem->getLayer()->getName());
    else
      ProjectUtils::RenameLayer(layerItem->getLayer(), inputName);
  }
}

void ShapeTree::onShapeTagStateChanged(int state) {
  int tagId = qobject_cast<QAction*>(sender())->data().toInt();
  bool on   = (Qt::CheckState)state == Qt::Checked;
  IwShapePairSelection::instance()->setShapeTag(tagId, on);
}

// ロック情報からドラッグ／ダブルクリック編集機能の切り替えを行う
void ShapeTree::onLayerChanged(IwLayer* layer) {
  QMap<IwLayer*, LayerItem*>::const_iterator i_lay = m_layerItems.constBegin();
  while (i_lay != m_layerItems.constEnd()) {
    if (i_lay.key() != layer) {
      ++i_lay;
      continue;
    }
    bool isLocked = i_lay.key()->isLocked();
    // 自身のLayerItemのフラグ
    if (isLocked)
      i_lay.value()->setFlags(Qt::ItemIsEnabled);
    else
      i_lay.value()->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
                              Qt::ItemIsDropEnabled | Qt::ItemIsEnabled |
                              Qt::ItemIsEditable);

    // 子シェイプのフラグ
    for (int i = 0; i < i_lay.value()->childCount(); i++) {
      ShapePairItem* shapeItem =
          dynamic_cast<ShapePairItem*>(i_lay.value()->child(i));
      if (!shapeItem) continue;
      if (isLocked)
        shapeItem->setFlags(Qt::ItemIsEnabled);
      else
        shapeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
                            Qt::ItemIsDropEnabled | Qt::ItemIsEnabled |
                            Qt::ItemIsEditable);

      for (int j = 0; j < shapeItem->childCount(); j++) {
        ShapePairItem* childShapeItem =
            dynamic_cast<ShapePairItem*>(shapeItem->child(j));
        if (!childShapeItem) continue;
        if (isLocked)
          childShapeItem->setFlags(Qt::ItemIsEnabled);
        else
          childShapeItem->setFlags(
              Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
              Qt::ItemIsDropEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
      }
    }

    ++i_lay;
  }

  update();
}

void ShapeTree::dragEnterEvent(QDragEnterEvent* e) {
  // シェイプを選択しているときはRootと子シェイプをドロップ不可に
  // レイヤーを選択しているときはRoot以外をドロップ不可にする

  enum DragType { Shape, Layer } dragType = Shape;

  for (auto index : selectedIndexes()) {
    QTreeWidgetItem* item        = itemFromIndex(index);
    ShapePairItem* shapePairItem = dynamic_cast<ShapePairItem*>(item);
    if (!shapePairItem) {
      dragType = Layer;
      break;
    }
  }

  QTreeWidgetItemIterator it(this);
  while (*it) {
    if (dragType == Shape) {
      ShapePairItem* shapePairItem = dynamic_cast<ShapePairItem*>(*it);

      if ((*it)->flags() & Qt::ItemIsSelectable) {
        if (shapePairItem && !shapePairItem->shapePair()->isParent())
          (*it)->setFlags((*it)->flags() & ~Qt::ItemIsDropEnabled);
        else
          (*it)->setFlags((*it)->flags() | Qt::ItemIsDropEnabled);
      }
    } else {  // dragType == Layer
      if ((*it)->flags() & Qt::ItemIsSelectable) {
        (*it)->setFlags((*it)->flags() & ~Qt::ItemIsDropEnabled);
      }
    }

    ++it;
  }

  if (dragType == Shape)
    invisibleRootItem()->setFlags(invisibleRootItem()->flags() &
                                  ~(Qt::ItemIsDropEnabled));
  else  // dragType == Layer
    invisibleRootItem()->setFlags(invisibleRootItem()->flags() |
                                  Qt::ItemIsDropEnabled);

  QTreeWidget::dragEnterEvent(e);
}

void ShapeTree::dropEvent(QDropEvent* e) {
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  enum DragType { Shape, Layer } dragType = Shape;

  QItemSelectionModel* modelSelection = selectionModel();

  int vPos = verticalScrollBar()->value();

  m_isSyncronizing = true;

  enum DropAtType { OnLayer, OnShape, BetweenShapes } dropAtType;
  if (dropIndicatorPosition() == QAbstractItemView::OnItem) {
    LayerItem* layerItem =
        dynamic_cast<LayerItem*>(itemFromIndex(indexAt(e->pos())));
    if (layerItem)
      dropAtType = OnLayer;
    else
      dropAtType = OnShape;
  } else  // dropIndicatorPosition() == AbstractItemView::AboveItem or BelowItem
    dropAtType = BetweenShapes;

  // シェイプのドラッグによって影響を受けるレイヤーを洗いだす
  QSet<LayerItem*> modifiedLayerItems;
  QMap<QTreeWidgetItem*, bool> expandMap;
  QList<ShapePair*> movedShapes;

  for (auto index : modelSelection->selectedIndexes()) {
    QTreeWidgetItem* item = itemFromIndex(index);
    expandMap[item]       = item->isExpanded();

    ShapePairItem* shapePairItem = dynamic_cast<ShapePairItem*>(item);
    if (!shapePairItem) {
      dragType = Layer;
      break;
    } else
      movedShapes.append(shapePairItem->shapePair());

    LayerItem* parentLayerItem =
        dynamic_cast<LayerItem*>(itemFromIndex(index.parent()));
    if (!parentLayerItem)
      parentLayerItem =
          dynamic_cast<LayerItem*>(itemFromIndex(index.parent().parent()));
    if (!parentLayerItem) continue;  // 念のため
    modifiedLayerItems.insert(parentLayerItem);
  }

  QTreeWidget::dropEvent(e);

  // expand状態の再現
  QMap<QTreeWidgetItem*, bool>::const_iterator i = expandMap.constBegin();
  while (i != expandMap.constEnd()) {
    i.key()->setExpanded(i.value());
    ++i;
  }

  if (dragType == Shape) {
    // ドロップ先のレイヤーも追加する
    QModelIndex dropIndex = indexAt(e->pos());
    while (1) {
      if (!dropIndex.isValid()) break;
      LayerItem* layerItem = dynamic_cast<LayerItem*>(itemFromIndex(dropIndex));
      if (layerItem) {
        modifiedLayerItems.insert(layerItem);
        break;
      }
      dropIndex = dropIndex.parent();
      if (dropIndex == rootIndex()) break;
    }
    QMap<IwLayer*, QList<QPair<ShapePair*, bool>>> layerShapeInfo;
    // 移動後のシェイプの並びを作る
    QSetIterator<LayerItem*> lay_itr(modifiedLayerItems);
    while (lay_itr.hasNext()) {
      LayerItem* layerItem = lay_itr.next();

      QList<QPair<ShapePair*, bool>> shapeInfo;
      for (int s = 0; s < layerItem->childCount(); s++) {
        ShapePairItem* shapePairItem =
            dynamic_cast<ShapePairItem*>(layerItem->child(s));
        if (!shapePairItem || !shapePairItem->shapePair()) continue;
        shapeInfo.append({shapePairItem->shapePair(), true});

        // シェイプ情報の登録
        for (int sub_s = 0; sub_s < shapePairItem->childCount(); sub_s++) {
          ShapePairItem* subItem =
              dynamic_cast<ShapePairItem*>(shapePairItem->child(sub_s));
          if (!subItem || !subItem->shapePair()) continue;
          shapeInfo.append({subItem->shapePair(), false});

          // 孫シェイプがある場合,
          // あとでツリーを再構築して孫シェイプを子に引き上げる
          for (int subsub_s = 0; subsub_s < subItem->childCount(); subsub_s++) {
            ShapePairItem* subsubItem =
                dynamic_cast<ShapePairItem*>(subItem->child(subsub_s));
            if (!subsubItem || !subsubItem->shapePair()) continue;
            shapeInfo.append({subsubItem->shapePair(), false});
          }
        }
      }

      int pastedIndexFirst = -1;
      int pastedIndexLast  = -1;
      for (int id = 0; id < shapeInfo.size(); id++) {
        // 移動してきたアイテムの範囲を取得。※pastedIndexLastのひとつ前までが範囲に含まれる
        if (movedShapes.contains(shapeInfo.at(id).first)) {
          if (pastedIndexFirst == -1) pastedIndexFirst = id;

          switch (dropAtType) {
          case OnLayer:
            shapeInfo[id].second = true;
            break;
          case OnShape:
            shapeInfo[id].second = false;
            break;
          case BetweenShapes:
            shapeInfo[id].second = shapeInfo.at(id).first->isParent();
            break;
          }
        } else if (!movedShapes.contains(shapeInfo.at(id).first) &&
                   pastedIndexFirst != -1 && pastedIndexLast == -1) {
          pastedIndexLast = id;
          break;
        }
      }

      // 移動してきたアイテムを重ね順に合わせてソートする
      if (pastedIndexFirst >= 0) {
        if (pastedIndexLast == -1) pastedIndexLast = shapeInfo.count();

        // sort shapes in stacking order
        std::sort(
            shapeInfo.begin() + pastedIndexFirst,
            shapeInfo.begin() + pastedIndexLast,
            [&](const QPair<ShapePair*, bool>& s1,
                const QPair<ShapePair*, bool>& s2) {
              int lay1, sha1, lay2, sha2;
              bool ret1 = project->getShapeIndex(s1.first, lay1, sha1);
              bool ret2 = project->getShapeIndex(s2.first, lay2, sha2);
              if (!ret2) return true;
              if (!ret1) return false;

              return (lay1 < lay2) ? true : (lay1 > lay2) ? false : sha1 < sha2;
            });
      }

      layerShapeInfo[layerItem->getLayer()] = shapeInfo;
    }

    ProjectUtils::ReorderShapes(layerShapeInfo);
  } else {  // dragType == Layer
    QList<IwLayer*> layers;
    for (int lay = 0; lay < topLevelItemCount(); lay++) {
      LayerItem* layerItem = dynamic_cast<LayerItem*>(topLevelItem(lay));
      if (!layerItem) continue;
      layers.append(layerItem->getLayer());
    }
    ProjectUtils::ReorderLayers(layers);
  }

  // revert scroll
  verticalScrollBar()->setValue(vPos);

  m_isSyncronizing = false;

  updateTree();
}

void ShapeTree::contextMenuEvent(QContextMenuEvent* e) {
  IwShapePairSelection* selection = IwShapePairSelection::instance();
  if (selection->isEmpty()) return;

  ShapeTagSettings* shapeTags =
      IwApp::instance()->getCurrentProject()->getProject()->getShapeTags();

  QMenu menu(this);  // マウスがある行

  for (int t = 0; t < shapeTags->tagCount(); t++) {
    ShapeTagInfo tagInfo = shapeTags->getTagAt(t);
    int tagId            = shapeTags->getIdAt(t);

    bool onFound = false, offFound = false;
    // 選択シェイプを走査して、シェイプタグの有無を調べる
    for (auto shape : selection->getShapes()) {
      if (shape.shapePairP->containsShapeTag(shape.fromTo, tagId))
        onFound = true;
      else
        offFound = true;
      if (onFound && offFound) break;
    }
    assert(onFound | offFound);
    Qt::CheckState state =
        (onFound) ? ((offFound) ? Qt::PartiallyChecked : Qt::Checked)
                  : Qt::Unchecked;
    ShapeTagAction* action =
        new ShapeTagAction(tagInfo.icon, tagInfo.name, state);
    action->setData(tagId);
    connect(action, SIGNAL(stateChanged(int)), this,
            SLOT(onShapeTagStateChanged(int)));

    menu.addAction(action);
  }

  menu.exec(e->globalPos());
}

//----------------------------------------------

ShapeTreeWindow::ShapeTreeWindow(QWidget* parent) : QDockWidget(parent) {
  setWindowTitle(tr("Shape Tree"));
  // The dock widget cannot be closed, moved, or floated.
  setFeatures(QDockWidget::NoDockWidgetFeatures);
  m_selectByTagBtn            = new QPushButton(tr("Select By Tag"), this);
  QPushButton* openTagEditBtn = new QPushButton(tr("Edit Tags"), this);

  m_shapeTree = new ShapeTree(this);

  m_shapeTagEditDialog = new ShapeTagEditDialog(this);

  QWidget* widget = new QWidget(this);

  QVBoxLayout* layout = new QVBoxLayout();
  layout->setMargin(0);
  layout->setSpacing(10);
  {
    QHBoxLayout* topLay = new QHBoxLayout();
    topLay->setMargin(10);
    topLay->setSpacing(20);
    {
      topLay->addWidget(m_selectByTagBtn, 0);
      topLay->addWidget(openTagEditBtn, 0);
      topLay->addStretch(1);
    }
    layout->addLayout(topLay, 0);

    layout->addWidget(m_shapeTree, 1);
  }
  widget->setLayout(layout);

  setWidget(widget);

  connect(m_selectByTagBtn, SIGNAL(clicked(bool)), this,
          SLOT(openSelectByTagMenu()));

  m_shapeTagEditDialog->hide();
  connect(openTagEditBtn, SIGNAL(clicked(bool)), m_shapeTagEditDialog,
          SLOT(show()));
}

void ShapeTreeWindow::openSelectByTagMenu() {
  ShapeTagSettings* shapeTags =
      IwApp::instance()->getCurrentProject()->getProject()->getShapeTags();
  int tagCount = shapeTags->tagCount();
  if (tagCount == 0) return;

  QMenu menu(this);
  for (int t = 0; t < tagCount; t++) {
    ShapeTagInfo tag = shapeTags->getTagAt(t);
    QAction* action  = new QAction(tag.name);
    action->setIcon(tag.icon);
    action->setData(shapeTags->getIdAt(t));

    connect(action, SIGNAL(triggered()), this, SLOT(onTagSelection()));

    menu.addAction(action);
  }

  menu.exec(m_selectByTagBtn->mapToGlobal(m_selectByTagBtn->rect().center()));
}

void ShapeTreeWindow::onTagSelection() {
  int tagId = qobject_cast<QAction*>(sender())->data().toInt();

  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  IwShapePairSelection* selection = IwShapePairSelection::instance();
  selection->selectNone();

  for (int lay = 0; lay < prj->getLayerCount(); lay++) {
    IwLayer* layer = prj->getLayer(lay);
    if (!layer) continue;
    // シェイプ
    for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
      ShapePair* shapePair = layer->getShapePair(sp);
      if (!shapePair) continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        if (shapePair->isLocked(fromTo)) continue;
        if (shapePair->containsShapeTag(fromTo, tagId))
          selection->addShape(OneShape(shapePair, fromTo));
      }
    }
  }

  IwApp::instance()->getCurrentSelection()->setSelection(selection);
  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//----------------------------------------------

ShapeTagEditDialog::ShapeTagEditDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(tr("Edit Tags"));
  m_tagList                    = new QListWidget(this);
  m_tagNameEdit                = new QLineEdit(this);
  m_tagColorBtn                = new QPushButton(this);
  m_tagShapeCombo              = new QComboBox(this);
  QPushButton* removeTagButton = new QPushButton(tr("Remove"), this);
  QPushButton* addTagButton    = new QPushButton(tr("Add"), this);

  m_tagList->setSelectionMode(QAbstractItemView::SingleSelection);

  setObjectName("ShapeTagEditDialog");

  for (auto shapeId : ShapeTagSettings::iconShapeIds()) {
    m_tagShapeCombo->addItem(ShapeTagSettings::getIconShape(shapeId), "",
                             shapeId);
  }
  m_tagShapeCombo->setIconSize(QSize(20, 20));

  removeTagButton->setFocusPolicy(Qt::NoFocus);
  addTagButton->setFocusPolicy(Qt::NoFocus);
  m_tagColorBtn->setFocusPolicy(Qt::NoFocus);

  QHBoxLayout* layout = new QHBoxLayout();
  layout->setMargin(10);
  layout->setSpacing(10);
  {
    layout->addWidget(m_tagList, 1);

    QGridLayout* rightLay = new QGridLayout();
    rightLay->setMargin(0);
    rightLay->setHorizontalSpacing(5);
    rightLay->setVerticalSpacing(10);
    {
      rightLay->addWidget(new QLabel(tr("Name :"), this), 0, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
      rightLay->addWidget(m_tagNameEdit, 0, 1);

      rightLay->addWidget(new QLabel(tr("Color :"), this), 1, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
      rightLay->addWidget(m_tagColorBtn, 1, 1);

      rightLay->addWidget(new QLabel(tr("Shape :"), this), 2, 0,
                          Qt::AlignRight | Qt::AlignVCenter);
      rightLay->addWidget(m_tagShapeCombo, 2, 1,
                          Qt::AlignLeft | Qt::AlignVCenter);

      QHBoxLayout* buttonLay = new QHBoxLayout();
      buttonLay->setMargin(0);
      buttonLay->setSpacing(20);
      {
        buttonLay->addWidget(removeTagButton, 0);
        buttonLay->addWidget(addTagButton, 0);
        buttonLay->addStretch(1);
      }
      rightLay->addLayout(buttonLay, 3, 0, 1, 2);
    }
    rightLay->setRowStretch(4, 1);
    layout->addLayout(rightLay, 1);
  }
  setLayout(layout);

  connect(m_tagList, SIGNAL(itemSelectionChanged()), this,
          SLOT(onSelectedItemChanged()));
  connect(IwApp::instance()->getCurrentProject(),
          SIGNAL(shapeTagSettingsChanged()), this, SLOT(updateTagList()));
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(projectSwitched()),
          this, SLOT(updateTagList()));

  connect(m_tagNameEdit, SIGNAL(editingFinished()), this,
          SLOT(onTagNameChanged()));
  connect(m_tagColorBtn, SIGNAL(clicked(bool)), this,
          SLOT(onTagColorBtnClicked()));
  connect(m_tagShapeCombo, SIGNAL(activated(int)), this,
          SLOT(onTagShapeComboActivated()));
  connect(addTagButton, SIGNAL(clicked(bool)), this, SLOT(onAddTag()));
  connect(removeTagButton, SIGNAL(clicked(bool)), this, SLOT(onRemoveTag()));
}

ShapeTagInfo ShapeTagEditDialog::currentShapeTagInfo(int& id) {
  id             = -1;
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return ShapeTagInfo();

  if (m_tagList->selectedItems().isEmpty()) return ShapeTagInfo();

  ShapeTagSettings* shapeTags = prj->getShapeTags();
  id = m_tagList->selectedItems()[0]->data(Qt::UserRole).toInt();
  return shapeTags->getTagFromId(id);
}

void ShapeTagEditDialog::updateTagList() {
  // 現在のShapeTag設定に更新する

  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  ShapeTagSettings* shapeTags = prj->getShapeTags();

  int currentTag = -1;
  if (!m_tagList->selectedItems().isEmpty())
    currentTag = m_tagList->selectedItems()[0]->data(Qt::UserRole).toInt();

  m_tagList->clear();

  bool hasTag = shapeTags->tagCount() > 0;

  m_tagNameEdit->setEnabled(hasTag);
  m_tagColorBtn->setEnabled(hasTag);
  m_tagShapeCombo->setEnabled(hasTag);
  if (!hasTag) return;

  QListWidgetItem* currentItem = nullptr;
  for (int t = 0; t < shapeTags->tagCount(); t++) {
    ShapeTagInfo tagInfo = shapeTags->getTagAt(t);

    QListWidgetItem* item = new QListWidgetItem(tagInfo.icon, tagInfo.name);
    item->setData(Qt::UserRole, shapeTags->getIdAt(t));
    QFont font = item->font();
    font.setPointSize(11);
    item->setFont(font);
    m_tagList->addItem(item);

    if (currentItem == nullptr || currentTag == shapeTags->getIdAt(t))
      currentItem = item;
  }

  currentItem->setSelected(true);
}

void ShapeTagEditDialog::showEvent(QShowEvent* /*event*/) { updateTagList(); }

void ShapeTagEditDialog::onSelectedItemChanged() {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  if (m_tagList->selectedItems().isEmpty()) return;

  QListWidgetItem* current = m_tagList->selectedItems()[0];

  ShapeTagSettings* shapeTags = prj->getShapeTags();
  int tagId                   = current->data(Qt::UserRole).toInt();
  ShapeTagInfo tagInfo        = shapeTags->getTagFromId(tagId);

  m_tagNameEdit->setText(tagInfo.name);

  QPixmap pm(20, 20);
  pm.fill(tagInfo.color);
  m_tagColorBtn->setIcon(pm);
  m_tagColorBtn->setText(tagInfo.color.name());

  m_tagShapeCombo->setCurrentIndex(
      m_tagShapeCombo->findData(tagInfo.iconShapeId));
}

void ShapeTagEditDialog::onTagNameChanged() {
  int tagId;
  ShapeTagInfo currentInfo = currentShapeTagInfo(tagId);
  if (tagId == -1) return;

  QString prevName = currentInfo.name;
  QString newName  = m_tagNameEdit->text();
  // 変わってなければreturn
  if (newName == prevName) return;

  // 空になっていたら元の名前を入れてreturn
  if (newName.isEmpty()) {
    m_tagNameEdit->setText(prevName);
    return;
  }

  ShapeTagInfo newInfo = currentInfo;
  newInfo.name         = newName;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new EditShapeTagInfoUndo(
      IwApp::instance()->getCurrentProject()->getProject(), tagId, currentInfo,
      newInfo));
}

void ShapeTagEditDialog::onTagColorBtnClicked() {
  int tagId;
  ShapeTagInfo currentInfo = currentShapeTagInfo(tagId);
  if (tagId == -1) return;

  QColor prevColor = currentInfo.color;

  QColor newColor = QColorDialog::getColor(currentInfo.color, this);

  if (!newColor.isValid() || prevColor == newColor) return;

  ShapeTagInfo newInfo = currentInfo;
  newInfo.color        = newColor;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new EditShapeTagInfoUndo(
      IwApp::instance()->getCurrentProject()->getProject(), tagId, currentInfo,
      newInfo));
}

void ShapeTagEditDialog::onTagShapeComboActivated() {
  int tagId;
  ShapeTagInfo currentInfo = currentShapeTagInfo(tagId);
  if (tagId == -1) return;

  int prevIndex = currentInfo.iconShapeId;
  int newIndex  = m_tagShapeCombo->currentData().toInt();
  if (prevIndex == newIndex) return;

  ShapeTagInfo newInfo = currentInfo;
  newInfo.iconShapeId  = newIndex;

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new EditShapeTagInfoUndo(
      IwApp::instance()->getCurrentProject()->getProject(), tagId, currentInfo,
      newInfo));
}

void ShapeTagEditDialog::onAddTag() {
  QColor randomColor      = QColor::fromHsv(rand() % 360, 255, 255);
  ShapeTagInfo newTagInfo = {tr("New tag"), randomColor, 0, QIcon()};

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new AddShapeTagUndo(
      IwApp::instance()->getCurrentProject()->getProject(), newTagInfo));
}

void ShapeTagEditDialog::onRemoveTag() {
  int tagId;
  ShapeTagInfo currentInfo = currentShapeTagInfo(tagId);
  if (tagId == -1) return;
  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(new RemoveShapeTagUndo(
      IwApp::instance()->getCurrentProject()->getProject(), tagId));
}

//----------------------------------------------

EditShapeTagInfoUndo::EditShapeTagInfoUndo(IwProject* project, int tagId,
                                           ShapeTagInfo before,
                                           ShapeTagInfo after)
    : m_project(project), m_tagId(tagId), m_before(before), m_after(after) {}

void EditShapeTagInfoUndo::undo() {
  m_project->getShapeTags()->setShapeTagInfo(m_tagId, m_before);
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyShapeTagSettingsChanged();
}
void EditShapeTagInfoUndo::redo() {
  m_project->getShapeTags()->setShapeTagInfo(m_tagId, m_after);
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyShapeTagSettingsChanged();
}

//----------------------------------------------

AddShapeTagUndo::AddShapeTagUndo(IwProject* project, ShapeTagInfo tagInfo)
    : m_project(project), m_tagInfo(tagInfo) {
  ShapeTagSettings* shapeTags = m_project->getShapeTags();
  if (shapeTags->tagCount() == 0)
    m_tagId = 0;
  else
    m_tagId = shapeTags->getIdAt(shapeTags->tagCount() - 1) + 1;
  std::cout << "tagId = " << m_tagId << std::endl;
}

void AddShapeTagUndo::undo() {
  ShapeTagSettings* shapeTags = m_project->getShapeTags();
  shapeTags->removeTag(m_tagId);
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyShapeTagSettingsChanged();
}

void AddShapeTagUndo::redo() {
  ShapeTagSettings* shapeTags = m_project->getShapeTags();
  shapeTags->addTag(m_tagId, m_tagInfo.name, m_tagInfo.color,
                    m_tagInfo.iconShapeId);
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyShapeTagSettingsChanged();
}

//----------------------------------------------

RemoveShapeTagUndo::RemoveShapeTagUndo(IwProject* project, int tagId)
    : m_project(project), m_tagId(tagId) {
  m_tagInfo = m_project->getShapeTags()->getTagFromId(tagId);

  // タグが付いているシェイプを洗い出す
  for (int lay = 0; lay < m_project->getLayerCount(); lay++) {
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;
    // シェイプ
    for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
      ShapePair* shapePair = layer->getShapePair(sp);
      if (!shapePair) continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        if (shapePair->containsShapeTag(fromTo, tagId))
          m_shapes.append(OneShape(shapePair, fromTo));
      }
    }
  }

  m_listPos = m_project->getShapeTags()->getListPosOf(tagId);
}

void RemoveShapeTagUndo::undo() {
  // タグを戻す
  m_project->getShapeTags()->insertTag(m_tagId, m_tagInfo, m_listPos);

  // シェイプにタグを再設定
  for (auto shape : m_shapes) {
    shape.shapePairP->addShapeTag(shape.fromTo, m_tagId);
  }

  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyShapeTagSettingsChanged();
}

void RemoveShapeTagUndo::redo() {
  // シェイプからタグを外す
  for (auto shape : m_shapes) {
    shape.shapePairP->removeShapeTag(shape.fromTo, m_tagId);
  }
  m_project->getShapeTags()->removeTag(m_tagId);
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyShapeTagSettingsChanged();
}
