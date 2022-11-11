#pragma once
#ifndef IWTRIANGLECACHE_H
#define IWTRIANGLECACHE_H

// プレビュー時はラスタライズを行わないで、GLのテクスチャ描画

#include <QMap>

class ShapePair;
class IwLayer;

struct Vertex {
  double pos[3];
  double uv[2];
};

struct Vertices {
  Vertex* vert;
  int* ids;
  int count;
  bool isValid;  // キャッシュが最新の情報を反映しているかどうか
};

class IwTriangleCache {  // singleton
  QMap<int, QMap<ShapePair*, Vertices>> m_data;
  IwTriangleCache();
  ~IwTriangleCache();

public:
  static IwTriangleCache* instance();

  // check
  bool isCached(int frame, ShapePair* shape);
  bool isValid(int frame, ShapePair* shape);
  int vertexCount(int frame, ShapePair* shape);
  Vertex* vertexData(int frame, ShapePair* shape);
  int* idsData(int frame, ShapePair* shape);
  const Vertices& data(int frame, ShapePair* shape);
  bool isFrameValid(int frame);

  // add cache
  void addCache(int frame, ShapePair* shape, Vertices vertices);

  // removing cache
  void removeAllCache();
  QMap<int, QMap<ShapePair*, Vertices>>::iterator removeFrameCache(int frame);
  QMap<ShapePair*, Vertices>::iterator removeCache(int frame, ShapePair* shape);
  void removeInvalid(int frame);

  // invalidate cache
  void invalidateAllCache();
  void invalidateShapeCache(ShapePair* shape);
  void invalidateLayerCache(IwLayer* layer);
  void invalidateFrameCache(int frame);
  void invalidateCache(int frame, ShapePair* shape);
  // フレーム範囲指定
  void invalidateCache(int from, int to, ShapePair* shape);

  // mutex
  void lock();
  void unlock();
};

#endif