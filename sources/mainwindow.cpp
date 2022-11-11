#include "mainwindow.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"
#include "toolcommandids.h"
#include "iocommand.h"
#include "iwapp.h"
#include "iwundomanager.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "projectutils.h"
#include "iwimagecache.h"

#include "iwlayerhandle.h"

#include "toolbox.h"
#include "mainstatusbar.h"
#include "viewsettings.h"
#include "iwfolders.h"
#include "iwshapepairselection.h"
#include "iwselectionhandle.h"
#include "aboutpopup.h"

#include <QString>
#include <QMenuBar>
#include <QActionGroup>
#include <QMessageBox>
#include <QEvent>
#include <iostream>
#include <QList>
#include <QApplication>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QLabel>
#include <QSettings>
#include <QCheckBox>
#include <QPainter>
#include <QToolButton>

#include "timelinewindow.h"
#include "tooloptionpanel.h"
#include "layeroptionpanel.h"
#include "sceneviewer.h"
#include "ruler.h"
#include "shapetreewindow.h"
#include "iwtrianglecache.h"
#include "preferences.h"

namespace {
const QString getAppStr() {
  return QString("%1 Version %2  Built %3")
      .arg(qApp->applicationName())
      .arg(qApp->applicationVersion())
      .arg(__DATE__);
}
}  // namespace

class Separator : public QFrame {
public:
  Separator(QWidget *parent) { setMinimumSize(1, 15); }

  void paintEvent(QPaintEvent *) {
    QPainter p(this);
    QColor lineColor = palette().alternateBase().color();
    lineColor.setAlpha(128);
    p.setPen(lineColor);
    p.drawLine(0, 0, 0, contentsRect().bottom());
  }
};

