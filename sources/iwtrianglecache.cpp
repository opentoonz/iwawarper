#include "iwtrianglecache.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwlayer.h"
#include "shapepair.h"
#include "outputsettings.h"

#include <QMutex>

QMutex mutex;

namespace {
void suspend(int frame) {
  IwApp::instance()->getCurrentProject()->suspendRender(frame);
}
};  // namespace

// Note: Q_MOVABLE_TYPE means it can be memcpy'd.
Q_DECLARE_TYPEINFO(MeshVertex, Q_MOVABLE_TYPE);

// Constructor
MeshVertex::MeshVertex(const QVector3D& position, const QVector2D& uv)
    : m_position(position), m_uv(uv) {}

//============================================================

IwTriangleCache::IwTriangleCache() {}

//---------------

IwTriangleCache::~IwTriangleCache() { removeAllCache(); }

//---------------

IwTriangleCache* IwTriangleCache::instance() {
  static IwTriangleCache _instance;
  return &_instance;
}

//---------------

bool IwTriangleCache::isCached(int frame, ShapePair* shape) {
  if (!m_data.contains(frame)) return false;
  return m_data.value(frame).contains(shape);
}

//---------------

bool IwTriangleCache::isValid(int frame, ShapePair* shape) {
  if (!isCached(frame, shape)) return false;
  return m_data.value(frame).value(shape).isValid;
}

//---------------

int IwTriangleCache::vertexCount(int frame, ShapePair* shape) {
  if (!isCached(frame, shape)) return -1;
  return m_data.value(frame).value(shape).vertexCount;
}

//---------------

int IwTriangleCache::pointCount(int frame, ShapePair* shape) {
  if (!isCached(frame, shape)) return -1;
  return m_data.value(frame).value(shape).pointCount;
}

//---------------

MeshVertex* IwTriangleCache::vertexData(int frame, ShapePair* shape) {
  if (!isCached(frame, shape)) return nullptr;
  return m_data.value(frame).value(shape).vert;
}

//---------------

int* IwTriangleCache::idsData(int frame, ShapePair* shape) {
  if (!isCached(frame, shape)) return nullptr;
  return m_data.value(frame).value(shape).ids;
}

//---------------
Vertices tmp;

const Vertices& IwTriangleCache::data(int frame, ShapePair* shape) {
  if (!isCached(frame, shape)) return tmp;
  return m_data[frame][shape];
}

//---------------------------------------------------
// キャッシュの確認用：親になっているシェイプのリストを返す
//---------------------------------------------------
bool IwTriangleCache::isFrameValid(int frame) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return false;

  int targetShapeTag =
      prj->getRenderQueue()->currentOutputSettings()->getShapeTagId();

  // 各レイヤについて
  for (int i = 0; i < prj->getLayerCount(); i++) {
    IwLayer* layer = prj->getLayer(i);
    if (!layer || layer->getImageFileName(frame).isEmpty()) continue;
    // レイヤにシェイプが無ければスキップ
    if (layer->getShapePairCount() == 0) continue;
    // レイヤがレンダリング非表示ならスキップ
    if (!layer->isVisibleInRender()) continue;

    // シェイプを回収していく
    for (int s = 0; s < layer->getShapePairCount(); s++) {
      ShapePair* shape = layer->getShapePair(s);
      if (!shape || !shape->isParent()) continue;

      // レンダリングの対象になっていなければスキップ
      if (!shape->isRenderTarget(targetShapeTag)) continue;

      // キャッシュが無いか、最新でなければfalseを返す
      if (!isValid(frame, shape)) return false;
    }
  }
  // 全てのチェックが終わったのでtrueを返す
  return true;
}

//---------------

void IwTriangleCache::addCache(int frame, ShapePair* shape, Vertices vertices) {
  lock();
  // 前のキャッシュを消す
  removeCache(frame, shape);
  if (m_data.contains(frame))
    m_data[frame].insert(shape, vertices);
  else {
    QMap<ShapePair*, Vertices> frameData;
    frameData[shape] = vertices;
    m_data[frame]    = frameData;
  }
  unlock();
}

//---------------

void IwTriangleCache::removeAllCache() {
  QMap<int, QMap<ShapePair*, Vertices>>::const_iterator i = m_data.constBegin();
  while (i != m_data.constEnd()) {
    i = removeFrameCache(i.key());
  }
}

//---------------

