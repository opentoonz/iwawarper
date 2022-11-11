#include "aboutpopup.h"

#include "iwfolders.h"

#include <QIcon>
#include <QPainter>
#include <QLabel>
#include <QFile>
#include <QTextStream>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QApplication>

AboutPopup::AboutPopup(QWidget* parent, Qt::WindowFlags f)
    : QDialog(parent, f) {
  setFixedSize(500, 500);
  setWindowTitle(tr("About %1").arg(qApp->applicationName()));

  QString softwareStr =
      qApp->applicationName() + "  Version " + qApp->applicationVersion();
  QString dayTimeStr = tr("Built %1").arg(__DATE__);
  QString copyrightStr =
      "All contributions by Shun Iwasawa :\n"
      "Copyright(c) Shun Iwasawa  All rights reserved.\n"
      "All other contributions : \n"
      "Copyright(c) the respective contributors.  All rights reserved.\n\n"
      "Each contributor holds copyright over their respective "
      "contributions.\n"
      "The project versioning(Git) records all such contribution source "
      "information.";
  QLabel* copyrightLabel = new QLabel(copyrightStr, this);
  copyrightLabel->setAlignment(Qt::AlignLeft);

  QTextEdit* licenseField = new QTextEdit(this);
  licenseField->setLineWrapMode(QTextEdit::NoWrap);
  licenseField->setReadOnly(true);
  QStringList licenseFiles;
  licenseFiles << "LICENSE_libjpeg.txt"
               << "LICENSE_libmypaint.txt"
               << "LICENSE_libtiff.txt"
               << "LICENSE_lzo.txt"
               << "LICENSE_opentoonz.txt"
               << "LICENSE_qt.txt";

  for (const QString& fileName : licenseFiles) {
    QString licenseTxt;
    licenseTxt.append("[ " + fileName + " ]\n\n");

    QFile file(IwFolders::getLicenseFolderPath() + "/" + fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

    QTextStream in(&file);
    while (!in.atEnd()) {
      QString line = in.readLine();
      licenseTxt.append(line);
      licenseTxt.append("\n");
    }

    licenseTxt.append("\n-------------------------\n");

    licenseField->append(licenseTxt);
  }
  licenseField->moveCursor(QTextCursor::Start);

  QVBoxLayout* lay = new QVBoxLayout();
  lay->setMargin(15);
  lay->setSpacing(3);
  {
    lay->addSpacing(120);

    lay->addWidget(new QLabel(softwareStr, this), 0, Qt::AlignLeft);
    lay->addWidget(new QLabel(dayTimeStr, this), 0, Qt::AlignLeft);

    lay->addSpacing(10);

    lay->addWidget(copyrightLabel, 0, Qt::AlignLeft);

    lay->addSpacing(10);

    lay->addWidget(new QLabel(tr("Licenses"), this), 0, Qt::AlignLeft);
    lay->addWidget(licenseField, 1);
  }
  setLayout(lay);
}

void AboutPopup::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.drawPixmap(128, 50, QIcon(":Resources/IwaWarper_logo.svg").pixmap(250, 50));
}
