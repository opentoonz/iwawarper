#include "matteinfodialog.h"

#include "iwapp.h"
#include "mainwindow.h"
#include "iwproject.h"
#include "iwprojecthandle.h"
#include "iwlayer.h"
#include "shapepair.h"
#include "iwselectionhandle.h"
#include "iwshapepairselection.h"
#include "menubarcommand.h"
#include "menubarcommandids.h"

#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QIntValidator>
#include <QColorDialog>
#include <QFileDialog>
#include <QSettings>

MatteInfoDialog::MatteInfoDialog()
    : IwDialog(IwApp::instance()->getMainWindow(), "MaskInfoDialog", false) {
  setSizeGripEnabled(false);
  setWindowTitle(tr("Alpha Matte Settings"));
  // setModal(true);

  m_layerCombo      = new QComboBox(this);
  m_colorListWidget = new QListWidget(this);
  m_toleranceEdit   = new QLineEdit(this);

  m_toleranceEdit->setValidator(new QIntValidator(0, 10, this));

  m_addColorBtn =
      new QPushButton(QIcon(":Resources/plus.svg"), tr("Add Color"), this);
  m_removeColorBtn =
      new QPushButton(QIcon(":Resources/minus.svg"), tr("Remove Color"), this);
  m_savePresetBtn =
      new QPushButton(QIcon(":Resources/save.svg"), tr("Save Preset"), this);
  m_loadPresetBtn =
      new QPushButton(QIcon(":Resources/load.svg"), tr("Load Preset"), this);

  m_addColorBtn->setFocusPolicy(Qt::NoFocus);
  m_removeColorBtn->setFocusPolicy(Qt::NoFocus);
  m_savePresetBtn->setFocusPolicy(Qt::NoFocus);
  m_loadPresetBtn->setFocusPolicy(Qt::NoFocus);

  QGridLayout* mainLay = new QGridLayout();
  mainLay->setHorizontalSpacing(5);
  mainLay->setVerticalSpacing(10);
  mainLay->setMargin(15);
  {
    mainLay->addWidget(new QLabel(tr("Matte Layer:"), this), 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_layerCombo, 0, 1, Qt::AlignLeft | Qt::AlignVCenter);

    mainLay->addWidget(new QLabel(tr("Matte Colors:"), this), 1, 0,
                       Qt::AlignRight | Qt::AlignTop);

    QVBoxLayout* colorLay = new QVBoxLayout();
    colorLay->setMargin(0);
    colorLay->setSpacing(2);
    {
      colorLay->addWidget(m_colorListWidget, 1);

      QGridLayout* buttonsLay = new QGridLayout();
      buttonsLay->setMargin(0);
      buttonsLay->setHorizontalSpacing(2);
      buttonsLay->setVerticalSpacing(2);
      {
        buttonsLay->addWidget(m_addColorBtn, 0, 0);
        buttonsLay->addWidget(m_removeColorBtn, 0, 1);
        buttonsLay->addWidget(m_savePresetBtn, 1, 0);
        buttonsLay->addWidget(m_loadPresetBtn, 1, 1);
      }
      colorLay->addLayout(buttonsLay, 0);
    }
    mainLay->addLayout(colorLay, 1, 1);

    mainLay->addWidget(new QLabel(tr("Tolerance:"), this), 2, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_toleranceEdit, 2, 1, Qt::AlignLeft | Qt::AlignVCenter);
  }
  setLayout(mainLay);

  // signal - slot
  bool ret = true;
  ret      = ret && connect(m_layerCombo, SIGNAL(activated(int)), this,
                            SLOT(onLayerComboActivated()));
  ret      = ret &&
        connect(m_colorListWidget, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
                this, SLOT(onColorItemDoubleClicked(QListWidgetItem*)));
  ret = ret && connect(m_addColorBtn, SIGNAL(clicked()), this,
                       SLOT(onAddColorClicked()));
  ret = ret && connect(m_removeColorBtn, SIGNAL(clicked()), this,
                       SLOT(onRemoveColorClicked()));
  ret = ret && connect(m_savePresetBtn, SIGNAL(clicked()), this,
                       SLOT(onSavePresetClicked()));
  ret = ret && connect(m_loadPresetBtn, SIGNAL(clicked()), this,
                       SLOT(onLoadPresetClicked()));
  ret = ret && connect(m_toleranceEdit, SIGNAL(editingFinished()), this,
                       SLOT(onToleranceEdited()));
  assert(ret);
}

void MatteInfoDialog::showEvent(QShowEvent*) {
  bool ret = true;
  ret      = ret && connect(IwApp::instance()->getCurrentSelection(),
                            SIGNAL(selectionChanged(IwSelection*)), this,
                            SLOT(onSelectionChanged(IwSelection*)));
  assert(ret);
  onSelectionChanged(IwShapePairSelection::instance());
}

void MatteInfoDialog::hideEvent(QHideEvent*) {
  bool ret = true;
  ret      = ret && disconnect(IwApp::instance()->getCurrentSelection(),
                               SIGNAL(selectionChanged(IwSelection*)), this,
                               SLOT(onSelectionChanged(IwSelection*)));
  assert(ret);
}

