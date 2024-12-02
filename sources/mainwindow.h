#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <iostream>
#include <QMainWindow>
#include "menubarcommand.h"

class QMenuBar;
class QActionGroup;
class QIcon;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QMenu;

class ToolBoxDock;
class SceneViewer;
class MainStatusBar;

class TimeLineWindow;
class ToolOptionPanel;
class LayerOptionPanel;
class ShapeTreeWindow;

class MainWindow : public QMainWindow {
  Q_OBJECT

  // 左側のツールボックス
  ToolBoxDock *m_toolBox;
  // 中心のワークエリア
  SceneViewer *m_viewer;
  // ステータスバーとフレームスライダ
  MainStatusBar *m_mainStatusBar;
  // タイムライン
  TimeLineWindow *m_timeLineWindow;
  // ツールオプション
  ToolOptionPanel *m_toolOptionPanel;
  // レイヤーオプション
  LayerOptionPanel *m_layerOptionPanel;
  // シェイプツリー
  ShapeTreeWindow *m_shapeTreeWindow;
  // ツールバーコマンドのグループ
  QActionGroup *m_toolsActionGroup;

  // メニューバーの作成
  QWidget *createMenuBarWidget();
  // 表示モード
  QComboBox *m_viewModeCombo;
  // 表示チェックボックス
  QCheckBox *m_fromShow, *m_toShow, *m_meshShow, *m_matteApply;
  // ワークエリアのサイズ
  QLineEdit *m_workAreaWidthEdit, *m_workAreaHeightEdit;

  // アクションの登録
  void defineActions();
  // アクションの登録（ひとつずつ呼ぶ）
  QAction *createAction(const char *id, const QString &name,
                        const QString &defaultShortcut,
                        CommandType type = MenuFileCommandType);

  // トグル可、アイコン付きのアクションを作る
  QAction *createToggleWithIcon(const char *id, const QString &iconName,
                                const QString &name,
                                const QString &defaultShortcut,
                                CommandType type = MenuFileCommandType);

  // ツールコマンドの登録（ひとつずつ呼ぶ）
  QAction *createToolAction(const char *id, const char *iconName,
                            const QString &name,
                            const QString &defaultShortcut);

  QAction *createMenuAction(const char *id, const QString &name);

  // アイコンを返す
  QIcon createIcon(const QString &iconName);

  void addActionToMenu(QMenu *menu, CommandId id);
  QMenu *setupLanguageMenu();

public:
  MainWindow(QWidget *parent = 0, Qt::WindowFlags flags = Qt::Widget);
  ~MainWindow() {}

  void onExit();
  void onNewProject();

  // プロジェクトの保存/ロード
  void onOpenProject();
  void onImportProject();
  void onSaveProject();
  void onSaveAsProject();
  void onSaveProjectWithDateTime();
  void onInsertImages();

  // Undo, Redo
  void onUndo();
  void onRedo();

  // ステータスバーにメッセージを表示する
  void showMessageOnStatusBar(const QString &message, int timeout = 0);

  // フレーム移動
  void onNextFrame();
  void onPreviousFrame();

  void onClearMeshCache();

  SceneViewer *getViewer() { return m_viewer; }

protected:
  void closeEvent(QCloseEvent *) override;

protected slots:
  // プレビューの計算が終わったらPreview表示モードにして表示を更新
  void onPreviewRenderCompleted(int frame);
  // コンボボックスが切り替わったら表示を更新
  void onViewModeComboActivated(int);

  void onProjectSwitched();
  void onProjectChanged();

  // ロック操作
  void onFromShowClicked(bool);
  void onToShowClicked(bool);
  // メッシュの表示
  void onMeshShowClicked(bool);
  // マットを適用表示
  void onMatteApplyClicked(bool);

  // ワークエリアサイズの値が編集されたとき、プロジェクトの内容を更新する
  void onWorkAreaSizeEdited();
  void onAboutTriggered();
  void onLanguageChanged(QAction *);
public slots:
  // タイトルバーの更新
  void updateTitle();
};

class RecentFiles : public QObject {
  Q_OBJECT
  QList<QString> m_recentFiles;

  RecentFiles();

public:
  static RecentFiles *instance();
  ~RecentFiles();

  void addPath(QString path);
  QString getPath(int index) const;
  void movePath(int fromIndex, int toIndex);
  void clearRecentFilesList();
  void loadRecentFiles();
  void saveRecentFiles();

protected:
  void refreshRecentFilesMenu();
  QList<QString> getFilesNameList();

protected slots:
  void onMenuTriggered(QAction *);
};
#endif