//---------------------------------------------------
// メニューバーの作成
//---------------------------------------------------
QWidget *MainWindow::createMenuBarWidget() {
  QWidget *widget = new QWidget(this);

  QMenuBar *menuBar = new QMenuBar(this);

  QMenu *fileMenu = new QMenu(tr("&File"), menuBar);
  {
    addActionToMenu(fileMenu, MI_NewProject);
    addActionToMenu(fileMenu, MI_OpenProject);
    addActionToMenu(fileMenu, MI_OpenRecentProject);
    addActionToMenu(fileMenu, MI_ImportProject);
    addActionToMenu(fileMenu, MI_SaveProject);
    addActionToMenu(fileMenu, MI_SaveAsProject);
    addActionToMenu(fileMenu, MI_SaveProjectWithDateTime);
    addActionToMenu(fileMenu, MI_ExportSelectedShapes);
    fileMenu->addSeparator();
    addActionToMenu(fileMenu, MI_InsertImages);
    fileMenu->addSeparator();
    addActionToMenu(fileMenu, MI_Preferences);
    fileMenu->addSeparator();
    addActionToMenu(fileMenu, MI_Exit);
  }
  menuBar->addMenu(fileMenu);

  QMenu *editMenu = new QMenu(tr("&Edit"), menuBar);
  {
    addActionToMenu(editMenu, MI_Undo);
    addActionToMenu(editMenu, MI_Redo);
    editMenu->addSeparator();
    addActionToMenu(editMenu, MI_Cut);
    addActionToMenu(editMenu, MI_Copy);
    addActionToMenu(editMenu, MI_Paste);
    addActionToMenu(editMenu, MI_Delete);
    addActionToMenu(editMenu, MI_SelectAll);
    editMenu->addSeparator();
    addActionToMenu(editMenu, MI_Duplicate);
  }
  menuBar->addMenu(editMenu);

  QMenu *viewMenu = new QMenu(tr("&View"), menuBar);
  {
    addActionToMenu(viewMenu, MI_ZoomIn);
    addActionToMenu(viewMenu, MI_ZoomOut);
    addActionToMenu(viewMenu, MI_ActualSize);
    // viewMenu->addSeparator();
    // addActionToMenu(viewMenu, MI_Colors);
    viewMenu->addSeparator();
    addActionToMenu(viewMenu, MI_FileInfo);
  }
  menuBar->addMenu(viewMenu);

  QMenu *renderMenu = new QMenu(tr("&Render"), menuBar);
  {
    addActionToMenu(renderMenu, MI_Preview);
    addActionToMenu(renderMenu, MI_Render);
    renderMenu->addSeparator();
    addActionToMenu(renderMenu, MI_OutputOptions);
    renderMenu->addSeparator();
    addActionToMenu(renderMenu, MI_ClearMeshCache);
    addActionToMenu(renderMenu, MI_RenderShapeImage);
  }
  menuBar->addMenu(renderMenu);

  // その他のパーツ
  m_viewModeCombo     = new QComboBox(this);
  QFrame *fromIconFrm = new QFrame(this);
  m_fromShow          = new QCheckBox(tr("Show From"), this);
  QFrame *toIconFrm   = new QFrame(this);
  m_toShow            = new QCheckBox(tr("Show To"), this);
  QFrame *meshIconFrm = new QFrame(this);
  m_meshShow          = new QCheckBox(tr("Show Mesh"), this);

  // ワークエリアのサイズ
  m_workAreaWidthEdit  = new QLineEdit(this);
  m_workAreaHeightEdit = new QLineEdit(this);

  fromIconFrm->setFrameStyle(QFrame::StyledPanel);
  toIconFrm->setFrameStyle(QFrame::StyledPanel);
  meshIconFrm->setFrameStyle(QFrame::StyledPanel);
  fromIconFrm->setFixedSize(8, 17);
  toIconFrm->setFixedSize(8, 17);
  meshIconFrm->setFixedSize(8, 17);
  fromIconFrm->setStyleSheet(QString("background:rgba(255,0,0,255);"));
  toIconFrm->setStyleSheet(QString("background:rgba(0,0,255,255);"));
  meshIconFrm->setStyleSheet(QString("background:rgba(128,255,0,255);"));

  QStringList items;
  items << "Edit"
        << "Half"
        << "Preview";
  m_viewModeCombo->addItems(items);
  // ワークエリアのサイズにバリデータを当てる
  QIntValidator *validator = new QIntValidator(1, 10000, this);
  m_workAreaWidthEdit->setValidator(validator);
  m_workAreaHeightEdit->setValidator(validator);

  QToolButton *miscBtn = new QToolButton(this);
  QMenu *miscMenu      = new QMenu();
  QAction *about = new QAction(tr("About %1").arg(qApp->applicationName()));
  miscBtn->setText(tr("Misc. Settings"));
  miscBtn->setIcon(QIcon(":Resources/menu.svg"));
  miscBtn->setPopupMode(QToolButton::MenuButtonPopup);
  miscBtn->setPopupMode(QToolButton::InstantPopup);
  miscBtn->setMenu(miscMenu);

  QMenu *languageMenu = setupLanguageMenu();

  miscMenu->addMenu(languageMenu);
  miscMenu->addSeparator();
  miscMenu->addAction(about);

  IwProject *prj = IwApp::instance()->getCurrentProject()->getProject();
  if (prj) {
    m_viewModeCombo->setCurrentIndex(
        (int)prj->getViewSettings()->getImageMode());
    m_fromShow->setChecked(prj->getViewSettings()->isFromToVisible(0));
    m_toShow->setChecked(prj->getViewSettings()->isFromToVisible(1));
    m_meshShow->setChecked(prj->getViewSettings()->isMeshVisible());
    QSize workAreaSize = prj->getWorkAreaSize();
    m_workAreaWidthEdit->setText(QString::number(workAreaSize.width()));
    m_workAreaHeightEdit->setText(QString::number(workAreaSize.height()));
  }
  QHBoxLayout *lay = new QHBoxLayout();
  lay->setMargin(0);
  lay->setSpacing(5);
  {
    lay->addWidget(menuBar);
    lay->addSpacing(20);
    lay->addWidget(new Separator(this), 0);
    lay->addWidget(new QLabel(tr("Mode:"), this), 0);
    lay->addWidget(m_viewModeCombo);
    lay->addSpacing(20);
    lay->addWidget(fromIconFrm, 0);
    lay->addWidget(m_fromShow, 0);
    lay->addSpacing(10);
    lay->addWidget(toIconFrm, 0);
    lay->addWidget(m_toShow, 0);
    lay->addSpacing(10);
    lay->addWidget(meshIconFrm, 0);
    lay->addWidget(m_meshShow, 0);
    lay->addSpacing(20);
    lay->addWidget(new Separator(this), 0);
    lay->addWidget(new QLabel(tr("Work Area:")), 0);
    QHBoxLayout *workAreaLay = new QHBoxLayout();
    workAreaLay->setSpacing(3);
    workAreaLay->setMargin(0);
    {
      workAreaLay->addWidget(new QLabel(tr("W:"), this), 0);
      workAreaLay->addWidget(m_workAreaWidthEdit, 1);
      workAreaLay->addSpacing(3);
      workAreaLay->addWidget(new QLabel(tr("H:"), this), 0);
      workAreaLay->addWidget(m_workAreaHeightEdit, 1);
    }
    lay->addLayout(workAreaLay, 0);
    lay->addStretch(1);
    lay->addWidget(miscBtn);
  }
  widget->setLayout(lay);

  bool ret = true;
  // プレビューの計算が終わったらPreview表示モードにして表示を更新
  ret = ret && connect(IwApp::instance()->getCurrentProject(),
                       SIGNAL(previewRenderFinished(int)), this,
                       SLOT(onPreviewRenderCompleted(int)));

  // コンボボックスが切り替わったら表示を更新
  ret = ret && connect(m_viewModeCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onViewModeComboActivated(int)));

  ret = ret &&
        connect(IwApp::instance()->getCurrentProject(),
                SIGNAL(projectSwitched()), this, SLOT(onProjectSwitched()));
  // ワークエリアのサイズ
  ret =
      ret && connect(IwApp::instance()->getCurrentProject(),
                     SIGNAL(projectChanged()), this, SLOT(onProjectChanged()));

  // ロック操作
  ret = ret && connect(m_fromShow, SIGNAL(clicked(bool)), this,
                       SLOT(onFromShowClicked(bool)));
  ret = ret && connect(m_toShow, SIGNAL(clicked(bool)), this,
                       SLOT(onToShowClicked(bool)));
  // メッシュの表示
  ret = ret && connect(m_meshShow, SIGNAL(clicked(bool)), this,
                       SLOT(onMeshShowClicked(bool)));

  // ワークエリアサイズの値が編集されたとき、プロジェクトの内容を更新する
  ret = ret && connect(m_workAreaWidthEdit, SIGNAL(editingFinished()), this,
                       SLOT(onWorkAreaSizeEdited()));
  ret = ret && connect(m_workAreaHeightEdit, SIGNAL(editingFinished()), this,
                       SLOT(onWorkAreaSizeEdited()));

  ret = ret &&
        connect(about, SIGNAL(triggered()), this, SLOT(onAboutTriggered()));
  assert(ret);
  return widget;
}

