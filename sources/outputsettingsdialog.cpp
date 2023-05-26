//---------------------------------
// Output Settings Dialog
// �����_�����O���ʂ̕ۑ��t�@�C���`��/�p�X���w�肷��
//---------------------------------

#include "outputsettingsdialog.h"
#include "iwapp.h"
#include "mainwindow.h"
#include "menubarcommandids.h"
#include "iwprojecthandle.h"
#include "iwproject.h"

#include "outputsettings.h"
#include "formatsettingspopups.h"  //toonz��肫��
#include "shapetagsettings.h"

#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QCompleter>
#include <QFileSystemModel>
#include <QDir>
#include <QMessageBox>
#include <QFileDialog>
#include <QIntValidator>

#include <QHBoxLayout>
#include <QVBoxLayout>

OutputSettingsDialog::OutputSettingsDialog()
    : IwDialog(IwApp::instance()->getMainWindow(), "OutputSettingsDialog",
               false) {
  setSizeGripEnabled(false);
  setWindowTitle(tr("Output Settings"));

  //--- �I�u�W�F�N�g�̐錾
  m_startFrameEdit = new QLineEdit(this);
  m_endFrameEdit   = new QLineEdit(this);
  m_stepFrameEdit  = new QLineEdit(this);
  m_shapeTagCombo  = new QComboBox(this);

  m_saverCombo = new QComboBox(this);

  QPushButton* parametersBtn = new QPushButton(tr("Parameters..."), this);

  m_directoryEdit             = new QLineEdit(this);
  QPushButton* openBrowserBtn = new QPushButton(tr("Output folder..."), this);

  QGroupBox* fileNameGroupBox = new QGroupBox(tr("File Name Control"), this);

  m_initialFrameNumberEdit = new QLineEdit(this);
  m_incrementEdit          = new QLineEdit(this);
  m_numberOfDigitsEdit     = new QLineEdit(this);
  m_extensionEdit          = new QLineEdit(this);

  m_useSourceCB  = new QCheckBox(tr("Use project name"), this);
  m_addNumberCB  = new QCheckBox(tr("Add frame number"), this);
  m_replaceExtCB = new QCheckBox(tr("Add extension:"), this);

  m_formatEdit   = new QLineEdit(this);
  m_exampleLabel = new QLabel(this);

  QPushButton* renderBtn = new QPushButton(tr("Render"), this);

  //--- �v���p�e�B�̐ݒ�
  m_startFrameEdit->setValidator(new QIntValidator(1, 9999, this));
  m_endFrameEdit->setValidator(new QIntValidator(1, 9999, this));
  m_stepFrameEdit->setValidator(new QIntValidator(1, 9999, this));

  m_shapeTagCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

  QStringList saverList;
  saverList << Saver_PNG << Saver_TIFF << Saver_SGI << Saver_TGA;
  m_saverCombo->addItems(saverList);

  parametersBtn->setFocusPolicy(Qt::NoFocus);

  openBrowserBtn->setFocusPolicy(Qt::NoFocus);

  QCompleter* completer      = new QCompleter(this);
  QFileSystemModel* tmpModel = new QFileSystemModel(completer);
  tmpModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
  completer->setModel(tmpModel);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  m_directoryEdit->setCompleter(completer);

  m_initialFrameNumberEdit->setValidator(new QIntValidator(0, 999, this));
  m_incrementEdit->setValidator(new QIntValidator(1, 99, this));
  m_numberOfDigitsEdit->setValidator(new QIntValidator(1, 10, this));

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
      rangeLay->addWidget(new QLabel(tr("Step:"), this), 1, Qt::AlignRight);
      rangeLay->addWidget(m_stepFrameEdit, 1);
    }
    mainLay->addLayout(rangeLay, 0);

    QHBoxLayout* shapeTagLay = new QHBoxLayout();
    {
      shapeTagLay->addWidget(new QLabel(tr("Target Shape Tag:"), this), 0,
                             Qt::AlignRight);
      shapeTagLay->addWidget(m_shapeTagCombo, 0);
      shapeTagLay->addStretch(1);
    }
    mainLay->addLayout(shapeTagLay, 0);

    // Saver : �t�@�C���`��
    QHBoxLayout* saverLay = new QHBoxLayout();
    saverLay->setSpacing(3);
    saverLay->setMargin(0);
    {
      saverLay->addWidget(new QLabel(tr("Format:"), this), 0);
      saverLay->addWidget(m_saverCombo, 1);
      saverLay->addSpacing(3);
      saverLay->addWidget(parametersBtn, 0);
    }
    mainLay->addLayout(saverLay, 0);

    // �o�̓t�H���_
    QHBoxLayout* dirLay = new QHBoxLayout();
    dirLay->setSpacing(3);
    dirLay->setMargin(0);
    {
      dirLay->addWidget(openBrowserBtn, 0);
      dirLay->addWidget(m_directoryEdit, 1);
    }
    mainLay->addLayout(dirLay, 0);

    // �t�@�C�����֌W
    QVBoxLayout* fileNameLay = new QVBoxLayout();
    fileNameLay->setSpacing(7);
    fileNameLay->setMargin(5);
    {
      QHBoxLayout* lay1 = new QHBoxLayout();
      lay1->setSpacing(2);
      lay1->setMargin(0);
      {
        lay1->addWidget(m_useSourceCB, 0);
        lay1->addStretch(1);
        lay1->addWidget(new QLabel(tr("Initial frame number:"), this), 0);
        lay1->addWidget(m_initialFrameNumberEdit, 0);
      }
      fileNameLay->addLayout(lay1, 0);

      QHBoxLayout* lay2 = new QHBoxLayout();
      lay2->setSpacing(2);
      lay2->setMargin(0);
      {
        lay2->addWidget(m_addNumberCB, 0);
        lay2->addStretch(1);
        lay2->addWidget(new QLabel(tr("Increment:"), this), 0);
        lay2->addWidget(m_incrementEdit, 0);
      }
      fileNameLay->addLayout(lay2, 0);

      QHBoxLayout* lay3 = new QHBoxLayout();
      lay3->setSpacing(2);
      lay3->setMargin(0);
      {
        lay3->addWidget(m_replaceExtCB, 0);
        lay3->addWidget(m_extensionEdit, 0);
        lay3->addStretch(1);
        lay3->addSpacing(10);
        lay3->addWidget(new QLabel(tr("Number of digits:"), this), 0);
        lay3->addWidget(m_numberOfDigitsEdit, 0);
      }
      fileNameLay->addLayout(lay3, 0);

      QHBoxLayout* lay4 = new QHBoxLayout();
      lay4->setSpacing(2);
      lay4->setMargin(0);
      {
        lay4->addWidget(new QLabel(tr("File name template:"), this), 0);
        lay4->addWidget(m_formatEdit, 1);
      }
      fileNameLay->addLayout(lay4, 0);

      QHBoxLayout* lay5 = new QHBoxLayout();
      lay5->setSpacing(10);
      lay5->setMargin(0);
      {
        lay5->addWidget(new QLabel(tr("Example file name:"), this), 0);
        lay5->addWidget(m_exampleLabel, 1);
      }
      fileNameLay->addLayout(lay5, 0);
    }
    fileNameGroupBox->setLayout(fileNameLay);
    mainLay->addWidget(fileNameGroupBox, 0);

    mainLay->addStretch(1);

    mainLay->addWidget(renderBtn, 0, Qt::AlignCenter);
  }
  setLayout(mainLay);

  //------ �V�O�i��/�X���b�g�ڑ�
  connect(m_startFrameEdit, SIGNAL(editingFinished()), this,
          SLOT(onStartFrameEditted()));
  connect(m_endFrameEdit, SIGNAL(editingFinished()), this,
          SLOT(onEndFrameEditted()));
  connect(m_stepFrameEdit, SIGNAL(editingFinished()), this,
          SLOT(onStepFrameEditted()));

  connect(m_saverCombo, SIGNAL(activated(const QString&)), this,
          SLOT(onSaverComboActivated(const QString&)));
  connect(parametersBtn, SIGNAL(clicked(bool)), this,
          SLOT(onParametersBtnClicked()));
  connect(m_directoryEdit, SIGNAL(editingFinished()), this,
          SLOT(onDirectoryEditted()));
  connect(openBrowserBtn, SIGNAL(clicked()), this,
          SLOT(onOpenBrowserBtnClicked()));
  connect(m_initialFrameNumberEdit, SIGNAL(editingFinished()), this,
          SLOT(onInitialFrameNumberEditted()));
  connect(m_incrementEdit, SIGNAL(editingFinished()), this,
          SLOT(onIncrementEditted()));
  connect(m_numberOfDigitsEdit, SIGNAL(editingFinished()), this,
          SLOT(onNumberOfDigitsEditted()));

  connect(m_useSourceCB, SIGNAL(clicked(bool)), this,
          SLOT(onCheckBoxClicked()));
  connect(m_addNumberCB, SIGNAL(clicked(bool)), this,
          SLOT(onCheckBoxClicked()));
  connect(m_replaceExtCB, SIGNAL(clicked(bool)), this,
          SLOT(onCheckBoxClicked()));
  connect(m_formatEdit, SIGNAL(editingFinished()), this,
          SLOT(onFormatEditted()));

  connect(renderBtn, SIGNAL(clicked()), this, SLOT(onRenderButtonClicked()));

  connect(m_shapeTagCombo, SIGNAL(activated(int)), this,
          SLOT(onShapeTagComboActivated()));
}