QMap<int, QMap<ShapePair*, Vertices>>::iterator
IwTriangleCache::removeFrameCache(int frame) {
  if (!m_data.contains(frame))
    QMap<int, QMap<ShapePair*, Vertices>>::iterator();
  QMap<ShapePair*, Vertices>::iterator i = m_data[frame].begin();
  while (i != m_data[frame].end()) {
    i = removeCache(frame, i.key());
  }
  suspend(frame);
  // m_data の要素を消す
  return m_data.erase(m_data.find(frame));
}

//---------------

QMap<ShapePair*, Vertices>::iterator IwTriangleCache::removeCache(
    int frame, ShapePair* shape) {
  if (!isCached(frame, shape)) return QMap<ShapePair*, Vertices>::iterator();
  delete[] m_data[frame][shape].vert;
  delete[] m_data[frame][shape].ids;
  // m_data[frame] の要素を消す
  return m_data[frame].erase(m_data[frame].find(shape));
}

//---------------

void IwTriangleCache::removeInvalid(int frame) {
  if (!m_data.contains(frame)) return;
  QMap<ShapePair*, Vertices>::iterator i = m_data[frame].begin();
  while (i != m_data[frame].end()) {
    if (!i.value().isValid)
      i = removeCache(frame, i.key());
    else
      ++i;
  }
}

//---------------

void IwTriangleCache::invalidateAllCache() {
  QMap<int, QMap<ShapePair*, Vertices>>::iterator i = m_data.begin();
  while (i != m_data.end()) {
    QMap<ShapePair*, Vertices>::iterator j = i.value().begin();
    while (j != i.value().end()) {
      j.value().isValid = false;
      ++j;
    }
    suspend(i.key());
    ++i;
  }
  // シグナル
  IwApp::instance()->getCurrentProject()->notifyPreviewCacheInvalidated();
}

//---------------

void IwTriangleCache::invalidateShapeCache(ShapePair* shape) {
  if (!shape) return;
  bool changed                                      = false;
  QMap<int, QMap<ShapePair*, Vertices>>::iterator i = m_data.begin();
  while (i != m_data.end()) {
    if (i.value().contains(shape) && i.value()[shape].isValid) {
      i.value()[shape].isValid = false;
      changed                  = true;
    }
    suspend(i.key());
    ++i;
  }
  // シグナル
  if (changed)
    IwApp::instance()->getCurrentProject()->notifyPreviewCacheInvalidated();
}

//---------------
// レイヤーに属するShapeを全部invalidate
void IwTriangleCache::invalidateLayerCache(IwLayer* layer) {
  for (int s = 0; s < layer->getShapePairCount(); s++) {
    ShapePair* shape = layer->getShapePair(s);
    if (!shape) continue;
    IwTriangleCache::instance()->invalidateShapeCache(shape);
  }
}

//---------------

void IwTriangleCache::invalidateFrameCache(int frame) {
  if (!m_data.contains(frame)) return;
  bool changed                           = false;
  QMap<ShapePair*, Vertices>::iterator i = m_data[frame].begin();
  while (i != m_data[frame].end()) {
    if (i.value().isValid) {
      i.value().isValid = false;
      changed           = true;
    }
    ++i;
  }
  // シグナル
  if (changed)
    IwApp::instance()->getCurrentProject()->notifyPreviewCacheInvalidated();
  suspend(frame);
}

void IwTriangleCache::invalidateCache(int frame, ShapePair* shape) {
  if (!isCached(frame, shape)) return;
  if (m_data[frame][shape].isValid) {
    m_data[frame][shape].isValid = false;
    // シグナル
    IwApp::instance()->getCurrentProject()->notifyPreviewCacheInvalidated();
  }
  suspend(frame);
}

// フレーム範囲指定
void IwTriangleCache::invalidateCache(int from, int to, ShapePair* shape) {
  bool changed = false;
  for (int f = from; f <= to; f++) {
    suspend(f);
    if (!isCached(f, shape)) continue;
    if (m_data[f][shape].isValid) {
      m_data[f][shape].isValid = false;
      changed                  = true;
    }
  }
  // シグナル
  if (changed)
    IwApp::instance()->getCurrentProject()->notifyPreviewCacheInvalidated();
}
// mutex
void IwTriangleCache::lock() { mutex.lock(); }
void IwTriangleCache::unlock() { mutex.unlock(); }