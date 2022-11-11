//------------------------------
// IwApp
// アプリケーションの現在の状況を全て持つ
//------------------------------
#ifndef IWAPP_H
#define IWAPP_H

#include <QObject>
#include <QList>

class IwProject;
class MainWindow;
class IwProjectHandle;
class IwSelectionHandle;
class IwToolHandle;
class IwLayerHandle;

class IwApp : public QObject  // singleton
{
  Q_OBJECT

  // メインウィンドウへのポインタ。ダイアログの親になる。
  MainWindow* m_mainWindow;
  // プロジェクト情報
  IwProjectHandle* m_projectHandle;
  // 選択 情報
  IwSelectionHandle* m_selectionHandle;
  // ツール情報
  IwToolHandle* m_toolHandle;
  // レイヤ情報
  IwLayerHandle* m_layerHandle;

  // 現在アプリケーションにロードされているプロジェクト
  QList<IwProject*> m_loadedProjects;

  IwApp();

public:
  static IwApp* instance();
  ~IwApp();

  // メインウィンドウへのポインタを返す
  MainWindow* getMainWindow() const { return m_mainWindow; }
  void setMainWindow(MainWindow* mainWindow) { m_mainWindow = mainWindow; }

  // プロジェクト情報
  IwProjectHandle* getCurrentProject() const { return m_projectHandle; }
  // 選択 情報
  IwSelectionHandle* getCurrentSelection() const { return m_selectionHandle; }
  // ツール情報
  IwToolHandle* getCurrentTool() const { return m_toolHandle; }
  // レイヤ情報
  IwLayerHandle* getCurrentLayer() const { return m_layerHandle; }

  // ロードされているプロジェクトリストにプロジェクトを挿入
  void insertLoadedProject(IwProject* project);

  // ステータスバーにメッセージを表示する
  void showMessageOnStatusBar(const QString& message, int timeout = 0);
};

#endif