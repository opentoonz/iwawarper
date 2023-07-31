//---------------------------------------------------
// Halfedge
//---------------------------------------------------

#include "halfedge.h"

#include <QGenericMatrix>
#include <QPolygonF>
#include <QMap>
#include <QPair>
#include <iostream>
#include <QVector2D>

typedef QGenericMatrix<3, 3, double> Matrix3x3d;

namespace {
bool isAlmostSame(double val1, double val2) {
  return std::abs(val1 - val2) < 0.001f;
}

bool judgeIntersected(const QPointF& pA, const QPointF& pB, const QPointF& pC,
                      const QPointF& pD) {
  double ta = (pC.x() - pD.x()) * (pA.y() - pC.y()) +
              (pC.y() - pD.y()) * (pC.x() - pA.x());
  double tb = (pC.x() - pD.x()) * (pB.y() - pC.y()) +
              (pC.y() - pD.y()) * (pC.x() - pB.x());
  double tc = (pA.x() - pB.x()) * (pC.y() - pA.y()) +
              (pA.y() - pB.y()) * (pA.x() - pC.x());
  double td = (pA.x() - pB.x()) * (pD.y() - pA.y()) +
              (pA.y() - pB.y()) * (pA.x() - pD.x());

  return tc * td < 0 && ta * tb < 0;
  // return tc * td <= 0 && ta * tb <= 0; // in case including edge points
  // 端点を含む場合
}

double determinant(const Matrix3x3d& m) {
  return m(0, 0) * m(1, 1) * m(2, 2) + m(0, 1) * m(1, 2) * m(2, 0) +
         m(0, 2) * m(1, 0) * m(2, 1) - m(0, 2) * m(1, 1) * m(2, 0) -
         m(0, 1) * m(1, 0) * m(2, 2) - m(0, 0) * m(1, 2) * m(2, 1);
}

}  // namespace
//---------------------------------------------------

bool HEVertex::operator==(const HEVertex& v) {
  return from_pos.toPointF() == v.from_pos.toPointF() &&
         to_pos.toPointF() == v.to_pos.toPointF();
}

bool HEVertex::isConnectedTo(const HEVertex* v) {
  Halfedge* tmp_he = halfedge;
  while (1) {
    if (tmp_he->next->vertex == v) return true;
    tmp_he = tmp_he->prev->pair;
    if (!tmp_he || tmp_he == halfedge) return false;
  }
  return false;
}

//---------------------------------------------------

Halfedge::Halfedge(HEVertex* v) {
  vertex = v;  // link from Halfedge to Vertex. Halfedge → Vertex のリンク
  if (v->halfedge == nullptr) {
    v->halfedge =
        this;  // link from Vertex to HalfEdge. Vertex → Halfedge のリンク
  }
}

void Halfedge::setConstrained(bool _constrained) {
  constrained         = _constrained;
  vertex->constrained = _constrained;
  if (pair) {
    pair->constrained         = _constrained;
    pair->vertex->constrained = _constrained;
  }
}

QPointF Halfedge::getFromPos2DVector() {
  if (!next) return QPointF();
  return next->vertex->from_pos.toPointF() - vertex->from_pos.toPointF();
}

QPointF Halfedge::getToPos2DVector() {
  if (!next) return QPointF();
  return next->vertex->to_pos.toPointF() - vertex->to_pos.toPointF();
}

bool Halfedge::isDiagonalOfConvexShape() {
  HEVertex* A = vertex;
  HEVertex* B = pair->vertex;
  HEVertex* C = prev->vertex;
  HEVertex* D = pair->prev->vertex;
  QPointF pA  = A->from_pos.toPointF();
  QPointF pB  = B->from_pos.toPointF();
  QPointF pC  = C->from_pos.toPointF();
  QPointF pD  = D->from_pos.toPointF();
  return judgeIntersected(pA, pB, pC, pD);
}

//---------------------------------------------------
// Make all edges to be constrained. エッジ全てをcontrainedにする
void HEFace::setConstrained(bool constrained) {
  halfedge->setConstrained(constrained);
  halfedge->next->setConstrained(constrained);
  halfedge->prev->setConstrained(constrained);
}

