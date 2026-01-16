//---------------------------------
// Shape Options Dialog
// 選択中のシェイプのプロパティを表示するダイアログ
// モーダルではない
//---------------------------------

#include "shapeoptionsdialog.h"
#include "iwapp.h"
#include "mainwindow.h"
#include "menubarcommandids.h"
#include "iwselectionhandle.h"
#include "iwprojecthandle.h"

#include "iwundomanager.h"
#include "iwproject.h"

#include <QSlider>
#include <QLineEdit>
#include <QLabel>

#include <QGridLayout>
#include <iostream>
#include <QIntValidator>
#include <QDoubleValidator>

#include "shapepair.h"
#include "iwshapepairselection.h"
#include "iwtrianglecache.h"

ShapeOptionsDialog::ShapeOptionsDialog()
    : IwDialog(IwApp::instance()->getMainWindow(), "ShapeOptionsDialog", false)
    , m_activeCorrPoint(QPair<OneShape, int>{OneShape(), -1}) {
  setSizeGripEnabled(false);

  //--- オブジェクトの宣言
  // 対応点の密度スライダ
  m_edgeDensitySlider = new QSlider(Qt::Horizontal, this);
  m_edgeDensityEdit   = new QLineEdit(this);
  // ウェイトのスライダ(大きいほどメッシュを引き寄せる)
  m_weightSlider = new QSlider(Qt::Horizontal, this);
  m_weightEdit   = new QLineEdit(this);
  // デプスのスライダ(大きいほどカメラから遠い＝後ろになる)
  m_depthSlider = new QSlider(Qt::Horizontal, this);
  m_depthEdit   = new QLineEdit(this);

  //--- プロパティの設定
  m_edgeDensityEdit->setFixedWidth(37);
  m_edgeDensitySlider->setRange(1, 50);
  QIntValidator *validator = new QIntValidator(1, 50, this);
  m_edgeDensityEdit->setValidator(validator);

  m_weightEdit->setFixedWidth(37);
  m_weightSlider->setRange(1, 50);  // 0.1-5.0
  QDoubleValidator *weightValidator = new QDoubleValidator(0.1, 5.0, 1, this);
  m_weightEdit->setValidator(weightValidator);

  m_depthEdit->setFixedWidth(37);
  m_depthSlider->setRange(1, 100);  // 0.1 - 10.0
  QDoubleValidator *depthValidator = new QDoubleValidator(0.1, 10.0, 1, this);
  m_depthEdit->setValidator(depthValidator);

  //--- レイアウト
  QGridLayout *mainLay = new QGridLayout();
  mainLay->setSpacing(3);
  mainLay->setMargin(3);
  {
    // 対応点の密度スライダ
    mainLay->addWidget(new QLabel(tr("Edge Density")), 0, 0, 1, 2,
                       Qt::AlignLeft | Qt::AlignVCenter);
    mainLay->addWidget(m_edgeDensitySlider, 1, 0);
    mainLay->addWidget(m_edgeDensityEdit, 1, 1);

    // ウェイトのスライダ
    mainLay->addWidget(new QLabel(tr("Weight")), 2, 0, 1, 2,
                       Qt::AlignLeft | Qt::AlignVCenter);
    mainLay->addWidget(m_weightSlider, 3, 0);
    mainLay->addWidget(m_weightEdit, 3, 1);

    // デプスのスライダ
    mainLay->addWidget(new QLabel(tr("Depth")), 4, 0, 1, 2,
                       Qt::AlignLeft | Qt::AlignVCenter);
    mainLay->addWidget(m_depthSlider, 5, 0);
    mainLay->addWidget(m_depthEdit, 5, 1);
  }
  mainLay->setColumnStretch(0, 1);
  mainLay->setColumnStretch(1, 0);
  mainLay->setRowStretch(6, 1);
  setLayout(mainLay);

  //--- シグナル/スロット接続
  // EdgeDensityのスライダ
  connect(m_edgeDensitySlider, SIGNAL(sliderMoved(int)), this,
          SLOT(onEdgeDensitySliderMoved(int)));
  connect(m_edgeDensityEdit, SIGNAL(editingFinished()), this,
          SLOT(onEdgeDensityEditEdited()));
  // ウェイトのスライダ
  connect(m_weightSlider, SIGNAL(sliderMoved(int)), this,
          SLOT(onWeightSliderMoved(int)));
  connect(m_weightEdit, SIGNAL(editingFinished()), this,
          SLOT(onWeightEditEdited()));
  // デプスのスライダ
  connect(m_depthSlider, SIGNAL(sliderMoved(int)), this,
          SLOT(onDepthSliderMoved(int)));
  connect(m_depthEdit, SIGNAL(editingFinished()), this,
          SLOT(onDepthEditEdited()));
}