//----------------------------------------------
// プレビューの計算が終わったらPreview表示モードにして表示を更新
//----------------------------------------------
void MainWindow::onPreviewRenderCompleted(int frame) {
  // プロジェクトを取得
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // 表示設定を取得
  ViewSettings *settings = project->getViewSettings();
  if (settings->getFrame() != frame) return;
  // Halfの場合はそのまま
  if (m_viewModeCombo->currentIndex() != 1 &&
      m_viewModeCombo->currentIndex() != 2)
    m_viewModeCombo->setCurrentIndex(2);
  // すでにPreview表示モードの場合も、Viewerの更新だけはする
  else
    // ViewSettingsが変わったシグナルをemit → SceneViewerをupdate
    IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
}

//----------------------------------------------
// コンボボックスが切り替わったら表示を更新
//----------------------------------------------
void MainWindow::onViewModeComboActivated(int index) {
  // プロジェクトを取得
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // 表示設定を取得
  ViewSettings *settings = project->getViewSettings();

  IMAGEMODE newMode = (IMAGEMODE)index;
  // 現在のViewSettingsと同じ設定のときはなにもせず
  if (settings->getImageMode() == newMode) return;

  // 違う場合は、ViewSettingsを書き換えてシグナルをEmit
  settings->setImageMode(newMode);

  IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
}

