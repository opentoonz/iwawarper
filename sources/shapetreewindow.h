#pragma once
#ifndef SHAPE_TREE_WINDOW_H
#define SHAPE_TREE_WINDOW_H

#include "shapepair.h"

#include <QDockWidget>
#include <QTreeWidget>
#include <QStyledItemDelegate>
#include <QWidgetAction>
#include <QDialog>
#include <QUndoCommand>
#include <QListWidget>

#include <QMap>
#include "shapetagsettings.h"

class IwLayer;
class LayerItem;
class ShapePair;
class ShapePairItem;
class IwSelection;
class QCheckBox;
class QPushButton;
class QListWidget;
class QLineEdit;
class QComboBox;
class QListWidgetItem;

// Tristateなチェックボックス＋アイコン＋タグ名
class ShapeTagAction : public QWidgetAction {
  Q_OBJECT
  QCheckBox* m_chkBox;

public:
  ShapeTagAction(const QIcon& icon, const QString& name,
                 const Qt::CheckState checkState, QWidget* parent = nullptr);

protected slots:
  void onCheckBoxClicked();
signals:
  void stateChanged(int);
};

class ShapeTreeDelegate : public QStyledItemDelegate {
  Q_OBJECT

  enum HoverOn {
    HoverOnLayerVisibleButton,
    HoverOnLayerRenderButton,
    HoverOnLayerLockButton,
    HoverOnLockButton,
    HoverOnLockButton_SelectedShapes,
    HoverOnPinButton,
    HoverOnPinButton_SelectedShapes,
    HoverOnVisibleButton,
    HoverOnVisibleButton_SelectedShapes,
    HoverOnMatteButton,
    HoverOnMatteButton_SelectedShapes,
    HoverOnNone
  } m_hoverOn;

public:
  ShapeTreeDelegate(QObject* parent)
      : QStyledItemDelegate(parent), m_hoverOn(HoverOnNone) {}

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;

  bool editorEvent(QEvent* event, QAbstractItemModel* model,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;

  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const override;
  void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const override;

  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
signals:
  void repaintView();
  void editingFinished(const QModelIndex&) const;
};

class ShapeTree : public QTreeWidget {
  Q_OBJECT

  bool m_isSyncronizing;

  QMap<IwLayer*, LayerItem*> m_layerItems;
  QMap<ShapePair*, ShapePairItem*> m_shapePairItems;

public:
  ShapeTree(QWidget* parent);

  OneShape shapeFromIndex(const QModelIndex& index) const;
  IwLayer* layerFromIndex(const QModelIndex& index) const;

public slots:
  void updateTree();

  // selection同期
  void onCurrentSelectionChanged(IwSelection* sel);
  void onModelSelectionChanged();

  void onItemEdited(const QModelIndex&);

  void onShapeTagStateChanged(int);

  // ロック情報からドラッグ／ダブルクリック編集機能の切り替えを行う
  void onLayerChanged(IwLayer*);

protected:
  void dragEnterEvent(QDragEnterEvent* e) override;
  void dropEvent(QDropEvent* e) override;
  void contextMenuEvent(QContextMenuEvent* e) override;
};

class ShapeTagEditDialog;

class ShapeTreeWindow : public QDockWidget {
  Q_OBJECT

  QPushButton* m_selectByTagBtn;
  ShapeTree* m_shapeTree;
  ShapeTagEditDialog* m_shapeTagEditDialog;

public:
  ShapeTreeWindow(QWidget* parent);
protected slots:
  void openSelectByTagMenu();
  void onTagSelection();
};

class ShapeTagEditDialog : public QDialog {
  Q_OBJECT

  QListWidget* m_tagList;
  QLineEdit* m_tagNameEdit;
  QPushButton* m_tagColorBtn;
  QComboBox* m_tagShapeCombo;

  ShapeTagInfo currentShapeTagInfo(int& id);

public:
  ShapeTagEditDialog(QWidget* parent = nullptr);

protected:
  void showEvent(QShowEvent* event) override;
protected slots:
  void onSelectedItemChanged();
  void updateTagList();

  void onTagNameChanged();
  void onTagColorBtnClicked();
  void onTagShapeComboActivated();
  void onAddTag();
  void onRemoveTag();
};

//--------
// 以下Undo

class EditShapeTagInfoUndo : public QUndoCommand {
  IwProject* m_project;
  int m_tagId;
  ShapeTagInfo m_before, m_after;

public:
  EditShapeTagInfoUndo(IwProject*, int tagId, ShapeTagInfo before,
                       ShapeTagInfo after);

  void undo();
  void redo();
};

class AddShapeTagUndo : public QUndoCommand {
  IwProject* m_project;
  ShapeTagInfo m_tagInfo;
  int m_tagId;

public:
  AddShapeTagUndo(IwProject*, ShapeTagInfo tagInfo);

  void undo();
  void redo();
};

class RemoveShapeTagUndo : public QUndoCommand {
  IwProject* m_project;
  ShapeTagInfo m_tagInfo;
  int m_tagId;
  int m_listPos;
  QList<OneShape> m_shapes;

public:
  RemoveShapeTagUndo(IwProject*, int tagId);

  void undo();
  void redo();
};
#endif