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
  // �[�_���܂ޏꍇ
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
  vertex = v;  // link from Halfedge to Vertex. Halfedge �� Vertex �̃����N
  if (v->halfedge == nullptr) {
    v->halfedge =
        this;  // link from Vertex to HalfEdge. Vertex �� Halfedge �̃����N
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
// Make all edges to be constrained. �G�b�W�S�Ă�contrained�ɂ���
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
  // �E�˂��̌������ۏ؂���Ă���̂�std::abs�͂Ȃ��Ă����v����
  return 0.5 * (ab.x() * ac.y() - ac.x() * ab.y());
  // return 0.5 * std::abs(ab.x()*ac.y() - ac.x()*ab.y());
}

// Add new vertex at centroid and returns its pointer
// �d�S��HEVertex���쐬���|�C���^��Ԃ�
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

// Returns depth of the centroid. �d�S��Depth��Ԃ�
double HEFace::centroidDepth() const {
  HEVertex* A = halfedge->vertex;
  HEVertex* B = halfedge->next->vertex;
  HEVertex* C = halfedge->prev->vertex;
  return (A->from_pos.z() + B->from_pos.z() + C->from_pos.z()) / 3.0;
}

//---------------------------------------------------

// Find a pair half edge of he and register it
// he �� pair �ƂȂ�n�[�t�G�b�W��T���ēo�^����
void HEModel::setHalfedgePair(Halfedge* he) {
  QList<HEFace*>::iterator it_f;  // ���ׂĂ�Face�����񂷂�
  for (it_f = faces.begin(); it_f != faces.end();
       it_f++) {  // ���݂�Face�Ɋ܂܂��n�[�t�G�b�W�����񂷂�
    Halfedge* halfedge_in_face = (*it_f)->halfedge;
    do {
      if (he->vertex == halfedge_in_face->next->vertex &&
          he->next->vertex ==
              halfedge_in_face
                  ->vertex) {  // �����̒[�_�����ʂ�������pair�ɓo�^����
        he->pair               = halfedge_in_face;
        halfedge_in_face->pair = he;
        return;
      }
      halfedge_in_face = halfedge_in_face->next;
    } while (halfedge_in_face != (*it_f)->halfedge);
  }
}

// Make two half edges as a pair
// ��̃n�[�t�G�b�W���݂��� pair �Ƃ���
void HEModel::setHalfedgePair(Halfedge* he0, Halfedge* he1) {
  he0->pair = he1;
  he1->pair = he0;
}

// Remove vertex from the list
// Vertex �̃��X�g���� vertex ���폜����
void HEModel::deleteVertex(HEVertex* vertex) { delete vertex; }

// Remove face from the list
// Face �̃��X�g���� face ���폜����
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
  // ���_���n�[�t�G�b�W�̍Đݒ�
  he->vertex->halfedge       = heAD;
  he->pair->vertex->halfedge = heBC;
  // Reset Half edge -> Vertex
  // �n�[�t�G�b�W�����_�̍Đݒ�
  he->vertex       = heDB->vertex;
  he->pair->vertex = heCA->vertex;

  // Update Face -> Half Edge
  // ��->�n�[�t�G�b�W�̍X�V
  he->face->halfedge       = he;
  he->pair->face->halfedge = he->pair;

  Halfedge* heDC = he;
  Halfedge* heCD = he->pair;

  HEFace* fDCA = he->face;
  HEFace* fCDB = he->pair->face;

  // �����N���Ȃ���
  heDC->setLink(fDCA, heCD, heCA, heAD);
  heCA->setLink(fDCA, heCA->pair, heAD, heDC);
  heAD->setLink(fDCA, heAD->pair, heDC, heCA);
  heCD->setLink(fCDB, heDC, heDB, heBC);
  heDB->setLink(fCDB, heDB->pair, heBC, heCD);
  heBC->setLink(fCDB, heBC->pair, heCD, heDB);
}