class ColorItem : public QListWidgetItem {
  QColor m_color;

public:
  ColorItem(const QColor& color) : QListWidgetItem() { setColor(color); }

  void setColor(const QColor& color) {
    QPixmap pm(18, 18);
    pm.fill(color);
    setIcon(QIcon(pm));

    setText(QString("%1, %2, %3")
                .arg(color.red())
                .arg(color.green())
                .arg(color.blue()));
  }
};

void MatteInfoDialog::onLayerComboActivated() {
  QString newLayerName = m_layerCombo->currentData().toString();

  int variousIndex = m_layerCombo->findData("__VARIOUS_VALUES__");
  // 複数値がある状態だった場合
  if (variousIndex >= 0) {
    // 変更なしの場合
    if (newLayerName == "__VARIOUS_VALUES__") return;

    m_layerCombo->removeItem(variousIndex);
  }
  // もともと値が統一だった場合
  else if (m_shapes[0]->matteInfo().layerName == newLayerName)
    return;

  for (auto shape : m_shapes) shape->setMatteLayerName(newLayerName);

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();

  // レイヤーが指定されていない場合は色とToleranceのUIを無効化する
  bool on = !newLayerName.isEmpty();
  m_colorListWidget->setEnabled(on);
  m_toleranceEdit->setEnabled(on);
  m_addColorBtn->setEnabled(on);
  m_removeColorBtn->setEnabled(on);
  m_savePresetBtn->setEnabled(on);
  m_loadPresetBtn->setEnabled(on);
}

