//--------------------------------------------------------------
// Free Hand Tool
// ���R�Ȑ��c�[��
//
// �}�E�X�h���b�O�ŗ��U�I�ȓ_���g���[�X���Ă����A
// �}�E�X�����[�X�Ńx�W�G�Ȑ��ɋߎ����ăV�F�C�v�����B
// �ݒ�_�C�A���O�ŋߎ��̐��x�Ȃǂ𒲐��ł���B
//--------------------------------------------------------------

#include "freehandtool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwtoolhandle.h"
#include "iwproject.h"
#include "colorsettings.h"
#include "viewsettings.h"
#include "iwundomanager.h"

#include "iwshapepairselection.h"
#include "reshapetool.h"
#include "sceneviewer.h"
#include "pentool.h"

#include "iwlayer.h"
#include "iwlayerhandle.h"

#include <QMouseEvent>
#include <QPair>
#include <math.h>
#include <QMessageBox>

/*
Some part of this source is based on the Graphics Gems code by Eric Haines
https://github.com/erich666/GraphicsGems/blob/master/gems/FitCurves.c

An Algorithm for Automatically Fitting Digitized Curves
by Philip J. Schneider
from "Graphics Gems", Academic Press, 1990
*/

//-------------
// �x�N�g�����Z�֌W

namespace {

/* returns squared length of input vector */
double V2SquaredLength(QPointF* a) {
  return ((a->x() * a->x()) + (a->y() * a->y()));
}

/* returns length of input vector */
double V2Length(QPointF* a) { return (sqrt(V2SquaredLength(a))); }

/* negates the input vector and returns it */
QPointF* V2Negate(QPointF* v) {
  v->setX(-v->x());
  v->setY(-v->y());
  return v;
}

/* scales the input vector to the new length and returns it */
QPointF* V2Scale(QPointF* v, double newlen) {
  double len = V2Length(v);
  if (len != 0.0) {
    v->setX(v->x() * newlen / len);
    v->setY(v->y() * newlen / len);
  }
  return v;
}

/* return vector sum c = a+b */
QPointF* V2Add(QPointF* a, QPointF* b, QPointF* c) {
  c->setX(a->x() + b->x());
  c->setY(a->y() + b->y());
  return c;
}

/* return the dot product of vectors a and b */
double V2Dot(QPointF* a, QPointF* b) {
  return (a->x() * b->x()) + (a->y() * b->y());
}

/* normalizes the input vector and returns it */
QPointF* V2Normalize(QPointF* v) {
  double len = V2Length(v);
  if (len != 0.0) {
    v->setX(v->x() / len);
    v->setY(v->y() / len);
  }
  return v;
}

/* return the distance between two points */
double V2DistanceBetween2Points(QPointF* a, QPointF* b) {
  double dx = a->x() - b->x();
  double dy = a->y() - b->y();
  return sqrt((dx * dx) + (dy * dy));
}

//-------------------
// �w��degree�A�w��x�W�G�ʒu�ł̍��W��Ԃ�
// Bezier :
//	Evaluate a Bezier curve at a particular parameter value
//-------------------
QPointF Bezier(int degree,         // The degree of the bezier curve
               QList<QPointF>& V,  // Array of control points
               double t)           // Parametric value to find point for
{
  QList<QPointF> Vtemp;  // Local copy of control points
  for (int i = 0; i <= degree; i++) {
    Vtemp.push_back(V[i]);
  }

  // Triangle computation
  for (int i = 1; i <= degree; i++) {
    for (int j = 0; j <= degree - i; j++) {
      Vtemp[j].setX((1.0 - t) * Vtemp[j].x() + t * Vtemp[j + 1].x());
      Vtemp[j].setY((1.0 - t) * Vtemp[j].y() + t * Vtemp[j + 1].y());
    }
  }

  return Vtemp[0];
}

//-------------------
//  B0, B1, B2, B3 : �x�W�G�̊e��
//	Bezier multipliers
//-------------------
double B0(double u) {
  double tmp = 1.0 - u;
  return tmp * tmp * tmp;
}

double B1(double u) {
  double tmp = 1.0 - u;
  return 3 * u * tmp * tmp;
}

double B2(double u) {
  double tmp = 1.0 - u;
  return 3 * u * u * tmp;
}

double B3(double u) { return u * u * u; }

};  // namespace

