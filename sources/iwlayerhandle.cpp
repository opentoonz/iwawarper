//---------------------------------------------------
// IwLayerHandle
//---------------------------------------------------

#include "iwlayerhandle.h"

IwLayerHandle::IwLayerHandle() : m_layer(0) {}

IwLayer* IwLayerHandle::getLayer() const { return m_layer; }

//---------------------------------------------------
// レイヤが本当に切り替わった場合trueを返す
//---------------------------------------------------
bool IwLayerHandle::setLayer(IwLayer* layer) {
  if (m_layer == layer) return false;
  m_layer = layer;
  if (m_layer) emit layerSwitched();
  return true;
}