Halfedge* HEFace::edgeFromVertex(HEVertex* v) {
  Halfedge* he = halfedge;
  for (int i = 0; i < 3; i++) {
    if (he->vertex == v) return he;
    he = he->next;
  }
  return nullptr;
}

double HEFace::size() const {
  QPointF ab = halfedge->getFromPos2DVector();
  QPointF ac = -(halfedge->prev->getFromPos2DVector());
  // std::abs is not necessary since it is always clockwise.
  // 右ねじの向きが保証されているのでstd::absはなくても大丈夫かも
  return 0.5 * (ab.x() * ac.y() - ac.x() * ab.y());
  // return 0.5 * std::abs(ab.x()*ac.y() - ac.x()*ab.y());
}

// Add new vertex at centroid and returns its pointer
// 重心にHEVertexを作成しポインタを返す
HEVertex* HEFace::createCentroidVertex() {
  HEVertex* A = halfedge->vertex;
  HEVertex* B = halfedge->next->vertex;
  HEVertex* C = halfedge->prev->vertex;
  QVector3D from =
      (A->from_pos * A->from_weight + B->from_pos * B->from_weight +
       C->from_pos * C->from_weight) /
      (A->from_weight + B->from_weight + C->from_weight);
  double from_weight = (A->from_weight + B->from_weight + C->from_weight) / 3.0;
  QVector3D to       = (A->to_pos * A->to_weight + B->to_pos * B->to_weight +
                  C->to_pos * C->to_weight) /
                 (A->to_weight + B->to_weight + C->to_weight);
  double to_weight = (A->to_weight + B->to_weight + C->to_weight) / 3.0;

  return new HEVertex(from.toPointF(), to.toPointF(), from.z(), from_weight,
                      to_weight);
}

bool HEFace::hasConstantDepth() const {
  HEVertex* A = halfedge->vertex;
  HEVertex* B = halfedge->next->vertex;
  HEVertex* C = halfedge->prev->vertex;

  return isAlmostSame(A->from_pos.z(), B->from_pos.z()) &&
         isAlmostSame(A->from_pos.z(), C->from_pos.z());
}

// Returns depth of the centroid. 重心のDepthを返す
double HEFace::centroidDepth() const {
  HEVertex* A = halfedge->vertex;
  HEVertex* B = halfedge->next->vertex;
  HEVertex* C = halfedge->prev->vertex;
  return (A->from_pos.z() + B->from_pos.z() + C->from_pos.z()) / 3.0;
}

//---------------------------------------------------

// Find a pair half edge of he and register it
// he と pair となるハーフエッジを探して登録する
void HEModel::setHalfedgePair(Halfedge* he) {
  QList<HEFace*>::iterator it_f;  // すべてのFaceを巡回する
  for (it_f = faces.begin(); it_f != faces.end();
       it_f++) {  // 現在のFaceに含まれるハーフエッジを巡回する
    Halfedge* halfedge_in_face = (*it_f)->halfedge;
    do {
      if (he->vertex == halfedge_in_face->next->vertex &&
          he->next->vertex ==
              halfedge_in_face
                  ->vertex) {  // 両方の端点が共通だったらpairに登録する
        he->pair               = halfedge_in_face;
        halfedge_in_face->pair = he;
        return;
      }
      halfedge_in_face = halfedge_in_face->next;
    } while (halfedge_in_face != (*it_f)->halfedge);
  }
}

// Make two half edges as a pair
// 二つのハーフエッジを互いに pair とする
void HEModel::setHalfedgePair(Halfedge* he0, Halfedge* he1) {
  he0->pair = he1;
  he1->pair = he0;
}

// Remove vertex from the list
// Vertex のリストから vertex を削除する
void HEModel::deleteVertex(HEVertex* vertex) { delete vertex; }

// Remove face from the list
// Face のリストから face を削除する
void HEModel::deleteFace(HEFace* face) {
  delete face->halfedge->next;
  delete face->halfedge->prev;
  delete face->halfedge;
  delete face;
}