//---------------------------------------------------
// アクションの登録
//---------------------------------------------------
void MainWindow::defineActions() {
  //---- メインウィンドウ メニューバーコマンド
  //---- Sequence Editor メニューバーコマンド
  // File Menu
  createAction(MI_NewProject, tr("&New Project"), "Ctrl+N",
               MenuFileCommandType);
  createAction(MI_OpenProject, tr("&Open Project..."), "Ctrl+O",
               MenuFileCommandType);
  createMenuAction(MI_OpenRecentProject, tr("&Open Recent Project File"));
  createAction(MI_ClearRecentProject, tr("&Clear Recent Project File List"),
               "");

  createAction(MI_ImportProject, tr("&Import Project/Shapes..."), "");

  createAction(MI_SaveProject, tr("&Save Project"), "Ctrl+S",
               MenuFileCommandType);
  createAction(MI_SaveAsProject, tr("Save Project &As..."), "",
               MenuFileCommandType);
  createAction(MI_SaveProjectWithDateTime,
               tr("Save Project with &Date && Time"), "Ctrl+Shift+S",
               MenuFileCommandType);
  createAction(MI_InsertImages, tr("&Insert Images..."), "Ctrl+L",
               MenuFileCommandType);
  createAction(MI_ExportSelectedShapes, tr("&Export Selected Shapes..."), "",
               MenuFileCommandType);
  createAction(MI_Preferences, tr("Settings..."), "", MenuFileCommandType);
  createAction(MI_Exit, tr("E&xit"), "Ctrl+Q", MenuFileCommandType);

  // Edit Menu
  createAction(MI_Undo, tr("&Undo"), "Ctrl+Z", MenuEditCommandType);
  createAction(MI_Redo, tr("&Redo"), "Ctrl+Shift+Z", MenuEditCommandType);
  createAction(MI_Cut, tr("Cu&t"), "Ctrl+X", MenuEditCommandType);
  createAction(MI_Copy, tr("&Copy"), "Ctrl+C", MenuEditCommandType);
  createAction(MI_Paste, tr("&Paste"), "Ctrl+V", MenuEditCommandType);
  createAction(MI_Delete, tr("&Delete"), "Delete", MenuEditCommandType);
  createAction(MI_SelectAll, tr("Select &All"), "Ctrl+A", MenuEditCommandType);
  createAction(MI_Duplicate, tr("Dup&licate"), "Ctrl+D", MenuEditCommandType);
  createAction(MI_Key, tr("&Key"), "Ctrl+K",
               MenuEditCommandType);  // KeyFrameEditorのみ
  createAction(MI_Unkey, tr("&Unkey"), "Ctrl+U",
               MenuEditCommandType);  // KeyFrameEditorのみ
  createAction(MI_ResetInterpolation, tr("Reset &Interpolation to Linear"), "",
               MenuEditCommandType);

  // View Menu
  createAction(MI_ZoomIn, tr("&Zoom In"), "Z", MenuViewCommandType);
  createAction(MI_ZoomOut, tr("Zoom &Out"), "X", MenuViewCommandType);
  createAction(MI_ActualSize, tr("Actual Si&ze"), "", MenuViewCommandType);
  // createAction(MI_Colors, tr("Co&lors"), "Ctrl+Shift+C",
  // MenuViewCommandType);

  createAction(MI_FileInfo, tr("File &Info"), "Ctrl+I", MenuViewCommandType);

  // Render Menu
  createAction(MI_Preview, tr("&Preview"), "Ctrl+P", MenuRenderCommandType);
  createAction(MI_Render, tr("&Render"), "Ctrl+R", MenuRenderCommandType);
  createAction(MI_OutputOptions, tr("&Output Options"), "Ctrl+Shift+O",
               MenuRenderCommandType);

  createAction(MI_RenderShapeImage, tr("&Render Shape Image"), "",
               MenuRenderCommandType);

  createAction(MI_ClearMeshCache, tr("&Clear All Mesh Cache "), "",
               MenuRenderCommandType);
  //---- ツールバーのコマンド
  createToolAction(T_Transform, "transform", tr("Transform tool"), "T");
  createToolAction(T_Reshape, "reshape", tr("Reshape tool"), "R");
  createToolAction(T_Correspondence, "correspondence",
                   tr("Correspondence tool"), "C");
  createToolAction(T_Pen, "pen", tr("Pen tool"), "P");
  createToolAction(T_Square, "square", tr("Square tool"), "Shift+S");
  createToolAction(T_Circle, "circle", tr("Circle tool"), "Shift+C");
  createToolAction(T_Freehand, "freehand", tr("Freehand tool"), "F");

  // 上側の表示コントロール
  // createToggleWithIcon(VMI_MultiFrameMode, "multiframemode",
  //                     tr("Multi-Frame mode"), "Shift+T",
  //                     ViewToggleCommandType);

  createAction(MI_InsertEmpty, tr("Insert Empty Frame"), "",
               MenuEditCommandType);
  createAction(MI_ReplaceImages, tr("Replace Images"), "", MenuEditCommandType);
  createAction(MI_ReloadImages, tr("Reload Images"), "", MenuEditCommandType);

  createAction(MI_NextFrame, tr("&Next Frame"), ".", MenuViewCommandType);
  createAction(MI_PreviousFrame, tr("&Previous Frame"), ",",
               MenuViewCommandType);

  createAction(MI_ToggleShapeLock, tr("Toggle Lock of Selected Shapes"), "",
               MenuShapeCommandType);
  createAction(MI_ToggleVisibility, tr("Toggle Visibility of Selected Shapes"),
               "", MenuShapeCommandType);
}

