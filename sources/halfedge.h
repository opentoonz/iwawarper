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
  // ���_���W
  QVector3D from_pos;
  // ���[�v��̍��W
  QVector3D to_pos;

  // ���̒��_���n�_�ɂ��n�[�t�G�b�W��1��
  Halfedge* halfedge = nullptr;
  // ��������i�����Ȃ��j���ǂ���
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
  // �n�_�ƂȂ钸�_
  HEVertex* vertex;
  // ���̃n�[�t�G�b�W���܂ޖ�
  HEFace* face = nullptr;
  // �Ő�������Ŕ��Α��̃n�[�t�G�b�W
  Halfedge* pair = nullptr;
  // ���̃n�[�t�G�b�W
  Halfedge* next = nullptr;
  // �O�̃n�[�t�G�b�W
  Halfedge* prev = nullptr;
  // ��������i�����Ȃ��j���ǂ���
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
  // �܂ރn�[�t�G�b�W�̂�����1��
  Halfedge* halfedge = nullptr;

  // �e�V�F�C�v�`��Ɋ܂܂�Ă��邩�ǂ���
  // ����CorrVec�̑}����A�ĕ����O�ɂP�x�����`�F�b�N����
  bool isVisible = true;

  HEFace(Halfedge* he) {
    assert(he);
    halfedge = he;  // Face �� Halfedge �̃����N
  }

  // �G�b�W�S�Ă�contrained�ɂ���
  void setConstrained(bool constrained);

  Halfedge* edgeFromVertex(HEVertex* v);
  // �ʐς�Ԃ�
  double size() const;
  // �d�S��HEVertex���쐬���|�C���^��Ԃ�
  HEVertex* createCentroidVertex();

  bool hasConstantDepth() const;
  // �d�S��Depth��Ԃ�
  double centroidDepth() const;
};

class HEModel {
public:
  QList<HEFace*> faces;
  QList<HEVertex*> vertices;
  QList<HEVertex*> superTriangleVertices;

private:
  // he �� pair �ƂȂ�n�[�t�G�b�W��T���ēo�^����
  void setHalfedgePair(Halfedge* he);
  // ��̃n�[�t�G�b�W���݂��� pair �Ƃ���
  void setHalfedgePair(Halfedge* he0, Halfedge* he1);
  // Vertex �̃��X�g���� vertex ���폜����
  void deleteVertex(HEVertex* vertex);
  // Face �̃��X�g���� face ���폜����
  void deleteFace(HEFace* face);

  void addVertexToEdge(Halfedge* he, HEVertex* p, HEVertex* constrain_pair,
                       QStack<Halfedge*>& heToBeChecked);
  void addVertexToFace(HEFace* face, HEVertex* p, HEVertex* constrain_pair,
                       QStack<Halfedge*>& heToBeChecked);

public:
  ~HEModel();

  HEFace* addFace(HEVertex* v0, HEVertex* v1, HEVertex* v2);

  void edgeSwap(Halfedge* he);

  // �ʒu����v���Ă��钸�_�����łɂ���ꍇ�A���̃|�C���^��Ԃ�
  HEVertex* findVertex(QPointF& from_p, QPointF& to_p);

  // �Ƃ肠���������ŒT���B�_���ӏ�ɂ���ꍇ��Edge�̃|�C���^���Ԃ�
  HEFace* findContainingFace(HEVertex* p, Halfedge** he_p);

  // ���f���ɓ_��ǉ�
  void addVertex(HEVertex* p, HEVertex* constrain_pair = nullptr);

  // he���܂ގO�p�`�̊O�ډ~���Ahe�̔��Α���
  // ���_���܂ޏꍇtrue��Ԃ�
  bool checkCircumcircle(Halfedge* he);

  // CD����������ł���l�p�`ABCD����
  bool checkConvexAndConstraint(Halfedge* he, HEVertex* p,
                                HEVertex* constrain_pair);

  void setConstraint(HEVertex* start, HEVertex* end, bool constraint);

  // �e�O�p�`�̏d�S��ǉ� �����͎O�p�`�𕪊����邩���f����ʐς�臒l
  void insertCentroids(double sizeThreshold);

  // �e�V�F�C�v�`��Ɋ܂܂�Ă��邩�ǂ���
  // ����CorrVec�̑}����A�ĕ����O�ɂP�x�����`�F�b�N����
  void checkFaceVisibility(const QPolygonF& parentShapePolygon);

  // �Ȃ��܂���
  void smooth(double offsetThreshold);

  // SuperTriangle������
  void deleteSuperTriangles();

  // start �� end �����Ԑ��������؂�G�b�W�̃��X�g�𓾂�
  void getCrossingEdges(QStack<Halfedge*>& edges, HEVertex* start,
                        HEVertex* end);

  void print();
};

#endif
