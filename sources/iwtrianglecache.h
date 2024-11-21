#pragma once
#ifndef IWTRIANGLECACHE_H
#define IWTRIANGLECACHE_H

// プレビュー時はラスタライズを行わないで、GLのテクスチャ描画

#include <QMap>
#include <QVector3D>
#include <QVector2D>

class ShapePair;
class IwLayer;

struct Vertex {
  double pos[3];
  double uv[2];
};

#define VertAmount 100000

class MeshVertex {
  QVector3D m_position;
  QVector2D m_uv;

public:
  // ctor
  MeshVertex() {}
  MeshVertex(const QVector3D& position, const QVector2D& uv);

  // Accessors / Mutators
  const QVector3D& position();
  const QVector2D& uv();
  void setPosition(const QVector3D& position);
  void setUV(const QVector2D& uv);

  // OpenGL Helpers
  static const int PositionTupleSize = 3;
  static const int UvTupleSize       = 2;
  static int positionOffset();
  static int uvOffset();
  static int stride();
};
// OpenGL Helpers
inline int MeshVertex::positionOffset() {
  return offsetof(MeshVertex, m_position);
}
inline int MeshVertex::uvOffset() { return offsetof(MeshVertex, m_uv); }
inline int MeshVertex::stride() { return sizeof(MeshVertex); }

// Accessors / Mutators
inline const QVector3D& MeshVertex::position() { return m_position; }
inline const QVector2D& MeshVertex::uv() { return m_uv; }
void inline MeshVertex::setPosition(const QVector3D& position) {
  m_position = position;
}
void inline MeshVertex::setUV(const QVector2D& uv) { m_uv = uv; }

struct Vertices {
  MeshVertex* vert;
  int* ids;
  int pointCount;
  int vertexCount;
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
  int pointCount(int frame, ShapePair* shape);
  MeshVertex* vertexData(int frame, ShapePair* shape);
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