//---------------------------------------------------
// アクションの登録（ひとつずつ呼ぶ）
//---------------------------------------------------
QAction *MainWindow::createAction(const char *id, const QString &name,
                                  const QString &defaultShortcut,
                                  CommandType type) {
  QAction *action = new MyAction(name, this);
  addAction(action);
  CommandManager::instance()->define(id, type, defaultShortcut.toStdString(),
                                     action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuAction(const char *id, const QString &name) {
  QMenu *menu     = new QMenu(name, this);
  QAction *action = menu->menuAction();
  CommandManager::instance()->define(id, MenuFileCommandType, "", action);
  action->setData(-1);
  return action;
}

//---------------------------------------------------
// トグル可、アイコン付きのアクションを作る
//---------------------------------------------------
QAction *MainWindow::createToggleWithIcon(const char *id,
                                          const QString &iconName,
                                          const QString &name,
                                          const QString &defaultShortcut,
                                          CommandType type) {
  QIcon icon      = createIcon(iconName);
  QAction *action = new MyAction(icon, name, this);
  action->setCheckable(true);

  CommandManager::instance()->define(id, type, defaultShortcut.toStdString(),
                                     action);
  return action;
}

//---------------------------------------------------
// ツールコマンドの登録（ひとつずつ呼ぶ）
//---------------------------------------------------
QAction *MainWindow::createToolAction(const char *id, const char *iconName,
                                      const QString &name,
                                      const QString &defaultShortcut) {
  QAction *action = createToggleWithIcon(id, QString("tool_") + iconName, name,
                                         defaultShortcut, ToolCommandType);
  action->setActionGroup(m_toolsActionGroup);

  // When the viewer is maximized (not fullscreen) the toolbar is hided and the
  // action are disabled, so the tool shortcuts don't work. Adding tool actions
  // to the main window solve this problem!
  addAction(action);

  action->setEnabled(true);

  return action;
}

// アイコンを返す
QIcon MainWindow::createIcon(const QString &iconName) {
  QString normalResource = QString(":Resources/") + iconName + ".svg";
  QString overResource   = QString(":Resources/") + iconName + "_rollover.svg";
  QIcon icon;
  icon.addFile(normalResource, QSize(), QIcon::Normal, QIcon::Off);
  icon.addFile(overResource, QSize(), QIcon::Normal, QIcon::On);
  icon.addFile(overResource, QSize(), QIcon::Active);
  return icon;
}

void MainWindow::addActionToMenu(QMenu *menu, CommandId id) {
  menu->addAction(CommandManager::instance()->getAction(id));
}

//---------------------------------------------------
void MainWindow::onExit() { close(); }

//---------------------------------------------------
void MainWindow::onNewProject() {
  if (!IoCmd::saveProjectIfNeeded(tr("New Project"))) {
    return;
  }
  IoCmd::newProject();

  IwProject *prj = IwApp::instance()->getCurrentProject()->getProject();
  if (prj) m_viewer->setProject(prj);
}

//---------------------------------------------------
// Undo, Redo
//---------------------------------------------------
void MainWindow::onUndo() {
  // これ以上Undoできなければ警告を出してreturn
  if (!IwUndoManager::instance()->canUndo()) {
    QMessageBox::warning(this, tr("Undo"),
                         tr("No more Undo operations available."),
                         QMessageBox::Ok);
    return;
  }

  // Undo
  IwUndoManager::instance()->undo();
}

void MainWindow::onRedo() {
  // これ以上Redoできなければ警告を出してreturn
  if (!IwUndoManager::instance()->canRedo()) {
    QMessageBox::warning(this, tr("Redo"),
                         tr("No more Redo operations available."),
                         QMessageBox::Ok);
    return;
  }

  // Undo
  IwUndoManager::instance()->redo();
}

//---------------------------------------------------
// プロジェクトの保存/ロード
//---------------------------------------------------
void MainWindow::onOpenProject() {
  if (!IoCmd::saveProjectIfNeeded(tr("Open Project"))) {
    return;
  }
  // ファイルダイアログを開く
  // 無効なパス/キャンセルの場合はreturn
  // プロジェクトをロード
  IoCmd::loadProject();
}
//---------------------------------------------------
void MainWindow::onImportProject() { IoCmd::importProject(); }

void MainWindow::onProjectSwitched() {
  IwProject *prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  m_viewer->setProject(prj);

  m_viewModeCombo->setCurrentIndex((int)prj->getViewSettings()->getImageMode());
  m_fromShow->setChecked(prj->getViewSettings()->isFromToVisible(0));
  m_toShow->setChecked(prj->getViewSettings()->isFromToVisible(1));
  m_meshShow->setChecked(prj->getViewSettings()->isMeshVisible());
}

void MainWindow::onProjectChanged() {
  IwProject *prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  QSize workAreaSize = prj->getWorkAreaSize();
  m_workAreaWidthEdit->setText(QString::number(workAreaSize.width()));
  m_workAreaHeightEdit->setText(QString::number(workAreaSize.height()));
}

void MainWindow::onSaveProject() {
  // 現在のプロジェクトの、保存ファイルパスがあるか？
  // あれば、そのパスにプロジェクトを保存
  // 無ければ、SaveAsと同じ処理
  IwProject *prj = IwApp::instance()->getCurrentProject()->getProject();
  if (prj) {
    IoCmd::saveProject(prj->getPath());
  }
}

void MainWindow::onSaveAsProject() {
  // ファイルダイアログで保存先を指定
  // プロジェクトを保存
  IoCmd::saveProject();
}

void MainWindow::onInsertImages() { IoCmd::insertImagesToSequence(); }

void MainWindow::onSaveProjectWithDateTime() {
  IwProject *prj = IwApp::instance()->getCurrentProject()->getProject();
  if (prj) {
    IoCmd::saveProjectWithDateTime(prj->getPath());
  }
}

//---------------------------------------------------
// MainWindow コンストラクタ
//---------------------------------------------------
MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags) {
  // ツールバーコマンドのグループの作成
  m_toolsActionGroup = new QActionGroup(this);
  m_toolsActionGroup->setExclusive(true);

  defineActions();

  IwApp::instance()->setMainWindow(this);

  setWindowTitle(getAppStr());

  //---- オブジェクトの宣言
  // 左側のツールボックス
  m_toolBox = new ToolBoxDock(this);
  // 中心のワークエリア
  m_viewer                 = new SceneViewer(this);
  Ruler *hRuler            = new Ruler(this, m_viewer, false);
  Ruler *vRuler            = new Ruler(this, m_viewer, true);
  QWidget *viewerContainer = new QWidget(this);
  QGridLayout *lay         = new QGridLayout();
  lay->setMargin(0);
  lay->setSpacing(0);
  {
    lay->addWidget(hRuler, 0, 1);
    lay->addWidget(vRuler, 1, 0);
    lay->addWidget(m_viewer, 1, 1);
  }
  lay->setColumnStretch(0, 0);
  lay->setColumnStretch(1, 1);
  lay->setRowStretch(0, 0);
  lay->setRowStretch(1, 1);
  viewerContainer->setLayout(lay);
  m_viewer->setRulers(hRuler, vRuler);

  // ステータスバーとフレームスライダ
  m_mainStatusBar = new MainStatusBar(this);

  // NEW タイムライン
  m_timeLineWindow = new TimeLineWindow(this);

  // ツールオプション
  m_toolOptionPanel = new ToolOptionPanel(this);
  // レイヤーオプション
  m_layerOptionPanel = new LayerOptionPanel(this);
  // シェイプツリーウィンドウ
  m_shapeTreeWindow = new ShapeTreeWindow(this);

  //---- レイアウト

  setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);

  // メニューバーの作成、セット
  setMenuWidget(createMenuBarWidget());
  // 左のツールボックス
  addDockWidget(Qt::LeftDockWidgetArea, m_toolBox);
  // addToolBar(Qt::LeftToolBarArea, m_toolBox);
  // 中心のワークエリア
  setCentralWidget(viewerContainer);
  // ステータスバーとフレームスライダ
  addToolBar(Qt::BottomToolBarArea, m_mainStatusBar);

  // ツールオプション
  addDockWidget(Qt::BottomDockWidgetArea, m_toolOptionPanel);
  addDockWidget(Qt::BottomDockWidgetArea, m_layerOptionPanel);
  tabifyDockWidget(m_layerOptionPanel, m_toolOptionPanel);
  // NEW タイムライン
  addDockWidget(Qt::BottomDockWidgetArea, m_timeLineWindow, Qt::Horizontal);

  // シェイプツリーウィンドウ
  addDockWidget(Qt::RightDockWidgetArea, m_shapeTreeWindow);

  RecentFiles::instance()->loadRecentFiles();

  // Transformツールにする
  CommandManager::instance()->execute(T_Transform);

  setCommandHandler(MI_Exit, this, &MainWindow::onExit);
  setCommandHandler(MI_NewProject, this, &MainWindow::onNewProject);
  setCommandHandler(MI_OpenProject, this, &MainWindow::onOpenProject);
  setCommandHandler(MI_ImportProject, this, &MainWindow::onImportProject);
  setCommandHandler(MI_SaveProject, this, &MainWindow::onSaveProject);
  setCommandHandler(MI_SaveAsProject, this, &MainWindow::onSaveAsProject);
  setCommandHandler(MI_SaveProjectWithDateTime, this,
                    &MainWindow::onSaveProjectWithDateTime);
  setCommandHandler(MI_InsertImages, this, &MainWindow::onInsertImages);
  setCommandHandler(MI_Undo, this, &MainWindow::onUndo);
  setCommandHandler(MI_Redo, this, &MainWindow::onRedo);

  setCommandHandler(MI_NextFrame, this, &MainWindow::onNextFrame);
  setCommandHandler(MI_PreviousFrame, this, &MainWindow::onPreviousFrame);
  setCommandHandler(MI_ClearMeshCache, this, &MainWindow::onClearMeshCache);

  // signal-slot connection
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(dirtyFlagChanged()),
          this, SLOT(updateTitle()));

  QString fp = IwFolders::getMyProfileFolderPath() + "/mainwindow.ini";
  QSettings settings(fp, QSettings::IniFormat);
  restoreGeometry(settings.value("MainWindowGeometry").toByteArray());

  showMessageOnStatusBar(tr("IwaWarper launched."), 10000);
}

