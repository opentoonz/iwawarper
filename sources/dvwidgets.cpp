
#include "dvwidgets.h"

#include <QLayout>
#include <QLabel>

#include <QIntValidator>
#include <QString>

using namespace DVGui;

//=============================================================================
// PropertyComboBox
//-----------------------------------------------------------------------------

PropertyComboBox::PropertyComboBox(QWidget *parent, TEnumProperty *prop)
    : QComboBox(parent), PropertyWidget(prop) {
  connect(this, SIGNAL(currentIndexChanged(const QString &)), this,
          SLOT(onCurrentIndexChanged(const QString &)));
  setMaximumHeight(20);
}

//-----------------------------------------------------------------------------

void PropertyComboBox::onCurrentIndexChanged(const QString & /*text*/) {
  TEnumProperty *prop = dynamic_cast<TEnumProperty *>(m_property);
  if (prop && prop->getValue() != itemText(currentIndex()).toStdWString())
    prop->setValue(itemText(currentIndex()).toStdWString());
}

//-----------------------------------------------------------------------------

void PropertyComboBox::onPropertyChanged() {
  TEnumProperty *prop = dynamic_cast<TEnumProperty *>(m_property);
  if (prop) {
    QString str = QString::fromStdWString(prop->getValue());
    int i       = 0;
    while (itemText(i) != str) i++;
    setCurrentIndex(i);
  }
}

//=============================================================================
// PropertyCheckBox
//-----------------------------------------------------------------------------

PropertyCheckBox::PropertyCheckBox(const QString &text, QWidget *parent,
                                   TBoolProperty *prop)
    : QCheckBox(text, parent), PropertyWidget(prop) {
  connect(this, SIGNAL(stateChanged(int)), this, SLOT(onStateChanged(int)));
  setMaximumHeight(20);
}

//-----------------------------------------------------------------------------

void PropertyCheckBox::onStateChanged(int /*state*/) {
  TBoolProperty *prop = dynamic_cast<TBoolProperty *>(m_property);
  if (prop && prop->getValue() != isChecked()) prop->setValue(isChecked());
}

//-----------------------------------------------------------------------------

void PropertyCheckBox::onPropertyChanged() {
  TBoolProperty *prop = dynamic_cast<TBoolProperty *>(m_property);
  if (prop) setChecked(prop->getValue());
}

//=============================================================================
// PropertyLineEdit
//-----------------------------------------------------------------------------

PropertyLineEdit::PropertyLineEdit(QWidget *parent, TStringProperty *prop)
    : QLineEdit(parent), PropertyWidget(prop) {
  connect(this, SIGNAL(textChanged(const QString &)), this,
          SLOT(onTextChanged(const QString &)));
  setMaximumSize(100, 20);
}

//-----------------------------------------------------------------------------

void PropertyLineEdit::onTextChanged(const QString &text) {
  TStringProperty *prop = dynamic_cast<TStringProperty *>(m_property);
  if (prop && prop->getValue() != text.toStdWString())
    prop->setValue(text.toStdWString());
}

//-----------------------------------------------------------------------------

void PropertyLineEdit::onPropertyChanged() {
  TStringProperty *prop = dynamic_cast<TStringProperty *>(m_property);
  if (prop) setText(QString::fromStdWString(prop->getValue()));
}

//=============================================================================
// PropertyIntField
//-----------------------------------------------------------------------------

PropertyIntField::PropertyIntField(QWidget *parent, TIntProperty *prop)
    : QLineEdit(parent), PropertyWidget(prop) {
  m_validator = new QIntValidator();
  setValidator(m_validator);

  connect(this, SIGNAL(editingFinished()), this, SLOT(onValueChanged()));
}

//-----------------------------------------------------------------------------

void PropertyIntField::setValues(int value, int minValue, int maxValue) {
  m_validator->setRange(minValue, maxValue);
  setText(QString::number(value));
}

//-----------------------------------------------------------------------------

void PropertyIntField::onValueChanged() {
  TIntProperty *prop = dynamic_cast<TIntProperty *>(m_property);
  if (prop) prop->setValue(text().toInt());
}

//-----------------------------------------------------------------------------

void PropertyIntField::onPropertyChanged() {
  TIntProperty *prop = dynamic_cast<TIntProperty *>(m_property);
  if (prop) this->setText(QString::number(prop->getValue()));
}