//---------------------------------------------------
// project���؂�ւ������A���e���X�V����
//---------------------------------------------------
void OutputSettingsDialog::showEvent(QShowEvent* event) {
  IwDialog::showEvent(event);

  IwProjectHandle* ph = IwApp::instance()->getCurrentProject();
  if (ph) {
    connect(ph, SIGNAL(projectSwitched()), this, SLOT(updateGuis()));
    connect(ph, SIGNAL(projectSwitched()), this,
            SLOT(updateShapeTagComboItems()));
    connect(ph, SIGNAL(shapeTagSettingsChanged()), this,
            SLOT(updateShapeTagComboItems()));
  }

  updateShapeTagComboItems();
  updateGuis();
}

void OutputSettingsDialog::hideEvent(QHideEvent* event) {
  IwDialog::hideEvent(event);

  IwProjectHandle* ph = IwApp::instance()->getCurrentProject();
  if (ph) {
    disconnect(ph, SIGNAL(projectSwitched()), this, SLOT(updateGuis()));
    disconnect(ph, SIGNAL(projectSwitched()), this,
               SLOT(updateShapeTagComboItems()));
    disconnect(ph, SIGNAL(shapeTagSettingsChanged()), this,
               SLOT(updateShapeTagComboItems()));
  }
}

//---------------------------------------------------
// ���݂�OutputSettings�ɍ��킹�ĕ\�����X�V����
//---------------------------------------------------
void OutputSettingsDialog::updateGuis() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  OutputSettings* settings = project->getOutputSettings();
  if (!settings) return;

  // start��end�͂P����
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

  m_stepFrameEdit->setText(QString::number(range.stepFrame));

  m_saverCombo->setCurrentIndex(m_saverCombo->findText(settings->getSaver()));

  m_directoryEdit->setText(settings->getDirectory());

  m_initialFrameNumberEdit->setText(QString::number(initialFrame));
  m_incrementEdit->setText(QString::number(increment));
  m_numberOfDigitsEdit->setText(QString::number(settings->getNumberOfDigits()));
  m_extensionEdit->setText(settings->getExtension());

  m_useSourceCB->setChecked(settings->isUseSource());
  m_addNumberCB->setChecked(settings->isAddNumber());
  m_replaceExtCB->setChecked(settings->isReplaceExt());

  m_formatEdit->setText(settings->getFormat());

  // format����example�𐶐�����
  // ���P�t���[���ڂ̃t�@�C���p�X������āA���̃t�@�C��������������B
  QString exampleStr = project->getOutputPath(0);
  std::cout << exampleStr.toStdString() << std::endl;
  exampleStr =
      exampleStr.remove(0, exampleStr.lastIndexOf(QRegExp("[/\\\\]")) + 1);
  m_exampleLabel->setText(exampleStr);

  m_shapeTagCombo->setCurrentIndex(
      m_shapeTagCombo->findData(settings->getShapeTagId()));

  update();
}

