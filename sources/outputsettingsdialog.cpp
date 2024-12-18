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
#include <QGridLayout>

QItemSelectionModel::SelectionFlags RenderQueueListWidget::selectionCommand(
    const QModelIndex& index, const QEvent* event) const {
  if (selectedIndexes().contains(index))
    return QItemSelectionModel::NoUpdate;
  else
    return QListWidget::selectionCommand(index, event);
}

//----------------------------------------------------------------------

OutputSettingsDialog::OutputSettingsDialog()
    : IwDialog(IwApp::instance()->getMainWindow(), "OutputSettingsDialog",
               false) {
  setSizeGripEnabled(false);
  setWindowTitle(tr("Output Settings"));

  //--- �I�u�W�F�N�g�̐錾
  m_itemList       = new RenderQueueListWidget(this);
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

  m_warningLabel = new QLabel(this);

  m_formatEdit = new QLineEdit(this);

  QLabel* macroInstruction = new QLabel("?", this);

  m_exampleLabel = new QLabel(this);

  QPushButton* addTaskBtn = new QPushButton(tr("Add Task"), this);
  m_removeTaskBtn         = new QPushButton(tr("Remove Task"), this);
  QPushButton* renderBtn  = new QPushButton(tr("Render"), this);

  //--- �v���p�e�B�̐ݒ�

  m_itemList->setSelectionMode(QListWidget::SingleSelection);
  m_itemList->setStyleSheet(
      "QListWidget::item{\n"
      "color: white;"
      "}\n"
      "QListWidget::item:selected {\n"
      "background-color: #264f78;\n"
      "}");

  m_startFrameEdit->setValidator(new QIntValidator(1, 9999, this));
  m_endFrameEdit->setValidator(new QIntValidator(1, 9999, this));
  m_stepFrameEdit->setValidator(new QIntValidator(1, 9999, this));

  m_shapeTagCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

  QStringList saverList;
  saverList << Saver_PNG << Saver_TIFF << Saver_SGI << Saver_TGA;
  m_saverCombo->addItems(saverList);

  parametersBtn->setFocusPolicy(Qt::NoFocus);
  openBrowserBtn->setFocusPolicy(Qt::NoFocus);
  addTaskBtn->setFocusPolicy(Qt::NoFocus);
  m_removeTaskBtn->setFocusPolicy(Qt::NoFocus);

  QCompleter* completer      = new QCompleter(this);
  QFileSystemModel* tmpModel = new QFileSystemModel(completer);
  tmpModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
  completer->setModel(tmpModel);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  m_directoryEdit->setCompleter(completer);

  m_initialFrameNumberEdit->setFixedWidth(80);
  m_incrementEdit->setFixedWidth(80);
  m_numberOfDigitsEdit->setFixedWidth(80);
  m_initialFrameNumberEdit->setValidator(new QIntValidator(0, 999, this));
  m_incrementEdit->setValidator(new QIntValidator(1, 99, this));
  m_numberOfDigitsEdit->setValidator(new QIntValidator(1, 10, this));

  m_warningLabel->setStyleSheet("QLabel{color: red;}");
  m_warningLabel->hide();

  macroInstruction->setFixedSize(20, 20);
  macroInstruction->setAlignment(Qt::AlignCenter);
  macroInstruction->setStyleSheet(
      "QLabel{border: 1px solid lightgray; border-radius: 2px;}");
  macroInstruction->setToolTip(
      tr("# Macro Instruction\n\n"
         "[dir] : Path specified in the \"Output folder\" field.\n"
         "[base] : Project name.\n"
         "[num] : Frame number.\n"
         "[ext] : Extension.\n"
         "[mattename] : Matte layer name. (*)\n"
         "[mattenum] : Frame number in the filename of the image used for the "
         "matte layer. (*)\n"
         "\n"
         "* N.B. If the shapes to be rendered has multiple matte layers, the "
         "first one found will be used."));

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
    QGridLayout* fileNameLay = new QGridLayout();
    fileNameLay->setVerticalSpacing(7);
    fileNameLay->setHorizontalSpacing(2);
    fileNameLay->setMargin(5);
    {
      fileNameLay->addWidget(new QLabel(tr("Initial frame number:"), this), 0,
                             0, Qt::AlignRight | Qt::AlignVCenter);
      fileNameLay->addWidget(m_initialFrameNumberEdit, 0, 1);

      fileNameLay->addWidget(new QLabel(tr("Increment:"), this), 0, 2,
                             Qt::AlignRight | Qt::AlignVCenter);
      fileNameLay->addWidget(m_incrementEdit, 0, 3);

      fileNameLay->addWidget(new QLabel(tr("Number of digits:"), this), 1, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      fileNameLay->addWidget(m_numberOfDigitsEdit, 1, 1);
      fileNameLay->addWidget(m_warningLabel, 1, 2, 1, 2,
                             Qt::AlignRight | Qt::AlignVCenter);

      fileNameLay->addWidget(new QLabel(tr("File name template:"), this), 2, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      QHBoxLayout* templateLay = new QHBoxLayout();
      templateLay->setSpacing(2);
      templateLay->setMargin(0);
      {
        templateLay->addWidget(m_formatEdit, 1);
        templateLay->addWidget(macroInstruction, 0);
      }
      fileNameLay->addLayout(templateLay, 2, 1, 1, 3);

      fileNameLay->addWidget(new QLabel(tr("Example file name:"), this), 3, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      fileNameLay->addWidget(m_exampleLabel, 3, 1, 1, 3);
    }
    fileNameLay->setColumnStretch(2, 1);

    fileNameGroupBox->setLayout(fileNameLay);
    mainLay->addWidget(fileNameGroupBox, 0);

    mainLay->addSpacing(5);
    mainLay->addWidget(new QLabel(tr("Render Queue"), this), 0);
    mainLay->addWidget(m_itemList, 1);

    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setMargin(0);
    buttonsLay->setSpacing(10);
    {
      buttonsLay->addWidget(addTaskBtn, 0);
      buttonsLay->addWidget(m_removeTaskBtn, 0);
      buttonsLay->addStretch(1);
      buttonsLay->addWidget(renderBtn, 0);
    }
    mainLay->addLayout(buttonsLay, 0);
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

  connect(m_formatEdit, SIGNAL(editingFinished()), this,
          SLOT(onFormatEditted()));

  connect(m_itemList, SIGNAL(itemClicked(QListWidgetItem*)), this,
          SLOT(onTaskClicked(QListWidgetItem*)));
  connect(addTaskBtn, SIGNAL(clicked()), this, SLOT(onAddTaskButtonClicked()));
  connect(m_removeTaskBtn, SIGNAL(clicked()), this,
          SLOT(onRemoveTaskButtonClicked()));

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

  // ���X�g�����
  m_itemList->clear();
  QList<OutputSettings*> allSettings = project->getRenderQueue()->allItems();
  int queueId                        = 0;
  for (auto& os : allSettings) {
    QListWidgetItem* item = new QListWidgetItem();
    item->setSizeHint(QSize(30, 30));
    m_itemList->addItem(item);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState((os->renderState() == OutputSettings::Off)
                            ? Qt::Unchecked
                            : Qt::Checked);
    int shapeTagId = os->getShapeTagId();
    if (shapeTagId != -1) {
      item->setIcon(project->getShapeTags()->getTagFromId(shapeTagId).icon);
    }
    int tmpInitialFrame                = os->getInitialFrameNumber();
    int tmpIncrement                   = os->getIncrement();
    OutputSettings::SaveRange tmpRange = os->getSaveRange();
    int from = tmpRange.startFrame * tmpIncrement + tmpInitialFrame;
    int to   = ((tmpRange.endFrame == -1) ? project->getProjectFrameLength() - 1
                                          : tmpRange.endFrame) *
                 tmpIncrement +
             tmpInitialFrame;
    os->obtainMatteLayerNames();
    QString outPath = project->getOutputPath(0, QString(), queueId);
    item->setText(tr("Frame %1 - %2 | %3").arg(from).arg(to).arg(outPath));

    if (project->getRenderQueue()->currentSettingsId() == queueId)
      m_itemList->setCurrentItem(item);

    // m_itemList->setItemWidget(item, itemFromOutputSetting(setting));
    queueId++;
  }
  m_removeTaskBtn->setEnabled(allSettings.size() > 1);

  OutputSettings* settings = project->getRenderQueue()->currentOutputSettings();
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

  m_formatEdit->setText(settings->getFormat());

  // format����example�𐶐�����
  // ���P�t���[���ڂ̃t�@�C���p�X������āA���̃t�@�C��������������B
  project->getRenderQueue()->currentOutputSettings()->obtainMatteLayerNames();
  QString exampleStr = project->getOutputPath(0);
  std::cout << exampleStr.toStdString() << std::endl;
  exampleStr =
      exampleStr.remove(0, exampleStr.lastIndexOf(QRegExp("[/\\\\]")) + 1);
  m_exampleLabel->setText(exampleStr);

  if (settings->getFormat().contains("[mattename]") ||
      settings->getFormat().contains("[mattenum]")) {
    if (exampleStr.contains("nomatte")) {
      m_warningLabel->setText(tr("NO MATTE LAYER FOUND!"));
      m_warningLabel->show();
    } else if (exampleStr.contains("----")) {
      m_warningLabel->setText(tr("NO NUMBER FOUND IN THE MATTE FILE NAME!"));
      m_warningLabel->show();
    } else
      m_warningLabel->hide();
  } else
    m_warningLabel->hide();

  m_shapeTagCombo->setCurrentIndex(
      m_shapeTagCombo->findData(settings->getShapeTagId()));

  update();
}

//---------------------------------------------------
OutputSettings* OutputSettingsDialog::getCurrentSettings() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return 0;

  return project->getRenderQueue()->currentOutputSettings();
}

//---------------------------------------------------

void OutputSettingsDialog::onStartFrameEditted() {
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

void OutputSettingsDialog::onEndFrameEditted() {
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

void OutputSettingsDialog::onStepFrameEditted() {
  // ���݂̃v���W�F�N�g��OutputSettings���擾
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  OutputSettings* settings = project->getRenderQueue()->currentOutputSettings();
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

void OutputSettingsDialog::onTaskClicked(QListWidgetItem* clickedItem) {
  assert(m_itemList->selectedItems().count() == 1);
  if (m_itemList->selectedItems().isEmpty()) return;
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  int row = m_itemList->row(m_itemList->selectedItems()[0]);

  project->getRenderQueue()->setCurrentSettingsId(row);

  int clickedRow = m_itemList->row(clickedItem);
  bool checked   = (clickedItem->checkState() == Qt::Checked);
  project->getRenderQueue()
      ->outputSettings(clickedRow)
      ->setRenderState((checked) ? OutputSettings::On : OutputSettings::Off);

  IwApp::instance()->getCurrentProject()->notifyPreviewCacheInvalidated();
  updateGuis();
}

void OutputSettingsDialog::onAddTaskButtonClicked() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  project->getRenderQueue()->cloneCurrentItem();
  updateGuis();
}

void OutputSettingsDialog::onRemoveTaskButtonClicked() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  project->getRenderQueue()->removeCurrentItem();
  updateGuis();
}

void OutputSettingsDialog::onRenderButtonClicked() {
  close();
  CommandManager::instance()->getAction(MI_Render)->trigger();
}

OpenPopupCommandHandler<OutputSettingsDialog> openOutputOptions(
    MI_OutputOptions);