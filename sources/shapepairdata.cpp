//---------------------------------------------------
// ShapePairData
// ShapePairのコピー／ペーストで扱うデータ
//---------------------------------------------------

#include "shapepairdata.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwlayerhandle.h"
#include "iwproject.h"
#include "iwlayer.h"
#include "shapepair.h"
#include "viewsettings.h"

//----------------------------------------------------

ShapePairData::~ShapePairData() {
  // すべてdelete
  for (int s = 0; s < m_data.size(); s++) {
    delete m_data.at(s);
  }
}

//----------------------------------------------------
// 選択範囲からデータを格納する
//----------------------------------------------------
void ShapePairData::setShapePairData(QList<ShapePair*>& shapes) {
  // 選択シェイプをそれぞれcloneする。
  for (int s = 0; s < shapes.size(); s++) {
    ShapePair* shape       = shapes.at(s);
    ShapePair* clonedShape = shape->clone();
    m_data.push_back(clonedShape);
  }
}

//----------------------------------------------------
// データをペーストする
//----------------------------------------------------
void ShapePairData::getShapePairData(IwLayer* layer,
                                     QList<ShapePair*>& shapes) const {
  // プロジェクト無ければreturn
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  for (int s = 0; s < m_data.size(); s++) {
    ShapePair* clonedShape = m_data.at(s)->clone();
    layer->addShapePair(clonedShape);
    shapes.append(clonedShape);
  }
}