//-------------

FreeHandTool::FreeHandTool()
    : IwTool("T_Freehand")
    , m_isClosed(false)
    , m_isDragging(false)
    , m_error(0.010001) {}

FreeHandTool::~FreeHandTool() {}

void FreeHandTool::onDeactivate() {
  // �V�F�C�v������ꍇ�́A������v���W�F�N�g�ɓo�^����
  finishShape();

  // ���ɐ���`���Ă���ꍇ�́A�V���ɂ���
  if (!m_points.isEmpty()) m_points.clear();
}

bool FreeHandTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return false;
  if (layer->isLocked()) {
    QMessageBox::critical(m_viewer, tr("Critical"),
                          tr("The current layer is locked."));
    return false;
  }

  // �V�F�C�v������ꍇ�́A������v���W�F�N�g�ɓo�^����
  finishShape();

  // ���ɐ���`���Ă���ꍇ�́A�V���ɂ���
  if (!m_points.isEmpty()) m_points.clear();

  m_points.push_back(pos);

  m_lastMousePos = e->localPos();

  m_isDragging = true;

  return true;
}

bool FreeHandTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  // �h���b�O���Ŗ������return
  if (!m_isDragging) return false;

  QPointF newMousePos = e->localPos();

  QPointF mouseMoveVec = newMousePos - m_lastMousePos;

  // �V�����}�E�X�ʒu�̃}���n�b�^���������R�s�N�Z���ȏ㗣��Ă�����A
  // �V���ȃg���[�X�ʒu�Ƃ��ēo�^����
  if (mouseMoveVec.manhattanLength() >= 3.0) {
    m_points.push_back(pos);
    m_lastMousePos = newMousePos;
    return true;
  }

  return false;
}

bool FreeHandTool::leftButtonUp(const QPointF&, const QMouseEvent*) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return false;

  // �����������Ă�����x�W�G�����(�|�C���g���Q�ȏ�)
  if (m_points.count() >= 2) {
    createShape();
    m_isDragging = false;
    return true;
  }

  m_isDragging = false;
  return false;
}

//--------------------------------------------------------
bool FreeHandTool::rightButtonDown(const QPointF&, const QMouseEvent*,
                                   bool& canOpenMenu, QMenu& /*menu*/) {
  // �V�F�C�v�̊���
  canOpenMenu = finishShape();

  if (m_points.isEmpty()) return false;

  m_points.clear();
  return true;
}

void FreeHandTool::draw() {
  // �g���[�X�����_���������return
  if (m_points.isEmpty()) return;

  if (!m_viewer) return;
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  //--- �g���[�X�����_��`��
  // �F�̎擾

  // �V�F�C�v�̌��݂̏󋵂ɍ��킹�ĐF�𓾂�
  QColor tracedLineColor =
      ColorSettings::instance()->getQColor(Color_FreeHandToolTracedLine);

  m_viewer->setColor(tracedLineColor);

  QVector3D* vert = new QVector3D[m_points.size()];
  for (int p = 0; p < m_points.size(); p++) {
    vert[p] = QVector3D(m_points.at(p));
  }
  m_viewer->doDrawLine((m_isClosed) ? GL_LINE_LOOP : GL_LINE_STRIP, vert,
                       m_points.size());
  delete[] vert;

  if (!m_currentShape.shapePairP) return;

  // �`���I����Ă�����x�W�G���\��

  // ���݂̃t���[��
  int frame = project->getViewFrame();

  // �V�F�C�v�̌��݂̏󋵂ɍ��킹�ĐF�𓾂�
  m_viewer->setColor(QColor(Qt::white));
  // glColor3d(1.0, 1.0, 1.0);

  QVector3D* vertexArray = m_currentShape.shapePairP->getVertexArray(
      frame, m_currentShape.fromTo, project);

  m_viewer->doDrawLine(GL_LINE_STRIP, vertexArray,
                       m_currentShape.shapePairP->getVertexAmount(project));

  // �f�[�^�����
  delete[] vertexArray;

  // �R���g���[���|�C���g��`��

  // �R���g���[���|�C���g�̐F�𓾂Ă���
  QColor cpColor    = ColorSettings::instance()->getQColor(Color_CtrlPoint);
  QColor cpSelected = ColorSettings::instance()->getQColor(Color_ActiveCtrl);
  // ���ꂼ��̃|�C���g�ɂ��āA�I������Ă�����n���h���t���ŕ`��
  // �I������Ă��Ȃ���Ε��ʂɎl�p��`��
  BezierPointList bPList = m_currentShape.shapePairP->getBezierPointList(
      frame, m_currentShape.fromTo);

  for (int p = 0; p < bPList.size(); p++) {
    // �P�ɃR���g���[���|�C���g��`�悷��
    m_viewer->setColor(cpColor);
    ReshapeTool::drawControlPoint(m_viewer, m_currentShape, bPList, p, false,
                                  m_viewer->getOnePixelLength());
  }
}