// Return if there is already a vertex at the specified positions
// �ʒu����v���Ă��钸�_�����łɂ���ꍇ�A���̃|�C���^��Ԃ�
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
// �Ƃ肠���������ŒT���B�_���ӏ�ɂ���ꍇ��Edge�̃|�C���^���Ԃ�
HEFace* HEModel::findContainingFace(HEVertex* p, Halfedge** he_p) {
  for (HEFace* face : faces) {
    *he_p = nullptr;
    // Check edges. Halfedge��������ƈ�{�����ǂ�
    Halfedge* tmp_he = face->halfedge;
    for (int i = 0; i < 3; i++) {
      // edge vector �ӂ̃x�N�g��
      QPointF heVec = tmp_he->getFromPos2DVector();
      // ���݂�Halfedge�̋N�_�ƂȂ钸�_����_�ւ̃x�N�g��
      QPointF pToVertVec =
          p->from_pos.toPointF() - tmp_he->vertex->from_pos.toPointF();
      // �O�ς��v�Z
      double cross = heVec.x() * pToVertVec.y() - heVec.y() * pToVertVec.x();
      // ���̂Ƃ��A����Face�͌����݂Ȃ��Ȃ̂�break
      if (cross < 0) break;
      // �[���̂Ƃ��A���݂�Halfedge���i�[
      if (isAlmostSame(cross, 0)) *he_p = tmp_he;
      // i==2�̂Ƃ��A�R�ӂƂ��܂�Face�����B����Face�ւ̃|�C���^��Ԃ�
      if (i == 2) return face;
      // ����ȊO�̏ꍇ�͎��̕ӂ�
      tmp_he = tmp_he->next;
    }
  }
  return nullptr;
}

void HEModel::addVertexToEdge(Halfedge* he, HEVertex* p,
                              HEVertex* constrain_pair,
                              QStack<Halfedge*>& heToBeChecked) {
  // ���̎O�p�`��2�̎O�p�`�ɕ����D
  // �ӂ��͂���ő΂ƂȂ�O�p�`������ꍇ�A������Q�ɕ���
  // P�����ޕӂ�AB�Ƃ���AAB���܂ގO�p�`�Q�̎c��̓_��C,D�Ƃ���
  // ��ABC�A��BAD �� ��APC ��PBC ��PAD ��BPD ��4�ɕ�������
  // �S���_
  HEVertex* A = he->vertex;
  HEVertex* B = he->pair->vertex;
  HEVertex* C = he->prev->vertex;
  HEVertex* D = he->pair->prev->vertex;
  // �O��Halfedge
  Halfedge* heBC = he->next;
  Halfedge* heCA = he->prev;
  Halfedge* heAD = he->pair->next;
  Halfedge* heDB = he->pair->prev;
  Halfedge* heAP = he;
  Halfedge* heBP = he->pair;
  // Halfedge�쐬
  Halfedge* hePC = new Halfedge(p);
  Halfedge* heCP = new Halfedge(C);
  Halfedge* hePB = new Halfedge(p);
  Halfedge* hePD = new Halfedge(p);
  Halfedge* heDP = new Halfedge(D);
  Halfedge* hePA = new Halfedge(p);
  // Face�쐬
  HEFace* fAPC = he->face;
  HEFace* fBPD = he->pair->face;
  HEFace* fPBC = new HEFace(hePB);
  HEFace* fPAD = new HEFace(hePA);
  // Face -> Halfedge
  fAPC->halfedge = he;
  fBPD->halfedge = he->pair;
  // ���_����Halfedge�̃����N
  // p->halfedge = hePA;
  // �����N�q���Ȃ���
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
  // Face�o�^
  faces << fPBC << fPAD;
  // Vertex�o�^
  vertices << p;
  // constrain������t��
  if (constrain_pair == A)
    heAP->setConstrained(true);
  else if (constrain_pair == B)
    heBP->setConstrained(true);
  else if (constrain_pair == C)
    heCP->setConstrained(true);
  else if (constrain_pair == D)
    heDP->setConstrained(true);
  // �Q�̎O�p�`�̊O���̂S�ӂ��X�^�b�N�ɐς�
  heToBeChecked.push(heAD);
  heToBeChecked.push(heDB);
  heToBeChecked.push(heBC);
  heToBeChecked.push(heCA);
}