HEModel::~HEModel() {
  for (HEFace* face : faces) deleteFace(face);
  faces.clear();

  for (HEVertex* vertex : vertices) deleteVertex(vertex);
  vertices.clear();
}

HEFace* HEModel::addFace(HEVertex* v0, HEVertex* v1, HEVertex* v2) {
  Halfedge* he0 = new Halfedge(v0);  // (5)
  Halfedge* he1 = new Halfedge(v1);  // (5)
  Halfedge* he2 = new Halfedge(v2);  // (5)

  HEFace* face = new HEFace(he0);  // (7)

  he0->setLink(face, nullptr, he1, he2);
  he1->setLink(face, nullptr, he2, he0);
  he2->setLink(face, nullptr, he0, he1);

  faces.push_back(face);

  setHalfedgePair(he0);  // (10)
  setHalfedgePair(he1);  // (10)
  setHalfedgePair(he2);  // (10)
  return face;
}

void HEModel::edgeSwap(Halfedge* he) {
  Halfedge* heBC = he->next;
  Halfedge* heCA = he->prev;
  Halfedge* heAD = he->pair->next;
  Halfedge* heDB = he->pair->prev;

  // Reset Vertex -> Half edge
  // 頂点→ハーフエッジの再設定
  he->vertex->halfedge       = heAD;
  he->pair->vertex->halfedge = heBC;
  // Reset Half edge -> Vertex
  // ハーフエッジ→頂点の再設定
  he->vertex       = heDB->vertex;
  he->pair->vertex = heCA->vertex;

  // Update Face -> Half Edge
  // 面->ハーフエッジの更新
  he->face->halfedge       = he;
  he->pair->face->halfedge = he->pair;

  Halfedge* heDC = he;
  Halfedge* heCD = he->pair;

  HEFace* fDCA = he->face;
  HEFace* fCDB = he->pair->face;

  // リンクしなおし
  heDC->setLink(fDCA, heCD, heCA, heAD);
  heCA->setLink(fDCA, heCA->pair, heAD, heDC);
  heAD->setLink(fDCA, heAD->pair, heDC, heCA);
  heCD->setLink(fCDB, heDC, heDB, heBC);
  heDB->setLink(fCDB, heDB->pair, heBC, heCD);
  heBC->setLink(fCDB, heBC->pair, heCD, heDB);
}

// Return if there is already a vertex at the specified positions
// 位置が一致している頂点がすでにある場合、そのポインタを返す
HEVertex* HEModel::findVertex(QPointF& from_p, QPointF& to_p) {
  for (HEVertex* v : vertices) {
    QPointF from_d = v->from_pos.toPointF() - from_p;
    QPointF to_d   = v->to_pos.toPointF() - to_p;
    if ((from_d.manhattanLength() < 0.1) && (to_d.manhattanLength() < 0.1))
      return v;
  }
  return nullptr;
}

// Find a face containing p. Returns a pointer to edge if the p is on it.
// とりあえず根性で探す。点が辺上にある場合はEdgeのポインタも返す
HEFace* HEModel::findContainingFace(HEVertex* p, Halfedge** he_p) {
  for (HEFace* face : faces) {
    *he_p = nullptr;
    // Check edges. Halfedgeをぐるっと一本ずつたどる
    Halfedge* tmp_he = face->halfedge;
    for (int i = 0; i < 3; i++) {
      // edge vector 辺のベクトル
      QPointF heVec = tmp_he->getFromPos2DVector();
      // 現在のHalfedgeの起点となる頂点から点へのベクトル
      QPointF pToVertVec =
          p->from_pos.toPointF() - tmp_he->vertex->from_pos.toPointF();
      // 外積を計算
      double cross = heVec.x() * pToVertVec.y() - heVec.y() * pToVertVec.x();
      // 負のとき、このFaceは見込みなしなのでbreak
      if (cross < 0) break;
      // ゼロのとき、現在のHalfedgeを格納
      if (isAlmostSame(cross, 0)) *he_p = tmp_he;
      // i==2のとき、３辺とも含むFace発見。このFaceへのポインタを返す
      if (i == 2) return face;
      // それ以外の場合は次の辺へ
      tmp_he = tmp_he->next;
    }
  }
  return nullptr;
}