//(�_�C�A���O����Ă΂��) ���x���ς������V�F�C�v��ύX����
void FreeHandTool::onPrecisionChanged(int prec) {
  // ���x�l���狖�����덷�l���v�Z
  m_error = std::pow((float)(100 - prec) / 100.f, 7.f) * 0.01f + 0.000001f;

  // ���݂̃c�[�����J�����g�A���V�F�C�v������΍X�V����
  if (IwApp::instance()->getCurrentTool()->getTool() == this &&
      m_currentShape.shapePairP) {
    createShape();

    m_viewer->update();
  }
}

// ���݂̃p�����[�^����V�F�C�v�����
void FreeHandTool::createShape() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  if (m_points.count() < 2) return;

  if (m_currentShape.shapePairP) delete m_currentShape.shapePairP;

  BezierPointList bpList = fitCurve(m_points, m_error, m_isClosed);

  // ���݂̃t���[��
  int frame = project->getViewFrame();

  int bpCount = bpList.count();

  CorrPointList cpList;
  for (int cp = 0; cp < 4; cp++) {
    cpList.push_back(
        {(double)((bpCount - 1) * cp) / ((m_isClosed) ? 4.0 : 3.0), 1.0});
  }

  m_currentShape =
      OneShape(new ShapePair(frame, m_isClosed, bpList, cpList, 0.01), 0);

  // �|�C���g�����G�~�b�g
  emit shapeChanged(bpCount);
}

// �V�F�C�v������ꍇ�́A������v���W�F�N�g�ɓo�^����
//  �����V�F�C�v������������A���j���[���o���Ȃ����߂�false��Ԃ�
bool FreeHandTool::finishShape() {
  if (!m_currentShape.shapePairP)
    // if(!m_currentShape)
    return true;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(
      new CreateFreeHandShapeUndo(m_currentShape.shapePairP, project, layer));

  // �V�F�C�v�̎��̂�Undo�ɓn�����̂ŁA�|�C���^�ɂ͂O����
  m_currentShape = 0;

  // Close����������
  m_isClosed = false;

  // �_�C�A���O�X�V
  emit shapeChanged(0);

  return false;
}

void FreeHandTool::toggleCloseShape(bool close) {
  if (!m_currentShape.shapePairP) return;

  m_isClosed = close;

  createShape();
  m_viewer->update();
}

void FreeHandTool::onDeleteCurrentShape() {
  if (!m_currentShape.shapePairP) return;

  delete m_currentShape.shapePairP;
  // delete m_currentShape;
  m_currentShape = 0;

  // Close����������
  m_isClosed = false;

  m_points.clear();

  m_viewer->update();

  // �_�C�A���O�X�V
  emit shapeChanged(0);
}

//------- �`������x�W�G�J�[�u���ߎ����镔��

BezierPointList FreeHandTool::fitCurve(
    QList<QPointF>& points,  // �}�E�X�ʒu�̋O��
    double error,  // �i���[�U���w�肷��j���x�A�x�W�G�J�[�u�ɋ����ꂽ�덷
    bool closed) {
  QPointF tHat1, tHat2;  // �[�_�̐ڐ��x�N�g��(���K����������)�����߂�
  tHat1 = computeLeftTangent(points, 0);
  tHat2 = computeRightTangent(points, points.size() - 1);

  QList<QPointF> outList;
  // ��������ċA�I�ɕ������x�W�G�ɋߎ����Ă���
  fitCubic(points, 0, points.size() - 1, tHat1, tHat2, error, outList);

  // outList��bpList�Ɉڂ�
  BezierPointList bpList;

  if (outList.size() >= 2) {
    QPointF firstHandle;
    int lastIndex = outList.size() - 1;

    if (closed)  // �����V�F�C�v�̏ꍇ�͏I�_�Ɍ������n���h���ɂ���
      firstHandle = outList[0] + (outList[lastIndex] - outList[0]) / 3.0;
    else
      firstHandle = outList[0] + outList[0] - outList[1];

    outList.push_front(firstHandle);

    lastIndex = outList.size() - 1;

    QPointF lastHandle;

    if (closed)
      lastHandle = outList[lastIndex] + (outList[1] - outList[lastIndex]) / 3.0;
    else
      lastHandle =
          outList[lastIndex] + outList[lastIndex] - outList[lastIndex - 1];

    outList.push_back(lastHandle);

    // 3���܂Ƃ߂�BezierPointList�ɓo�^���Ă���
    for (int index = 0; index < outList.size(); index += 3) {
      BezierPoint bp = {
          outList[index + 1],  // pos
          outList[index],      // firstHandle
          outList[index + 2]   // secondHandle
      };
      bpList.push_back(bp);
    }
  }
  return bpList;
}

QPointF FreeHandTool::computeLeftTangent(
    QList<QPointF>& points,  // Digitized points �O�Ճf�[�^
    int end  // Index to "left" end of region //���[�̃C���f�b�N�X
) {
  QPointF tHat1;
  tHat1 = points[end + 1] - points[end];
  tHat1 = *V2Normalize(&tHat1);
  return tHat1;
}

QPointF FreeHandTool::computeRightTangent(
    QList<QPointF>& points,  // Digitized points �O�Ճf�[�^
    int end  // Index to "left" end of region //���[�̃C���f�b�N�X
) {
  QPointF tHat2;
  tHat2 = points[end - 1] - points[end];
  tHat2 = *V2Normalize(&tHat2);
  return tHat2;
}

QPointF FreeHandTool::computeCenterTangent(
    QList<QPointF>& points,  // Digitized points �O�Ճf�[�^
    int center  // �����_�̃C���f�b�N�X Index to point inside region
) {
  QPointF tHatCenter;
  tHatCenter = points[center - 1] - points[center + 1];
  V2Normalize(&tHatCenter);
  return tHatCenter;
}

void FreeHandTool::computeCenterTangents(
    QList<QPointF>& points,  // Digitized points �O�Ճf�[�^
    int center,  // �����_�̃C���f�b�N�X Index to point inside region
    QPointF* V1, QPointF* V2) {
  QPointF tHatCenter;

  *V1 = points[center - 1] - points[center];
  *V2 = points[center + 1] - points[center];

  V2Normalize(V1);
  V2Normalize(V2);

  // �s�p�ɂ������
  if (V2Dot(V1, V2) > -0.7) return;

  // �X���[�X�ȋȐ��ɂ���
  *V1 = *V2 = computeCenterTangent(points, center);
  V2Negate(V2);
}

// ���ʂɃx�W�G�|�C���g4�_��ǉ�
void FreeHandTool::addBezierCurve(QList<QPointF>& outList,
                                  QList<QPointF>& bezierCurve) {
  // outList�̍Ō�̓_��bezierCurve�̂P�_�ڂƒu�������A�c���Push����
  // �܂��_�������ꍇ�͍ŏ�����Push
  if (outList.isEmpty())
    outList.push_back(bezierCurve[0]);
  else
    outList.last() = bezierCurve[0];

  for (int i = 1; i < 4; i++) outList.push_back(bezierCurve[i]);
}

