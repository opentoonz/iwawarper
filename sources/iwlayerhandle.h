//---------------------------------------------------
// IwLayerHandle
// シークエンスに代わり、レイヤで管理する
//---------------------------------------------------

#ifndef IWLAYERHANDLE_H
#define IWLAYERHANDLE_H

#include <QObject>

#include "iwlayer.h"

class IwLayerHandle : public QObject {
  Q_OBJECT

  IwLayer* m_layer;

public:
  IwLayerHandle();

  IwLayer* getLayer() const;

  // レイヤが本当に切り替わった場合trueを返す
  bool setLayer(IwLayer* layer);

  // レイヤの変更をエミットする
  void notifyLayerChanged(IwLayer* lay) { emit layerChanged(lay); }

signals:
  // カレントレイヤが切り替わった
  void layerSwitched();
  // レイヤが変更された
  void layerChanged(IwLayer*);
};

#endif