void HEModel::addVertexToEdge(Halfedge* he, HEVertex* p,
                              HEVertex* constrain_pair,
                              QStack<Halfedge*>& heToBeChecked) {
  // この三角形を2個の三角形に分割．
  // 辺をはさんで対となる三角形がある場合、それも２つに分割
  // Pを挟む辺をABとする、ABを含む三角形２つの残りの点をC,Dとする
  // ΔABC、ΔBAD を ΔAPC ΔPBC ΔPAD ΔBPD の4つに分割する
  // ４頂点
  HEVertex* A = he->vertex;
  HEVertex* B = he->pair->vertex;
  HEVertex* C = he->prev->vertex;
  HEVertex* D = he->pair->prev->vertex;
  // 外周Halfedge
  Halfedge* heBC = he->next;
  Halfedge* heCA = he->prev;
  Halfedge* heAD = he->pair->next;
  Halfedge* heDB = he->pair->prev;
  Halfedge* heAP = he;
  Halfedge* heBP = he->pair;
  // Halfedge作成
  Halfedge* hePC = new Halfedge(p);
  Halfedge* heCP = new Halfedge(C);
  Halfedge* hePB = new Halfedge(p);
  Halfedge* hePD = new Halfedge(p);
  Halfedge* heDP = new Halfedge(D);
  Halfedge* hePA = new Halfedge(p);
  // Face作成
  HEFace* fAPC = he->face;
  HEFace* fBPD = he->pair->face;
  HEFace* fPBC = new HEFace(hePB);
  HEFace* fPAD = new HEFace(hePA);
  // Face -> Halfedge
  fAPC->halfedge = he;
  fBPD->halfedge = he->pair;
  // 頂点からHalfedgeのリンク
  // p->halfedge = hePA;
  // リンク繋ぎなおし
  heAP->setLink(fAPC, hePA, hePC, heCA);
  hePC->setLink(fAPC, heCP, heCA, heAP);
  heCA->setLink(fAPC, heCA->pair, heAP, hePC);
  hePB->setLink(fPBC, heBP, heBC, heCP);
  heBC->setLink(fPBC, heBC->pair, heCP, hePB);
  heCP->setLink(fPBC, hePC, hePB, heBC);
  heBP->setLink(fBPD, hePB, hePD, heDB);
  hePD->setLink(fBPD, heDP, heDB, heBP);
  heDB->setLink(fBPD, heDB->pair, heBP, hePD);
  hePA->setLink(fPAD, heAP, heAD, heDP);
  heAD->setLink(fPAD, heAD->pair, heDP, hePA);
  heDP->setLink(fPAD, hePD, hePA, heAD);
  // Face登録
  faces << fPBC << fPAD;
  // Vertex登録
  vertices << p;
  // constrain条件を付加
  if (constrain_pair == A)
    heAP->setConstrained(true);
  else if (constrain_pair == B)
    heBP->setConstrained(true);
  else if (constrain_pair == C)
    heCP->setConstrained(true);
  else if (constrain_pair == D)
    heDP->setConstrained(true);
  // ２つの三角形の外側の４辺をスタックに積む
  heToBeChecked.push(heAD);
  heToBeChecked.push(heDB);
  heToBeChecked.push(heBC);
  heToBeChecked.push(heCA);
}

