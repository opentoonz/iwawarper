
#include "rendershapeimagedialog.h"
#include "iwapp.h"
#include "mainwindow.h"
#include "menubarcommandids.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "renderprogresspopup.h"
#include "iwlayer.h"
#include "viewsettings.h"
#include "shapepair.h"
#include "iwimagecache.h"
#include "iocommand.h"
#include "sceneviewer.h"

#include "outputsettings.h"
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QIntValidator>
#include <QCompleter>
#include <QFileSystemModel>
#include <QMessageBox>
#include <QFileDialog>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCoreApplication>

RenderShapeImageDialog::RenderShapeImageDialog()
    : IwDialog(IwApp::instance()->getMainWindow(), "RenderShapeImageDialog",
               false) {
  setSizeGripEnabled(false);
  setWindowTitle(tr("Render Shape Image"));

  m_startFrameEdit            = new QLineEdit(this);
  m_endFrameEdit              = new QLineEdit(this);
  m_fileNameEdit              = new QLineEdit(this);
  m_directoryEdit             = new QLineEdit(this);
  m_sizeCombo                 = new QComboBox(this);
  QPushButton* openBrowserBtn = new QPushButton(tr("Output folder..."), this);
  QPushButton* renderBtn      = new QPushButton(tr("Render"), this);
  //--- �v���p�e�B�̐ݒ�
  m_startFrameEdit->setValidator(new QIntValidator(1, 9999, this));
  m_endFrameEdit->setValidator(new QIntValidator(1, 9999, this));
  QCompleter* completer      = new QCompleter(this);
  QFileSystemModel* tmpModel = new QFileSystemModel(completer);
  tmpModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
  completer->setModel(tmpModel);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  m_directoryEdit->setCompleter(completer);
  renderBtn->setFocusPolicy(Qt::NoFocus);
  renderBtn->setFixedWidth(200);

  //--- ���C�A�E�g
  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setSpacing(7);
  mainLay->setMargin(10);
  {
    // �t���[���͈�
    QHBoxLayout* rangeLay = new QHBoxLayout();
    rangeLay->setSpacing(3);
    rangeLay->setMargin(0);
    {
      rangeLay->addWidget(new QLabel(tr("Start:"), this), 0, Qt::AlignRight);
      rangeLay->addWidget(m_startFrameEdit, 1);
      rangeLay->addWidget(new QLabel(tr("End:"), this), 1, Qt::AlignRight);
      rangeLay->addWidget(m_endFrameEdit, 1);
    }
    mainLay->addLayout(rangeLay, 0);

    // �o�̓t�H���_
    QHBoxLayout* dirLay = new QHBoxLayout();
    dirLay->setSpacing(3);
    dirLay->setMargin(0);
    {
      dirLay->addWidget(openBrowserBtn, 0);
      dirLay->addWidget(m_directoryEdit, 1);
    }
    mainLay->addLayout(dirLay, 0);

    QHBoxLayout* fileNameLay = new QHBoxLayout();
    fileNameLay->setSpacing(3);
    fileNameLay->setMargin(0);
    {
      fileNameLay->addWidget(new QLabel(tr("File Name:"), this), 0,
                             Qt::AlignRight);
      fileNameLay->addWidget(m_fileNameEdit, 1);
      fileNameLay->addSpacing(50);
      fileNameLay->addWidget(new QLabel(tr("Output Size:"), this), 0,
                             Qt::AlignRight);
      fileNameLay->addWidget(m_sizeCombo, 1);
    }
    mainLay->addLayout(fileNameLay, 0);

    mainLay->addStretch(1);

    mainLay->addWidget(renderBtn, 0, Qt::AlignCenter);
  }
  setLayout(mainLay);

  connect(m_startFrameEdit, SIGNAL(editingFinished()), this,
          SLOT(onStartFrameEditted()));
  connect(m_endFrameEdit, SIGNAL(editingFinished()), this,
          SLOT(onEndFrameEditted()));
  connect(m_fileNameEdit, SIGNAL(editingFinished()), this,
          SLOT(onFileNameEditted()));
  connect(m_directoryEdit, SIGNAL(editingFinished()), this,
          SLOT(onDirectoryEditted()));
  connect(openBrowserBtn, SIGNAL(clicked()), this,
          SLOT(onOpenBrowserBtnClicked()));
  connect(m_sizeCombo, SIGNAL(activated(int)), this,
          SLOT(onSizeComboActivated()));
  connect(renderBtn, SIGNAL(clicked()), this, SLOT(onRenderButtonClicked()));
}
//---------------------------------------------------
OutputSettings* RenderShapeImageDialog::getCurrentSettings() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return 0;

  return project->getRenderQueue()->currentOutputSettings();
}
//---------------------------------------------------

