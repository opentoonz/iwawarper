//---------------------------------------------------
// InfoDialog
// �t�@�C������\������
//---------------------------------------------------

#include "infodialog.h"

#include "iwapp.h"
#include "mainwindow.h"
#include "iwselectionhandle.h"
#include "iwtimelineselection.h"
#include "iwlayerhandle.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "menubarcommandids.h"

#include "tlevel_io.h"
#include "timageinfo.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QFileInfo>
#include <QDateTime>

InfoDialog::InfoDialog()
    : IwDialog(IwApp::instance()->getMainWindow(), "InfoDialog", false)
    , m_currentPath() {
  // �I�u�W�F�N�g���
  m_layerLabel  = new QLabel(this);
  m_frameLabel  = new QLabel(this);
  m_frameSlider = new QSlider(Qt::Horizontal, this);

  // �����C�A�E�g�ɓo�^
  QGridLayout* mainLay = new QGridLayout();
  mainLay->setMargin(5);
  mainLay->setHorizontalSpacing(5);
  mainLay->setVerticalSpacing(8);
  mainLay->setColumnStretch(0, 0);
  mainLay->setColumnStretch(1, 1);

  mainLay->addWidget(new QLabel("Layer:", this), 0, 0);
  mainLay->addWidget(m_layerLabel, 0, 1);

  mainLay->addWidget(new QLabel("Frame:", this), 1, 0);
  QHBoxLayout* headLay = new QHBoxLayout();
  headLay->setMargin(0);
  headLay->setSpacing(5);
  headLay->addWidget(m_frameLabel, 0);
  headLay->addWidget(m_frameSlider, 1);
  mainLay->addLayout(headLay, 1, 1);

  createItem(eFullpath, tr("Fullpath"), mainLay);
  createItem(eFileType, tr("File Type"), mainLay);
  createItem(eOwner, tr("Owner"), mainLay);
  createItem(eSize, tr("Size"), mainLay);
  createItem(eCreated, tr("Created"), mainLay);
  createItem(eModified, tr("Modified"), mainLay);
  createItem(eLastAccess, tr("Last Access"), mainLay);

  createItem(eImageSize, tr("Image Size"), mainLay);
  createItem(eSaveBox, tr("SaveBox"), mainLay);
  createItem(eBitsSample, tr("Bits/Sample"), mainLay);
  createItem(eSamplePixel, tr("Sample/Pixel"), mainLay);
  createItem(eDpi, tr("Dpi"), mainLay);

  setLayout(mainLay);

  // �V�O�i���^�X���b�g�ڑ�
  connect(m_frameSlider, SIGNAL(sliderMoved(int)), this,
          SLOT(onSliderMoved(int)));
}

void InfoDialog::showEvent(QShowEvent*) {
  // �I�����ς������
  connect(IwApp::instance()->getCurrentSelection(),
          SIGNAL(selectionChanged(IwSelection*)), this,
          SLOT(onSelectionChanged()));
  connect(IwApp::instance()->getCurrentLayer(), SIGNAL(layerSwitched()), this,
          SLOT(onSelectionChanged()));

  onSelectionChanged();
}

void InfoDialog::hideEvent(QHideEvent*) {
  disconnect(IwApp::instance()->getCurrentSelection(),
             SIGNAL(selectionChanged(IwSelection*)), this,
             SLOT(onSelectionChanged()));
  disconnect(IwApp::instance()->getCurrentLayer(), SIGNAL(layerSwitched()),
             this, SLOT(onSelectionChanged()));
}

void InfoDialog::createItem(FormatItem id, const QString& name,
                            QGridLayout* layout) {
  QLabel* itemLabel = new QLabel(this);
  int row           = layout->rowCount();

  layout->addWidget(new QLabel(QString("%1:").arg(name), this), row, 0,
                    Qt::AlignLeft | Qt::AlignVCenter);
  layout->addWidget(itemLabel, row, 1);
  // ���ڃ��X�g�ɓo�^
  m_items[id] = itemLabel;
}