//---------------------------------------------------
OutputSettings* OutputSettingsDialog::getCurrentSettings() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return 0;

  return project->getOutputSettings();
}

//---------------------------------------------------

void OutputSettingsDialog::onStartFrameEditted() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  OutputSettings* settings = project->getOutputSettings();
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

void OutputSettingsDialog::onEndFrameEditted() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  OutputSettings* settings = project->getOutputSettings();
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

void OutputSettingsDialog::onStepFrameEditted() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  OutputSettings* settings = project->getOutputSettings();
  if (!settings) return;

  int newStep = m_stepFrameEdit->text().toInt();

  OutputSettings::SaveRange saveRange = settings->getSaveRange();

  // �l���ς���Ă��Ȃ����return
  if (newStep == saveRange.stepFrame) return;

  // FrameRange��ύX����
  saveRange.stepFrame = newStep;
  settings->setSaveRange(saveRange);

  // GUI���X�V
  updateGuis();
}

//---------------------------------------------------
// �t�@�C���`�����ς������ �g���q���ύX
//---------------------------------------------------
void OutputSettingsDialog::onSaverComboActivated(const QString& text) {
  OutputSettings* settings = getCurrentSettings();
  if (!settings) return;

  settings->setSaver(text);
  settings->setExtension(OutputSettings::getStandardExtension(text));
  updateGuis();
}

