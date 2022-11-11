//-----------------------------------------------------------------------------
// IwMimedata
// クリップボードに入れるデータ。
// Undoに対応させるためにデータのClone機能を追加している
//-----------------------------------------------------------------------------
#ifndef IWMIMEDATA_H
#define IWMIMEDATA_H

#include <QMimeData>

class IwMimeData : public QMimeData {
public:
  IwMimeData();
  virtual ~IwMimeData();

  virtual IwMimeData* clone() const = 0;
  virtual void releaseData() {}
};

QMimeData* cloneData(const QMimeData* data);

#endif
