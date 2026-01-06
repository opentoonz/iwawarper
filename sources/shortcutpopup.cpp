

#include "shortcutpopup.h"

#include "iwapp.h"
#include "mainwindow.h"
#include "menubarcommand.h"
#include "menubarcommandids.h"

// Qt includes
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QSizePolicy>
#include <QPushButton>
#include <QPainter>
#include <QAction>
#include <QKeyEvent>
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QApplication>
#include <QTextStream>
#include <QGroupBox>
#include <QMessageBox>

// STD includes
#include <vector>

//=============================================================================
// ShortcutItem
// ------------
// Lo ShortcutTree visualizza ShortcutItem (organizzati in folder)
// ogni ShortcutItem rappresenta una QAction (e relativo Shortcut)
//-----------------------------------------------------------------------------

class ShortcutItem final : public QTreeWidgetItem {
  QAction* m_action;

public:
  ShortcutItem(QTreeWidgetItem* parent, QAction* action)
      : QTreeWidgetItem(parent, UserType), m_action(action) {
    setFlags(parent->flags());
    updateText();
  }
  void updateText() {
    QString text = m_action->text();
    // removing accelerator key indicator
    text = text.replace(QRegExp("&([^& ])"), "\\1");
    // removing doubled &s
    text = text.replace("&&", "&");
    setText(0, text);
    QString shortcut = m_action->shortcut().toString();
    setText(1, shortcut);
  }
  QAction* getAction() const { return m_action; }
};

//=============================================================================
// ShortcutViewer
//-----------------------------------------------------------------------------

ShortcutViewer::ShortcutViewer(QWidget* parent)
    : QKeySequenceEdit(parent), m_action(0), m_keysPressed(0) {
  setObjectName("ShortcutViewer");
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  connect(this, SIGNAL(editingFinished()), this, SLOT(onEditingFinished()));
}

//-----------------------------------------------------------------------------

ShortcutViewer::~ShortcutViewer() {}

//-----------------------------------------------------------------------------

void ShortcutViewer::setAction(QAction* action) {
  m_action = action;
  if (m_action) {
    setEnabled(true);
    setKeySequence(m_action->shortcut());
    setFocus();
  } else {
    setEnabled(false);
    setKeySequence(QKeySequence());
  }
}

//-----------------------------------------------------------------------------

void ShortcutViewer::keyPressEvent(QKeyEvent* event) {
  int key                         = event->key();
  Qt::KeyboardModifiers modifiers = event->modifiers();

  if (m_keysPressed == 0) {
    if (key == Qt::Key_Home || key == Qt::Key_End || key == Qt::Key_PageDown ||
        key == Qt::Key_PageUp || key == Qt::Key_Escape ||
        key == Qt::Key_Print || key == Qt::Key_Pause ||
        key == Qt::Key_ScrollLock) {
      event->ignore();
      return;
    }
  }

  m_keysPressed++;

  QKeySequenceEdit::keyPressEvent(event);
}

//-----------------------------------------------------------------------------

void ShortcutViewer::onEditingFinished() {
  // Get the current key sequence and its string representation
  QKeySequence keySeq = keySequence();
  QString keyStr      = keySeq.toString();

  // Reset the keys pressed counter (used for tracking multi-key sequences)
  m_keysPressed = 0;

  // Extract individual keys from the key sequence
  QVector<int> keys;
  for (int i = 0; i < keySeq.count(); i++) {
    keys.append(keySeq[i]);
  }

  // Check for conflicts with existing shortcuts
  if (m_action) {
    CommandManager* cm = CommandManager::instance();

    // Iterate through the key sequence to check for partial conflicts
    for (int i = 0; i < keys.size(); i++) {
      // Create a partial key sequence (e.g., k1, k1+k2, k1+k2+k3, etc.)
      QKeySequence partialSeq(keys[0], (i >= 1 ? keys[1] : 0),
                              (i >= 2 ? keys[2] : 0), (i >= 3 ? keys[3] : 0));

      // Check if the partial sequence conflicts with an existing shortcut
      QAction* conflictingAction =
          cm->getActionFromShortcut(partialSeq.toString().toStdString());

      if (conflictingAction == m_action) return;

      // If a conflict is found with another action
      if (conflictingAction) {
        QString msg;
        // Check if the conflict is with the full sequence or a partial sequence
        if (keys.size() == (i + 1)) {
          // Conflict with the full sequence
          msg = tr("'%1' is already assigned to '%2'\nAssign to '%3'?")
                    .arg(partialSeq.toString())
                    .arg(conflictingAction->iconText())
                    .arg(m_action->iconText());
        } else {
          // Conflict with a partial sequence
          msg = tr("Initial sequence '%1' is assigned to '%2' which takes "
                   "priority.\nAssign shortcut sequence anyway?")
                    .arg(partialSeq.toString())
                    .arg(conflictingAction->iconText());
        }

        // Show a warning message box to the user
        QMessageBox::StandardButton ret =
            QMessageBox::question(this, tr("Question"), msg);
        activateWindow();

        // If the user chooses "No" or closes the dialog, reset the shortcut and
        // exit
        if (ret != QMessageBox::Yes) {
          setKeySequence(
              m_action->shortcut());  // Reset to the original shortcut
          setFocus();                 // Set focus back to the widget
          return;
        }
      }
    }

    // If no conflicts are found, assign the new shortcut
    std::string shortcutString = keySeq.toString().toStdString();
    cm->setShortcut(m_action, shortcutString);
    emit shortcutChanged();
  }

  setKeySequence(keySeq);  // Update the displayed key sequence in the UI
}