void HEModel::addVertexToFace(HEFace* face, HEVertex* p,
                              HEVertex* constrain_pair,
                              QStack<Halfedge*>& heToBeChecked) {
  // P���܂ގO�p�`��ABC����ABP ��BCP ��CAP��3�ɕ�������
  // 3���_
  HEVertex* A = face->halfedge->vertex;
  HEVertex* B = face->halfedge->next->vertex;
  HEVertex* C = face->halfedge->prev->vertex;
  assert(A);
  assert(B);
  assert(C);
  // �O��Halfedge
  Halfedge* heAB = face->halfedge;
  Halfedge* heBC = face->halfedge->next;
  Halfedge* heCA = face->halfedge->prev;
  assert(heAB);
  assert(heBC);
  assert(heCA);
  // Halfedge�쐬
  Halfedge* heAP = new Halfedge(A);
  Halfedge* hePA = new Halfedge(p);
  Halfedge* heBP = new Halfedge(B);
  Halfedge* hePB = new Halfedge(p);
  Halfedge* heCP = new Halfedge(C);
  Halfedge* hePC = new Halfedge(p);
  // Face�쐬
  HEFace* fABP = face;
  HEFace* fBCP = new HEFace(heBC);
  HEFace* fCAP = new HEFace(heCA);
  // ���_����Halfedge�̃����N
  // p->halfedge = hePA;
  // �����N�q���Ȃ���
  heAB->setLink(fABP, heAB->pair, heBP, hePA);
  heBP->setLink(fABP, hePB, hePA, heAB);
  hePA->setLink(fABP, heAP, heAB, heBP);
  heBC->setLink(fBCP, heBC->pair, heCP, hePB);
  heCP->setLink(fBCP, hePC, hePB, heBC);
  hePB->setLink(fBCP, heBP, heBC, heCP);
  heCA->setLink(fCAP, heCA->pair, heAP, hePC);
  heAP->setLink(fCAP, hePA, hePC, heCA);
  hePC->setLink(fCAP, heCP, heCA, heAP);
  // Face�o�^
  faces << fBCP << fCAP;
  // Vertex�o�^
  vertices << p;
  // constrain������t��
  if (constrain_pair == A)
    heAP->setConstrained(true);
  else if (constrain_pair == B)
    heBP->setConstrained(true);
  else if (constrain_pair == C)
    heCP->setConstrained(true);
  // �Q�̎O�p�`�̊O����3�ӂ��X�^�b�N�ɐς�
  heToBeChecked.push(heAB);
  heToBeChecked.push(heBC);
  heToBeChecked.push(heCA);
}