void RenderShapeImageDialog::showEvent(QShowEvent* event) {
  IwDialog::showEvent(event);
  IwProjectHandle* ph = IwApp::instance()->getCurrentProject();
  if (ph) connect(ph, SIGNAL(projectSwitched()), this, SLOT(updateGuis()));
  updateGuis();
}

//---------------------------------------------------

void RenderShapeImageDialog::hideEvent(QHideEvent* event) {
  IwDialog::hideEvent(event);
  IwProjectHandle* ph = IwApp::instance()->getCurrentProject();
  if (ph) disconnect(ph, SIGNAL(projectSwitched()), this, SLOT(updateGuis()));
}

//---------------------------------------------------
// ���݂�OutputSettings�ɍ��킹�ĕ\�����X�V����
void RenderShapeImageDialog::updateGuis() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  OutputSettings* settings = project->getRenderQueue()->currentOutputSettings();
  if (!settings) return;

  int initialFrame = settings->getInitialFrameNumber();
  int increment    = settings->getIncrement();
  m_startFrameEdit->setValidator(new QIntValidator(initialFrame, 9999, this));
  m_endFrameEdit->setValidator(new QIntValidator(initialFrame, 9999, this));

  OutputSettings::SaveRange range = settings->getSaveRange();
  m_startFrameEdit->setText(
      QString::number(range.startFrame * increment + initialFrame));
  if (range.endFrame == -1)
    m_endFrameEdit->setText(QString::number(
        (project->getProjectFrameLength() - 1) * increment + initialFrame));
  else
    m_endFrameEdit->setText(
        QString::number(range.endFrame * increment + initialFrame));

  m_directoryEdit->setText(settings->getDirectory());

  m_fileNameEdit->setText(settings->getShapeImageFileName());

  m_sizeCombo->clear();
  m_sizeCombo->addItem(tr("Work area"), -1);
  for (int lay = 0; lay < project->getLayerCount(); lay++) {
    m_sizeCombo->addItem(tr("Layer %1").arg(project->getLayer(lay)->getName()),
                         lay);
  }

  int currentShapeImageSizeId = settings->getShapeImageSizeId();
  int index                   = m_sizeCombo->findData(currentShapeImageSizeId);
  m_sizeCombo->setCurrentIndex((index >= 0) ? index : 0);

  update();
}
//---------------------------------------------------
void RenderShapeImageDialog::onStartFrameEditted() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  OutputSettings* settings = project->getRenderQueue()->currentOutputSettings();
  if (!settings) return;

  int initialFrame = settings->getInitialFrameNumber();
  int increment    = settings->getIncrement();

  int newStart = (m_startFrameEdit->text().toInt() - initialFrame) / increment;

  OutputSettings::SaveRange saveRange = settings->getSaveRange();

  // �l���ς���Ă��Ȃ����return
  if (newStart == saveRange.startFrame) return;

  // �t���[�����𒴂����ꍇ�́A�t���[�����ɃN�����v
  int frameLength = project->getProjectFrameLength();
  if (newStart > frameLength - 1) newStart = frameLength - 1;

  // FrameRange��ύX����
  saveRange.startFrame = newStart;

  // End��-1�łȂ��A���AEnd�𒴂����ꍇ�́AEnd��Start�ɂ���
  if (saveRange.endFrame != -1 && newStart > saveRange.endFrame)
    saveRange.endFrame = saveRange.startFrame;

  settings->setSaveRange(saveRange);
  // GUI���X�V
  updateGuis();
}
//---------------------------------------------------
void RenderShapeImageDialog::onEndFrameEditted() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  OutputSettings* settings = project->getRenderQueue()->currentOutputSettings();
  if (!settings) return;

  int initialFrame = settings->getInitialFrameNumber();
  int increment    = settings->getIncrement();

  int newEnd = (m_endFrameEdit->text().toInt() - initialFrame) / increment;

  OutputSettings::SaveRange saveRange = settings->getSaveRange();

  // �l���ς���Ă��Ȃ����return
  if (newEnd == saveRange.endFrame) return;

  // �t���[�����𒴂����ꍇ�́A�t���[�����ɃN�����v
  int frameLength = project->getProjectFrameLength();
  if (newEnd > frameLength - 1) newEnd = frameLength - 1;

  // FrameRange��ύX����
  saveRange.endFrame = newEnd;

  // Start����������ꍇ�́AStart��End�ɂ���
  if (newEnd < saveRange.startFrame) saveRange.startFrame = saveRange.endFrame;

  settings->setSaveRange(saveRange);
  // GUI���X�V
  updateGuis();
}
//---------------------------------------------------
void RenderShapeImageDialog::onFileNameEditted() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  OutputSettings* settings = project->getRenderQueue()->currentOutputSettings();
  if (!settings) return;

  QString shapeImageFileName = settings->getShapeImageFileName();
  QString newFileName        = m_fileNameEdit->text();

  if (newFileName.isEmpty()) {
    m_fileNameEdit->setText(shapeImageFileName);
    update();
    return;
  }

  // �l���ς���Ă��Ȃ����return
  if (newFileName == shapeImageFileName) return;

  settings->setShapeImageFileName(shapeImageFileName);
}
//---------------------------------------------------
void RenderShapeImageDialog::onSizeComboActivated() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  OutputSettings* settings = project->getRenderQueue()->currentOutputSettings();
  if (!settings) return;

  int shapeImageSizeId = settings->getShapeImageSizeId();
  int newId            = m_sizeCombo->currentData().toInt();

  // �l���ς���Ă��Ȃ����return
  if (newId == shapeImageSizeId) return;

  settings->setShapeImageSizeId(newId);
}
//---------------------------------------------------
void RenderShapeImageDialog::onDirectoryEditted() {
  static bool isBusy = false;
  if (isBusy) return;
  isBusy                   = true;
  OutputSettings* settings = getCurrentSettings();
  if (!settings) {
    isBusy = false;
    return;
  }
  /*
          if(settings->getDirectory() == m_directoryEdit->text())
          {
                  isBusy = false;
                  return;
          }
          */

  QDir dir(m_directoryEdit->text());
  if (!dir.exists()) {
    QMessageBox::StandardButton ret = QMessageBox::question(
        this, tr("Do you want to create folder?"),
        QString("The folder %1 does not exist.\nDo you want to create it?")
            .arg(m_directoryEdit->text()),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (ret == QMessageBox::Yes) {
      std::cout << "yes" << std::endl;
      // �t�H���_���
      bool ok = dir.mkpath(dir.path());
      if (!ok) {
        QMessageBox::critical(
            this, tr("Failed to create folder."),
            QString("Failed to create folder %1.").arg(dir.path()));
        // ���ɖ߂�
        m_directoryEdit->setText(settings->getDirectory());
        m_directoryEdit->update();
        isBusy = false;
        return;
      }
    } else {
      std::cout << "no" << std::endl;
      // ���ɖ߂�
      m_directoryEdit->setText(settings->getDirectory());
      m_directoryEdit->update();
      isBusy = false;
      return;
    }
  }

  // �ݒ�̍X�V
  settings->setDirectory(m_directoryEdit->text());

  updateGuis();
  isBusy = false;
}
//---------------------------------------------------
void RenderShapeImageDialog::onOpenBrowserBtnClicked() {
  OutputSettings* settings = getCurrentSettings();
  if (!settings) return;

  QString dir = QFileDialog::getExistingDirectory(
      this, tr("Choose Folder."), settings->getDirectory(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  // �ݒ�̍X�V
  settings->setDirectory(dir);
  updateGuis();
}
//---------------------------------------------------
void RenderShapeImageDialog::onRenderButtonClicked() {
  IwApp::instance()->getMainWindow()->getViewer()->doShapeRender();
}

OpenPopupCommandHandler<RenderShapeImageDialog> openRenderShapeImageDialog(
    MI_RenderShapeImage);