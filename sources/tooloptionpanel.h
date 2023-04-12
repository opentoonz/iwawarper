#ifndef TOOLOPTIONPANEL_H
#define TOOLOPTIONPANEL_H

//------------------------------------------
// ToolOptionPanel
// �펞�\������c�[���I�v�V����
//------------------------------------------

#include <QDockWidget>
#include <QMap>
#include <QWidget>

class QStackedWidget;
class IwTool;

class MyIntSlider;
class QCheckBox;

//------------------------------------------
// ReshapeTool "Lock Points"�R�}���h�̋�����臒l�̃X���C�_
//------------------------------------------

class ReshapeToolOptionPanel : public QWidget {
  Q_OBJECT

  MyIntSlider* m_lockThresholdSlider;
  QCheckBox* m_transformHandlesCB;

public:
  ReshapeToolOptionPanel(QWidget* parent);
protected slots:
  void onProjectSwitched();
  void onLockThresholdSliderChanged();
  void onTransformHandlesCBClicked(bool);
};

//------------------------------------------

class ToolOptionPanel : public QDockWidget {
  Q_OBJECT

  // ���ꂼ��̃c�[���ɑΉ������I�v�V����������Ċi�[���Ă���
  QMap<IwTool*, QWidget*> m_panelMap;
  // �\���p�p�l��
  QStackedWidget* m_stackedPanels;

public:
  ToolOptionPanel(QWidget* parent);

protected slots:
  // �c�[�����؂�ւ������A���̂��߂̃p�l����panelMap������o���Đ؂�ւ���
  // �܂�������΍����panelMap��stackedWidget�ɓo�^����
  void onToolSwitched();

private:
  // �c�[������p�l��������ĕԂ��B�Ή��p�l����������΋��Widget��Ԃ�
  QWidget* getPanel(IwTool*);
};

#endif