void MatteInfoDialog::onColorItemDoubleClicked(QListWidgetItem* item) {
  int index = m_colorListWidget->row(item);
  // 複数の色設定があった場合
  if (item->text() == tr("-- Various Values --")) {
    QColor newColor = QColorDialog::getColor(
        QColor(Qt::magenta), this, tr("Please select the matte color."));
    if (!newColor.isValid()) return;

    delete m_colorListWidget->takeItem(index);
    m_colorListWidget->addItem(new ColorItem(newColor));
    QList<QColor> colors = {newColor};
    for (auto shape : m_shapes) shape->setMatteColors(colors);
  }
  // ひとつ共通の色設定の場合
  else {
    QList<QColor> colors = m_shapes[0]->matteInfo().colors;

    QColor newColor = QColorDialog::getColor(
        colors.at(index), this, tr("Please select the matte color."));
    if (!newColor.isValid()) return;

    dynamic_cast<ColorItem*>(item)->setColor(newColor);
    colors[index] = newColor;
    for (auto shape : m_shapes) shape->setMatteColors(colors);
  }
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void MatteInfoDialog::onAddColorClicked() {
  QList<QColor> colors;
  if (m_colorListWidget->item(0)->text() == tr("-- Various Values --")) {
    delete m_colorListWidget->takeItem(0);
  } else
    colors = m_shapes[0]->matteInfo().colors;

  QColor newColor(Qt::magenta);
  colors.append(newColor);

  for (auto shape : m_shapes) shape->setMatteColors(colors);
  m_colorListWidget->addItem(new ColorItem(newColor));
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void MatteInfoDialog::onRemoveColorClicked() {
  QListWidgetItem* item = m_colorListWidget->currentItem();

  if (item->text() == tr("-- Various Values --")) return;
  int index = m_colorListWidget->row(item);
  if (index < 0) return;

  QList<QColor> colors = m_shapes[0]->matteInfo().colors;
  colors.removeAt(index);
  for (auto shape : m_shapes) shape->setMatteColors(colors);

  delete m_colorListWidget->takeItem(index);
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void MatteInfoDialog::onSavePresetClicked() {
  if (m_colorListWidget->item(0)->text() == tr("-- Various Values --")) return;

  QList<QColor> colors = m_shapes[0]->matteInfo().colors;
  if (colors.isEmpty()) return;

  QString prjPath =
      IwApp::instance()->getCurrentProject()->getProject()->getPath();
  QString defaultPath;
  if (prjPath.isEmpty()) {
    defaultPath = QString(
        "//ponoc.jp/io/project/features/blue/075__IwaWarper/untitled.matcol");
  } else {
    defaultPath = QFileInfo(prjPath).dir().filePath("untitled.matcol");
  }

  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Save Preset"), defaultPath, tr("Matte Color File (*.matcol)"));

  if (fileName.isEmpty()) return;

  QSettings settings(fileName, QSettings::IniFormat);
  settings.beginGroup("MatteColors");
  int id = 0;
  for (auto color : colors) {
    settings.setValue(QString::number(id), color.name());
    id++;
  }
  settings.endGroup();
}

void MatteInfoDialog::onLoadPresetClicked() {
  QString prjPath =
      IwApp::instance()->getCurrentProject()->getProject()->getPath();
  QString defaultFolder;
  if (prjPath.isEmpty()) {
    defaultFolder =
        QString("//ponoc.jp/io/project/features/blue/075__IwaWarper");
  } else {
    defaultFolder = QFileInfo(prjPath).dir().path();
  }

  QString fileName =
      QFileDialog::getOpenFileName(this, tr("Load Preset"), defaultFolder,
                                   tr("Matte Color File (*.matcol)"));
  if (fileName.isEmpty()) return;

  QList<QColor> newColors;
  QSettings settings(fileName, QSettings::IniFormat);
  settings.beginGroup("MatteColors");

  m_colorListWidget->clear();
  for (auto key : settings.childKeys()) {
    QColor col;
    col.setNamedColor(settings.value(key).toString());
    newColors.append(col);
    m_colorListWidget->addItem(new ColorItem(col));
  }

  settings.endGroup();

  for (auto shape : m_shapes) shape->setMatteColors(newColors);
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void MatteInfoDialog::onToleranceEdited() {
  int newVal = m_toleranceEdit->text().toInt();

  bool somethingChanged = false;
  for (auto shape : m_shapes) {
    if (shape->matteInfo().tolerance == newVal) continue;
    shape->setMatteTolerance(newVal);
    somethingChanged = true;
  }

  if (somethingChanged)
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void MatteInfoDialog::onSelectionChanged(IwSelection* sel) {
  // シェイプ選択以外ならreturn
  if (dynamic_cast<IwShapePairSelection*>(sel) == nullptr) return;

  IwShapePairSelection* selection = IwShapePairSelection::instance();

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  m_shapes.clear();

  m_layerCombo->clear();
  m_layerCombo->addItem(tr("-- No Alpha Matting --"), QString());
  for (int lay = 0; lay < project->getLayerCount(); lay++) {
    IwLayer* layer = project->getLayer(lay);
    QString name   = layer->getName();
    m_layerCombo->addItem(name, name);
  }

  // 共通Info
  ShapePair::MatteInfo info;
  bool firstShape = true;

  for (int i = 0; i < selection->getShapesCount(); i++) {
    ShapePair* shape = selection->getShape(i).shapePairP;
    shape            = project->getParentShape(shape);
    // parentが存在しないないならcontinue
    if (!shape) continue;

    // 既に確認済みのshapeならcontinue
    if (m_shapes.contains(shape)) continue;

    m_shapes.append(shape);
    // 一つ目のシェイプならinfoをまるまる代入
    if (firstShape) {
      info       = shape->matteInfo();
      firstShape = false;
    }
    // 二つ目以降なら、これまでの集計と同じか確認
    else {
      ShapePair::MatteInfo newInfo = shape->matteInfo();

      // レイヤー名
      if (info.layerName != "__VARIOUS_VALUES__" &&
          info.layerName != newInfo.layerName)
        info.layerName = "__VARIOUS_VALUES__";

      // 色
      if (info.colors[0] != QColor(Qt::transparent) &&
          info.colors != newInfo.colors) {
        info.colors.clear();
        info.colors.append(QColor(Qt::transparent));
      }

      // tolerance
      if (info.tolerance != -1 && info.tolerance != newInfo.tolerance)
        info.tolerance = -1;
    }
  }

  // 選択シェイプがEmpty、またはひとつも親シェイプが見つからなかった場合
  if (firstShape) {
    m_layerCombo->addItem(tr("No Parent Shapes Selected"),
                          "__NO_SHAPES_SELECTED__");
    m_layerCombo->setCurrentIndex(
        m_layerCombo->findData("__NO_SHAPES_SELECTED__"));
    m_layerCombo->setEnabled(false);
    m_colorListWidget->setEnabled(false);
    m_toleranceEdit->setEnabled(false);
    m_addColorBtn->setEnabled(false);
    m_removeColorBtn->setEnabled(false);
    m_savePresetBtn->setEnabled(false);
    m_loadPresetBtn->setEnabled(false);
    return;
  }

  // レイヤー
  if (info.layerName == "__VARIOUS_VALUES__")
    m_layerCombo->addItem(tr("-- Various Layers --"), "__VARIOUS_VALUES__");
  int index = m_layerCombo->findData(info.layerName);
  m_layerCombo->setCurrentIndex((index == -1) ? 0 : index);

  // 色
  m_colorListWidget->clear();
  if (info.colors[0] == QColor(Qt::transparent)) {
    m_colorListWidget->addItem(tr("-- Various Values --"));
  } else {
    for (auto color : info.colors) {
      m_colorListWidget->addItem(new ColorItem(color));
    }
  }

  // tolerance
  if (info.tolerance == -1)
    m_toleranceEdit->setText(tr("-- Various Values --"));
  else
    m_toleranceEdit->setText(QString::number(info.tolerance));

  bool on = !info.layerName.isEmpty();
  m_layerCombo->setEnabled(true);
  m_colorListWidget->setEnabled(on);
  m_toleranceEdit->setEnabled(on);
  m_addColorBtn->setEnabled(on);
  m_removeColorBtn->setEnabled(on);
  m_savePresetBtn->setEnabled(on);
  m_loadPresetBtn->setEnabled(on);
}

OpenPopupCommandHandler<MatteInfoDialog> openMatteInfoDialog(MI_MatteInfo);