//---------------------------------
// Shape Options Dialog
// 選択中のシェイプのプロパティを表示するダイアログ
// モーダルではない
//---------------------------------
#ifndef SHAPEOPTIONSDIALOG_H
#define SHAPEOPTIONSDIALOG_H

#include "iwdialog.h"

#include <QUndoCommand>
#include <QList>

#include "shapepair.h"

// class IwShape;
class QSlider;
class QLineEdit;
class IwSelection;
class IwProject;

class ShapeOptionsDialog : public IwDialog {
  Q_OBJECT

  // 現在選択されているシェイプ
  QList<OneShape> m_selectedShapes;
  // 選択中の対応点
  QPair<OneShape, int> m_activeCorrPoint;

  // 対応点の密度スライダ
  QSlider* m_edgeDensitySlider;
  QLineEdit* m_edgeDensityEdit;

  // ウェイトのスライダ(大きいほどメッシュを引き寄せる)
  QSlider* m_weightSlider;
  QLineEdit* m_weightEdit;

  // デプスのスライダ(大きいほどカメラから遠い＝後ろになる)
  QSlider* m_depthSlider;
  QLineEdit* m_depthEdit;

public:
  ShapeOptionsDialog();

  void setDensity(int value);
  void setWeight(double weight);
  void setDepth(double depth);

protected:
  void showEvent(QShowEvent*);
  void hideEvent(QHideEvent*);

protected slots:

  void onSelectionSwitched(IwSelection*, IwSelection*);
  void onSelectionChanged(IwSelection*);
  void onViewFrameChanged();

  void onEdgeDensitySliderMoved(int val);
  void onEdgeDensityEditEdited();
  void onWeightSliderMoved(int val);
  void onWeightEditEdited();
  void onDepthSliderMoved(int val);
  void onDepthEditEdited();
};

//-------------------------------------
// 以下、Undoコマンド
//-------------------------------------

class ChangeEdgeDensityUndo : public QUndoCommand {
public:
  struct EdgeDensityInfo {
    OneShape shape;
    int beforeED;
  };

private:
  IwProject* m_project;
  QList<EdgeDensityInfo> m_info;
  int m_afterED;

public:
  ChangeEdgeDensityUndo(QList<EdgeDensityInfo>& info, int afterED,
                        IwProject* project);

  void undo();
  void redo();
};

class ChangeWeightUndo : public QUndoCommand {
  IwProject* m_project;
  QList<QPair<OneShape, int>>
      m_targetCorrs;  // シェイプ全体の場合はsecondに-1を入れる
  QList<double> m_beforeWeights;
  double m_afterWeight;
  int m_frame;
  QList<OneShape> m_wasKeyShapes;

public:
  ChangeWeightUndo(QList<QPair<OneShape, int>>& targets, double afterWeight,
                   IwProject* project);

  void undo();
  void redo();
};

class ChangeDepthUndo : public QUndoCommand {
  IwProject* m_project;
  QList<QPair<OneShape, int>>
      m_targetCorrs;  // シェイプ全体の場合はsecondに-1を入れる
  QList<double> m_beforeDepths;
  double m_afterDepth;
  int m_frame;
  QList<OneShape> m_wasKeyShapes;

public:
  ChangeDepthUndo(QList<QPair<OneShape, int>>& targets, double afterDepth,
                  IwProject* project);

  void undo();
  void redo();
};

#endif