// �I�����ς������A�X���C�_�͈͂�؂�ւ��A
// ���݂̃t���[���̏��ɍX�V����
void InfoDialog::onSelectionChanged() {
  // IwTimeLineSelection�ȊO�̏ꍇ�̓��V
  if (IwSelection::getCurrent() != IwTimeLineSelection::instance()) return;
  // �J�����gLayer��������΃��V
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  if (IwTimeLineSelection::instance()->isEmpty()) return;

  // �t���[���I��͈͂𓾂�
  int start, end;
  bool rightIncluded;
  IwTimeLineSelection::instance()->getRange(start, end, rightIncluded);
  if (!rightIncluded) end -= 1;
  // ���E�P�����I��ł����ꍇ
  if (start > end) return;

  // ���C�����ɃN�����v
  if (end >= layer->getFrameLength()) end = layer->getFrameLength() - 1;

  m_frameSlider->setRange(start, end);

  // ���݂̃t���[��
  int currentFrame =
      IwApp::instance()->getCurrentProject()->getProject()->getViewFrame();
  if (currentFrame < start || currentFrame > end)
    m_frameSlider->setValue(start);
  else
    m_frameSlider->setValue(currentFrame);

  // ���C����������
  m_layerLabel->setText(layer->getName());

  onSliderMoved(m_frameSlider->value());
}

void InfoDialog::onSliderMoved(int frame) {
  // ���x���𓯊�
  m_frameLabel->setText(QString::number(frame + 1));

  // �J�����gLayer��������΃��V
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  QString path = layer->getImageFilePath(frame);
  if (path.isEmpty()) return;

  // �\���̍X�V
  updateFields(path);
}

void InfoDialog::updateFields(QString& path) {
  if (m_currentPath == path) return;

  m_currentPath = path;

  int frameNumber = 0;
  // ���[�r�[�t�@�C���̏ꍇ
  if (path.lastIndexOf('#') == (path.length() - 5)) {
    frameNumber = path.right(4).toInt();
    path.chop(5);
  }

  QString ext = path.right(path.length() - path.lastIndexOf('.') - 1);

  QFileInfo fi(path);
  assert(fi.exists());

  setVal(eFullpath, fi.absoluteFilePath());
  setVal(eFileType, getTypeString(ext));
  setVal(eOwner, fi.owner());
  setVal(eSize, fileSizeString(fi.size()));
  setVal(eCreated, fi.birthTime().toString());
  setVal(eModified, fi.lastModified().toString());
  setVal(eLastAccess, fi.lastRead().toString());

  TFilePath fp(path.toStdWString());
  TLevelReaderP lr(fp);
  const TImageInfo* ii;
  try {
    ii = lr->getImageInfo(TFrameId(frameNumber));
  } catch (...) {
    return;
  }

  if (lr && ii) {
    setVal(eImageSize, QString("%1 X %2").arg(ii->m_lx).arg(ii->m_ly));
    if (ii->m_x0 <= ii->m_x1)
      setVal(eSaveBox, QString("(%1, %2, %3, %4)")
                           .arg(ii->m_x0)
                           .arg(ii->m_y0)
                           .arg(ii->m_x1)
                           .arg(ii->m_y1));
    else
      setVal(eSaveBox, QString());

    if (ii->m_bitsPerSample > 0)
      setVal(eBitsSample, QString::number(ii->m_bitsPerSample));
    else
      setVal(eBitsSample, QString());

    if (ii->m_samplePerPixel > 0)
      setVal(eSamplePixel, QString::number(ii->m_samplePerPixel));
    else
      setVal(eSamplePixel, QString());

    if (ii->m_dpix > 0 || ii->m_dpiy > 0)
      setVal(eDpi, QString("(%1, %2)").arg(ii->m_dpix).arg(ii->m_dpiy));
    else
      setVal(eDpi, QString());
  }

  update();
}

void InfoDialog::setVal(FormatItem id, const QString& str) {
  m_items.value(id)->setText(str);
}

QString InfoDialog::fileSizeString(qint64 size, int precision) {
  if (size < 1024)
    return QString::number(size) + " Bytes";
  else if (size < 1024 * 1024)
    return QString::number(size / (1024.0), 'f', precision) + " KB";
  else if (size < 1024 * 1024 * 1024)
    return QString::number(size / (1024 * 1024.0), 'f', precision) + " MB";
  else
    return QString::number(size / (1024 * 1024 * 1024.0), 'f', precision) +
           " GB";
}

QString InfoDialog::getTypeString(QString& ext) {
  if (ext == "tlv" || ext == "tzp" || ext == "tzu")
    return "Toonz Cmapped Raster Level Image";
  else if (ext == "mov" || ext == "avi" || ext == "3gp")
    return "Movie File";
  else if (ext == "pic")
    return "Pic File";
  else
    return "Raster Image";
}

OpenPopupCommandHandler<InfoDialog> openFileInfo(MI_FileInfo);