//---------------------------------------------------

void OutputSettingsDialog::onParametersBtnClicked() {
  OutputSettings* settings = getCurrentSettings();
  if (!settings) return;

  QString ext = OutputSettings::getStandardExtension(settings->getSaver());
  // ������Toonz�̃_�C�A���O���g��
  openFormatSettingsPopup(
      this, ext.toStdString(),
      settings->getFileFormatProperties(settings->getSaver()));
}

//---------------------------------------------------
void OutputSettingsDialog::onDirectoryEditted() {
  static bool isBusy = false;
  if (isBusy) return;
  isBusy                   = true;
  OutputSettings* settings = getCurrentSettings();
  if (!settings) {
    isBusy = false;
    return;
  }
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
void OutputSettingsDialog::onOpenBrowserBtnClicked() {
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
void OutputSettingsDialog::onInitialFrameNumberEditted() {
  OutputSettings* settings = getCurrentSettings();
  if (!settings) return;

  int newVal = m_initialFrameNumberEdit->text().toInt();

  if (settings->getInitialFrameNumber() == newVal) return;

  settings->setInitiaFrameNumber(newVal);
  updateGuis();

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
void OutputSettingsDialog::onIncrementEditted() {
  OutputSettings* settings = getCurrentSettings();
  if (!settings) return;

  int newVal = m_incrementEdit->text().toInt();

  if (settings->getIncrement() == newVal) return;

  settings->setIncrement(newVal);
  updateGuis();

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
void OutputSettingsDialog::onNumberOfDigitsEditted() {
  OutputSettings* settings = getCurrentSettings();
  if (!settings) return;

  int newVal = m_numberOfDigitsEdit->text().toInt();

  if (settings->getNumberOfDigits() == newVal) return;

  settings->setNumberOfDigits(newVal);
  updateGuis();
}

//---------------------------------------------------
// �`�F�b�N�{�b�N�X�͂R�܂Ƃ߂ē���SLOT�ɂ���
//---------------------------------------------------
void OutputSettingsDialog::onCheckBoxClicked() {
  OutputSettings* settings = getCurrentSettings();
  if (!settings) return;

  // �S�Č��݂̃{�^���̏�ԂƓ���������
  settings->setIsUseSource(m_useSourceCB->isChecked());
  settings->setIsAddNumber(m_addNumberCB->isChecked());
  settings->setIsReplaceExt(m_replaceExtCB->isChecked());

  updateGuis();
}

//---------------------------------------------------
void OutputSettingsDialog::onFormatEditted() {
  OutputSettings* settings = getCurrentSettings();
  if (!settings) return;

  settings->setFormat(m_formatEdit->text());

  updateGuis();
}

//---------------------------------------------------

void OutputSettingsDialog::updateShapeTagComboItems() {
  int previousId = m_shapeTagCombo->currentData().toInt();

  m_shapeTagCombo->clear();

  m_shapeTagCombo->addItem(tr("None"), -1);

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  ShapeTagSettings* shapeTags = project->getShapeTags();

  for (int t = 0; t < shapeTags->tagCount(); t++) {
    ShapeTagInfo tagInfo = shapeTags->getTagAt(t);
    int tagId            = shapeTags->getIdAt(t);
    m_shapeTagCombo->addItem(tagInfo.icon, tagInfo.name, tagId);
  }

  int index = m_shapeTagCombo->findData(previousId);
  if (index >= 0)
    m_shapeTagCombo->setCurrentIndex(index);
  else
    m_shapeTagCombo->setCurrentIndex(0);
}

void OutputSettingsDialog::onShapeTagComboActivated() {
  OutputSettings* settings = getCurrentSettings();
  if (!settings) return;

  int oldId = settings->getShapeTagId();
  int newId = m_shapeTagCombo->currentData().toInt();
  if (oldId == newId) return;

  settings->setShapeTagId(newId);
  IwApp::instance()->getCurrentProject()->notifyPreviewCacheInvalidated();
  updateGuis();
}

void OutputSettingsDialog::onRenderButtonClicked() {
  close();
  CommandManager::instance()->getAction(MI_Render)->trigger();
}

OpenPopupCommandHandler<OutputSettingsDialog> openOutputOptions(
    MI_OutputOptions);