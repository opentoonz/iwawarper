//---------------------------------------------------
// Halfedge
//---------------------------------------------------
#ifndef HALFEDGE_H
#define HALFEDGE_H

#include <cassert>
#include <QVector3D>
#include <QList>
#include <QStack>

class HEVertex;
class Halfedge;
class HEFace;
class HEModel;

class HEVertex {
public:
  // 頂点座標
  QVector3D from_pos;
  // ワープ後の座標
  QVector3D to_pos;

  // この頂点を始点にもつハーフエッジの1つ
  Halfedge* halfedge = nullptr;
  // 線分制約（動かない）かどうか
  bool constrained = false;

  HEVertex(double _x, double _y, double _z)
      : from_pos(_x, _y, _z), halfedge(nullptr) {
    to_pos = from_pos;
  }
  HEVertex(const QPointF& _pos, double depth)
      : HEVertex(_pos.x(), _pos.y(), depth) {}

  HEVertex(const QPointF& _from_pos, const QPointF& _to_pos, double depth)
      : from_pos(_from_pos.x(), _from_pos.y(), depth)
      , to_pos(_to_pos.x(), _to_pos.y(), depth) {}

  bool operator==(const HEVertex& v);

  bool isConnectedTo(const HEVertex* v);
};

class Halfedge {
public:
  // 始点となる頂点
  HEVertex* vertex;
  // このハーフエッジを含む面
  HEFace* face = nullptr;
  // 稜線を挟んで反対側のハーフエッジ
  Halfedge* pair = nullptr;
  // 次のハーフエッジ
  Halfedge* next = nullptr;
  // 前のハーフエッジ
  Halfedge* prev = nullptr;
  // 線分制約（動かない）かどうか
  bool constrained = false;

  Halfedge(HEVertex* v);
  void setConstrained(bool _constrained);
  QPointF getFromPos2DVector();
  QPointF getToPos2DVector();

  void setLink(HEFace* _face, Halfedge* _pair, Halfedge* _next,
               Halfedge* _prev) {
    face = _face;
    pair = _pair;
    next = _next;
    prev = _prev;
  }

  bool isConsistOf(HEVertex* v1, HEVertex* v2) {
    return (vertex == v1 && next->vertex == v2) ||
           (vertex == v2 && next->vertex == v1);
  }

  bool isDiagonalOfConvexShape();
};

class HEFace {
public:
  // 含むハーフエッジのうちの1つ
  Halfedge* halfedge = nullptr;

  // 親シェイプ形状に含まれているかどうか
  // 初期CorrVecの挿入後、再分割前に１度だけチェックする
  bool isVisible = true;

  HEFace(Halfedge* he) {
    assert(he);
    halfedge = he;  // Face → Halfedge のリンク
  }

  // エッジ全てをcontrainedにする
  void setConstrained(bool constrained);

  Halfedge* edgeFromVertex(HEVertex* v);
  // 面積を返す
  double size() const;
  // 重心にHEVertexを作成しポインタを返す
  HEVertex* createCentroidVertex();

  bool hasConstantDepth() const;
  // 重心のDepthを返す
  double centroidDepth() const;
};

class HEModel {
public:
  QList<HEFace*> faces;
  QList<HEVertex*> vertices;
  QList<HEVertex*> superTriangleVertices;

private:
  // he と pair となるハーフエッジを探して登録する
  void setHalfedgePair(Halfedge* he);
  // 二つのハーフエッジを互いに pair とする
  void setHalfedgePair(Halfedge* he0, Halfedge* he1);
  // Vertex のリストから vertex を削除する
  void deleteVertex(HEVertex* vertex);
  // Face のリストから face を削除する
  void deleteFace(HEFace* face);

  void addVertexToEdge(Halfedge* he, HEVertex* p, HEVertex* constrain_pair,
                       QStack<Halfedge*>& heToBeChecked);
  void addVertexToFace(HEFace* face, HEVertex* p, HEVertex* constrain_pair,
                       QStack<Halfedge*>& heToBeChecked);

public:
  ~HEModel();

  HEFace* addFace(HEVertex* v0, HEVertex* v1, HEVertex* v2);

  void edgeSwap(Halfedge* he);

  // 位置が一致している頂点がすでにある場合、そのポインタを返す
  HEVertex* findVertex(QPointF& from_p, QPointF& to_p);

  // とりあえず根性で探す。点が辺上にある場合はEdgeのポインタも返す
  HEFace* findContainingFace(HEVertex* p, Halfedge** he_p);

  // モデルに点を追加
  void addVertex(HEVertex* p, HEVertex* constrain_pair = nullptr);

  // heを含む三角形の外接円が、heの反対側の
  // 頂点を含む場合trueを返す
  bool checkCircumcircle(Halfedge* he);

  // CDが線分制約であり四角形ABCDが凸
  bool checkConvexAndConstraint(Halfedge* he, HEVertex* p,
                                HEVertex* constrain_pair);

  void setConstraint(HEVertex* start, HEVertex* end, bool constraint);

  // 各三角形の重心を追加 引数は三角形を分割するか判断する面積の閾値
  void insertCentroids(double sizeThreshold);

  // 親シェイプ形状に含まれているかどうか
  // 初期CorrVecの挿入後、再分割前に１度だけチェックする
  void checkFaceVisibility(const QPolygonF& parentShapePolygon);

  // なじませる
  void smooth(double offsetThreshold);

  // SuperTriangleを消す
  void deleteSuperTriangles();

  // start と end を結ぶ線分を横切るエッジのリストを得る
  void getCrossingEdges(QStack<Halfedge*>& edges, HEVertex* start,
                        HEVertex* end);

  void print();
};

#endif