// add vertex and apply delaunay tessellation
// ���f���ɓ_��ǉ�
void HEModel::addVertex(HEVertex* p, HEVertex* constrain_pair) {
  //    B2 - 2) pi���܂ގO�p�`ABC�𔭌���,
  Halfedge* pointContainingEdge = nullptr;
  HEFace* face                  = findContainingFace(p, &pointContainingEdge);
  if (!face) return;  // should not happen

  // �t���b�v���ƂȂ�Halfedge�̃X�^�b�N��p��
  QStack<Halfedge*> heToBeChecked;
  // �ӏ�ɓ_��ǉ�����ꍇ
  if (pointContainingEdge) {
    // ���̎O�p�`��2�̎O�p�`�ɕ����D
    // �ӂ��͂���ő΂ƂȂ�O�p�`������ꍇ�A������Q�ɕ���
    //    �Q�̎O�p�`�̊O���̂S�ӂ��X�^�b�N�ɐς�
    addVertexToEdge(pointContainingEdge, p, constrain_pair, heToBeChecked);
  }
  // �O�p�`���ɓ_��ǉ�����ꍇ
  else {
    //  ���̎O�p�`��AB pi, BC pi, CA pi ��3�̎O�p�`�ɕ����D
    //  ���̎�, ��AB, BC, CA���X�^�b�NS�ɐςށD
    addVertexToFace(face, p, constrain_pair, heToBeChecked);
  }

  //    B2 - 3) �X�^�b�NS����ɂȂ�܂ňȉ����J��Ԃ�
  while (!heToBeChecked.isEmpty()) {
    //  B2 - 3 - 1)�X�^�b�NS����ӂ�pop���C������AB�Ƃ���D
    Halfedge* heAB = heToBeChecked.pop();
    //  ��AB���܂�2�̎O�p�`��ABC��BAD�Ƃ���
    //  if (AB����������ł���) �������Ȃ�
    if (heAB->constrained) continue;
    //  else if (CD����������ł���l�p�`ABCD����)
    //    ��AB��flip�C��AD / DB / BC / CA���X�^�b�N�ɐς�
    else if (checkConvexAndConstraint(heAB, p, constrain_pair)) {
      edgeSwap(heAB);
      heToBeChecked.push(heAB->pair->next);
      heToBeChecked.push(heAB->pair->prev);
      heAB->setConstrained(true);
    }
    //  else if (ABC�̊O�ډ~����D������)
    //    ��AB��flip�C��AD / DB / BC / CA���X�^�b�N�ɐς�
    else if (checkCircumcircle(heAB)) {
      edgeSwap(heAB);
      heToBeChecked.push(heAB->pair->next);
      heToBeChecked.push(heAB->pair->prev);
    }
    //  else �������Ȃ�
  }

  //      B3) ��������̕��A���s��
  // constrain_pair������̂ɁAp�Ƃ̐ڑ��Ɏ��s���Ă���ꍇ
  if (constrain_pair && !p->isConnectedTo(constrain_pair)) {
    //      B3 - 1)�e�������AB�ɂ��Ĉȉ����J��Ԃ�
    // ��������G�b�W�̃X�^�b�N
    QStack<Halfedge*> crossingEdges;
    //      B3 - 2)���ݐ}�`��AB�ƌ�������S�ẴG�b�W���L���[K�ɑ}��
    getCrossingEdges(crossingEdges, p, constrain_pair);
    assert(heToBeChecked.isEmpty());
    //      B3 - 3)�L���[K����ɂȂ�܂Ŏ����J��Ԃ�
    int itrCount = 0;  // FIXME: dirty fix in order to prevent infinite loop
    while (!crossingEdges.isEmpty()) {
      //      B3 - 3 -
      //      1)�L���[K�̐擪�v�f��pop����CD�Ƃ��DCD���܂�2�̎O�p�`��CDE,
      //      CDF�Ƃ���
      bool isLastItem   = (crossingEdges.size() == 1);
      Halfedge* crossHe = crossingEdges.pop();
      //      if (�l�p�`ECFD����)
      if (crossHe->isDiagonalOfConvexShape()) {
        //        �G�b�WCD��flip.�V���ȃG�b�WEF���X�^�b�NN��push.
        edgeSwap(crossHe);
        heToBeChecked.push(crossHe);
        // ���Α��ɂ��`�F�b�N��L�΂�����
        heToBeChecked.push(crossHe->pair->prev);
        heToBeChecked.push(crossHe->pair->next);
        itrCount = 0;
      }
      //      else
      else {
        if (isLastItem || itrCount >= 10) break;
        //        �G�b�WCD���L���[K�Ɍ�납��push.
        crossingEdges.push_front(crossHe);
        itrCount++;
      }
    }
    // �����ǉ�
    setConstraint(p, constrain_pair, true);

    //        B3 - 3) B - 2 - 3�Ɠ��l�̏������L���[N�ɑ΂��čs��
    //        (�V���ȃG�b�W�̃h���l�[���̊m�F���s��)
    //    B2 - 3) �X�^�b�NS����ɂȂ�܂ňȉ����J��Ԃ�
    while (!heToBeChecked.isEmpty()) {
      //  B2 - 3 - 1)�X�^�b�NS����ӂ�pop���C������AB�Ƃ���D
      Halfedge* heAB = heToBeChecked.pop();
      //  ��AB���܂�2�̎O�p�`��ABC��BAD�Ƃ���
      //  if (AB����������ł���) �������Ȃ�
      if (heAB->constrained) continue;
      //  else if (CD����������ł���l�p�`ABCD����)
      //    ��AB��flip�C��AD / DB / BC / CA���X�^�b�N�ɐς�
      else if (checkConvexAndConstraint(heAB, p, constrain_pair)) {
        edgeSwap(heAB);
        heToBeChecked.push(heAB->pair->next);
        heToBeChecked.push(heAB->pair->prev);
        heAB->setConstrained(true);
      }
      //  else if (ABC�̊O�ډ~����D������)
      //    ��AB��flip�C��AD / DB / BC / CA���X�^�b�N�ɐς�
      else if (checkCircumcircle(heAB)) {
        edgeSwap(heAB);
        heToBeChecked.push(heAB->pair->next);
        heToBeChecked.push(heAB->pair->prev);
      }
      //  else �������Ȃ�
    }
  }
}

