//-----------------------------------------------------------------------------
// MyPropertyGroup
// TPropertyGroup ‚É saveData‚ð‰Â”\‚É‚µ‚½‚à‚Ì
//-----------------------------------------------------------------------------
#ifndef MYPROPERTYGROUP_H
#define MYPROPERTYGROUP_H

#include "tproperty.h"

class TPropertyGroup;
class QXmlStreamWriter;
class QXmlStreamReader;

class MyPropertyWriter : public TProperty::Visitor {
  QXmlStreamWriter& m_writer;

public:
  MyPropertyWriter(QXmlStreamWriter& writer) : m_writer(writer) {}
  void visit(TDoubleProperty* p);
  void visit(TDoublePairProperty* p);
  void visit(TIntPairProperty* p);
  void visit(TIntProperty* p);
  void visit(TBoolProperty* p);
  void visit(TStringProperty* p);
  void visit(TStyleIndexProperty* p);
  void visit(TEnumProperty* p);
  void visit(TPointerProperty* p);
};

class MyPropertyGroup {
  TPropertyGroup* m_prop;

public:
  MyPropertyGroup(TPropertyGroup* prop);

  TPropertyGroup* getProp() { return m_prop; }

  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);
};

#endif