//---------------------------------------------------

void ShapeOptionsDialog::showEvent(QShowEvent *) {
  IwSelectionHandle *selectionHandle = IwApp::instance()->getCurrentSelection();
  if (selectionHandle) {
    // 選択が切り替わったら
    connect(selectionHandle,
            SIGNAL(selectionSwitched(IwSelection *, IwSelection *)), this,
            SLOT(onSelectionSwitched(IwSelection *, IwSelection *)));
    // 選択が変わったら
    connect(selectionHandle, SIGNAL(selectionChanged(IwSelection *)), this,
            SLOT(onSelectionChanged(IwSelection *)));
  }

  IwProjectHandle *projectHandle = IwApp::instance()->getCurrentProject();
  if (projectHandle) {
    // 表示フレームが切り替わったら
    connect(projectHandle, SIGNAL(viewFrameChanged()), this,
            SLOT(onViewFrameChanged()));
    connect(projectHandle, SIGNAL(projectChanged()), this,
            SLOT(onViewFrameChanged()));
  }

  if (selectionHandle) onSelectionChanged(selectionHandle->getSelection());

  // ★フレーム変わったらウェイトの表示を更新、を実装
}

//---------------------------------------------------

void ShapeOptionsDialog::hideEvent(QHideEvent *) {
  IwSelectionHandle *selectionHandle = IwApp::instance()->getCurrentSelection();
  if (selectionHandle) {
    // 選択が切り替わったら
    disconnect(selectionHandle,
               SIGNAL(selectionSwitched(IwSelection *, IwSelection *)), this,
               SLOT(onSelectionSwitched(IwSelection *, IwSelection *)));
    // 選択が変わったら
    disconnect(selectionHandle, SIGNAL(selectionChanged(IwSelection *)), this,
               SLOT(onSelectionChanged(IwSelection *)));

    onSelectionChanged(selectionHandle->getSelection());
  }
  IwProjectHandle *projectHandle = IwApp::instance()->getCurrentProject();
  if (projectHandle) {
    disconnect(projectHandle, SIGNAL(viewFrameChanged()), this,
               SLOT(onViewFrameChanged()));
    disconnect(projectHandle, SIGNAL(projectChanged()), this,
               SLOT(onViewFrameChanged()));
  }
}

//---------------------------------------------------

void ShapeOptionsDialog::onSelectionSwitched(IwSelection * /*oldSelection*/,
                                             IwSelection *newSelection) {
  // std::cout<<"ShapeOptionsDialog::onSelectionSwitched"<<std::endl;
  onSelectionChanged(newSelection);
}

//---------------------------------------------------

