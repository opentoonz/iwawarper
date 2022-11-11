//-----------------------------------------------------------------------------
// MyPropertyGroup
// TPropertyGroup に saveDataを可能にしたもの
//-----------------------------------------------------------------------------

#include "mypropertygroup.h"

#include <QXmlStreamWriter>
#include <QXmlStreamReader>

void MyPropertyWriter::visit(TDoubleProperty* p) {
  m_writer.writeEmptyElement("double");
  m_writer.writeAttribute("name", QString::fromStdString(p->getName()));
  m_writer.writeAttribute("min", QString::number(p->getRange().first));
  m_writer.writeAttribute("max", QString::number(p->getRange().second));
  m_writer.writeAttribute("value", QString::number(p->getValue()));
  /*
  std::map<string,string> attr;
  attr["type"]="double";
  attr["name"]=p->getName();
  attr["min"]=toString(p->getRange().first);
  attr["max"]=toString(p->getRange().second);
  attr["value"]=toString(p->getValue());
  m_os.openCloseChild("property", attr);
  */
}

void MyPropertyWriter::visit(TDoublePairProperty* p) {
  m_writer.writeEmptyElement("doublePair");
  m_writer.writeAttribute("name", QString::fromStdString(p->getName()));
  m_writer.writeAttribute("min", QString::number(p->getRange().first));
  m_writer.writeAttribute("max", QString::number(p->getRange().second));
  m_writer.writeAttribute("valueMin", QString::number(p->getValue().first));
  m_writer.writeAttribute("valueMax", QString::number(p->getValue().second));
  /*
  std::map<string,string> attr;
  attr["type"]="pair";
  attr["name"]=p->getName();
  attr["min"]=toString(p->getRange().first);
  attr["max"]=toString(p->getRange().second);
  TDoublePairProperty::Value value = p->getValue();
  attr["value"]=toString(value.first)+" "+toString(value.second);
  m_os.openCloseChild("property", attr);
  */
}

void MyPropertyWriter::visit(TIntPairProperty* p) {
  m_writer.writeEmptyElement("intPair");
  m_writer.writeAttribute("name", QString::fromStdString(p->getName()));
  m_writer.writeAttribute("min", QString::number(p->getRange().first));
  m_writer.writeAttribute("max", QString::number(p->getRange().second));
  m_writer.writeAttribute("valueMin", QString::number(p->getValue().first));
  m_writer.writeAttribute("valueMax", QString::number(p->getValue().second));
  /*
  std::map<string,string> attr;
  attr["type"]="pair";
  attr["name"]=p->getName();
  attr["min"]=toString(p->getRange().first);
  attr["max"]=toString(p->getRange().second);
  TIntPairProperty::Value value = p->getValue();
  attr["value"]=toString(value.first)+" "+toString(value.second);
  m_os.openCloseChild("property", attr);
  */
}

void MyPropertyWriter::visit(TIntProperty* p) {
  m_writer.writeEmptyElement("int");
  m_writer.writeAttribute("name", QString::fromStdString(p->getName()));
  m_writer.writeAttribute("min", QString::number(p->getRange().first));
  m_writer.writeAttribute("max", QString::number(p->getRange().second));
  m_writer.writeAttribute("value", QString::number(p->getValue()));
  /*
  std::map<string,string> attr;
  attr["type"]="int";
  attr["name"]=p->getName();
  attr["min"]=toString(p->getRange().first);
  attr["max"]=toString(p->getRange().second);
  attr["value"]=toString(p->getValue());
  m_os.openCloseChild("property", attr);
  */
}

void MyPropertyWriter::visit(TBoolProperty* p) {
  m_writer.writeEmptyElement("bool");
  m_writer.writeAttribute("name", QString::fromStdString(p->getName()));
  m_writer.writeAttribute("value", (p->getValue()) ? "True" : "False");
  /*
  std::map<string,string> attr;
  attr["type"]="bool";
  attr["name"]=p->getName();
  attr["value"]=p->getValue()?"true":"false";
  m_os.openCloseChild("property", attr);
  */
}

void MyPropertyWriter::visit(TStringProperty* p) {
  m_writer.writeEmptyElement("string");
  m_writer.writeAttribute("name", QString::fromStdString(p->getName()));
  m_writer.writeAttribute("value", QString::fromStdWString(p->getValue()));
  /*
  std::map<string,string> attr;
  attr["type"]="string";
  attr["name"]=p->getName();
  attr["value"]=toString(p->getValue());
  m_os.openCloseChild("property", attr);
  */
}

void MyPropertyWriter::visit(TStyleIndexProperty* p) {
  m_writer.writeEmptyElement("styleIndex");
  m_writer.writeAttribute("name", QString::fromStdString(p->getName()));
  m_writer.writeAttribute("value", QString::fromStdWString(p->getValue()));
  /*
  std::map<string,string> attr;
  attr["type"]="string";
  attr["name"]=p->getName();
  attr["value"]=p->getValueAsString();
  m_os.openCloseChild("property", attr);
  */
}