//---------------------------------------------------
// ステータスバーにメッセージを表示する
//---------------------------------------------------
void MainWindow::showMessageOnStatusBar(const QString &message, int timeout) {
  m_mainStatusBar->showMessageOnStatusBar(message, timeout);
}

//---------------------------------------------------
// タイトルバーの更新 プロジェクト名とズーム倍率
//---------------------------------------------------
void MainWindow::updateTitle() {
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) {
    setWindowTitle(getAppStr());
    return;
  }
  QString fileName;
  QString path = project->getPath();
  if (path.isEmpty())
    fileName = tr("Untitled");
  else
    fileName = path.right(path.size() - path.lastIndexOf("/") - 1);

  if (IwApp::instance()->getCurrentProject()->isDirty()) fileName += " *";

  int zoomStep          = project->getViewSettings()->getZoomStep();
  double zoomPercentage = 100.0 * powf(2.0, (double)zoomStep);

  QString titleStr = QString("%1 | Zoom:%2% | %3")
                         .arg(fileName)
                         .arg(zoomPercentage)
                         .arg(getAppStr());

  setWindowTitle(titleStr);
}

//---------------------------------------------------
// フレーム移動
//---------------------------------------------------
void MainWindow::onNextFrame() {
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  if (project->getProjectFrameLength() == 0 ||
      project->getViewFrame() == project->getProjectFrameLength() - 1)
    return;

  project->setViewFrame(project->getViewFrame() + 1);
}