void ShapeOptionsDialog::onSelectionChanged(IwSelection *selection) {
  // std::cout<<"ShapeOptionsDialog::onSelectionChanged"<<std::endl;

  IwShapePairSelection *shapeSelection =
      dynamic_cast<IwShapePairSelection *>(selection);
  if (shapeSelection) {
    IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
    int frame          = project->getViewFrame();

    m_selectedShapes.clear();
    m_selectedShapes = shapeSelection->getShapes();

    m_activeCorrPoint = shapeSelection->getActiveCorrPoint();

    // ★シェイプ選択無ければ無効化＆グローバル表示
    if (m_selectedShapes.isEmpty()) {
      m_edgeDensitySlider->setDisabled(true);
      m_edgeDensityEdit->setDisabled(true);
      m_edgeDensityEdit->setText("");

      m_weightSlider->setDisabled(true);
      m_weightEdit->setDisabled(true);
      m_weightEdit->setText("");

      m_depthSlider->setDisabled(true);
      m_depthEdit->setDisabled(true);
      m_depthEdit->setText("");
    } else {
      m_edgeDensitySlider->setEnabled(true);
      m_edgeDensityEdit->setEnabled(true);
      int minPrec = INT_MAX;
      int maxPrec = 0;
      for (int s = 0; s < m_selectedShapes.size(); s++) {
        if (minPrec > m_selectedShapes[s].shapePairP->getEdgeDensity())
          minPrec = m_selectedShapes[s].shapePairP->getEdgeDensity();
        if (maxPrec < m_selectedShapes[s].shapePairP->getEdgeDensity())
          maxPrec = m_selectedShapes[s].shapePairP->getEdgeDensity();
      }

      // 値を格納
      m_edgeDensitySlider->setValue(maxPrec);
      if (minPrec == maxPrec)
        m_edgeDensityEdit->setText(QString::number(minPrec));
      else
        m_edgeDensityEdit->setText(QString("%1-%2").arg(minPrec).arg(maxPrec));

      m_weightSlider->setEnabled(true);
      m_weightEdit->setEnabled(true);

      //---続いて、ウェイトの更新
      double minWeight  = 100.;
      double maxWeight  = 0;
      double minDepth   = 100.;
      double maxDepth   = -100.;
      bool foundToShape = false;
      // アクティブ対応点がある場合はその点のウェイト
      // アクティブ対応点がある場合はその点のデプス
      if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.second >= 0) {
        OneShape shape = m_activeCorrPoint.first;
        minWeight      = shape.shapePairP->getCorrPointList(frame, shape.fromTo)
                        .at(m_activeCorrPoint.second)
                        .weight;
        maxWeight = minWeight;
        if (shape.fromTo == 1) {
          minDepth = shape.shapePairP->getCorrPointList(frame, shape.fromTo)
                         .at(m_activeCorrPoint.second)
                         .depth;
          maxDepth     = minDepth;
          foundToShape = true;
        }
      } else {
        for (auto shape : m_selectedShapes) {
          CorrPointList cpList =
              shape.shapePairP->getCorrPointList(frame, shape.fromTo);
          for (auto cp : cpList) {
            minWeight = std::min(minWeight, cp.weight);
            maxWeight = std::max(maxWeight, cp.weight);
            minDepth  = std::min(minDepth, cp.depth);
            maxDepth  = std::max(maxDepth, cp.depth);
          }
          foundToShape = true;
        }
      }
      // 値を格納
      m_weightSlider->setValue((int)(maxWeight * 10.));
      if (minWeight == maxWeight)
        m_weightEdit->setText(QString::number(minWeight));
      else
        m_weightEdit->setText(QString("%1 - %2").arg(minWeight).arg(maxWeight));

      //---続いて、デプスの更新
      if (foundToShape) {
        m_depthSlider->setEnabled(true);
        m_depthEdit->setEnabled(true);
        // 値を格納
        m_depthSlider->setValue((int)(maxDepth * 10.));
        if (minDepth == maxDepth)
          m_depthEdit->setText(QString::number(minDepth));
        else
          m_depthEdit->setText(QString("%1 - %2").arg(minDepth).arg(maxDepth));
      } else {
        m_depthSlider->setDisabled(true);
        m_depthEdit->setDisabled(true);
        m_depthEdit->setText("");
      }
    }
  }
}

//---------------------------------------------------