void MyPropertyWriter::visit(TEnumProperty* p) {
  if (TEnumProperty::isRangeSavingEnabled())
    m_writer.writeStartElement("enum");
  else
    m_writer.writeEmptyElement("enum");

  m_writer.writeAttribute("name", QString::fromStdString(p->getName()));
  m_writer.writeAttribute("value", QString::fromStdWString(p->getValue()));

  if (TEnumProperty::isRangeSavingEnabled()) {
    std::vector<std::wstring> range = p->getRange();
    for (int i = 0; i < (int)range.size(); i++) {
      m_writer.writeEmptyElement("item");
      m_writer.writeAttribute("value", QString::fromStdWString(range[i]));
    }
    m_writer.writeEndElement();
  }

  /*
  std::map<string,string> attr;
  attr["type"]="enum";
  attr["name"]=p->getName();
  attr["value"]=toString(p->getValue());
  if(TEnumProperty::isRangeSavingEnabled())
  {
          m_os.openChild("property", attr);
          std::vector<wstring> range = p->getRange();
          for(int i=0;i<(int)range.size();i++)
          {
                  attr.clear();
                  attr["value"] = toString(range[i]);
                  m_os.openCloseChild("item", attr);
          }
          m_os.closeChild();
  }
  else
  m_os.openCloseChild("property", attr);
  */
}

void MyPropertyWriter::visit(TPointerProperty* p) {
  m_writer.writeEmptyElement("pointer");
  m_writer.writeAttribute("name", QString::fromStdString(p->getName()));
  m_writer.writeAttribute("value",
                          QString::fromStdString(p->getValueAsString()));
  /*
  std::map<string,string> attr;
  attr["type"]="pointer";
  attr["name"]=p->getName();
  attr["value"]=p->getValueAsString();
  m_os.openCloseChild("property", attr);
  */
}

//-----------------------------------------------------------------------------

MyPropertyGroup::MyPropertyGroup(TPropertyGroup* prop) : m_prop(prop) {}

// セーブ/ロード
void MyPropertyGroup::saveData(QXmlStreamWriter& writer) {
  MyPropertyWriter visitor(writer);
  m_prop->accept(visitor);
}

void MyPropertyGroup::loadData(QXmlStreamReader& reader) {
  // プロパティをクリアする
  for (int p = 0; p < m_prop->getPropertyCount(); p++) {
    delete m_prop->getProperty(p);
  }
  m_prop->clear();

  while (reader.readNextStartElement()) {
    // name アトリビュートは共通
    std::string name;
    if (reader.attributes().hasAttribute("name"))
      name = reader.attributes().value("name").toString().toStdString();

    if (reader.name() == "double") {
      double min = reader.attributes().value("min").toString().toDouble();
      double max = reader.attributes().value("max").toString().toDouble();
      double val = reader.attributes().value("value").toString().toDouble();
      m_prop->add(new TDoubleProperty(name, min, max, val));
      reader.skipCurrentElement();
    } else if (reader.name() == "doublePair") {
      double min = reader.attributes().value("min").toString().toDouble();
      double max = reader.attributes().value("max").toString().toDouble();
      double valMin =
          reader.attributes().value("valueMin").toString().toDouble();
      double valMax =
          reader.attributes().value("valueMax").toString().toDouble();
      m_prop->add(new TDoublePairProperty(name, min, max, valMin, valMax));
      reader.skipCurrentElement();
    } else if (reader.name() == "intPair") {
      int min    = reader.attributes().value("min").toString().toInt();
      int max    = reader.attributes().value("max").toString().toInt();
      int valMin = reader.attributes().value("valueMin").toString().toInt();
      int valMax = reader.attributes().value("valueMax").toString().toInt();
      m_prop->add(new TIntPairProperty(name, min, max, valMin, valMax));
      reader.skipCurrentElement();
    } else if (reader.name() == "int") {
      int min = reader.attributes().value("min").toString().toInt();
      int max = reader.attributes().value("max").toString().toInt();
      int val = reader.attributes().value("value").toString().toInt();
      m_prop->add(new TIntProperty(name, min, max, val));
      reader.skipCurrentElement();
    } else if (reader.name() == "bool") {
      bool value = (reader.attributes().value("value").toString() == "True")
                       ? true
                       : false;
      m_prop->add(new TBoolProperty(name, value));
      reader.skipCurrentElement();
    } else if (reader.name() == "string") {
      std::wstring value =
          reader.attributes().value("value").toString().toStdWString();
      m_prop->add(new TStringProperty(name, value));
      reader.skipCurrentElement();
    } else if (reader.name() == "styleIndex") {
      std::wstring value =
          reader.attributes().value("value").toString().toStdWString();
      m_prop->add(new TStyleIndexProperty(name, value));
      reader.skipCurrentElement();
    } else if (reader.name() == "enum") {
      std::wstring value =
          reader.attributes().value("value").toString().toStdWString();
      TEnumProperty* p = new TEnumProperty(name);
      // itemエレメントがあるかどうかチェック
      bool itemFound = false;
      while (reader.readNextStartElement()) {
        if (reader.name() == "item") {
          std::wstring itemStr =
              reader.attributes().value("value").toString().toStdWString();
          p->addValue(itemStr);
          itemFound = true;
          reader.skipCurrentElement();
        } else
          reader.skipCurrentElement();
      }
      if (!itemFound) p->addValue(value);

      p->setValue(value);
      m_prop->add(p);
    } else
      reader.skipCurrentElement();
  }
}