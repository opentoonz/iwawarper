#pragma once

#ifndef SHORTCUTPOPUP_H
#define SHORTCUTPOPUP_H

#include <QDialog>
#include <QTreeWidget>
#include <QComboBox>
#include <QKeySequenceEdit>
#include "iwdialog.h"

// forward declaration
class QPushButton;
class QLabel;
class ShortcutItem;

//=============================================================================
// ShortcutViewer
// --------------
// E' l'editor dello shortcut associato all'azione corrente
// Visualizza lo shortcut e permette di cambiarlo digitando direttamente
// la nuova sequenza di tasti
// Per cancellarlo bisogna chiamare removeShortcut()
//-----------------------------------------------------------------------------

class ShortcutViewer final : public QKeySequenceEdit {
  Q_OBJECT
  QAction* m_action;

  int m_keysPressed;

public:
  ShortcutViewer(QWidget* parent);
  ~ShortcutViewer();

protected:
  void enterEvent(QEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
public slots:
  void setAction(QAction* action);
  void removeShortcut();
  void onEditingFinished();

signals:
  void shortcutChanged();
};

//=============================================================================
// ShortcutTree
// ------------
// Visualizza tutti le QAction (con gli eventuali shortcut assegnati)
// Serve per selezionare la QAction corrente
//-----------------------------------------------------------------------------

class ShortcutTree final : public QTreeWidget {
  Q_OBJECT
  std::vector<ShortcutItem*> m_items;
  std::vector<QTreeWidgetItem*> m_folders;
  std::vector<QTreeWidgetItem*> m_subFolders;

public:
  ShortcutTree(QWidget* parent = 0);
  ~ShortcutTree();

  void searchItems(const QString& searchWord = QString());

protected:
  // aggiunge un blocco di QAction. commandType e' un
  // CommandType::MenubarCommandType
  void addFolder(const QString& title, int commandType,
                 QTreeWidgetItem* folder = 0);

public slots:
  void onCurrentItemChanged(QTreeWidgetItem* current,
                            QTreeWidgetItem* previous);
  void onShortcutChanged();

  void onItemClicked(const QModelIndex&);

signals:
  void actionSelected(QAction*);
  void searched(bool haveResult);
};

//=============================================================================
// ShortcutPopup
// -------------
// Questo e' il popup che l'utente utilizza per modificare gli shortcut
//-----------------------------------------------------------------------------

class ShortcutPopup final : public IwDialog {
  Q_OBJECT
  QPushButton* m_removeBtn;
  ShortcutViewer* m_sViewer;
  ShortcutTree* m_list;

public:
  ShortcutPopup();
  ~ShortcutPopup();

protected slots:
  void onSearchTextChanged(const QString& text);
};

#endif  //  SHORTCUTPOPUP_H