void ShapeOptionsDialog::onViewFrameChanged() {
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  int frame          = project->getViewFrame();

  if (m_selectedShapes.isEmpty()) return;
  double minWeight  = 100.;
  double maxWeight  = 0;
  double minDepth   = 100.;
  double maxDepth   = -100.;
  bool foundToShape = false;
  // アクティブ対応点がある場合はその点のウェイト
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.second >= 0) {
    OneShape shape = m_activeCorrPoint.first;
    minWeight      = shape.shapePairP->getCorrPointList(frame, shape.fromTo)
                    .at(m_activeCorrPoint.second)
                    .weight;
    maxWeight = minWeight;
    if (shape.fromTo == 1) {
      minDepth = shape.shapePairP->getCorrPointList(frame, shape.fromTo)
                     .at(m_activeCorrPoint.second)
                     .depth;
      maxDepth     = minDepth;
      foundToShape = true;
    }
  } else {
    for (auto shape : m_selectedShapes) {
      CorrPointList cpList =
          shape.shapePairP->getCorrPointList(frame, shape.fromTo);
      for (auto cp : cpList) {
        minWeight = std::min(minWeight, cp.weight);
        maxWeight = std::max(maxWeight, cp.weight);
        minDepth  = std::min(minDepth, cp.depth);
        maxDepth  = std::max(maxDepth, cp.depth);
      }
      foundToShape = true;
    }
  }
  // 値を格納
  m_weightSlider->setValue((int)(maxWeight * 10.));
  if (minWeight == maxWeight)
    m_weightEdit->setText(QString::number(minWeight));
  else
    m_weightEdit->setText(QString("%1 - %2").arg(minWeight).arg(maxWeight));

  //---続いて、デプスの更新
  if (foundToShape) {
    m_depthSlider->setEnabled(true);
    m_depthEdit->setEnabled(true);
    // 値を格納
    m_depthSlider->setValue((int)(maxDepth * 10.));
    if (minDepth == maxDepth)
      m_depthEdit->setText(QString::number(minDepth));
    else
      m_depthEdit->setText(QString("%1 - %2").arg(minDepth).arg(maxDepth));
  } else {
    m_depthSlider->setDisabled(true);
    m_depthEdit->setDisabled(true);
    m_depthEdit->setText("");
  }
}

//---------------------------------------------------

void ShapeOptionsDialog::onEdgeDensitySliderMoved(int val) {
  m_edgeDensityEdit->setText(QString::number(val));
  setDensity(val);
}

void ShapeOptionsDialog::onEdgeDensityEditEdited() {
  int val = m_edgeDensityEdit->text().toInt();
  m_edgeDensitySlider->setValue(val);
  setDensity(val);
}

void ShapeOptionsDialog::setDensity(int val) {
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // シェイプ選択が無い場合、グローバルをいじる
  // あとで
  if (m_selectedShapes.isEmpty()) return;

  // 全てのシェイプのEdgeDensityを変更
  QList<ChangeEdgeDensityUndo::EdgeDensityInfo> infoList;
  for (int s = 0; s < m_selectedShapes.size(); s++) {
    OneShape shape = m_selectedShapes[s];
    // IwShape* shape = m_selectedShapes[s];
    if (!shape.shapePairP) continue;

    ChangeEdgeDensityUndo::EdgeDensityInfo info = {
        shape, shape.shapePairP->getEdgeDensity()};
    infoList.append(info);
  }

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new ChangeEdgeDensityUndo(infoList, val, project));
}

//---------------------------------------------------

void ShapeOptionsDialog::onWeightSliderMoved(int val) {
  double weight = (double)val * 0.1;
  m_weightEdit->setText(QString::number(weight));
  setWeight(weight);
}

void ShapeOptionsDialog::onWeightEditEdited() {
  double weight = m_weightEdit->text().toDouble();
  m_weightSlider->setValue((int)(weight * 10.));
  setWeight(weight);
}

void ShapeOptionsDialog::setWeight(double weight) {
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  if (m_selectedShapes.isEmpty()) return;

  // 現在のフレームを得る
  int frame = project->getViewFrame();
  // 対象となる対応点のリストを得る
  QList<QPair<OneShape, int>> targetCorrList;
  // activeCorrがある場合はその１点だけを編集する
  // それ以外の場合は、選択シェイプの全ての対応点を対象とする
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.second >= 0) {
    targetCorrList.append(m_activeCorrPoint);
  } else {
    for (auto shape : m_selectedShapes) {
      targetCorrList.append({shape, -1});
    }
  }

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new ChangeWeightUndo(targetCorrList, weight, project));
}

//---------------------------------------------------

void ShapeOptionsDialog::onDepthSliderMoved(int val) {
  double depth = (double)val * 0.1;
  m_depthEdit->setText(QString::number(depth));
  setDepth(depth);
}

