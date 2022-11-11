//-----------------------------------------------------------------------------
// IwMimedata
// クリップボードに入れるデータ。
// Undoに対応させるためにデータのClone機能を追加している
//-----------------------------------------------------------------------------
#include "iwmimedata.h"

#include <QStringList>

IwMimeData::IwMimeData() {}

//-----------------------------------------------------------------------------

IwMimeData::~IwMimeData() {}

//=============================================================================
// cloneData
//-----------------------------------------------------------------------------

QMimeData* cloneData(const QMimeData* data) {
  const IwMimeData* iwData = dynamic_cast<const IwMimeData*>(data);
  if (iwData) return iwData->clone();

  QMimeData* newData = new QMimeData();

  QStringList list = data->formats();
  if (list.isEmpty()) return newData;
  QString format = list.first();
  if (format.isEmpty()) return newData;
  QByteArray byteArray = data->data(format);
  if (byteArray.isEmpty()) return newData;

  newData->setData(format, byteArray);

  return newData;
}