void HEModel::addVertexToFace(HEFace* face, HEVertex* p,
                              HEVertex* constrain_pair,
                              QStack<Halfedge*>& heToBeChecked) {
  // Pを含む三角形ΔABCをΔABP ΔBCP ΔCAPの3つに分割する
  // 3頂点
  HEVertex* A = face->halfedge->vertex;
  HEVertex* B = face->halfedge->next->vertex;
  HEVertex* C = face->halfedge->prev->vertex;
  assert(A);
  assert(B);
  assert(C);
  // 外周Halfedge
  Halfedge* heAB = face->halfedge;
  Halfedge* heBC = face->halfedge->next;
  Halfedge* heCA = face->halfedge->prev;
  assert(heAB);
  assert(heBC);
  assert(heCA);
  // Halfedge作成
  Halfedge* heAP = new Halfedge(A);
  Halfedge* hePA = new Halfedge(p);
  Halfedge* heBP = new Halfedge(B);
  Halfedge* hePB = new Halfedge(p);
  Halfedge* heCP = new Halfedge(C);
  Halfedge* hePC = new Halfedge(p);
  // Face作成
  HEFace* fABP = face;
  HEFace* fBCP = new HEFace(heBC);
  HEFace* fCAP = new HEFace(heCA);
  // 頂点からHalfedgeのリンク
  // p->halfedge = hePA;
  // リンク繋ぎなおし
  heAB->setLink(fABP, heAB->pair, heBP, hePA);
  heBP->setLink(fABP, hePB, hePA, heAB);
  hePA->setLink(fABP, heAP, heAB, heBP);
  heBC->setLink(fBCP, heBC->pair, heCP, hePB);
  heCP->setLink(fBCP, hePC, hePB, heBC);
  hePB->setLink(fBCP, heBP, heBC, heCP);
  heCA->setLink(fCAP, heCA->pair, heAP, hePC);
  heAP->setLink(fCAP, hePA, hePC, heCA);
  hePC->setLink(fCAP, heCP, heCA, heAP);
  // Face登録
  faces << fBCP << fCAP;
  // Vertex登録
  vertices << p;
  // constrain条件を付加
  if (constrain_pair == A)
    heAP->setConstrained(true);
  else if (constrain_pair == B)
    heBP->setConstrained(true);
  else if (constrain_pair == C)
    heCP->setConstrained(true);
  // ２つの三角形の外側の3辺をスタックに積む
  heToBeChecked.push(heAB);
  heToBeChecked.push(heBC);
  heToBeChecked.push(heCA);
}