//------------------------------------------
// �e�_�̋������O�`�P�ɐ��K���������̂��i�[
// chordLengthParameterize :
// Assign parameter values to digitized points
// using relative distances between points.
//------------------------------------------
QList<double> FreeHandTool::chordLengthParameterize(QList<QPointF>& points,
                                                    int first, int last) {
  QList<double> u; /*  Parameterization */

  // �n�_����̋�����ώZ���Ċi�[���Ă���
  u.push_back(0.0);
  for (int i = first + 1; i <= last; i++) {
    u.push_back(u.last() +
                V2DistanceBetween2Points(&points[i], &points[i - 1]));
  }

  // 0-1�ɐ��K��
  for (int i = first + 1; i <= last; i++) {
    u[i - first] = u[i - first] / u[last - first];
  }

  return u;
}

//------------------------------------------
//  FitCubic :
//  �|�C���g�́i�T�u�j�Z�b�g�ɁA�x�W�G�Ȑ����t�B�b�g������
//  Fit a Bezier curve to a (sub)set of digitized points
//------------------------------------------

void FreeHandTool::fitCubic(
    QList<QPointF>&
        points,  // �O�Ճf�[�^			Array of digitized points
    int first,
    int last,  // �[�_�̃C���f�b�N�X	Indices of first and last pts in region
    QPointF& tHat1,
    QPointF&
        tHat2,     // �[�_�̐ڐ��x�N�g��	Unit tangent vectors at endpoints
    double error,  // ���[�U�̌��߂����x�i�������덷�l�j	User-defined
                   // error squared
    QList<QPointF>& outList  // �����Ɍ��ʂ����Ă���
) {
  QList<QPointF>
      bezCurve;  // QPointF�̔z��ɂ��悤�BControl points of fitted Bezier curve

  double iterationError =
      error * error;  // �덷�l Error below which you try iterating
  int nPts =
      last - first + 1;  // �Z�b�g���ɂ���|�C���g�� Number of points in subset

  //  ��_�����Ȃ��ꍇ�͔����I���@���g�� Use heuristic if region only has two
  //  points in it
  if (nPts == 2) {
    // �n���h���̒�����2�_�Ԃ̋�����1/3�ɂ���
    double dist = V2DistanceBetween2Points(&points[last], &points[first]) / 3.0;
    // 4�_�ǉ�
    bezCurve.push_back(points[first]);
    bezCurve.push_back(points[first] + *V2Scale(&tHat1, dist));
    bezCurve.push_back(points[last] + *V2Scale(&tHat2, dist));
    bezCurve.push_back(points[last]);
    addBezierCurve(outList, bezCurve);
    return;
  }

  QList<double>
      u;  // �e�_�̋������O�`�P�ɐ��K���������̂��i�[ Parameter values for point

  // �n�_����̊e�_������ώZ���Đ��K���������̂��i�[
  //  Parameterize points, and attempt to fit curve
  u = chordLengthParameterize(points, first, last);

  // �x�W�G�̐��� �ŏ����@�Ō덷���ŏ���
  bezCurve = generateBezier(points, first, last, u, tHat1, tHat2);

  // �K�p�����x�W�G�ŁA�ł��Y���Ă���_�̃C���f�b�N�X�ƁA�덷�̕�(���)�𓾂�
  double maxError;  // �ő�덷�l Maximum fitting error
  int splitPoint;   // �������ׂ��_�̃C���f�b�N�X Point to split point set at
                    //  Find max deviation of points to fitted curve
  maxError = computeMaxError(points, first, last, bezCurve, u, &splitPoint);

  // �덷�l�����e�ʂ���������������A����ȏ�_�͒ǉ�����
  if (maxError < error) {
    addBezierCurve(outList, bezCurve);
    return;
  }

  // �p�����[�^�̍Đݒ�A�A����̂��H�H �܂�������Ɠ���Ă݂悤
  //  If error not too large, try some reparameterization
  //  and iteration

  int maxIterations =
      4;  //  �p�����[�^�Đݒ�̎��s�� Max times to try iterating
  if (maxError < iterationError) {
    for (int i = 0; i < maxIterations; i++) {
      QList<double> uPrime;
      // �p�����[�^���Đݒ肷��(�j���[�g���@)
      reparameterize(uPrime, points, first, last, u, bezCurve);
      bezCurve = generateBezier(points, first, last, uPrime, tHat1, tHat2);
      maxError =
          computeMaxError(points, first, last, bezCurve, uPrime, &splitPoint);

      if (maxError < error) {
        addBezierCurve(outList, bezCurve);
        return;
      }
      u = uPrime;
    }
  }

  // �t�B�b�g�����܂������Ȃ������̂ŁA��������
  // Fitting failed -- split at max error point and fit recursively

  QList<char> splitarr;  // yes or no split

  // long�̓R���p�C���Ɋւ�炸�K��4�o�C�g
  for (int i = first; i < last; i++) {
    long c    = 0;
    double dx = points[i].x() - points[i + 1].x();
    double dy = points[i].y() - points[i + 1].y();
    if (dx < 0.0)
      c |= 1;
    else if (dx > 0.0)
      c |= 2;
    if (dy < 0.0)
      c |= 4;
    else if (dy > 0.0)
      c |= 8;
    splitarr.push_back(c);
  }
  // �r���_���a�ɂ���
  for (int i = 0; i < nPts - 2; i++)
    // for (int i = 0; i < nPts - 1; i++)
    splitarr[i] ^= splitarr[i + 1];

  long best  = splitPoint;
  long mdist = 0xffffff;

  for (int i = 1; i < nPts - 3; i++) {
    if (splitarr[i]) {
      long dist = (i + first + 1) - splitPoint;
      if (dist < 0) dist = -dist;
      if (dist < mdist) {
        best  = i + first + 1;
        mdist = dist;
      }
    }
  }
  splitPoint = best;

  QPointF tHatLeft, tHatRight;
  // �����_�ł̐ڐ��x�N�g���𓾂�
  computeCenterTangents(points, splitPoint, &tHatLeft, &tHatRight);
  // �x�W�G�𕪊����čċA�I�Ɍv�Z
  fitCubic(points, first, splitPoint, tHat1, tHatLeft, error, outList);
  fitCubic(points, splitPoint, last, tHatRight, tHat2, error, outList);
}

