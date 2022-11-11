#ifndef FORMATSETTINGSPOPUPS_H
#define FORMATSETTINGSPOPUPS_H

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"
#include <QMap>

class TPropertyGroup;
class QLabel;

namespace DVGui {
class PropertyWidget;
class PropertyComboBox;
}  // namespace DVGui

using namespace std;
using namespace DVGui;

void openFormatSettingsPopup(QWidget *parent, string format,
                             TPropertyGroup *props,
                             const TFilePath &levelPath = TFilePath(),
                             QWidget *saveInFileFld     = 0);

void closeFormatSettingsPopup();

// Scene‚ªØ‚è‘Ö‚í‚Á‚½‚çAFormatSettingsPopup‚ð‘S‚ÄhideAdelete‚µ‚Äì‚è’¼‚³‚¹‚é
void deleteAllFormatSettingsPopup();

//-----------------------------------------------------------------------------

class FormatSettingsPopup : public QDialog {
  Q_OBJECT

  string m_format;
  TPropertyGroup *m_props;
  TFilePath m_levelPath;
  // Property name->PropertyWidget

  QMap<string, PropertyWidget *> m_widgets;
  QVBoxLayout *m_mainLayout;

public:
  FormatSettingsPopup(const string format, TPropertyGroup *props,
                      QWidget *parent);
  void setLevelPath(const TFilePath &path) { m_levelPath = path; }
  void setFormatProperties(TPropertyGroup *props);

protected:
  void buildPropertyComboBox(int index, TPropertyGroup *props);
  void buildValueField(int index, TPropertyGroup *props);
  void buildPropertyCheckBox(int index, TPropertyGroup *props);
  void buildPropertyLineEdit(int index, TPropertyGroup *props);
  void showEvent(QShowEvent *se);
};
#endif  // FORMATSETTINGSPOPUPS_H