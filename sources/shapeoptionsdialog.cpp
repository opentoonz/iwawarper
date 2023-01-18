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

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <iostream>
#include <QIntValidator>

#include "shapepair.h"
#include "iwshapepairselection.h"
#include "iwtrianglecache.h"

ShapeOptionsDialog::ShapeOptionsDialog()
    : IwDialog(IwApp::instance()->getMainWindow(), "ShapeOptionsDialog",
               false) {
  setSizeGripEnabled(false);

  //--- �I�u�W�F�N�g�̐錾
  // �Ή��_�̖��x�X���C�_
  m_edgeDensitySlider = new QSlider(Qt::Horizontal, this);
  m_edgeDensityEdit   = new QLineEdit(this);

  //--- �v���p�e�B�̐ݒ�
  m_edgeDensityEdit->setFixedWidth(37);
  m_edgeDensitySlider->setRange(1, 50);
  QIntValidator *validator = new QIntValidator(1, 50, this);
  m_edgeDensityEdit->setValidator(validator);

  //--- ���C�A�E�g
  QVBoxLayout *mainLay = new QVBoxLayout();
  mainLay->setSpacing(3);
  mainLay->setMargin(3);
  {
    // �Ή��_�̖��x�X���C�_
    mainLay->addWidget(new QLabel(tr("Edge Density")), 0,
                       Qt::AlignLeft | Qt::AlignVCenter);
    QHBoxLayout *edgeDensityLay = new QHBoxLayout();
    edgeDensityLay->setSpacing(3);
    edgeDensityLay->setMargin(0);
    {
      edgeDensityLay->addWidget(m_edgeDensitySlider, 1);
      edgeDensityLay->addWidget(m_edgeDensityEdit, 0);
    }
    mainLay->addLayout(edgeDensityLay, 0);

    mainLay->addStretch(1);
  }
  setLayout(mainLay);

  //--- �V�O�i��/�X���b�g�ڑ�
  // EdgeDensity�̃X���C�_
  connect(m_edgeDensitySlider, SIGNAL(sliderMoved(int)), this,
          SLOT(onEdgeDensitySliderMoved(int)));
  connect(m_edgeDensityEdit, SIGNAL(editingFinished()), this,
          SLOT(onEdgeDensityEditEdited()));
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

  if (selectionHandle) onSelectionChanged(selectionHandle->getSelection());
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
    m_selectedShapes.clear();
    m_selectedShapes = shapeSelection->getShapes();

    // ���V�F�C�v�I�𖳂���Ζ��������O���[�o���\��
    if (m_selectedShapes.isEmpty()) {
      m_edgeDensitySlider->setDisabled(true);
      m_edgeDensityEdit->setDisabled(true);
      m_edgeDensityEdit->setText("");
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
    }
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
OpenPopupCommandHandler<ShapeOptionsDialog> openShapeOptions(MI_ShapeOptions);