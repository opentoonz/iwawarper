//---------------------------------
// Shape Options Dialog
// �I�𒆂̃V�F�C�v�̃v���p�e�B��\������_�C�A���O
// ���[�_���ł͂Ȃ�
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

  //--- �I�u�W�F�N�g�̐錾
  // �Ή��_�̖��x�X���C�_
  m_edgeDensitySlider = new QSlider(Qt::Horizontal, this);
  m_edgeDensityEdit   = new QLineEdit(this);
  // �E�F�C�g�̃X���C�_(�傫���قǃ��b�V���������񂹂�)
  m_weightSlider = new QSlider(Qt::Horizontal, this);
  m_weightEdit   = new QLineEdit(this);

  //--- �v���p�e�B�̐ݒ�
  m_edgeDensityEdit->setFixedWidth(37);
  m_edgeDensitySlider->setRange(1, 50);
  QIntValidator *validator = new QIntValidator(1, 50, this);
  m_edgeDensityEdit->setValidator(validator);

  m_weightEdit->setFixedWidth(37);
  m_weightSlider->setRange(1, 50);  // 0.1-5.0
  QDoubleValidator *weightValidator = new QDoubleValidator(0.1, 5.0, 1, this);
  m_weightEdit->setValidator(weightValidator);

  //--- ���C�A�E�g
  QGridLayout *mainLay = new QGridLayout();
  mainLay->setSpacing(3);
  mainLay->setMargin(3);
  {
    // �Ή��_�̖��x�X���C�_
    mainLay->addWidget(new QLabel(tr("Edge Density")), 0, 0, 1, 2,
                       Qt::AlignLeft | Qt::AlignVCenter);
    mainLay->addWidget(m_edgeDensitySlider, 1, 0);
    mainLay->addWidget(m_edgeDensityEdit, 1, 1);

    // �E�F�C�g�̃X���C�_
    mainLay->addWidget(new QLabel(tr("Weight")), 2, 0, 1, 2,
                       Qt::AlignLeft | Qt::AlignVCenter);
    mainLay->addWidget(m_weightSlider, 3, 0);
    mainLay->addWidget(m_weightEdit, 3, 1);
  }
  mainLay->setColumnStretch(0, 1);
  mainLay->setColumnStretch(1, 0);
  mainLay->setRowStretch(4, 1);
  setLayout(mainLay);

  //--- �V�O�i��/�X���b�g�ڑ�
  // EdgeDensity�̃X���C�_
  connect(m_edgeDensitySlider, SIGNAL(sliderMoved(int)), this,
          SLOT(onEdgeDensitySliderMoved(int)));
  connect(m_edgeDensityEdit, SIGNAL(editingFinished()), this,
          SLOT(onEdgeDensityEditEdited()));
  // �E�F�C�g�̃X���C�_
  connect(m_weightSlider, SIGNAL(sliderMoved(int)), this,
          SLOT(onWeightSliderMoved(int)));
  connect(m_weightEdit, SIGNAL(editingFinished()), this,
          SLOT(onWeightEditEdited()));
}

//---------------------------------------------------

void ShapeOptionsDialog::showEvent(QShowEvent *) {
  IwSelectionHandle *selectionHandle = IwApp::instance()->getCurrentSelection();
  if (selectionHandle) {
    // �I�����؂�ւ������
    connect(selectionHandle,
            SIGNAL(selectionSwitched(IwSelection *, IwSelection *)), this,
            SLOT(onSelectionSwitched(IwSelection *, IwSelection *)));
    // �I�����ς������
    connect(selectionHandle, SIGNAL(selectionChanged(IwSelection *)), this,
            SLOT(onSelectionChanged(IwSelection *)));
  }

  IwProjectHandle *projectHandle = IwApp::instance()->getCurrentProject();
  if (projectHandle) {
    // �\���t���[�����؂�ւ������
    connect(projectHandle, SIGNAL(viewFrameChanged()), this,
            SLOT(onViewFrameChanged()));
    connect(projectHandle, SIGNAL(projectChanged()), this,
            SLOT(onViewFrameChanged()));
  }

  if (selectionHandle) onSelectionChanged(selectionHandle->getSelection());

  // ���t���[���ς������E�F�C�g�̕\�����X�V�A������
}

//---------------------------------------------------