// add vertex and apply delaunay tessellation
// モデルに点を追加
void HEModel::addVertex(HEVertex* p, HEVertex* constrain_pair) {
  //    B2 - 2) piを含む三角形ABCを発見し,
  Halfedge* pointContainingEdge = nullptr;
  HEFace* face                  = findContainingFace(p, &pointContainingEdge);
  if (!face) return;  // should not happen

  // フリップ候補となるHalfedgeのスタックを用意
  QStack<Halfedge*> heToBeChecked;
  // 辺上に点を追加する場合
  if (pointContainingEdge) {
    // この三角形を2個の三角形に分割．
    // 辺をはさんで対となる三角形がある場合、それも２つに分割
    //    ２つの三角形の外側の４辺をスタックに積む
    addVertexToEdge(pointContainingEdge, p, constrain_pair, heToBeChecked);
  }
  // 三角形内に点を追加する場合
  else {
    //  この三角形をAB pi, BC pi, CA pi の3個の三角形に分割．
    //  この時, 辺AB, BC, CAをスタックSに積む．
    addVertexToFace(face, p, constrain_pair, heToBeChecked);
  }

  //    B2 - 3) スタックSが空になるまで以下を繰り返す
  while (!heToBeChecked.isEmpty()) {
    //  B2 - 3 - 1)スタックSから辺をpopし，これを辺ABとする．
    Halfedge* heAB = heToBeChecked.pop();
    //  辺ABを含む2個の三角形をABCとBADとする
    //  if (ABが線分制約である) 何もしない
    if (heAB->constrained) continue;
    //  else if (CDが線分制約であり四角形ABCDが凸)
    //    辺ABをflip，辺AD / DB / BC / CAをスタックに積む
    else if (checkConvexAndConstraint(heAB, p, constrain_pair)) {
      edgeSwap(heAB);
      heToBeChecked.push(heAB->pair->next);
      heToBeChecked.push(heAB->pair->prev);
      heAB->setConstrained(true);
    }
    //  else if (ABCの外接円内にDが入る)
    //    辺ABをflip，辺AD / DB / BC / CAをスタックに積む
    else if (checkCircumcircle(heAB)) {
      edgeSwap(heAB);
      heToBeChecked.push(heAB->pair->next);
      heToBeChecked.push(heAB->pair->prev);
    }
    //  else 何もしない
  }

  //      B3) 制約線部の復帰を行う
  // constrain_pairがあるのに、pとの接続に失敗している場合
  if (constrain_pair && !p->isConnectedTo(constrain_pair)) {
    //      B3 - 1)各制約線分ABについて以下を繰り返す
    // 交差するエッジのスタック
    QStack<Halfedge*> crossingEdges;
    //      B3 - 2)現在図形とABと交差する全てのエッジをキューKに挿入
    getCrossingEdges(crossingEdges, p, constrain_pair);
    assert(heToBeChecked.isEmpty());
    //      B3 - 3)キューKが空になるまで次を繰り返す
    int itrCount = 0;  // FIXME: dirty fix in order to prevent infinite loop
    while (!crossingEdges.isEmpty()) {
      //      B3 - 3 -
      //      1)キューKの先頭要素をpopし辺CDとす．CDを含む2個の三角形をCDE,
      //      CDFとする
      bool isLastItem   = (crossingEdges.size() == 1);
      Halfedge* crossHe = crossingEdges.pop();
      //      if (四角形ECFDが凸)
      if (crossHe->isDiagonalOfConvexShape()) {
        //        エッジCDをflip.新たなエッジEFをスタックNにpush.
        edgeSwap(crossHe);
        heToBeChecked.push(crossHe);
        // 反対側にもチェックを伸ばすため
        heToBeChecked.push(crossHe->pair->prev);
        heToBeChecked.push(crossHe->pair->next);
        itrCount = 0;
      }
      //      else
      else {
        if (isLastItem || itrCount >= 10) break;
        //        エッジCDをキューKに後ろからpush.
        crossingEdges.push_front(crossHe);
        itrCount++;
      }
    }
    // 制約を追加
    setConstraint(p, constrain_pair, true);

    //        B3 - 3) B - 2 - 3と同様の処理をキューNに対して行う
    //        (新たなエッジのドロネー性の確認を行う)
    //    B2 - 3) スタックSが空になるまで以下を繰り返す
    while (!heToBeChecked.isEmpty()) {
      //  B2 - 3 - 1)スタックSから辺をpopし，これを辺ABとする．
      Halfedge* heAB = heToBeChecked.pop();
      //  辺ABを含む2個の三角形をABCとBADとする
      //  if (ABが線分制約である) 何もしない
      if (heAB->constrained) continue;
      //  else if (CDが線分制約であり四角形ABCDが凸)
      //    辺ABをflip，辺AD / DB / BC / CAをスタックに積む
      else if (checkConvexAndConstraint(heAB, p, constrain_pair)) {
        edgeSwap(heAB);
        heToBeChecked.push(heAB->pair->next);
        heToBeChecked.push(heAB->pair->prev);
        heAB->setConstrained(true);
      }
      //  else if (ABCの外接円内にDが入る)
      //    辺ABをflip，辺AD / DB / BC / CAをスタックに積む
      else if (checkCircumcircle(heAB)) {
        edgeSwap(heAB);
        heToBeChecked.push(heAB->pair->next);
        heToBeChecked.push(heAB->pair->prev);
      }
      //  else 何もしない
    }
  }
}

// Returns true if the circumcircle of a trangle ABC contains a point D
// (D is an opposite point of a triangle sharing he with at triangle ABC)
// heの反対側の三角形ABCの外接円が、heを含む三角形の
// 反対側の頂点Dを含む場合trueを返す
bool HEModel::checkCircumcircle(Halfedge* he) {
  QVector2D D = he->pair->prev->vertex->from_pos.toVector2D();
  QVector2D A = he->vertex->from_pos.toVector2D() - D;
  QVector2D B = he->next->vertex->from_pos.toVector2D() - D;
  QVector2D C = he->prev->vertex->from_pos.toVector2D() - D;

  double elements[] = {A.x(), A.y(), A.lengthSquared(),
                       B.x(), B.y(), B.lengthSquared(),
                       C.x(), C.y(), C.lengthSquared()};

  return determinant(Matrix3x3d(elements)) > 0.0;
}

