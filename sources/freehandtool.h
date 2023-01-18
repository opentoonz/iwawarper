//--------------------------------------------------------------
// Free Hand Tool
// ���R�Ȑ��c�[��
//
// �}�E�X�h���b�O�ŗ��U�I�ȓ_���g���[�X���Ă����A
// �}�E�X�����[�X�Ńx�W�G�Ȑ��ɋߎ����ăV�F�C�v�����B
// �ݒ�_�C�A���O�ŋߎ��̐��x�Ȃǂ𒲐��ł���B
//--------------------------------------------------------------
#ifndef FREEHANDTOOL_H
#define FREEHANDTOOL_H

#include "iwtool.h"

#include <QList>
#include <QPointF>
#include <QUndoCommand>

#include "shapepair.h"

class IwProject;
class IwLayer;

class FreeHandTool : public IwTool {
  Q_OBJECT

  // �g���[�X�����|�C���g�̃��X�g
  QList<QPointF> m_points;
  // ���Ă��邩�ǂ���
  bool m_isClosed;

  // ���O�̃}�E�X���W
  QPointF m_lastMousePos;
  bool m_isDragging;

  OneShape m_currentShape;
  // IwShape* m_currentShape;

  // ���x�B�������덷�l
  double m_error;

  //------- �`������x�W�G�J�[�u���ߎ����镔��
  BezierPointList fitCurve(QList<QPointF>& points, double error, bool isClosed);
  // �[�_�̃^���W�F���g�E�x�N�g�������߂�
  QPointF computeLeftTangent(QList<QPointF>& points, int end);
  QPointF computeRightTangent(QList<QPointF>& points, int end);
  QPointF computeCenterTangent(QList<QPointF>& points, int center);
  void computeCenterTangents(QList<QPointF>& points, int center, QPointF* V1,
                             QPointF* V2);
  //  �|�C���g�́i�T�u�j�Z�b�g�ɁA�x�W�G�Ȑ����t�B�b�g������
  void fitCubic(QList<QPointF>& points, int first, int last, QPointF& tHat1,
                QPointF& tHat2, double error, QList<QPointF>& outList);
  // ���ʂɃx�W�G�|�C���g4�_��ǉ�
  void addBezierCurve(QList<QPointF>& outList, QList<QPointF>& bezierCurve);
  // �e�_�̋������O�`�P�ɐ��K���������̂��i�[
  QList<double> chordLengthParameterize(QList<QPointF>& points, int first,
                                        int last);

  // �x�W�G�̐���
  QList<QPointF> generateBezier(QList<QPointF>& points, int first, int last,
                                QList<double>& uPrime, QPointF tHat1,
                                QPointF tHat2);
  // �K�p�����x�W�G�ŁA�ł��Y���Ă���_�̃C���f�b�N�X�ƁA�덷�̕��𓾂�
  double computeMaxError(QList<QPointF>& points, int first, int last,
                         QList<QPointF>& bezCurve, QList<double>& u,
                         int* splitPoint);
  // �p�����[�^���Đݒ肷��
  void reparameterize(QList<double>& uPrime,  // �����Ɍ��ʂ����߂�
                      QList<QPointF>& points, int first, int last,
                      QList<double>& u, QList<QPointF>& bezCurve);
  //-------

  // ���݂̃p�����[�^����V�F�C�v�����
  void createShape();

public:
  FreeHandTool();
  ~FreeHandTool();

  bool leftButtonDown(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonDrag(const QPointF&, const QMouseEvent*) final override;
  bool leftButtonUp(const QPointF&, const QMouseEvent*) final override;

  bool rightButtonDown(const QPointF&, const QMouseEvent*, bool& canOpenMenu,
                       QMenu& menu) final override;

  void draw() final override;

  void onDeactivate() final override;

  //(�_�C�A���O����Ă΂��) ���x���ς������V�F�C�v��ύX����
  void onPrecisionChanged(int prec);

  // �V�F�C�v������ꍇ�́A������v���W�F�N�g�ɓo�^����
  // �����V�F�C�v������������A���j���[���o���Ȃ����߂�false��Ԃ�
  bool finishShape();

  bool isClosed() { return m_isClosed; }

  void toggleCloseShape(bool close);

  int getCursorId(const QMouseEvent*) override;
public slots:
  void onDeleteCurrentShape();

signals:
  // �V�F�C�v����蒼���i���邢�͏����j�x�ɌĂ�ŁA�_�C�A���O�̒l���X�V����
  void shapeChanged(int);
};

//---------------------------------------------------
// �ȉ��AUndo�R�}���h��o�^
//---------------------------------------------------

class CreateFreeHandShapeUndo : public QUndoCommand {
  IwProject* m_project;
  int m_index;
  ShapePair* m_shape;
  IwLayer* m_layer;

public:
  CreateFreeHandShapeUndo(ShapePair* shape, IwProject* prj, IwLayer* layer);
  ~CreateFreeHandShapeUndo();

  void undo();
  void redo();
};

#endif