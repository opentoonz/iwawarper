//---------------------------------------------------
// ShapePairData
// ShapePairのコピー／ペーストで扱うデータ
//---------------------------------------------------

#ifndef SHAPEPAIRDATA_H
#define SHAPEPAIRDATA_H

#include "iwmimedata.h"

#include <QList>
#include <QMap>

class ShapePair;
class IwLayer;

class ShapePairData : public IwMimeData {
public:
  QList<ShapePair*> m_data;

  ShapePairData() {}
  ShapePairData(const ShapePairData* src) { m_data = src->m_data; }
  ~ShapePairData();

  ShapePairData* clone() const { return new ShapePairData(this); }

  // 選択範囲からデータを格納する
  void setShapePairData(QList<ShapePair*>& shapes);

  // データをペーストする
  void getShapePairData(IwLayer* layer, QList<ShapePair*>& shapes) const;
};

#endif