// Returns true if :
// Regarding a quadrilateral including a diagonal he;
// - Another diagonal is constrained, and
// - The quadrilateral is convex
// he を挟む三角形２つで作る四角形の逆の対角線が線分制約であり
// 四角形が凸なときtrueを返す
bool HEModel::checkConvexAndConstraint(Halfedge* he, HEVertex* p,
                                       HEVertex* constrain_pair) {
  HEVertex* C = he->prev->vertex;
  HEVertex* D = he->pair->prev->vertex;
  if (!(C == p && D == constrain_pair) && !(C == constrain_pair && D == p))
    return false;
  // 凸四角形の判定
  return he->isDiagonalOfConvexShape();
}

void HEModel::setConstraint(HEVertex* start, HEVertex* end, bool constraint) {
  Halfedge* tmpEdge = start->halfedge;
  while (1) {
    if (tmpEdge->pair->vertex == end) {
      tmpEdge->setConstrained(constraint);
      return;
    }
    tmpEdge = tmpEdge->pair->next;
    if (tmpEdge == start->halfedge) return;
  }
}

// Add centroids of each face, sizeThreshold is a threshold of area to decide
// whether to divide the face
// 各三角形の重心を追加 引数は三角形を分割するか判断する面積の閾値
void HEModel::insertCentroids(double sizeThreshold) {
  // まず、挿入するHEVertexを作成
  QList<HEVertex*> verticesToBeInserted;
  // 各三角形について
  for (HEFace* face : faces) {
    // checkFaceVisibility でシェイプ外と判断されたFaceはスキップ
    if (!face->isVisible) continue;
    // すでにサイズがsizeThreshold以下の場合はパス
    if (face->size() < sizeThreshold) continue;
    // 頂点を登録
    verticesToBeInserted.append(face->createCentroidVertex());
  }

  // 頂点を追加
  for (HEVertex* vert : verticesToBeInserted) addVertex(vert);
}

// Check if the face is included in the parent shape
// 親シェイプ形状に含まれているかどうか
// 初期CorrVecの挿入後、再分割前に１度だけチェックする
void HEModel::checkFaceVisibility(const QPolygonF& parentShapePolygon) {
  // 各三角形について
  for (HEFace* face : faces) {
    // 頂点のデプスが1つでも負なら不可視にする。描画しないので
    Halfedge* he = face->halfedge;
    if (he->vertex->from_pos.z() < 0.0 ||
        he->prev->vertex->from_pos.z() < 0.0 ||
        he->next->vertex->from_pos.z() < 0.0) {
      face->isVisible = false;
      continue;
    }
    // 重心がポリゴン内に含まれていなければ不可視にする
    HEVertex* A            = he->vertex;
    HEVertex* B            = he->next->vertex;
    HEVertex* C            = he->prev->vertex;
    QVector3D fromCentroid = (A->from_pos + B->from_pos + C->from_pos) / 3.0;
    if (!parentShapePolygon.containsPoint(fromCentroid.toPointF(),
                                          Qt::WindingFill))
      face->isVisible = false;
  }
}

