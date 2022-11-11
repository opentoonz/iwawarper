//---------------------------------------------------
// IwLayerHandle
//---------------------------------------------------

#include "iwlayerhandle.h"

IwLayerHandle::IwLayerHandle() : m_layer(0) {}

IwLayer* IwLayerHandle::getLayer() const { return m_layer; }

//---------------------------------------------------
// ƒŒƒCƒ„‚ª–{“–‚ÉØ‚è‘Ö‚í‚Á‚½ê‡true‚ğ•Ô‚·
//---------------------------------------------------
bool IwLayerHandle::setLayer(IwLayer* layer) {
  if (m_layer == layer) return false;
  m_layer = layer;
  if (m_layer) emit layerSwitched();
  return true;
}