void ShapeOptionsDialog::onDepthEditEdited() {
  double depth = m_depthEdit->text().toDouble();
  m_depthSlider->setValue((int)(depth * 10.));
  setDepth(depth);
}

void ShapeOptionsDialog::setDepth(double depth) {
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  if (m_selectedShapes.isEmpty()) return;

  // 現在のフレームを得る
  int frame = project->getViewFrame();
  // 対象となる対応点のリストを得る
  QList<QPair<OneShape, int>> targetCorrList;
  bool foundToShape = false;
  // activeCorrがある場合はその１点だけを編集する
  // それ以外の場合は、選択シェイプの全ての対応点を対象とする
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.second >= 0) {
    if (m_activeCorrPoint.first.fromTo == 1) {
      targetCorrList.append(m_activeCorrPoint);
      foundToShape = true;
    }
  } else {
    for (auto shape : m_selectedShapes) {
      if (shape.fromTo == true) {
        targetCorrList.append({shape, -1});
        foundToShape = true;
      }
    }
  }

  if (!foundToShape) return;
  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new ChangeDepthUndo(targetCorrList, depth, project));
}

//-------------------------------------
// 以下、Undoコマンド
//-------------------------------------
ChangeEdgeDensityUndo::ChangeEdgeDensityUndo(QList<EdgeDensityInfo> &info,
                                             int afterED, IwProject *project)
    : m_project(project), m_info(info), m_afterED(afterED) {}

void ChangeEdgeDensityUndo::undo() {
  for (int i = 0; i < m_info.size(); i++)
    m_info.at(i).shape.shapePairP->setEdgeDensity(m_info.at(i).beforeED);

  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // ShapeOptionDialogの更新のため
    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();

    for (auto info : m_info)
      IwTriangleCache::instance()->invalidateShapeCache(
          m_project->getParentShape(info.shape.shapePairP));
  }
}

void ChangeEdgeDensityUndo::redo() {
  for (int i = 0; i < m_info.size(); i++)
    m_info.at(i).shape.shapePairP->setEdgeDensity(m_afterED);
  // m_info.at(i).shape->setEdgeDensity( m_afterED );

  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // ShapeOptionDialogの更新のため
    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
    for (auto info : m_info)
      IwTriangleCache::instance()->invalidateShapeCache(
          m_project->getParentShape(info.shape.shapePairP));
  }
}
//---------------------------------------------------

ChangeWeightUndo::ChangeWeightUndo(QList<QPair<OneShape, int>> &targets,
                                   double afterWeight, IwProject *project)
    : m_project(project), m_targetCorrs(targets), m_afterWeight(afterWeight) {
  m_frame = project->getViewFrame();

  for (auto target : m_targetCorrs) {
    OneShape shape = target.first;
    if (shape.shapePairP->isCorrKey(m_frame, shape.fromTo)) {
      m_wasKeyShapes.append(shape);
      if (target.second >= 0) {
        double beforeWeight =
            shape.shapePairP->getCorrPointList(m_frame, shape.fromTo)
                .at(target.second)
                .weight;
        m_beforeWeights.append(beforeWeight);
      } else {
        for (int c = 0; c < shape.shapePairP->getCorrPointAmount(); c++) {
          double beforeWeight =
              shape.shapePairP->getCorrPointList(m_frame, shape.fromTo)
                  .at(c)
                  .weight;
          m_beforeWeights.append(beforeWeight);
        }
      }
    }
  }
}