//-----------------------------------------------------
// �ŏ����@�Ńx�W�G�̃R���g���[���|�C���g��T��
//  GenerateBezier :
//  Use least-squares method to find Bezier control points for region.
//-----------------------------------------------------
// �x�W�G�̐���
QList<QPointF> FreeHandTool::generateBezier(
    QList<QPointF>& points,  //  �O�Ղ̃g���[�X�f�[�^ Array of digitized points
    int first, int last,  //  ���ꂩ��x�W�G�𓖂Ă�͈͂̃C���f�b�N�X Indices
                          //  defining region
    QList<double>&
        uPrime,  //  �O�Ղ̊e�_�́A�n�_����̋�����0-1�ɐ��K����������
                 //  Parameter values for region
    QPointF tHat1, QPointF tHat2  //  Unit tangents at endpoints
) {
  QList<QPointF>
      bezCurve;  // �߂�l���i�[���邽�߂̓��ꕨ RETURN bezier curve ctl pts

  int nPts = last - first +
             1;  // ���̃T�u�J�[�u���̃g���[�X�_�̐� Number of pts in sub-curve

  // ���[�_�̃x�N�g���́A�e�O�Փ_�ł̉e�� �I�ȃp�����[�^ Precomputed rhs for eqn
  QList<QPair<QPointF, QPointF>> A;
  // �s��A�̌v�Z Compute the A's
  for (int i = 0; i < nPts; i++) {
    QPointF v1, v2;
    v1 = tHat1;
    v2 = tHat2;
    V2Scale(&v1, B1(uPrime[i]));
    V2Scale(&v2, B2(uPrime[i]));
    A.push_back(QPair<QPointF, QPointF>(v1, v2));
    // A[i].first	= v1;
    // A[i].second = v2;
  }

  // �s�� C��X
  double C[2][2];  // Matrix C
  double X[2];     // Matrix X
  // Create the C and X matrices
  C[0][0] = C[0][1] = C[1][0] = C[1][1] = 0.0;
  X[0] = X[1] = 0.0;

  for (int i = 0; i < nPts; i++) {
    C[0][0] += V2Dot(&A[i].first, &A[i].first);
    C[1][0] = C[0][1] += V2Dot(&A[i].first, &A[i].second);
    C[1][1] += V2Dot(&A[i].second, &A[i].second);

    QPointF tmp =
        points[first + i] -
        (points[first] * B0(uPrime[i]) + points[first] * B1(uPrime[i]) +
         points[last] * B2(uPrime[i]) + points[last] * B3(uPrime[i]));

    X[0] += V2Dot(&A[i].first, &tmp);
    X[1] += V2Dot(&A[i].second, &tmp);
  }

  // Determinants of matrices
  double det_C0_C1 = C[0][0] * C[1][1] - C[1][0] * C[0][1];
  double det_C0_X  = C[0][0] * X[1] - C[0][1] * X[0];
  double det_X_C1  = X[0] * C[1][1] - X[1] * C[0][1];

  // Finally, derive alpha values
  if (det_C0_C1 == 0.0) {
    det_C0_C1 = (C[0][0] * C[1][1]) * 10e-12;
  }

  // Alpha values, left and right
  // ���[����L�т�n���h���̒���
  double alpha_l = det_X_C1 / det_C0_C1;
  double alpha_r = det_C0_X / det_C0_C1;

  //  If alpha negative, use the Wu/Barsky heuristic (see text)
  if (alpha_l < 0.0 || alpha_r < 0.0) {
    double dist = V2DistanceBetween2Points(&points[last], &points[first]) / 3.0;

    bezCurve.push_back(points[first]);                           // 0
    bezCurve.push_back(points[first] + *V2Scale(&tHat1, dist));  // 1
    bezCurve.push_back(points[last] + *V2Scale(&tHat2, dist));   // 2
    bezCurve.push_back(points[last]);                            // 3
    return bezCurve;
  }

  //  First and last control points of the Bezier curve are
  //  positioned exactly at the first and last data points
  //  Control points 1 and 2 are positioned an alpha distance out
  //  on the tangent vectors, left and right, respectively
  bezCurve.push_back(points[first]);                              // 0
  bezCurve.push_back(points[first] + *V2Scale(&tHat1, alpha_l));  // 1
  bezCurve.push_back(points[last] + *V2Scale(&tHat2, alpha_r));   // 2
  bezCurve.push_back(points[last]);                               // 3

  return bezCurve;
}