//-----------------------------------------------------------------------------

void ShortcutViewer::removeShortcut() {
  if (m_action) {
    CommandManager::instance()->setShortcut(m_action, "", false);
    emit shortcutChanged();
    clear();
  }
}

//-----------------------------------------------------------------------------

void ShortcutViewer::enterEvent(QEvent* event) {
  setFocus();
  update();
}

//-----------------------------------------------------------------------------

void ShortcutViewer::leaveEvent(QEvent* event) { update(); }

//=============================================================================
// ShortcutTree
//-----------------------------------------------------------------------------

ShortcutTree::ShortcutTree(QWidget* parent) : QTreeWidget(parent) {
  setObjectName("ShortcutTree");
  setIndentation(14);
  setAlternatingRowColors(true);

  setColumnCount(2);
  header()->close();
  header()->setSectionResizeMode(0, QHeaderView::ResizeMode::Fixed);
  header()->setSectionResizeMode(1, QHeaderView::ResizeMode::ResizeToContents);
  header()->setDefaultSectionSize(300);
  // setStyleSheet("border-bottom:1px solid rgb(120,120,120); border-left:1px
  // solid rgb(120,120,120); border-top:1px solid rgb(120,120,120)");

  addFolder(tr("File"), MenuFileCommandType);
  addFolder(tr("Edit"), MenuEditCommandType);
  addFolder(tr("Shape"), MenuShapeCommandType);
  addFolder(tr("View"), MenuViewCommandType);
  addFolder(tr("Render"), MenuRenderCommandType);
  addFolder(tr("Tools"), ToolCommandType);
  //addFolder(tr("View Toggle"), ViewToggleCommandType);

  sortItems(0, Qt::AscendingOrder);

  connect(this, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
          this, SLOT(onCurrentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)));
  connect(this, SIGNAL(clicked(const QModelIndex&)), this,
          SLOT(onItemClicked(const QModelIndex&)));
}

//-----------------------------------------------------------------------------

ShortcutTree::~ShortcutTree() {}

//-----------------------------------------------------------------------------

void ShortcutTree::addFolder(const QString& title, int commandType,
                             QTreeWidgetItem* parentFolder) {
  QTreeWidgetItem* folder;
  if (!parentFolder) {
    folder = new QTreeWidgetItem(this);
    m_folders.push_back(folder);
  } else {
    folder = new QTreeWidgetItem(parentFolder);
    m_subFolders.push_back(folder);
  }
  assert(folder);
  folder->setText(0, title);

  std::vector<QAction*> actions;
  CommandManager::instance()->getActions((CommandType)commandType, actions);
  for (int i = 0; i < (int)actions.size(); i++) {
    ShortcutItem* item = new ShortcutItem(folder, actions[i]);
    m_items.push_back(item);
  }
}

//-----------------------------------------------------------------------------