// smoothing なじませる
void HEModel::smooth(double offsetThreshold) {
  // 各動ける頂点ごとに、from,toの移動ベクトルのペアを格納
  QMap<HEVertex*, QPair<QPointF, QPointF>> offsetMap;
  double maxOffsetLength2 = 0.0;
  double smoothStepFactor = 0.03;
  // 全ての頂点について
  for (HEVertex* v : vertices) {
    // 　頂点制約の場合はスキップ
    if (v->constrained) continue;
    // 頂点回りのheをぐるりと回って移動ベクトルを足しこんでいく
    QPointF fromOffset, toOffset;
    Halfedge* tmp_he = v->halfedge;
    while (1) {
      fromOffset +=
          tmp_he->getFromPos2DVector() * tmp_he->pair->vertex->from_weight;
      toOffset += tmp_he->getToPos2DVector() * tmp_he->pair->vertex->to_weight;
      // 次のエッジへ
      tmp_he = tmp_he->prev->pair;
      if (tmp_he == v->halfedge) break;
    }
    // 格納
    offsetMap.insert(v, {fromOffset, toOffset});
    // 移動ベクトルの最大値更新
    double fromLength2 =
        fromOffset.x() * fromOffset.x() + fromOffset.y() * fromOffset.y();
    double toLength2 =
        toOffset.x() * toOffset.x() + toOffset.y() * toOffset.y();
    if (fromLength2 > maxOffsetLength2) maxOffsetLength2 = fromLength2;
    if (toLength2 > maxOffsetLength2) maxOffsetLength2 = toLength2;
  }

  // ポイントを移動させる
  QMap<HEVertex*, QPair<QPointF, QPointF>>::const_iterator i =
      offsetMap.begin();
  while (i != offsetMap.end()) {
    HEVertex* v     = i.key();
    QPointF fromOff = i.value().first * smoothStepFactor;
    v->from_pos += QVector3D(fromOff.x(), fromOff.y(), 0.0);
    QPointF toOff = i.value().second * smoothStepFactor;
    v->to_pos += QVector3D(toOff.x(), toOff.y(), 0.0);
    ++i;
  }

  // もし、最大の移動距離が閾値より大きかったら、再び繰り返す
  if (maxOffsetLength2 > offsetThreshold * offsetThreshold)
    smooth(offsetThreshold);
}

// Delete super triangles (= initial triangles covering the workspace)
// SuperTriangleを消す
void HEModel::deleteSuperTriangles() {
  // 全てのfacesについて
  QList<HEFace*>::iterator itr;
  for (itr = faces.begin(); itr != faces.end(); ++itr) {
    if (!(*itr)->isVisible) {
      deleteFace(*itr);
      itr = faces.erase(itr);
    }
  }
  // 頂点削除
  for (HEVertex* v : superTriangleVertices) {
    delete v;
    vertices.removeOne(v);
  }
  superTriangleVertices.clear();
}

// obtain a list of edges which intersect a line between start and end
// start と end を結ぶ線分を横切るエッジのリストを得る
void HEModel::getCrossingEdges(QStack<Halfedge*>& edges, HEVertex* start,
                               HEVertex* end) {
  QStack<Halfedge*> edgesToBeChecked;
  // 調査対象のエッジ
  Halfedge* tmp_he = start->halfedge;
  while (1) {
    edgesToBeChecked.push(tmp_he->next);
    tmp_he = tmp_he->prev->pair;
    if (!tmp_he || tmp_he == start->halfedge) break;
  }

  // スタックが空になるまで以下を繰り返す
  while (!edgesToBeChecked.isEmpty()) {
    // 辺をpopし、start-end　の線分との交差判定を行う
    Halfedge* AB = edgesToBeChecked.pop();
    if (AB->constrained) continue;
    if (AB->vertex == end || AB->pair->vertex == end) continue;
    // 交差する場合
    if (judgeIntersected(AB->vertex->from_pos.toPointF(),
                         AB->pair->vertex->from_pos.toPointF(),
                         start->from_pos.toPointF(),
                         end->from_pos.toPointF())) {
      // ABをスタックに追加
      edges.push(AB);
      // 反対側の辺を調査対象に加える
      edgesToBeChecked.push(AB->pair->next);
      edgesToBeChecked.push(AB->pair->prev);
    }
    // 交差しない場合はなにもしない
  }
}

void HEModel::print() {
  std::cout << "------------" << std::endl
            << "Vertex count = " << vertices.size() << std::endl;
  int i = 0;
  for (HEVertex* v : vertices) {
    std::cout << i << ": (" << v->from_pos.x() << ", " << v->from_pos.y()
              << ") -> (" << v->to_pos.x() << ", " << v->to_pos.y() << ")"
              << std::endl;
    i++;
  }
  std::cout << "Triangle count = " << faces.size() << std::endl;
  i = 0;
  for (HEFace* face : faces) {
    std::cout << i << ": (" << vertices.indexOf(face->halfedge->vertex) << ", "
              << vertices.indexOf(face->halfedge->next->vertex) << ", "
              << vertices.indexOf(face->halfedge->prev->vertex) << ")"
              << std::endl;
    i++;
  }
  std::cout << "------------" << std::endl << std::endl;
}