void MainWindow::onPreviousFrame() {
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  if (project->getProjectFrameLength() == 0 || project->getViewFrame() == 0)
    return;

  project->setViewFrame(project->getViewFrame() - 1);
}

void MainWindow::onClearMeshCache() {
  IwTriangleCache::instance()->removeAllCache();
  IwApp::instance()->getCurrentProject()->notifyPreviewCacheInvalidated();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (!IoCmd::saveProjectIfNeeded(tr("Exit"))) {
    event->ignore();
    return;
  }

  // Main window settings
  QString fp = IwFolders::getMyProfileFolderPath() + "/mainwindow.ini";
  QSettings settings(fp, QSettings::IniFormat);
  settings.setValue("MainWindowGeometry", saveGeometry());

  IwImageCache::instance()->clear();
  m_viewer->deleteTextures();
}

//----------------------------------------------
// ロック操作
//----------------------------------------------
void MainWindow::onFromShowClicked(bool checked) {
  IwProject *prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  prj->getViewSettings()->setIsFromToVisible(0, checked);

  if (!checked && IwShapePairSelection::instance()->getCurrent() ==
                      IwShapePairSelection::instance()) {
    // FromTo指定で選択を解除
    if (IwShapePairSelection::instance()->removeFromToShapes(0))
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
  }

  IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
}
//----------------------------------------------
void MainWindow::onToShowClicked(bool checked) {
  IwProject *prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  prj->getViewSettings()->setIsFromToVisible(1, checked);

  if (!checked && IwShapePairSelection::instance()->getCurrent() ==
                      IwShapePairSelection::instance()) {
    // FromTo指定で選択を解除
    if (IwShapePairSelection::instance()->removeFromToShapes(1))
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
  }

  IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
}
//----------------------------------------------
void MainWindow::onMeshShowClicked(bool checked) {
  IwProject *prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  prj->getViewSettings()->setMeshVisible(checked);

  IwApp::instance()->getCurrentProject()->notifyViewSettingsChanged();
}

//------------------------------------
// ワークエリアサイズの値が編集されたとき、プロジェクトの内容を更新する
//------------------------------------
void MainWindow::onWorkAreaSizeEdited() {
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  static bool isBusy = false;
  if (isBusy) return;
  isBusy                = true;
  QSize oldWorkAreaSize = project->getWorkAreaSize();
  QSize newWorkAreaSize = QSize(m_workAreaWidthEdit->text().toInt(),
                                m_workAreaHeightEdit->text().toInt());

  // もし内容が違っていたら、内容を更新してシグナルをエミット
  if (oldWorkAreaSize != newWorkAreaSize) {
    bool accepted = ProjectUtils::resizeWorkArea(newWorkAreaSize);
    // キャンセルされた場合、フィールド値をもとにもどす
    if (!accepted) {
      m_workAreaWidthEdit->setText(QString::number(oldWorkAreaSize.width()));
      m_workAreaHeightEdit->setText(QString::number(oldWorkAreaSize.height()));
    }
  }
  isBusy = false;
}

//------------------------------------

void MainWindow::onAboutTriggered() {
  AboutPopup aboutPopup(this);
  aboutPopup.exec();
}

//------------------------------------

QMenu *MainWindow::setupLanguageMenu() {
  QMenu *langMenu = new QMenu(tr("Language"));

  // 設定ファイルから現在の言語を確認
  QString currentLang = Preferences::instance()->language();

  QActionGroup *langGroup = new QActionGroup(this);
  QAction *engAction      = langMenu->addAction("English");
  langGroup->addAction(engAction);
  engAction->setData("English");
  engAction->setCheckable(true);
  engAction->setChecked(currentLang == "English");

  // stuff/locのフォルダ一覧
  QDir locDir(IwFolders::getLocalizationFolderPath());
  QStringList folders = locDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  folders.prepend("English");
  for (auto folder : folders) {
    if (!locDir.cd(folder)) continue;
    if (locDir.exists("IwaWarper.qm")) {
      QAction *locAction = langMenu->addAction(folder);
      langGroup->addAction(locAction);
      locAction->setData(folder);
      locAction->setCheckable(true);
      locAction->setChecked(currentLang == folder);
    }
    locDir.cdUp();
  }

  connect(langGroup, SIGNAL(triggered(QAction *)), this,
          SLOT(onLanguageChanged(QAction *)));

  return langMenu;
}

//------------------------------------