//-----------------------------------------------
// �K�p�����x�W�G�ŁA�ł��Y���Ă���_�̃C���f�b�N�X�ƁA�덷�̕�(���)�𓾂�
//  ComputeMaxError :
//	Find the maximum squared distance of digitized points
//	to fitted curve.
//-----------------------------------------------

double FreeHandTool::computeMaxError(
    QList<QPointF>& points,  // �O�Ղ̃g���[�X�f�[�^Array of digitized points
    int first, int last,     // ���ꂩ��x�W�G�𓖂Ă�͈͂̃C���f�b�N�X Indices
                             // defining region
    QList<QPointF>& bezCurve,  // �������t�B�b�g�������x�W�G Fitted Bezier curve
    QList<double>& u,          // �n�_����̊e�_������ώZ���Đ��K����������
                               // Parameterization of points
    int* splitPoint)  // �ł��Y���Ă���_�̃C���f�b�N�X�����ĕԂ� Point of
                      // maximum error
{
  // �Ƃ肠�������_
  *splitPoint = (last - first + 1) / 2;

  // �덷�̍ő�l
  double maxDist = 0.0;

  // ���ԂɃC���f�b�N�X��i�߂āA����̋������ő�ɂȂ�ʒu��T��
  for (int i = first + 1; i < last; i++) {
    QPointF P   = Bezier(3, bezCurve, u[i - first]);  //  Point on curve
    QPointF v   = P - points[i];        //  Vector from point to curve
    double dist = V2SquaredLength(&v);  //  Current error

    // �덷�����傫��������C���f�b�N�X���X�V
    if (dist >= maxDist) {
      maxDist     = dist;
      *splitPoint = i;
    }
  }

  return maxDist;
}