// Returns true if the circumcircle of a trangle ABC contains a point D
// (D is an opposite point of a triangle sharing he with at triangle ABC)
// he�̔��Α��̎O�p�`ABC�̊O�ډ~���Ahe���܂ގO�p�`��
// ���Α��̒��_D���܂ޏꍇtrue��Ԃ�
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
// he �����ގO�p�`�Q�ō��l�p�`�̋t�̑Ίp������������ł���
// �l�p�`���ʂȂƂ�true��Ԃ�
bool HEModel::checkConvexAndConstraint(Halfedge* he, HEVertex* p,
                                       HEVertex* constrain_pair) {
  HEVertex* C = he->prev->vertex;
  HEVertex* D = he->pair->prev->vertex;
  if (!(C == p && D == constrain_pair) && !(C == constrain_pair && D == p))
    return false;
  // �ʎl�p�`�̔���
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
// �e�O�p�`�̏d�S��ǉ� �����͎O�p�`�𕪊����邩���f����ʐς�臒l
void HEModel::insertCentroids(double sizeThreshold) {
  // �܂��A�}������HEVertex���쐬
  QList<HEVertex*> verticesToBeInserted;
  // �e�O�p�`�ɂ���
  for (HEFace* face : faces) {
    // checkFaceVisibility �ŃV�F�C�v�O�Ɣ��f���ꂽFace�̓X�L�b�v
    if (!face->isVisible) continue;
    // ���łɃT�C�Y��sizeThreshold�ȉ��̏ꍇ�̓p�X
    if (face->size() < sizeThreshold) continue;
    // ���_��o�^
    verticesToBeInserted.append(face->createCentroidVertex());
  }

  // ���_��ǉ�
  for (HEVertex* vert : verticesToBeInserted) addVertex(vert);
}

// Check if the face is included in the parent shape
// �e�V�F�C�v�`��Ɋ܂܂�Ă��邩�ǂ���
// ����CorrVec�̑}����A�ĕ����O�ɂP�x�����`�F�b�N����
void HEModel::checkFaceVisibility(const QPolygonF& parentShapePolygon) {
  // �e�O�p�`�ɂ���
  for (HEFace* face : faces) {
    // ���_�̃f�v�X��1�ł����Ȃ�s���ɂ���B�`�悵�Ȃ��̂�
    Halfedge* he = face->halfedge;
    if (he->vertex->from_pos.z() < 0.0 ||
        he->prev->vertex->from_pos.z() < 0.0 ||
        he->next->vertex->from_pos.z() < 0.0) {
      face->isVisible = false;
      continue;
    }
    // �d�S���|���S�����Ɋ܂܂�Ă��Ȃ���Εs���ɂ���
    HEVertex* A            = he->vertex;
    HEVertex* B            = he->next->vertex;
    HEVertex* C            = he->prev->vertex;
    QVector3D fromCentroid = (A->from_pos + B->from_pos + C->from_pos) / 3.0;
    if (!parentShapePolygon.containsPoint(fromCentroid.toPointF(),
                                          Qt::WindingFill))
      face->isVisible = false;
  }
}

// smoothing �Ȃ��܂���
void HEModel::smooth(double offsetThreshold) {
  // �e�����钸�_���ƂɁAfrom,to�̈ړ��x�N�g���̃y�A���i�[
  QMap<HEVertex*, QPair<QPointF, QPointF>> offsetMap;
  double maxOffsetLength2 = 0.0;
  double smoothStepFactor = 0.03;
  // �S�Ă̒��_�ɂ���
  for (HEVertex* v : vertices) {
    // �@���_����̏ꍇ�̓X�L�b�v
    if (v->constrained) continue;
    // ���_����he�������Ɖ���Ĉړ��x�N�g���𑫂�����ł���
    QPointF fromOffset, toOffset;
    Halfedge* tmp_he = v->halfedge;
    while (1) {
      fromOffset +=
          tmp_he->getFromPos2DVector() * tmp_he->pair->vertex->from_weight;
      toOffset += tmp_he->getToPos2DVector() * tmp_he->pair->vertex->to_weight;
      // ���̃G�b�W��
      tmp_he = tmp_he->prev->pair;
      if (tmp_he == v->halfedge) break;
    }
    // �i�[
    offsetMap.insert(v, {fromOffset, toOffset});
    // �ړ��x�N�g���̍ő�l�X�V
    double fromLength2 =
        fromOffset.x() * fromOffset.x() + fromOffset.y() * fromOffset.y();
    double toLength2 =
        toOffset.x() * toOffset.x() + toOffset.y() * toOffset.y();
    if (fromLength2 > maxOffsetLength2) maxOffsetLength2 = fromLength2;
    if (toLength2 > maxOffsetLength2) maxOffsetLength2 = toLength2;
  }

  // �|�C���g���ړ�������
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

  // �����A�ő�̈ړ�������臒l���傫��������A�ĂьJ��Ԃ�
  if (maxOffsetLength2 > offsetThreshold * offsetThreshold)
    smooth(offsetThreshold);
}

// Delete super triangles (= initial triangles covering the workspace)
// SuperTriangle������
void HEModel::deleteSuperTriangles() {
  // �S�Ă�faces�ɂ���
  QList<HEFace*>::iterator itr;
  for (itr = faces.begin(); itr != faces.end(); ++itr) {
    if (!(*itr)->isVisible) {
      deleteFace(*itr);
      itr = faces.erase(itr);
    }
  }
  // ���_�폜
  for (HEVertex* v : superTriangleVertices) {
    delete v;
    vertices.removeOne(v);
  }
  superTriangleVertices.clear();
}

// obtain a list of edges which intersect a line between start and end
// start �� end �����Ԑ��������؂�G�b�W�̃��X�g�𓾂�
void HEModel::getCrossingEdges(QStack<Halfedge*>& edges, HEVertex* start,
                               HEVertex* end) {
  QStack<Halfedge*> edgesToBeChecked;
  // �����Ώۂ̃G�b�W
  Halfedge* tmp_he = start->halfedge;
  while (1) {
    edgesToBeChecked.push(tmp_he->next);
    tmp_he = tmp_he->prev->pair;
    if (!tmp_he || tmp_he == start->halfedge) break;
  }

  // �X�^�b�N����ɂȂ�܂ňȉ����J��Ԃ�
  while (!edgesToBeChecked.isEmpty()) {
    // �ӂ�pop���Astart-end�@�̐����Ƃ̌���������s��
    Halfedge* AB = edgesToBeChecked.pop();
    if (AB->constrained) continue;
    if (AB->vertex == end || AB->pair->vertex == end) continue;
    // ��������ꍇ
    if (judgeIntersected(AB->vertex->from_pos.toPointF(),
                         AB->pair->vertex->from_pos.toPointF(),
                         start->from_pos.toPointF(),
                         end->from_pos.toPointF())) {
      // AB���X�^�b�N�ɒǉ�
      edges.push(AB);
      // ���Α��̕ӂ𒲍��Ώۂɉ�����
      edgesToBeChecked.push(AB->pair->next);
      edgesToBeChecked.push(AB->pair->prev);
    }
    // �������Ȃ��ꍇ�͂Ȃɂ����Ȃ�
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