void MainWindow::onLanguageChanged(QAction *action) {
  QString langName = action->data().toString();
  Preferences::instance()->setLanguage(langName);

  QMessageBox msgBox;
  msgBox.setText(
      tr("Changes will take effect the next time you run IwaWarper."));
  msgBox.exec();
}

//=============================================================================
// RecentFiles
//=============================================================================

RecentFiles::RecentFiles() : m_recentFiles() {
  QAction *act = CommandManager::instance()->getAction(MI_OpenRecentProject);
  if (!act) return;
  QMenu *menu = act->menu();
  if (!menu) return;

  connect(menu, SIGNAL(triggered(QAction *)), this,
          SLOT(onMenuTriggered(QAction *)));
}

//-----------------------------------------------------------------------------

RecentFiles *RecentFiles::instance() {
  static RecentFiles _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

RecentFiles::~RecentFiles() {}

//-----------------------------------------------------------------------------

void RecentFiles::addPath(QString path) {
  int i;
  for (i = 0; i < m_recentFiles.size(); i++)
    if (m_recentFiles.at(i) == path) {
      m_recentFiles.removeAt(i);
    }
  m_recentFiles.insert(0, path);
  int maxSize = 10;
  if (m_recentFiles.size() > maxSize) m_recentFiles.removeAt(maxSize);

  refreshRecentFilesMenu();
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

QString RecentFiles::getPath(int index) const { return m_recentFiles[index]; }

//-----------------------------------------------------------------------------

void RecentFiles::clearRecentFilesList() {
  m_recentFiles.clear();
  refreshRecentFilesMenu();
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::loadRecentFiles() {
  QString fp = IwFolders::getMyProfileFolderPath() + "/recentfiles.ini";
  QSettings settings(fp, QSettings::IniFormat);
  if (settings.contains("RecentProjects")) {
    QList<QVariant> projects =
        settings.value(QString("RecentProjects")).toList();
    if (!projects.isEmpty())
      for (const QVariant &v : projects) m_recentFiles.append(v.toString());
    else  // in case the history has only one item
      m_recentFiles.append(
          settings.value(QString("RecentProjects")).toString());
  }
  refreshRecentFilesMenu();
}

//-----------------------------------------------------------------------------

void RecentFiles::saveRecentFiles() {
  QString fp = IwFolders::getMyProfileFolderPath() + "/recentfiles.ini";
  QSettings settings(fp, QSettings::IniFormat);
  settings.setValue(QString("RecentProjects"), QVariant(m_recentFiles));
}

//-----------------------------------------------------------------------------

QList<QString> RecentFiles::getFilesNameList() {
  QList<QString> names;
  int i = 0;
  for (const QString &filePath : m_recentFiles) {
    names.append(QString::number(i + 1) + QString(". ") + filePath);
    i++;
  }
  return names;
}

//-----------------------------------------------------------------------------

void RecentFiles::refreshRecentFilesMenu() {
  QAction *act = CommandManager::instance()->getAction(MI_OpenRecentProject);
  if (!act) return;
  QMenu *menu = act->menu();
  if (!menu) return;
  QList<QString> names = getFilesNameList();
  if (names.isEmpty())
    menu->setEnabled(false);
  else {
    if (act->data().toInt() == -1) {
      menu->clear();
      for (int i = 0; i < names.size(); i++) {
        QString actionId = names.at(i);
        QAction *action  = menu->addAction(actionId);
        action->setData(QVariant(i));
      }
    }
    menu->addSeparator();
    QAction *clearAction =
        CommandManager::instance()->getAction(MI_ClearRecentProject);
    assert(clearAction);
    menu->addAction(clearAction);
    if (!menu->isEnabled()) menu->setEnabled(true);
  }
}

void RecentFiles::onMenuTriggered(QAction *action) {
  QAction *clearAction =
      CommandManager::instance()->getAction(MI_ClearRecentProject);
  if (action == clearAction) {
    return;
  }

  QVariant data = action->data();
  if (!data.isValid()) return;

  QAction *recentProject =
      CommandManager::instance()->getAction(MI_OpenRecentProject);
  int clickedId = data.toInt();
  recentProject->setData(data);
  CommandManager::instance()->execute(MI_OpenRecentProject);
  recentProject->setData(-1);

  if (clickedId == 0) return;

  QMenu *menu = recentProject->menu();
  if (!menu) return;
  QList<QAction *> acts = menu->actions();
  menu->removeAction(action);
  menu->insertAction(acts[0], action);

  acts = menu->actions();
  for (int i = 0; i <= clickedId; i++) {
    QAction *a = acts.at(i);

    QString newStr = a->text();
    int n          = (i >= 10) ? 4 : 3;
    newStr.replace(0, n, QString::number(i + 1) + QString(". "));
    a->setText(newStr);
    a->setData(QVariant(i));
  }
}

void RecentFiles::movePath(int fromIndex, int toIndex) {
  m_recentFiles.move(fromIndex, toIndex);
  saveRecentFiles();
}
