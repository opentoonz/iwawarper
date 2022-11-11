#ifndef TOOLOPTIONPANEL_H
#define TOOLOPTIONPANEL_H

//------------------------------------------
// ToolOptionPanel
// 常時表示するツールオプション
//------------------------------------------

#include <QDockWidget>
#include <QMap>
#include <QWidget>

class QStackedWidget;
class IwTool;

class MyIntSlider;

//------------------------------------------
// ReshapeTool "Lock Points"コマンドの距離の閾値のスライダ
//------------------------------------------

class ReshapeToolOptionPanel : public QWidget {
  Q_OBJECT

  MyIntSlider* m_lockThresholdSlider;

public:
  ReshapeToolOptionPanel(QWidget* parent);
protected slots:
  void onProjectSwitched();
  void onLockThresholdSliderChanged();
};

//------------------------------------------

class ToolOptionPanel : public QDockWidget {
  Q_OBJECT

  // それぞれのツールに対応したオプションを作って格納しておく
  QMap<IwTool*, QWidget*> m_panelMap;
  // 表示用パネル
  QStackedWidget* m_stackedPanels;

public:
  ToolOptionPanel(QWidget* parent);

protected slots:
  // ツールが切り替わったら、そのためのパネルをpanelMapから取り出して切り替える
  // まだ無ければ作ってpanelMapとstackedWidgetに登録する
  void onToolSwitched();

private:
  // ツールからパネルを作って返す。対応パネルが無ければ空のWidgetを返す
  QWidget* getPanel(IwTool*);
};

#endif