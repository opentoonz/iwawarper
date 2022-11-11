#include "formatsettingspopups.h"
#include "toonz/tscenehandle.h"
#include "dvwidgets.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "tlevel_io.h"
#include "toutputproperties.h"
#include "tproperty.h"
#include "toonz/tcamera.h"
#include "timageinfo.h"

#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QString>

//=============================================================================
// FormatSettingsPopup
//-----------------------------------------------------------------------------

FormatSettingsPopup::FormatSettingsPopup(const string format,
                                         TPropertyGroup *props, QWidget *parent)
    : QDialog(parent)
    , m_format(format)
    , m_props(props)
    , m_levelPath(TFilePath()) {
  QString name = tr("File Settings");
  setWindowTitle(name);

  setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);

  m_mainLayout = new QVBoxLayout();
  m_mainLayout->setMargin(5);
  m_mainLayout->setSpacing(5);

  int i = 0;
  for (i = 0; i < m_props->getPropertyCount(); i++) {
    if (dynamic_cast<TEnumProperty *>(m_props->getProperty(i)))
      buildPropertyComboBox(i, m_props);
    else if (dynamic_cast<TIntProperty *>(m_props->getProperty(i)))
      buildValueField(i, m_props);
    else if (dynamic_cast<TBoolProperty *>(m_props->getProperty(i)))
      buildPropertyCheckBox(i, m_props);
    else if (dynamic_cast<TStringProperty *>(m_props->getProperty(i)))
      buildPropertyLineEdit(i, m_props);
    else
      assert(false);
  }

  setLayout(m_mainLayout);
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::setFormatProperties(TPropertyGroup *props) {
  if (props == m_props) return;
  TPropertyGroup *oldProps = m_props;
  m_props                  = props;
  int i;
  for (i = 0; i < m_props->getPropertyCount(); i++) {
    TProperty *prop = m_props->getProperty(i);
    if (prop) m_widgets[prop->getName()]->setProperty(prop);
  }
  delete oldProps;
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::buildPropertyComboBox(int index,
                                                TPropertyGroup *props) {
  TEnumProperty *prop = (TEnumProperty *)(props->getProperty(index));
  assert(prop);

  PropertyComboBox *comboBox = new PropertyComboBox(this, prop);
  m_widgets[prop->getName()] = comboBox;
  TEnumProperty::Range range = prop->getRange();
  int currIndex              = -1;
  wstring defaultVal         = prop->getValue();

  for (int i = 0; i < (int)range.size(); i++) {
    wstring nameProp = range[i];

    if (nameProp.find(L"16(GREYTONES)") != -1) continue;

    if (nameProp == defaultVal) currIndex = comboBox->count();
    comboBox->addItem(QString::fromStdWString(nameProp));
  }
  if (currIndex >= 0) comboBox->setCurrentIndex(currIndex);

  QHBoxLayout *layout = new QHBoxLayout();
  layout->setMargin(0);
  layout->setSpacing(5);
  {
    layout->addWidget(new QLabel(tr(prop->getName().c_str()) + ":", this), 0);
    layout->addWidget(comboBox, 1);
  }
  m_mainLayout->addLayout(layout);
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::buildValueField(int index, TPropertyGroup *props) {
  TIntProperty *prop = (TIntProperty *)(props->getProperty(index));
  assert(prop);

  PropertyIntField *v = new PropertyIntField(this, prop);

  m_widgets[prop->getName()] = v;

  QHBoxLayout *layout = new QHBoxLayout();
  layout->setMargin(0);
  layout->setSpacing(5);
  {
    layout->addWidget(new QLabel(tr(prop->getName().c_str()) + ":", this), 0);
    layout->addWidget(v, 1);
  }
  m_mainLayout->addLayout(layout);

  v->setValues(prop->getValue(), prop->getRange().first,
               prop->getRange().second);
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::buildPropertyCheckBox(int index,
                                                TPropertyGroup *props) {
  TBoolProperty *prop = (TBoolProperty *)(props->getProperty(index));
  assert(prop);

  PropertyCheckBox *v =
      new PropertyCheckBox(tr(prop->getName().c_str()), this, prop);
  m_widgets[prop->getName()] = v;

  m_mainLayout->addWidget(v);

  v->setChecked(prop->getValue());
}

//-----------------------------------------------------------------------------

void FormatSettingsPopup::buildPropertyLineEdit(int index,
                                                TPropertyGroup *props) {
  TStringProperty *prop = (TStringProperty *)(props->getProperty(index));
  assert(prop);

  PropertyLineEdit *lineEdit = new PropertyLineEdit(this, prop);
  m_widgets[prop->getName()] = lineEdit;
  lineEdit->setText(QString::fromStdWString(prop->getValue()));

  QHBoxLayout *layout = new QHBoxLayout();
  layout->setMargin(0);
  layout->setSpacing(5);
  {
    layout->addWidget(new QLabel(tr(prop->getName().c_str()) + ":", this), 0);
    layout->addWidget(lineEdit, 1);
  }
  m_mainLayout->addLayout(layout);
}

void FormatSettingsPopup::showEvent(QShowEvent *se) { QDialog::showEvent(se); }

//-----------------------------------------------------------------------------
namespace
//-----------------------------------------------------------------------------
{
std::map<string, QDialog *> FormatPopupsMap;

//-----------------------------------------------------------------------------

FormatSettingsPopup *createFormatSettingPopup(QWidget *parent, string format,
                                              TPropertyGroup *props) {
  if (!props || props->getPropertyCount() == 0) return 0;

  return new FormatSettingsPopup(format, props, parent);
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

void openFormatSettingsPopup(QWidget *parent, string format,
                             TPropertyGroup *props, const TFilePath &levelPath,
                             QWidget *saveInFileFld  // 110521 í«â¡ iwasawa
) {
  std::map<string, QDialog *>::const_iterator it = FormatPopupsMap.find(format);

  if (it == FormatPopupsMap.end()) {
    FormatPopupsMap[format] = createFormatSettingPopup(parent, format, props);
    it                      = FormatPopupsMap.find(format);
  }

  if (it != FormatPopupsMap.end() && it->second) {
    if (it->second->isHidden()) {
      closeFormatSettingsPopup();
      FormatSettingsPopup *popup =
          dynamic_cast<FormatSettingsPopup *>(it->second);
      if (popup) {
        if (!levelPath.isEmpty()) popup->setLevelPath(levelPath);
        popup->setFormatProperties(props);
      }
      it->second->show();
      it->second->raise();
    }
  }
}

//-----------------------------------------------------------------------------

void closeFormatSettingsPopup() {
  std::map<string, QDialog *>::iterator it = FormatPopupsMap.begin();
  for (; it != FormatPopupsMap.end(); ++it)
    if (it->second && !it->second->isHidden()) {
      it->second->close();
      return;
    }
}
//-----------------------------------------------------------------------------
// FormatSettingsPopupÇëSÇƒhideÅAdeleteÇµÇƒçÏÇËíºÇ≥ÇπÇÈ
//-----------------------------------------------------------------------------
void deleteAllFormatSettingsPopup() {
  closeFormatSettingsPopup();

  std::map<string, QDialog *>::iterator it = FormatPopupsMap.begin();
  for (; it != FormatPopupsMap.end(); ++it) delete it->second;

  FormatPopupsMap.clear();
}
//-----------------------------------------------------------------------------