void ShortcutTree::searchItems(const QString& searchWord) {
  if (searchWord.isEmpty()) {
    for (int i = 0; i < (int)m_items.size(); i++) m_items[i]->setHidden(false);
    for (int f = 0; f < m_subFolders.size(); f++) {
      m_subFolders[f]->setHidden(false);
      m_subFolders[f]->setExpanded(false);
    }
    for (int f = 0; f < m_folders.size(); f++) {
      m_folders[f]->setHidden(false);
      m_folders[f]->setExpanded(f == 0);
    }
    show();
    emit searched(true);
    update();
    return;
  }

  QList<QTreeWidgetItem*> foundItems =
      findItems(searchWord, Qt::MatchContains | Qt::MatchRecursive, 0);
  if (foundItems.isEmpty()) {
    hide();
    emit searched(false);
    update();
    return;
  }

  // show all matched items, hide all unmatched items
  for (int i = 0; i < (int)m_items.size(); i++)
    m_items[i]->setHidden(!foundItems.contains(m_items[i]));

  // hide folders which does not contain matched items
  // show and expand folders containing matched items
  bool found;
  for (int f = 0; f < m_subFolders.size(); f++) {
    QTreeWidgetItem* sf = m_subFolders.at(f);
    found               = false;
    for (int i = 0; i < sf->childCount(); i++) {
      if (!sf->child(i)->isHidden()) {
        found = true;
        break;
      }
    }
    sf->setHidden(!found);
    sf->setExpanded(found);
  }
  for (int f = 0; f < m_folders.size(); f++) {
    QTreeWidgetItem* fol = m_folders.at(f);
    found                = false;
    for (int i = 0; i < fol->childCount(); i++) {
      if (!fol->child(i)->isHidden()) {
        found = true;
        break;
      }
    }
    fol->setHidden(!found);
    fol->setExpanded(found);
  }

  show();
  emit searched(true);
  update();
}

//-----------------------------------------------------------------------------

void ShortcutTree::onCurrentItemChanged(QTreeWidgetItem* current,
                                        QTreeWidgetItem* previous) {
  ShortcutItem* item = dynamic_cast<ShortcutItem*>(current);
  emit actionSelected(item ? item->getAction() : 0);
}

//-----------------------------------------------------------------------------

void ShortcutTree::onShortcutChanged() {
  int i;
  for (i = 0; i < (int)m_items.size(); i++) m_items[i]->updateText();
}

//-----------------------------------------------------------------------------

void ShortcutTree::onItemClicked(const QModelIndex& index) {
  isExpanded(index) ? collapse(index) : expand(index);
}

//=============================================================================
// ShortcutPopup
//-----------------------------------------------------------------------------

ShortcutPopup::ShortcutPopup()
    : IwDialog(IwApp::instance()->getMainWindow(), "Shortcut", true) {
  setWindowTitle(tr("Configure Shortcuts"));
  m_list = new ShortcutTree(this);
  m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  m_sViewer   = new ShortcutViewer(this);
  m_removeBtn = new QPushButton(tr("Remove"), this);
  QLabel* noSearchResultLabel =
      new QLabel(tr("Couldn't find any matching command."), this);
  noSearchResultLabel->setHidden(true);

  QLineEdit* searchEdit = new QLineEdit(this);

  QVBoxLayout* topLayout = new QVBoxLayout();
  topLayout->setMargin(10);
  topLayout->setSpacing(8);
  {
    QHBoxLayout* searchLay = new QHBoxLayout();
    searchLay->setMargin(0);
    searchLay->setSpacing(5);
    {
      searchLay->addWidget(new QLabel(tr("Search:"), this), 0);
      searchLay->addWidget(searchEdit);
    }
    topLayout->addLayout(searchLay, 0);

    QVBoxLayout* listLay = new QVBoxLayout();
    listLay->setMargin(0);
    listLay->setSpacing(0);
    {
      listLay->addWidget(noSearchResultLabel, 0,
                         Qt::AlignTop | Qt::AlignHCenter);
      listLay->addWidget(m_list, 1);
    }
    topLayout->addLayout(listLay, 1);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setMargin(0);
    bottomLayout->setSpacing(1);
    {
      bottomLayout->addWidget(m_sViewer, 1);
      bottomLayout->addWidget(m_removeBtn, 0);
    }
    topLayout->addLayout(bottomLayout, 0);
  }
  setLayout(topLayout);

  connect(m_list, SIGNAL(actionSelected(QAction*)), m_sViewer,
          SLOT(setAction(QAction*)));

  connect(m_removeBtn, SIGNAL(clicked()), m_sViewer, SLOT(removeShortcut()));

  connect(m_sViewer, SIGNAL(shortcutChanged()), m_list,
          SLOT(onShortcutChanged()));

  connect(m_list, SIGNAL(searched(bool)), noSearchResultLabel,
          SLOT(setHidden(bool)));
  connect(searchEdit, SIGNAL(textChanged(const QString&)), this,
          SLOT(onSearchTextChanged(const QString&)));
}

//-----------------------------------------------------------------------------

ShortcutPopup::~ShortcutPopup() {}

//-----------------------------------------------------------------------------

void ShortcutPopup::onSearchTextChanged(const QString& text) {
  static bool busy = false;
  if (busy) return;
  busy = true;
  m_list->searchItems(text);
  busy = false;
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<ShortcutPopup> openShortcutPopup(MI_ShortcutPopup);