void ChangeWeightUndo::undo() {
  QList<OneShape> targetShapes;
  QList<double>::iterator before_itr = m_beforeWeights.begin();
  for (auto target : m_targetCorrs) {
    OneShape shape = target.first;

    // キーフレームじゃなかったシェイプは、単にキーを消去
    if (!m_wasKeyShapes.contains(shape)) {
      shape.shapePairP->removeCorrKey(m_frame, shape.fromTo);
      continue;
    }

    CorrPointList cpList =
        shape.shapePairP->getCorrPointList(m_frame, shape.fromTo);
    if (target.second >= 0) {
      cpList[target.second].weight = *before_itr;
      before_itr++;
    } else {
      for (auto &cp : cpList) {
        cp.weight = *before_itr;
        before_itr++;
      }
    }
    shape.shapePairP->setCorrKey(m_frame, shape.fromTo, cpList);

    // 変更されるフレームをinvalidate
    if (m_project->isCurrent()) {
      int start, end;
      shape.shapePairP->getCorrKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  }
}

void ChangeWeightUndo::redo() {
  for (auto target : m_targetCorrs) {
    OneShape shape = target.first;

    CorrPointList cpList =
        shape.shapePairP->getCorrPointList(m_frame, shape.fromTo);
    if (target.second >= 0)
      cpList[target.second].weight = m_afterWeight;
    else {
      for (auto &cp : cpList) {
        cp.weight = m_afterWeight;
      }
    }

    shape.shapePairP->setCorrKey(m_frame, shape.fromTo, cpList);

    // 変更されるフレームをinvalidate
    if (m_project->isCurrent()) {
      int start, end;
      shape.shapePairP->getCorrKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  }
}
//---------------------------------------------------

ChangeDepthUndo::ChangeDepthUndo(QList<QPair<OneShape, int>> &targets,
                                 double afterDepth, IwProject *project)
    : m_project(project), m_targetCorrs(targets), m_afterDepth(afterDepth) {
  m_frame = project->getViewFrame();

  for (auto target : m_targetCorrs) {
    OneShape shape = target.first;
    // 保険
    if (shape.fromTo == 0) continue;
    if (shape.shapePairP->isCorrKey(m_frame, shape.fromTo)) {
      m_wasKeyShapes.append(shape);
      if (target.second >= 0) {
        double beforeDepth =
            shape.shapePairP->getCorrPointList(m_frame, shape.fromTo)
                .at(target.second)
                .depth;
        m_beforeDepths.append(beforeDepth);
      } else {
        for (int c = 0; c < shape.shapePairP->getCorrPointAmount(); c++) {
          double beforeDepth =
              shape.shapePairP->getCorrPointList(m_frame, shape.fromTo)
                  .at(c)
                  .depth;
          m_beforeDepths.append(beforeDepth);
        }
      }
    }
  }
}

void ChangeDepthUndo::undo() {
  QList<OneShape> targetShapes;
  QList<double>::iterator before_itr = m_beforeDepths.begin();
  for (auto target : m_targetCorrs) {
    OneShape shape = target.first;
    // 保険
    if (shape.fromTo == 0) continue;
    // キーフレームじゃなかったシェイプは、単にキーを消去
    if (!m_wasKeyShapes.contains(shape)) {
      shape.shapePairP->removeCorrKey(m_frame, shape.fromTo);
      continue;
    }

    CorrPointList cpList =
        shape.shapePairP->getCorrPointList(m_frame, shape.fromTo);
    if (target.second >= 0) {
      cpList[target.second].depth = *before_itr;
      before_itr++;
    } else {
      for (auto &cp : cpList) {
        cp.depth = *before_itr;
        before_itr++;
      }
    }
    shape.shapePairP->setCorrKey(m_frame, shape.fromTo, cpList);

    // 変更されるフレームをinvalidate
    if (m_project->isCurrent()) {
      int start, end;
      shape.shapePairP->getCorrKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  }
}

void ChangeDepthUndo::redo() {
  for (auto target : m_targetCorrs) {
    OneShape shape = target.first;
    // 保険
    if (shape.fromTo == 0) continue;
    CorrPointList cpList =
        shape.shapePairP->getCorrPointList(m_frame, shape.fromTo);
    if (target.second >= 0)
      cpList[target.second].depth = m_afterDepth;
    else {
      for (auto &cp : cpList) {
        cp.depth = m_afterDepth;
      }
    }

    shape.shapePairP->setCorrKey(m_frame, shape.fromTo, cpList);

    // 変更されるフレームをinvalidate
    if (m_project->isCurrent()) {
      int start, end;
      shape.shapePairP->getCorrKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  }
}
//---------------------------------------------------
OpenPopupCommandHandler<ShapeOptionsDialog> openShapeOptions(MI_ShapeOptions);