void ShapeOptionsDialog::hideEvent(QHideEvent *) {
  IwSelectionHandle *selectionHandle = IwApp::instance()->getCurrentSelection();
  if (selectionHandle) {
    // �I�����؂�ւ������
    disconnect(selectionHandle,
               SIGNAL(selectionSwitched(IwSelection *, IwSelection *)), this,
               SLOT(onSelectionSwitched(IwSelection *, IwSelection *)));
    // �I�����ς������
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

    // ���V�F�C�v�I�𖳂���Ζ��������O���[�o���\��
    if (m_selectedShapes.isEmpty()) {
      m_edgeDensitySlider->setDisabled(true);
      m_edgeDensityEdit->setDisabled(true);
      m_edgeDensityEdit->setText("");

      m_weightSlider->setDisabled(true);
      m_weightEdit->setDisabled(true);
      m_weightEdit->setText("");
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

      // �l���i�[
      m_edgeDensitySlider->setValue(maxPrec);
      if (minPrec == maxPrec)
        m_edgeDensityEdit->setText(QString::number(minPrec));
      else
        m_edgeDensityEdit->setText(QString("%1-%2").arg(minPrec).arg(maxPrec));

      m_weightSlider->setEnabled(true);
      m_weightEdit->setEnabled(true);

      //---�����āA�E�F�C�g�̍X�V
      double minWeight = 100.;
      double maxWeight = 0;
      // �A�N�e�B�u�Ή��_������ꍇ�͂��̓_�̃E�F�C�g
      if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.second >= 0) {
        OneShape shape = m_activeCorrPoint.first;
        minWeight      = shape.shapePairP->getCorrPointList(frame, shape.fromTo)
                        .at(m_activeCorrPoint.second)
                        .weight;
        maxWeight = minWeight;
      } else {
        for (auto shape : m_selectedShapes) {
          CorrPointList cpList =
              shape.shapePairP->getCorrPointList(frame, shape.fromTo);
          for (auto cp : cpList) {
            minWeight = std::min(minWeight, cp.weight);
            maxWeight = std::max(maxWeight, cp.weight);
          }
        }
      }
      // �l���i�[
      m_weightSlider->setValue((int)(maxWeight * 10.));
      if (minWeight == maxWeight)
        m_weightEdit->setText(QString::number(minWeight));
      else
        m_weightEdit->setText(QString("%1-%2").arg(minWeight).arg(maxWeight));

      // �N���b�N���Ƀt�H�[�J�X���āA�����ɐ��l���͂��\�ɂ���B
      // �Ή��_�h���b�O���ɍēx�r���[�A�[�Ƀt�H�[�J�X����
      m_weightEdit->setFocus();
      m_weightEdit->selectAll();
    }
  }
}

//---------------------------------------------------

void ShapeOptionsDialog::onViewFrameChanged() {
  IwProject *project = IwApp::instance()->getCurrentProject()->getProject();
  int frame          = project->getViewFrame();

  if (m_selectedShapes.isEmpty()) return;
  double minWeight = 100.;
  double maxWeight = 0;
  // �A�N�e�B�u�Ή��_������ꍇ�͂��̓_�̃E�F�C�g
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.second >= 0) {
    OneShape shape = m_activeCorrPoint.first;
    minWeight      = shape.shapePairP->getCorrPointList(frame, shape.fromTo)
                    .at(m_activeCorrPoint.second)
                    .weight;
    maxWeight = minWeight;
  } else {
    for (auto shape : m_selectedShapes) {
      CorrPointList cpList =
          shape.shapePairP->getCorrPointList(frame, shape.fromTo);
      for (auto cp : cpList) {
        minWeight = std::min(minWeight, cp.weight);
        maxWeight = std::max(maxWeight, cp.weight);
      }
    }
  }
  // �l���i�[
  m_weightSlider->setValue((int)(maxWeight * 10.));
  if (minWeight == maxWeight)
    m_weightEdit->setText(QString::number(minWeight));
  else
    m_weightEdit->setText(QString("%1-%2").arg(minWeight).arg(maxWeight));
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

  // �V�F�C�v�I���������ꍇ�A�O���[�o����������
  // ���Ƃ�
  if (m_selectedShapes.isEmpty()) return;

  // �S�ẴV�F�C�v��EdgeDensity��ύX
  QList<ChangeEdgeDensityUndo::EdgeDensityInfo> infoList;
  for (int s = 0; s < m_selectedShapes.size(); s++) {
    OneShape shape = m_selectedShapes[s];
    // IwShape* shape = m_selectedShapes[s];
    if (!shape.shapePairP) continue;

    ChangeEdgeDensityUndo::EdgeDensityInfo info = {
        shape, shape.shapePairP->getEdgeDensity()};
    infoList.append(info);
  }

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
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

  // ���݂̃t���[���𓾂�
  int frame = project->getViewFrame();
  // �ΏۂƂȂ�Ή��_�̃��X�g�𓾂�
  QList<QPair<OneShape, int>> targetCorrList;
  // activeCorr������ꍇ�͂��̂P�_������ҏW����
  // ����ȊO�̏ꍇ�́A�I���V�F�C�v�̑S�Ă̑Ή��_��ΏۂƂ���
  if (m_activeCorrPoint.first.shapePairP && m_activeCorrPoint.second >= 0) {
    targetCorrList.append(m_activeCorrPoint);
  } else {
    for (auto shape : m_selectedShapes) {
      targetCorrList.append({shape, -1});
    }
  }

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(
      new ChangeWeightUndo(targetCorrList, weight, project));
}
//-------------------------------------
// �ȉ��AUndo�R�}���h
//-------------------------------------
ChangeEdgeDensityUndo::ChangeEdgeDensityUndo(QList<EdgeDensityInfo> &info,
                                             int afterED, IwProject *project)
    : m_project(project), m_info(info), m_afterED(afterED) {}

void ChangeEdgeDensityUndo::undo() {
  for (int i = 0; i < m_info.size(); i++)
    m_info.at(i).shape.shapePairP->setEdgeDensity(m_info.at(i).beforeED);

  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // ShapeOptionDialog�̍X�V�̂���
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
    // ShapeOptionDialog�̍X�V�̂���
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

    // �L�[�t���[������Ȃ������V�F�C�v�́A�P�ɃL�[������
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

    // �ύX�����t���[����invalidate
    if (m_project->isCurrent()) {
      int start, end;
      shape.shapePairP->getCorrKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
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

    // �ύX�����t���[����invalidate
    if (m_project->isCurrent()) {
      int start, end;
      shape.shapePairP->getCorrKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  }
}

//---------------------------------------------------
OpenPopupCommandHandler<ShapeOptionsDialog> openShapeOptions(MI_ShapeOptions);