//------------------------------------------------
// �p�����[�^���Đݒ肷��
//  Reparameterize:
//	Given set of points and their parameterization, try to find
//   a better parameterization.
//------------------------------------------------

void FreeHandTool::reparameterize(
    QList<double>& uPrime,    // �����Ɍ��ʂ����߂�
    QList<QPointF>& points,   //  Array of digitized points
    int first, int last,      //  Indices defining region
    QList<double>& u,         //  Current parameter values
    QList<QPointF>& bezCurve  //  Current fitted curve
) {
  for (int i = first; i <= last; i++) {
    // �ȉ��AnewtonRaphsonRootFind�̓��e������
    QPointF current_p = points[i];
    double current_u  = u[i - first];

    // Compute Q(u)
    QPointF Q_u = Bezier(3, bezCurve, current_u);

    QList<QPointF> Q1, Q2;  //  Q' and Q''
    // Generate control vertices for Q'
    for (int j = 0; j <= 2; j++) {
      Q1.push_back((bezCurve[j + 1] - bezCurve[j]) * 3.0);
    }
    // Generate control vertices for Q''
    for (int j = 0; j <= 1; j++) {
      Q2.push_back((Q1[j + 1] - Q1[j]) * 2.0);
    }
    // Compute Q'(u) and Q''(u)
    QPointF Q1_u = Bezier(2, Q1, current_u);
    QPointF Q2_u = Bezier(1, Q2, current_u);

    // Compute f(u)/f'(u)
    double numerator = (Q_u.x() - current_p.x()) * (Q1_u.x()) +
                       (Q_u.y() - current_p.y()) * (Q1_u.y());
    double denominator = (Q1_u.x()) * (Q1_u.x()) + (Q1_u.y()) * (Q1_u.y()) +
                         (Q_u.x() - current_p.x()) * Q2_u.x() +
                         (Q_u.y() - current_p.y()) * Q2_u.y();

    double improved_u = current_u - (numerator / denominator);

    uPrime.push_back(improved_u);
  }
}

//--------------------------------------------------------

int FreeHandTool::getCursorId(const QMouseEvent* /*e*/) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer || layer->isLocked()) {
    IwApp::instance()->showMessageOnStatusBar(
        tr("Current layer is locked or not exist."));
    return ToolCursor::ForbiddenCursor;
  }

  IwApp::instance()->showMessageOnStatusBar(
      tr("[Drag and draw] to create a new shape."));
  return ToolCursor::ArrowCursor;
  ;
}

//---------------------------------------------------
// �ȉ��AUndo�R�}���h��o�^
//---------------------------------------------------

CreateFreeHandShapeUndo::CreateFreeHandShapeUndo(ShapePair* shape,
                                                 IwProject* prj, IwLayer* layer)
    // CreateFreeHandShapeUndo::CreateFreeHandShapeUndo( IwShape* shape,
    // IwProject* prj)
    : m_project(prj), m_shape(shape), m_layer(layer) {
  // �V�F�C�v��}������C���f�b�N�X�́A�V�F�C�v���X�g�̍Ō�
  m_index = m_layer->getShapePairCount();
  // m_index = m_project->getShapeAmount();
}

CreateFreeHandShapeUndo::~CreateFreeHandShapeUndo() {
  // �V�F�C�v��delete
  delete m_shape;
  // delete m_shape;
}

void CreateFreeHandShapeUndo::undo() {
  // �V�F�C�v����������
  m_layer->deleteShapePair(m_index);
  // m_project->deleteShape(m_index);
  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void CreateFreeHandShapeUndo::redo() {
  // �V�F�C�v���v���W�F�N�g�ɒǉ�
  m_layer->addShapePair(m_shape, m_index);
  // m_project->addShape(m_shape,m_index);

  // �������ꂪ�J�����g�Ȃ�A
  if (m_project->isCurrent()) {
    // �쐬�����V�F�C�v��I����Ԃɂ���
    IwShapePairSelection::instance()->selectOneShape(OneShape(m_shape, 0));
    // �V�O�i�����G�~�b�g
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}

FreeHandTool freehandTool;