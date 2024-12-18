//---------------------------------------
// ShapePair
// From-To�̌`����y�A�Ŏ���
//---------------------------------------

#include "shapepair.h"

#include "iwproject.h"
#include "preferences.h"
#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"

#include "iwselectionhandle.h"
#include "iwtimelinekeyselection.h"

#include "iwselectionhandle.h"
#include "iwshapepairselection.h"

#include "viewsettings.h"
#include "iwtrianglecache.h"
#include "projectutils.h"

#include "keydragtool.h"
#include "outputsettings.h"

#include <cassert>

#include <QRectF>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QMessageBox>
#include <QList>

#include <QPainter>
#include <QPolygon>
#include <QVector2D>

#include <QMap>
#include <QMutexLocker>
#include <QMouseEvent>

#include "logger.h"

#define PRINT_LOG(message)                                                     \
  { Logger::Write(message); }

namespace {

const int LayerIndent = 10;
const int LockPos     = 40;
const int PinPos      = 70;
const int CorrIndent  = 100;

// �����������

// verticeP�ɁAStart, End�����j�A��Ԃ������W���i�[���Ă���
// endPoint�̂�������O�܂�
void makeLinearPoints(QVector3D* vertexP, QPointF startPoint, QPointF endPoint,
                      int precision) {
  double ratioStep = 1.0 / (double)precision;
  double tmpR      = 0.0;
  for (int p = 0; p < precision; p++) {
    // ���j�A��Ԃ������W���擾
    QPointF tmpPoint = startPoint * (1.0 - tmpR) + endPoint * tmpR;
    // �l���i�[�A�|�C���^��i�߂�
    *vertexP = QVector3D(tmpPoint.x(), tmpPoint.y(), 0.0);
    vertexP++;

    // ratio���C���N�������g
    tmpR += ratioStep;
  }
}

// �Е��̃`�����l���̃x�W�G�l���v�Z����
double calcBezier(double t, double v0, double v1, double v2, double v3) {
  return v0 * (1.0 - t) * (1.0 - t) * (1.0 - t) +
         v1 * (1.0 - t) * (1.0 - t) * t * 3.0 + v2 * (1.0 - t) * t * t * 3.0 +
         v3 * t * t * t;
}

// �x�W�G���W���v�Z����
QPointF calcBezier(double t, QPointF& v0, QPointF& v1, QPointF& v2,
                   QPointF& v3) {
  return v0 * (1.0 - t) * (1.0 - t) * (1.0 - t) +
         v1 * (1.0 - t) * (1.0 - t) * t * 3.0 + v2 * (1.0 - t) * t * t * 3.0 +
         v3 * t * t * t;
}

// ���x�W�G�Ȑ������
void makeBezierPoints(QVector3D* vertexP, QPointF p0, QPointF p1, QPointF p2,
                      QPointF p3, int precision) {
  double ratioStep = 1.0 / (double)precision;
  double tmpR      = 0.0;
  for (int p = 0; p < precision; p++) {
    // ���j�A��Ԃ������W���擾
    QPointF tmpPoint = p0 * (1.0 - tmpR) * (1.0 - tmpR) * (1.0 - tmpR) +
                       p1 * (1.0 - tmpR) * (1.0 - tmpR) * tmpR * 3.0 +
                       p2 * (1.0 - tmpR) * tmpR * tmpR * 3.0 +
                       p3 * tmpR * tmpR * tmpR;
    // �l���i�[�A�|�C���^��i�߂�
    *vertexP = QVector3D(tmpPoint.x(), tmpPoint.y(), 0.0);
    vertexP++;

    // ratio���C���N�������g
    tmpR += ratioStep;
  }
}

// �x�W�G�̃o�E���f�B���O�{�b�N�X���v�Z����
QRectF getBezierBBox(QPointF p0, QPointF p1, QPointF p2, QPointF p3) {
  QPointF topLeft, bottomRight;
  // double left, right, top, bottom;
  for (int xy = 0; xy < 2; xy++) {
    double v0, v1, v2, v3;
    // X����
    if (xy == 0) {
      v0 = p0.x();
      v1 = p1.x();
      v2 = p2.x();
      v3 = p3.x();
    } else {
      v0 = p0.y();
      v1 = p1.y();
      v2 = p2.y();
      v3 = p3.y();
    }

    double a, b, c;

    std::vector<double> valVec;

    a = v3 - 3.0 * v2 + 3.0 * v1 - v0;
    b = (v0 - v1) * v3 + v2 * v2 + (-v1 - v0) * v2 + v1 * v1;
    c = v2 - 2.0 * v1 + v0;

    valVec.push_back(v0);
    valVec.push_back(v3);

    // �d���̏ꍇ
    if (a == 0.0) {
      double d = 2.0 * (v2 - 2.0 * v1 + v0);
      if (d != 0.0) {
        double t = -(v1 - v0) / d;
        if (0.0 < t && t < 1.0) {
          valVec.push_back(calcBezier(t, v0, v1, v2, v3));
        }
      }
    }
    // �������݂���ꍇ
    else if (b >= 0.0) {
      double bb = sqrt(b);
      // ���P��
      double t = (bb - c) / a;
      if (0.0 < t && t < 1.0) valVec.push_back(calcBezier(t, v0, v1, v2, v3));

      // ���Q��
      t = -(bb + c) / a;
      if (0.0 < t && t < 1.0) valVec.push_back(calcBezier(t, v0, v1, v2, v3));
    }

    // �ő�A�ŏ��l�𓾂�
    double minPos = v0;
    double maxPos = v0;
    for (int i = 0; i < valVec.size(); i++) {
      if (valVec[i] < minPos) minPos = valVec[i];
      if (valVec[i] > maxPos) maxPos = valVec[i];
    }

    if (xy == 0) {
      topLeft.setX(minPos);
      bottomRight.setX(maxPos);
    } else {
      topLeft.setY(minPos);
      bottomRight.setY(maxPos);
    }
  }

  return QRectF(topLeft, bottomRight);
}

// �ŋߖT�����Ƃ��� t �̒l�𓾂�(�͈͂�)
double getNearestPos(QPointF& p0, QPointF& p1, QPointF& p2, QPointF& p3,
                     const QPointF& pos, double& t, double rangeBefore,
                     double rangeAfter) {
  double firstDist2         = 10000.0;
  double secondDist2        = 10000.0;
  double firstNearestRatio  = 0.0;
  double secondNearestRatio = 0.0;

  int beforeIndex = (int)(rangeBefore * 100.0);
  int afterIndex  = (int)(rangeAfter * 100.0);

  // t�̊e�����_���ƂɁA�����𑪂��Ă���
  // �܂�100����
  for (int i = beforeIndex; i <= afterIndex; i++) {
    double tmpR = (double)i / 100.0;
    // ���j�A��Ԃ������W���擾
    QPointF tmpPoint = calcBezier(tmpR, p0, p1, p2, p3);
    QPointF tmpVec   = pos - tmpPoint;
    // ��_����̋����̓����v��
    double tmp_dist2 = tmpVec.x() * tmpVec.x() + tmpVec.y() * tmpVec.y();
    // �����̓�悪�Z��������L�^���X�V
    if (tmp_dist2 < firstDist2) {
      secondDist2        = firstDist2;
      secondNearestRatio = firstNearestRatio;
      firstDist2         = tmp_dist2;
      firstNearestRatio  = tmpR;
    }
    // 2�Ԗڂ����X�V�̉\��
    else if (tmp_dist2 < secondDist2) {
      secondDist2        = tmp_dist2;
      secondNearestRatio = tmpR;
    }
  }

  // �����āA���2�_���A�����ɍ��킹�Đ��`��Ԃ��Č��ʂ𓾂�
  //  Ratio���Q�ȏ㗣��Ă���ꍇ�A��ԋ߂����Ԃ�
  if (std::abs(firstNearestRatio - secondNearestRatio) > 0.019) {
    t = firstNearestRatio;
    return std::sqrt(firstDist2);
  }

  double firstDist  = std::sqrt(firstDist2);
  double secondDist = std::sqrt(secondDist2);

  // �����ɍ��킹�Đ��`���
  t = firstNearestRatio * secondDist / (firstDist + secondDist) +
      secondNearestRatio * firstDist / (firstDist + secondDist);

  if (t < rangeBefore)
    t = rangeBefore;
  else if (t > rangeAfter)
    t = rangeAfter;

  QPointF nearestPoint = calcBezier(t, p0, p1, p2, p3);
  QPointF nearestVec   = pos - nearestPoint;
  // ��_����̋�����Ԃ�
  return std::sqrt(nearestVec.x() * nearestVec.x() +
                   nearestVec.y() * nearestVec.y());
}

// �ŋߖT�����Ƃ��� t �̒l�𓾂�
double getNearestPos(QPointF& p0, QPointF& p1, QPointF& p2, QPointF& p3,
                     const QPointF& pos, double& t) {
  return getNearestPos(p0, p1, p2, p3, pos, t, 0.0, 1.0);
}

// �}�����ꂽ�n���h���̍��W�𓾂�
QPointF calcAddedHandlePos(double t, QPointF& p0, QPointF& p1, QPointF& p2) {
  return t * t * p0 + 2.0 * t * (1.0 - t) * p1 + (1.0 - t) * (1.0 - t) * p2;
}

// �x�W�G�l���ł̔����l�����߂�
double getBezierDifferntial(double t, double v0, double v1, double v2,
                            double v3) {
  return 3.0 * t * t * (v3 - v2) + 6.0 * (1.0 - t) * t * (v2 - v1) +
         3.0 * (1.0 - t) * (1.0 - t) * (v1 - v0);
}

// �x�W�G�̋�������ɗp���鐸�x�B
const double BEZ_PREC = 1000.0;

// �x�W�G�̒��������߂�(�s�N�Z������)
double getBezierLength(const QPointF& onePix, QPointF& p0, QPointF& p1,
                       QPointF& p2, QPointF& p3) {
  double amount = 0.0;
  // 1000�����ŋ��߂悤
  for (int i = 0; i <= (int)BEZ_PREC; i++) {
    double ratio = (double)i / BEZ_PREC;
    double dx    = getBezierDifferntial(ratio, p0.x(), p1.x(), p2.x(), p3.x()) /
                onePix.x();
    double dy = getBezierDifferntial(ratio, p0.y(), p1.y(), p2.y(), p3.y()) /
                onePix.y();
    double tmpLen = std::sqrt(dx * dx + dy * dy) / BEZ_PREC;
    if (i == 0 || i == (int)BEZ_PREC) tmpLen /= 2.0;
    amount += tmpLen;
  }

  return amount;
}

// �x�W�G�̒��������߂�(�s�N�Z������) (Start��End��Ratio���w��ł����)
double getBezierLength(const double start, const double end,
                       const QPointF& onePix, QPointF& p0, QPointF& p1,
                       QPointF& p2, QPointF& p3) {
  double amount     = 0.0;
  int startIndex    = (int)(start * BEZ_PREC);
  int endIndex      = (int)(end * BEZ_PREC);
  double startRatio = BEZ_PREC * start - (double)startIndex;
  double endRatio   = BEZ_PREC * end - (double)endIndex;

  for (int i = startIndex; i <= endIndex; i++) {
    double ratio = (double)i / BEZ_PREC;
    double dx    = getBezierDifferntial(ratio, p0.x(), p1.x(), p2.x(), p3.x()) /
                onePix.x();
    double dy = getBezierDifferntial(ratio, p0.y(), p1.y(), p2.y(), p3.y()) /
                onePix.y();
    double tmpLen = std::sqrt(dx * dx + dy * dy) / BEZ_PREC;
    if (i == start)
      tmpLen *= (1.0 - startRatio);
    else if (i == end)
      tmpLen *= endRatio;
    amount += tmpLen;
  }

  return amount;
}

// �s�N�Z��������x�W�G���W���v�Z�B���ӂꂽ�ꍇ��1��Ԃ�
double getBezierRatioByLength(const QPointF& onePix, double start,
                              double& remainLength, QPointF& p0, QPointF& p1,
                              QPointF& p2, QPointF& p3) {
  double amount     = 0.0;
  int startIndex    = (int)(start * BEZ_PREC);
  double startRatio = BEZ_PREC * start - (double)startIndex;

  // 1000�����ŋ��߂悤
  for (int i = startIndex; i < (int)BEZ_PREC; i++) {
    double ratio = (double)i / BEZ_PREC;
    double dx    = getBezierDifferntial(ratio, p0.x(), p1.x(), p2.x(), p3.x()) /
                onePix.x();
    double dy = getBezierDifferntial(ratio, p0.y(), p1.y(), p2.y(), p3.y()) /
                onePix.y();
    double tmpLen = std::sqrt(dx * dx + dy * dy) / BEZ_PREC;
    if (i == startIndex) tmpLen *= 1.0 - startRatio;

    // ��������
    if (remainLength <= amount + tmpLen) {
      // ���`��ԂŕԂ�
      return ratio + (1.0 / BEZ_PREC) * (remainLength - amount) / tmpLen;
    }

    amount += tmpLen;
  }

  remainLength -= amount;

  return 1.0;
}

const QColor selectedColor(128, 128, 250, 100);

};  // namespace

ShapePair::ShapePair(int frame, bool isClosed, BezierPointList bPointList,
                     CorrPointList cPointList,
                     double dstShapeOffset)  // �I�t�Z�b�g�̓f�t�H���g�O
    : m_precision(3)
    , m_isClosed(isClosed)
    , m_isExpanded({false, true})
    , m_isLocked({false, false})
    , m_isPinned({false, false})
    , m_name(tr("Shape"))
    , m_isParent(
          isClosed)  // �e�V�F�C�v�t���O�̏����l�́A���Ă��邩�ǂ����Ō��߂�
    , m_isVisible(true)
    , m_animationSmoothness(0.) {
  // �w�肵���t���[���Ɍ`��L�[������
  if (dstShapeOffset == 0.0) {
    for (int i = 0; i < 2; i++) m_formKeys[i].setKeyData(frame, bPointList);
  } else  // �I�t�Z�b�g������ꍇ�A���炵�Ċi�[
  {
    m_formKeys[0].setKeyData(frame, bPointList);
    for (int bp = 0; bp < bPointList.size(); bp++) {
      bPointList[bp].firstHandle += QPointF(dstShapeOffset, dstShapeOffset);
      bPointList[bp].pos += QPointF(dstShapeOffset, dstShapeOffset);
      bPointList[bp].secondHandle += QPointF(dstShapeOffset, dstShapeOffset);
    }
    m_formKeys[1].setKeyData(frame, bPointList);
  }
  // �x�W�G�̒��_���������f�[�^����i�[
  m_bezierPointAmount = bPointList.size();

  // �w�肵���t���[���ɑΉ��_�L�[������
  for (int i = 0; i < 2; i++) m_corrKeys[i].setKeyData(frame, cPointList);
  // �Ή��_�̐��������f�[�^����i�[
  m_corrPointAmount = cPointList.size();
}

//-----------------------------------------------------------------------------
// (�t�@�C���̃��[�h���̂ݎg��)��̃V�F�C�v�̃R���X�g���N�^
//-----------------------------------------------------------------------------
ShapePair::ShapePair()
    : m_precision(1)
    , m_isClosed(true)
    , m_isExpanded({false, true})
    , m_isLocked({false, false})
    , m_isPinned({false, false})
    , m_isParent(true)
    , m_isVisible(true)
    , m_animationSmoothness(0.) {}

//-----------------------------------------------------------------------------
// �R�s�[�R���X�g���N�^
//-----------------------------------------------------------------------------
ShapePair::ShapePair(ShapePair* src)
    : m_precision(src->getEdgeDensity())
    , m_isClosed(src->isClosed())
    , m_bezierPointAmount(src->getBezierPointAmount())
    , m_corrPointAmount(src->getCorrPointAmount())
    , m_isExpanded({false, true})
    , m_isLocked({false, false})
    , m_isPinned({false, false})
    , m_shapeTag({src->getShapeTags(0), src->getShapeTags(1)})
    , m_isParent(src->isParent())
    , m_isVisible(src->isVisible())
    , m_animationSmoothness(src->getAnimationSmoothness())
    , m_matteInfo(src->matteInfo()) {
  for (int i = 0; i < 2; i++) {
    m_formKeys[i].setData(src->getFormData(i));
    m_formKeys[i].setInterpolation(src->getFormInterpolation(i));
    m_corrKeys[i].setData(src->getCorrData(i));
    m_corrKeys[i].setInterpolation(src->getCorrInterpolation(i));
  }

  m_name = src->getName();
}

//-----------------------------------------------------------------------------

ShapePair* ShapePair::clone() { return new ShapePair(this); }

//-----------------------------------------------------------------------------
// ����t���[���ł̃x�W�G�`���Ԃ�
//-----------------------------------------------------------------------------
BezierPointList ShapePair::getBezierPointList(int frame, int fromTo) {
  return m_formKeys[fromTo].getData(frame, 0, m_animationSmoothness);
}

//-----------------------------------------------------------------------------
// ����t���[���ł̑Ή��_��Ԃ�
//-----------------------------------------------------------------------------
CorrPointList ShapePair::getCorrPointList(int frame, int fromTo) {
  return m_corrKeys[fromTo].getData(
      frame, (m_isClosed) ? m_bezierPointAmount : m_bezierPointAmount - 1);
}

double ShapePair::getBezierInterpolation(int frame, int fromTo) {
  assert(isFormKey(frame, fromTo));
  return m_formKeys[fromTo].getInterpolation().value(frame, 0.5);
}

double ShapePair::getCorrInterpolation(int frame, int fromTo) {
  assert(isCorrKey(frame, fromTo));
  return m_corrKeys[fromTo].getInterpolation().value(frame, 0.5);
}

void ShapePair::setBezierInterpolation(int frame, int fromTo, double interp) {
  assert(isFormKey(frame, fromTo));
  if (interp == 0.5)
    m_formKeys[fromTo].getInterpolation().remove(frame);
  else
    m_formKeys[fromTo].getInterpolation().insert(frame, interp);
}

void ShapePair::setCorrInterpolation(int frame, int fromTo, double interp) {
  assert(isCorrKey(frame, fromTo));
  if (interp == 0.5)
    m_corrKeys[fromTo].getInterpolation().remove(frame);
  else
    m_corrKeys[fromTo].getInterpolation().insert(frame, interp);
}

//-----------------------------------------------------------------------------
// �\���p�̒��_�������߂�B�������Ă��邩�󂢂Ă��邩�ňقȂ�
//-----------------------------------------------------------------------------
int ShapePair::getVertexAmount(IwProject* /*project*/) {
  // �x�W�G�̕����������߂�
  int bezierPrec = Preferences::instance()->getBezierActivePrecision();

  return (m_isClosed) ? (bezierPrec * m_bezierPointAmount)
                      : (bezierPrec * (m_bezierPointAmount - 1) + 1);
}

//-----------------------------------------------------------------------------
// �w�肵���t���[���̃x�W�G���_����Ԃ�
//-----------------------------------------------------------------------------
QVector3D* ShapePair::getVertexArray(int frame, int fromTo,
                                     IwProject* project) {
  PRINT_LOG("   getVertexArray frame " + std::to_string(frame) + "  start")
  BezierPointList pointList = getBezierPointList(frame, fromTo);

  // �x�W�G�̕����������߂�
  int bezierPrec = (int)Preferences::instance()->getBezierActivePrecision();

  // ���_�������߂�B�������Ă��邩�󂢂Ă��邩�ňقȂ�
  int vertSize = getVertexAmount(project);

  QVector3D* vertices = new QVector3D[vertSize];

  QVector3D* v_p = vertices;

  // �����V�F�C�v�̏ꍇ
  if (m_isClosed) {
    // �e�|�C���g�ɂ���
    for (int p = 0; p < m_bezierPointAmount; p++) {
      // �x�W�G�̂S�̃R���g���[���|�C���g�𓾂�
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = (p == m_bezierPointAmount - 1)
                                   ? pointList.at(0)
                                   : pointList.at(p + 1);

      // �Q�̃n���h�������O�̂Ƃ��A���j�A�Ōq��
      if (startPoint.pos == startPoint.secondHandle &&
          endPoint.pos == endPoint.firstHandle) {
        makeLinearPoints(v_p, startPoint.pos, endPoint.pos, bezierPrec);
        v_p += bezierPrec;
      }
      // �S�̃R���g���[���|�C���g����A�e�x�W�G�̓_�𕪊������i�[����
      else {
        makeBezierPoints(v_p, startPoint.pos, startPoint.secondHandle,
                         endPoint.firstHandle, endPoint.pos, bezierPrec);
        v_p += bezierPrec;
      }
    }
  }
  // �J�����V�F�C�v�̏ꍇ
  else {
    // �e�|�C���g�ɂ���
    for (int p = 0; p < m_bezierPointAmount - 1; p++) {
      // �x�W�G�̂S�̃R���g���[���|�C���g�𓾂�
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = pointList.at(p + 1);

      // �Q�̃n���h�������O�̂Ƃ��A���j�A�Ōq��
      if (startPoint.pos == startPoint.secondHandle &&
          endPoint.pos == endPoint.firstHandle) {
        makeLinearPoints(v_p, startPoint.pos, endPoint.pos, bezierPrec);
        v_p += bezierPrec;
      }
      // �S�̃R���g���[���|�C���g����A�e�x�W�G�̓_�𕪊������i�[����
      else {
        makeBezierPoints(v_p, startPoint.pos, startPoint.secondHandle,
                         endPoint.firstHandle, endPoint.pos, bezierPrec);
        v_p += bezierPrec;
      }
    }
    // �Ō�ɒ[�_�̍��W��ǉ�
    BezierPoint lastPoint = pointList.at(pointList.size() - 1);
    *v_p = QVector3D(lastPoint.pos.x(), lastPoint.pos.y(), 0.0);
  }
  PRINT_LOG("   getVertexArray frame " + std::to_string(frame) + "  end")

  return vertices;
}

//-----------------------------------------------------------------------------
// �V�F�C�v�̃o�E���f�B���O�{�b�N�X��Ԃ�
//-----------------------------------------------------------------------------
QRectF ShapePair::getBBox(int frame, int fromTo) {
  BezierPointList pointList = getBezierPointList(frame, fromTo);

  // �e�Z�O�����g�̃o�E���f�B���O�{�b�N�X���������Ă���
  QRectF resultRect = QRectF(pointList.at(0).pos, pointList.at(0).pos);

  // �����V�F�C�v�̏ꍇ
  if (m_isClosed) {
    // �e�|�C���g�ɂ���
    for (int p = 0; p < m_bezierPointAmount; p++) {
      // �x�W�G�̂S�̃R���g���[���|�C���g�𓾂�
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = (p == m_bezierPointAmount - 1)
                                   ? pointList.at(0)
                                   : pointList.at(p + 1);

      // �Q�̃n���h�������O�̂Ƃ��A���j�A�Ȃ̂ŁA�[�_���G�b�W�ɂȂ�
      if (startPoint.pos == startPoint.secondHandle &&
          endPoint.pos == endPoint.firstHandle) {
        QRectF tmpRect(startPoint.pos, endPoint.pos);
        tmpRect    = tmpRect.normalized();
        resultRect = resultRect.united(tmpRect);
      }
      // �S�̃R���g���[���|�C���g����A�e�x�W�G�̃o�E���f�B���O�{�b�N�X�𑫂�����
      else {
        resultRect = resultRect.united(
            getBezierBBox(startPoint.pos, startPoint.secondHandle,
                          endPoint.firstHandle, endPoint.pos));
      }
    }
  }
  // �J�����V�F�C�v�̏ꍇ
  else {
    // �e�|�C���g�ɂ���
    for (int p = 0; p < m_bezierPointAmount - 1; p++) {
      // �x�W�G�̂S�̃R���g���[���|�C���g�𓾂�
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = pointList.at(p + 1);

      // �Q�̃n���h�������O�̂Ƃ��A���j�A�Ȃ̂ŁA�[�_���G�b�W�ɂȂ�
      if (startPoint.pos == startPoint.secondHandle &&
          endPoint.pos == endPoint.firstHandle) {
        QRectF tmpRect(startPoint.pos, endPoint.pos);
        tmpRect    = tmpRect.normalized();
        resultRect = resultRect.united(tmpRect);
      }
      // �S�̃R���g���[���|�C���g����A�e�x�W�G�̃o�E���f�B���O�{�b�N�X�𑫂�����
      else {
        resultRect = resultRect.united(
            getBezierBBox(startPoint.pos, startPoint.secondHandle,
                          endPoint.firstHandle, endPoint.pos));
      }
    }
  }

  return resultRect;
}

//-----------------------------------------------------------------------------
// �`��L�[�t���[���̃Z�b�g
//-----------------------------------------------------------------------------
void ShapePair::setFormKey(int frame, int fromTo, BezierPointList bPointList) {
  m_formKeys[fromTo].setKeyData(frame, bPointList);
}

//-----------------------------------------------------------------------------
// �`��L�[�t���[������������
// enableRemoveLastKey(�f�t�H���gfalse)��true�̏ꍇ�A�Ō��1�̃L�[�t���[���������ɂ���B
// ���̂܂܂ł͗�����̂ŁA���ƂŊm���ɉ��Ƃ����邱�ƁI�I
//-----------------------------------------------------------------------------
void ShapePair::removeFormKey(int frame, int fromTo, bool enableRemoveLastKey) {
  // frame �� �L�[�t���[������Ȃ����return
  if (!isFormKey(frame, fromTo)) return;
  // �L�[�t���[���̐����c��P�Ȃ�return
  if (m_formKeys[fromTo].getKeyCount() == 1 && !enableRemoveLastKey) return;

  // ����
  m_formKeys[fromTo].removeKeyData(frame);
}

//-----------------------------------------------------------------------------
// �Ή��_�L�[�t���[���̃Z�b�g
//-----------------------------------------------------------------------------

void ShapePair::setCorrKey(int frame, int fromTo, CorrPointList cPointList) {
  m_corrKeys[fromTo].setKeyData(frame, cPointList);
}

//-----------------------------------------------------------------------------
// �Ή��_�L�[�t���[������������
// enableRemoveLastKey(�f�t�H���gfalse)��true�̏ꍇ�A�Ō��1�̃L�[�t���[���������ɂ���B
// ���̂܂܂ł͗�����̂ŁA���ƂŊm���ɉ��Ƃ����邱�ƁI�I
//-----------------------------------------------------------------------------
void ShapePair::removeCorrKey(int frame, int fromTo, bool enableRemoveLastKey) {
  // frame �� �L�[�t���[������Ȃ����return
  if (!isCorrKey(frame, fromTo)) return;
  // �L�[�t���[���̐����c��P�Ȃ�return
  if (m_corrKeys[fromTo].getKeyCount() == 1 && !enableRemoveLastKey) return;

  // ����
  m_corrKeys[fromTo].removeKeyData(frame);
}

//-----------------------------------------------------------------------------
// �w��ʒu�ɃR���g���[���|�C���g��ǉ�����
// �R���g���[���|�C���g�̃C���f�b�N�X��Ԃ�
//-----------------------------------------------------------------------------
int ShapePair::addControlPoint(const QPointF& pos, int frame, int fromTo) {
  // �|�C���g���X�g�𓾂�
  BezierPointList pointList = getBezierPointList(frame, fromTo);
  // �e�Z�O�����g�ɂ��āA�N���b�N���ꂽ�ʒu�ƃZ�O�����g�Ƃ̍ŋߖT���������߂āA
  // �ǂ��̃Z�O�����g�Ƀ|�C���g��ǉ�����̂����ׂ�

  // �}�E�X�ʒu�ɍŋߖT�̃x�W�G���W�𓾂�
  double nearestBezierPos = getNearestBezierPos(pos, frame, fromTo);

  int nearestSegmentIndex = (int)nearestBezierPos;
  double nearestRatio     = nearestBezierPos - (double)nearestSegmentIndex;

  for (int ft = 0; ft < 2; ft++) {
    QMap<int, BezierPointList> formData = m_formKeys[ft].getData();
    // �`��f�[�^�̊e�L�[�t���[���ɂ���
    QMap<int, BezierPointList>::const_iterator i = formData.constBegin();
    while (i != formData.constEnd()) {
      int kf                 = i.key();
      BezierPointList bPList = i.value();

      // �O��̓_�𓾂�
      BezierPoint Point0 = bPList[nearestSegmentIndex];
      BezierPoint Point1 =
          (m_isClosed && nearestSegmentIndex == m_bezierPointAmount - 1)
              ? bPList[0]
              : bPList[nearestSegmentIndex + 1];

      BezierPoint newPoint;

      // �e�_���X�V���Ă���
      newPoint.pos = calcBezier(nearestRatio, Point0.pos, Point0.secondHandle,
                                Point1.firstHandle, Point1.pos);
      // �}�����ꂽ�n���h���̍��W�𓾂�
      newPoint.firstHandle =
          calcAddedHandlePos(1.0 - nearestRatio, Point0.pos,
                             Point0.secondHandle, Point1.firstHandle);
      newPoint.secondHandle =
          calcAddedHandlePos(1.0 - nearestRatio, Point0.secondHandle,
                             Point1.firstHandle, Point1.pos);

      // �O�̃R���g���[���|�C���g��secondHandle��ҏW
      QPointF tmpVec      = Point0.secondHandle - Point0.pos;
      Point0.secondHandle = Point0.pos + tmpVec * nearestRatio;

      tmpVec             = Point1.firstHandle - Point1.pos;
      Point1.firstHandle = Point1.pos + tmpVec * (1.0 - nearestRatio);

      // �O��_�̍X�V
      bPList.replace(nearestSegmentIndex, Point0);
      if (m_isClosed && nearestSegmentIndex == m_bezierPointAmount - 1)
        bPList.replace(0, Point1);
      else
        bPList.replace(nearestSegmentIndex + 1, Point1);

      // �V�����_�̑}��
      bPList.insert(nearestSegmentIndex + 1, newPoint);

      // �L�[�t���[�������X�V
      m_formKeys[ft].setKeyData(kf, bPList);

      ++i;
    }

    // �����āA�Ή��_��ǉ����鏈��������
    QMap<int, CorrPointList> corrData = m_corrKeys[ft].getData();
    // �Ή��_�f�[�^�̊e�L�[�t���[���ɂ���
    QMap<int, CorrPointList>::const_iterator c = corrData.constBegin();
    while (c != corrData.constEnd()) {
      int kf = c.key();

      // �eCorr�f�[�^�̍X�V
      CorrPointList cPList = c.value();

      for (int p = 0; p < cPList.size(); p++) {
        // nearestSegmentIndex��菬���������烀�V
        if (cPList.at(p).value < (double)nearestSegmentIndex) continue;
        // nearestSegmentIndex+1���傫��������P�ɒl���{�P����
        if (cPList.at(p).value > (double)nearestSegmentIndex + 1.0) {
          cPList[p].value += 1.0;
          continue;
        }

        // ����ȊO�̏ꍇ
        double oldCorr = cPList.at(p).value;

        double oldCorrRatio = oldCorr - (double)nearestSegmentIndex;

        // �V�����R���g���[���|�C���g���O��Corr�_�̏ꍇ
        if (oldCorrRatio < nearestRatio)
          cPList[p].value =
              oldCorrRatio / nearestRatio + (double)nearestSegmentIndex;
        // �V�����R���g���[���|�C���g�����Corr�_�̏ꍇ
        else
          cPList[p].value =
              1.0 + (double)nearestSegmentIndex +
              (oldCorrRatio - nearestRatio) / (1.0 - nearestRatio);
      }

      // �L�[�t���[�������X�V
      m_corrKeys[ft].setKeyData(kf, cPList);

      ++c;
    }
  }

  // �x�W�G�|�C���g�̒ǉ�
  m_bezierPointAmount++;

  return nearestSegmentIndex + 1;
}

//-----------------------------------------------------------------------------
// �w��ʒu�ɑΉ��_��ǉ����� ���܂���������true��Ԃ�
//-----------------------------------------------------------------------------

bool ShapePair::addCorrPoint(const QPointF& pos, int frame, int fromTo) {
  // ���̑Ή��_
  CorrPointList orgCorrs = getCorrPointList(frame, fromTo);

  // �}�E�X�ʒu�ɍŋߖT�̃x�W�G���W�𓾂�
  double nearestBezierPos = getNearestBezierPos(pos, frame, fromTo);

  // �ǂ��̑Ή��_�̊Ԃɑ}������邩���ׂ�
  int neighbourIndex = 0;
  double minDiff     = 100.0;
  for (int c = 0; c < m_corrPointAmount; c++) {
    double tmpDiff = nearestBezierPos - orgCorrs.at(c).value;
    if (abs(minDiff) > abs(tmpDiff)) {
      minDiff        = tmpDiff;
      neighbourIndex = c;
    }
  }
  // �O��̑Ή��_�C���f�b�N�X
  int beforeIndex, afterIndex;
  if (minDiff > 0)  // �ߖT�_�������ɂ���ꍇ
  {
    beforeIndex = neighbourIndex;
    afterIndex =
        (neighbourIndex == m_corrPointAmount - 1) ? 0 : neighbourIndex + 1;
  } else  // �ߖT�_���E���ɂ���ꍇ
  {
    beforeIndex =
        (neighbourIndex == 0) ? m_corrPointAmount - 1 : neighbourIndex - 1;
    afterIndex = neighbourIndex;
  }
  // �ʒu�̒��� �O��̓_����0.05������
  double beforeBPos = orgCorrs.at(beforeIndex).value;
  if (m_isClosed && beforeBPos > nearestBezierPos)
    beforeBPos -= (double)m_bezierPointAmount;
  double afterBPos = orgCorrs.at(afterIndex).value;
  if (m_isClosed && afterBPos < nearestBezierPos)
    afterBPos += (double)m_bezierPointAmount;
  std::cout << "insert corr point between " << beforeIndex << "(" << beforeBPos
            << ") and " << afterIndex << "(" << afterBPos << ")." << std::endl;
  // �O��̓_�̊Ԋu��0.1�����̏ꍇ�A�A���[�g���o����return
  if (afterBPos - beforeBPos < 0.1) {
    QMessageBox::warning(
        0, "IwaWarper",
        "Cannot add a correspondence point too close to existing ones.",
        QMessageBox::Ok, QMessageBox::Ok);
    return false;
  }
  // Corr�_��ǉ����� �������A���̃t���[�����L�[�t���[���ɂȂ�킯�ł͂Ȃ��B
  double ratio = (nearestBezierPos - beforeBPos) / (afterBPos - beforeBPos);

  // �Ή��_�̑}��
  //  Shape�Q�ɑ΂��čs��
  for (int ft = 0; ft < 2; ft++) {
    QMap<int, CorrPointList> corrKeysMap = getCorrData(ft);
    // �e�L�[�t���[���f�[�^�ɑΉ��_��}������
    QMap<int, CorrPointList>::iterator i = corrKeysMap.begin();
    while (i != corrKeysMap.end()) {
      CorrPointList cpList = i.value();

      double before = cpList.at(beforeIndex).value;
      double after  = cpList.at(afterIndex).value;
      if (isClosed() && after < before) after += (double)getBezierPointAmount();

      double insertedPos = before + (after - before) * ratio;
      if (isClosed() && insertedPos > (double)getBezierPointAmount())
        insertedPos -= (double)getBezierPointAmount();

      double beforeWeight = cpList.at(beforeIndex).weight;
      double afterWeight  = cpList.at(afterIndex).weight;
      double insertedWeight =
          beforeWeight + (afterWeight - beforeWeight) * ratio;
      // ���X�g�ɓ_��}��
      cpList.insert(beforeIndex + 1, {insertedPos, insertedWeight});
      // insert�R�}���h�́A���ɓ���Key�Ƀf�[�^���L�����ꍇ�A�u����������
      corrKeysMap.insert(i.key(), cpList);
      ++i;
    }
    // �f�[�^��߂� �Ή��_�J�E���g�͒��ōX�V���Ă���
    setCorrData(corrKeysMap, ft);
  }

  return true;
}

//--------------------------------------------------------
// �w��C���f�b�N�X�̑Ή��_���������� ���܂���������true��Ԃ�
//--------------------------------------------------------

bool ShapePair::deleteCorrPoint(int index) {
  if (index < 0 || index >= getCorrPointAmount()) return false;
  // Shape�Q�ɑ΂��čs��
  for (int ft = 0; ft < 2; ft++) {
    QMap<int, CorrPointList> corrKeysMap = getCorrData(ft);
    // �e�L�[�t���[���f�[�^����Ή��_����������
    QMap<int, CorrPointList>::iterator i = corrKeysMap.begin();
    while (i != corrKeysMap.end()) {
      CorrPointList cpList = i.value();
      cpList.removeAt(index);
      // insert�R�}���h�́A���ɓ���Key�Ƀf�[�^���L�����ꍇ�A�u����������
      corrKeysMap.insert(i.key(), cpList);
      ++i;
    }
    // �f�[�^��߂� �Ή��_�J�E���g�͒��ōX�V���Ă���
    setCorrData(corrKeysMap, ft);
  }

  return true;
}

//--------------------------------------------------------
// �}�E�X�ʒu�ɍŋߖT�̃x�W�G���W�𓾂�
//--------------------------------------------------------

double ShapePair::getNearestBezierPos(const QPointF& pos, int frame, int fromTo,
                                      double& dist) {
  // �|�C���g���X�g�𓾂�
  BezierPointList pointList = getBezierPointList(frame, fromTo);

  int nearestSegmentIndex = 0;
  double nearestRatio     = 0.;

  double tmpDist = 10000.0;

  // �����V�F�C�v�̏ꍇ
  if (m_isClosed) {
    // �e�|�C���g�ɂ���
    for (int p = 0; p < m_bezierPointAmount; p++) {
      // �x�W�G�̂S�̃R���g���[���|�C���g�𓾂�
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = (p == m_bezierPointAmount - 1)
                                   ? pointList.at(0)
                                   : pointList.at(p + 1);

      // �ŋߖT�����Ƃ��� t �̒l�𓾂�
      double t;
      double pDist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                                   endPoint.firstHandle, endPoint.pos, pos, t);
      // ����܂łň�ԋ߂�������A�l���X�V
      if (pDist < tmpDist) {
        tmpDist             = pDist;
        nearestSegmentIndex = p;
        nearestRatio        = t;
      }
    }
  }
  // �J�����V�F�C�v�̏ꍇ
  else {
    // �e�|�C���g�ɂ���
    for (int p = 0; p < m_bezierPointAmount - 1; p++) {
      // �x�W�G�̂S�̃R���g���[���|�C���g�𓾂�
      BezierPoint startPoint = pointList.at(p);
      BezierPoint endPoint   = pointList.at(p + 1);

      // �ŋߖT�����Ƃ��� t �̒l�𓾂�
      double t;
      double pDist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                                   endPoint.firstHandle, endPoint.pos, pos, t);
      // ����܂łň�ԋ߂�������A�l���X�V
      if (pDist < tmpDist) {
        tmpDist             = pDist;
        nearestSegmentIndex = p;
        nearestRatio        = t;
      }
    }
  }
  dist = tmpDist;

  return (double)nearestSegmentIndex + nearestRatio;
}

double ShapePair::getNearestBezierPos(const QPointF& pos, int frame,
                                      int fromTo) {
  double dist;
  return getNearestBezierPos(pos, frame, fromTo, dist);
}

//--------------------------------------------------------
// �}�E�X�ʒu�ɍŋߖT�̃x�W�G���W�𓾂�(�͈͂���)
//--------------------------------------------------------
double ShapePair::getNearestBezierPos(const QPointF& pos, int frame, int fromTo,
                                      const double rangeBefore,
                                      const double rangeAfter,
                                      double& nearestDist) {
  // �|�C���g���X�g�𓾂�
  BezierPointList pointList = getBezierPointList(frame, fromTo);

  int nearestSegmentIndex;
  double nearestRatio = 0.;

  double tmpDist = 10000.0;

  int beforeIndex    = (int)rangeBefore;
  double beforeRatio = rangeBefore - (double)beforeIndex;
  int afterIndex     = (int)rangeAfter;
  double afterRatio  = rangeAfter - (double)afterIndex;
  if (!isClosed() && afterIndex == pointList.size() - 1) {
    afterIndex--;
    afterRatio = 1.0;
  }

  // �����V�F�C�v ���� �O���܂����ł��� �ꍇ
  if (m_isClosed && (rangeBefore > rangeAfter)) {
    // �Q�ɕ�����
    double beforeDist;
    double nearestPosBefore =
        getNearestBezierPos(pos, frame, fromTo, rangeBefore,
                            (double)m_bezierPointAmount - 0.0001, beforeDist);

    double afterDist;
    double nearestPosAfter =
        getNearestBezierPos(pos, frame, fromTo, 0.0, rangeAfter, afterDist);

    // �߂������`���C�X
    if (beforeDist < afterDist) {
      tmpDist             = beforeDist;
      nearestSegmentIndex = (int)nearestPosBefore;
      nearestRatio        = nearestPosBefore - (double)nearestSegmentIndex;
    } else {
      tmpDist             = afterDist;
      nearestSegmentIndex = (int)nearestPosAfter;
      nearestRatio        = nearestPosAfter - (double)nearestSegmentIndex;
    }
  }
  // �����ɕ���ł���ꍇ
  else {
    // �����Z�O�����g�ɂ���ꍇ
    if (beforeIndex == afterIndex) {
      // �x�W�G�̂S�̃R���g���[���|�C���g�𓾂�
      BezierPoint startPoint = pointList.at(beforeIndex);
      BezierPoint endPoint =
          (m_isClosed && beforeIndex == m_bezierPointAmount - 1)
              ? pointList.at(0)
              : pointList.at(beforeIndex + 1);
      double t;
      // �͈͂��ŋߖT�_��T��
      double dist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                                  endPoint.firstHandle, endPoint.pos, pos, t,
                                  beforeRatio, afterRatio);
      tmpDist     = dist;
      nearestSegmentIndex = beforeIndex;
      nearestRatio        = t;
    } else {
      nearestSegmentIndex = beforeIndex;
      // �e�|�C���g�ɂ���
      for (int p = beforeIndex; p <= afterIndex; p++) {
        // �x�W�G�̂S�̃R���g���[���|�C���g�𓾂�
        BezierPoint startPoint = pointList.at(p);
        BezierPoint endPoint   = (m_isClosed && p == m_bezierPointAmount - 1)
                                     ? pointList.at(0)
                                     : pointList.at(p + 1);

        // �ŋߖT�����Ƃ��� t �̒l�𓾂�
        double t;
        double dist;
        // �ŏ��̓_
        if (p == beforeIndex) {
          // �͈͂��ŋߖT�_��T��
          dist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                               endPoint.firstHandle, endPoint.pos, pos, t,
                               beforeRatio, 1.0);
        }
        // ��̓_
        else if (p == afterIndex) {
          // �͈͂��ŋߖT�_��T��
          dist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                               endPoint.firstHandle, endPoint.pos, pos, t, 0.0,
                               afterRatio);
        } else {
          dist = getNearestPos(startPoint.pos, startPoint.secondHandle,
                               endPoint.firstHandle, endPoint.pos, pos, t);
        }
        // ����܂łň�ԋ߂�������A�l���X�V
        if (dist < tmpDist) {
          tmpDist             = dist;
          nearestSegmentIndex = p;
          nearestRatio        = t;
        }
      }
    }
  }

  nearestDist = tmpDist;

  return (double)nearestSegmentIndex + nearestRatio;
}

//--------------------------------------------------------
// ���[�N�G���A�̃��T�C�Y�ɍ��킹�t���ŃX�P�[�����ăV�F�C�v�̃T�C�Y���ێ�����
//--------------------------------------------------------
void ShapePair::counterResize(QSizeF workAreaScale) {
  auto c_scale = [&](QPointF pos) {
    return QPointF(0.5 + (pos.x() - 0.5) / workAreaScale.width(),
                   0.5 + (pos.y() - 0.5) / workAreaScale.height());
  };

  for (int ft = 0; ft < 2; ft++) {
    QMap<int, BezierPointList> formData = m_formKeys[ft].getData();
    // �`��f�[�^�̊e�L�[�t���[���ɂ���
    QMap<int, BezierPointList>::const_iterator i = formData.constBegin();
    while (i != formData.constEnd()) {
      int frame              = i.key();
      BezierPointList bPList = i.value();

      for (BezierPoint& bp : bPList) {
        bp.firstHandle  = c_scale(bp.firstHandle);
        bp.pos          = c_scale(bp.pos);
        bp.secondHandle = c_scale(bp.secondHandle);
      }

      // �L�[�t���[�������X�V
      m_formKeys[ft].setKeyData(frame, bPList);

      ++i;
    }
  }
}

//--------------------------------------------------------
// �`��f�[�^��Ԃ�
//--------------------------------------------------------
QMap<int, BezierPointList> ShapePair::getFormData(int fromTo) {
  return m_formKeys[fromTo].getData();
}
QMap<int, double> ShapePair::getFormInterpolation(int fromTo) {
  return m_formKeys[fromTo].getInterpolation();
}

//--------------------------------------------------------
// �`��f�[�^���Z�b�g
//--------------------------------------------------------
void ShapePair::setFormData(QMap<int, BezierPointList> data, int fromTo) {
  m_formKeys[fromTo].setData(data);
  // �`��_�̐����X�V
  m_bezierPointAmount = data.values().first().size();
}

//--------------------------------------------------------
// �Ή��_�f�[�^��Ԃ�
//--------------------------------------------------------
QMap<int, CorrPointList> ShapePair::getCorrData(int fromTo) {
  return m_corrKeys[fromTo].getData();
}
QMap<int, double> ShapePair::getCorrInterpolation(int fromTo) {
  return m_corrKeys[fromTo].getInterpolation();
}

//--------------------------------------------------------
// �Ή��_�f�[�^���Z�b�g
//--------------------------------------------------------
void ShapePair::setCorrData(QMap<int, CorrPointList> data, int fromTo) {
  m_corrKeys[fromTo].setData(data);
  // �Ή��_�̐����X�V
  m_corrPointAmount = data.values().first().size();
}

//--------------------------------------------------------
// �Ή��_�̍��W���X�g�𓾂�
//--------------------------------------------------------
QList<QPointF> ShapePair::getCorrPointPositions(int frame, int fromTo) {
  // ���݂̌`��f�[�^�𓾂�
  BezierPointList bPList = getBezierPointList(frame, fromTo);
  // ���݂̑Ή��_�f�[�^�𓾂�
  CorrPointList cPList = getCorrPointList(frame, fromTo);

  QList<QPointF> points;

  for (int c = 0; c < cPList.size(); c++) {
    double cPValue = cPList.at(c).value;

    int segmentIndex = (int)cPValue;
    double ratio     = cPValue - (double)segmentIndex;

    BezierPoint Point1 = bPList.at(segmentIndex);
    BezierPoint Point2 = bPList.at(
        (segmentIndex == m_bezierPointAmount - 1) ? 0 : segmentIndex + 1);

    points.push_back(calcBezier(ratio, Point1.pos, Point1.secondHandle,
                                Point2.firstHandle, Point2.pos));
  }

  return points;
}

//--------------------------------------------------------
// �Ή��_�̃E�F�C�g�̃��X�g�𓾂�
//--------------------------------------------------------
QList<double> ShapePair::getCorrPointWeights(int frame, int fromTo) {
  // ���݂̑Ή��_�f�[�^�𓾂�
  CorrPointList cPList = getCorrPointList(frame, fromTo);
  QList<double> weights;
  for (auto cp : cPList) {
    weights.push_back(cp.weight);
  }
  return weights;
}

//--------------------------------------------------------
// ���݂̃t���[����Correnspondence(�Ή��_)�Ԃ𕪊������_�̕����l�𓾂�
// ���j�F�ł��邾�����Ԋu�ɕ����������B
// �� �Ή��_�̃E�F�C�g�ɉ����ĕ����_���΂点��
//--------------------------------------------------------
QList<double> ShapePair::getDiscreteCorrValues(const QPointF& onePix, int frame,
                                               int fromTo) {
  QMutexLocker lock(&mutex_);
  BezierPointList bPList = getBezierPointList(frame, fromTo);
  // ���ʂ����߂���
  QList<double> discreteCorrValues;

  // TODO: �Ή��_�̏��Ԃ����]���Ă���΂������l��?

  CorrPointList cPList = getCorrPointList(frame, fromTo);

  int corrSegAmount = (m_isClosed) ? m_corrPointAmount : m_corrPointAmount - 1;
  for (int c = 0; c < corrSegAmount; c++) {
    double Corr1 = cPList.at(c).value;
    double Corr2 =
        cPList.at((m_isClosed && c == m_corrPointAmount - 1) ? 0 : c + 1).value;

    double weight1 = cPList.at(c).weight;
    double weight2 =
        cPList.at((m_isClosed && c == m_corrPointAmount - 1) ? 0 : c + 1)
            .weight;

    // �Ή��_�Ԃ̃s�N�Z�������v�Z����
    double corrSegLength =
        getBezierLengthFromValueRange(onePix, frame, fromTo, Corr1, Corr2);

    double pre_divideLength = 0.0;  // corrSegLength / (double)m_precision;

    double currentCorrPos = Corr1;

    // �����_���Ƀx�W�G���W�_��ǉ����Ă���
    for (int p = 0; p < m_precision; p++) {
      // �i�[
      discreteCorrValues.push_back(currentCorrPos);

      // �Ō�̓_���i�[�����炨���܂�
      if (p == m_precision - 1) break;

      double t = (double)(p + 1) / (double)m_precision;
      // �Ή��_�𕪊�����s�N�Z������
      double divideLength =
          corrSegLength * (t * weight2) / (t * (weight2 - weight1) + weight1);
      double remainLength = divideLength - pre_divideLength;
      pre_divideLength    = divideLength;

      while (1) {
        int currentIndex    = (int)currentCorrPos;
        double currentRatio = currentCorrPos - (double)currentIndex;

        // ���݂̃x�W�G�����Ă���
        BezierPoint Point1 = bPList.at(currentIndex);
        BezierPoint Point2 =
            bPList.at((m_isClosed && currentIndex == m_bezierPointAmount - 1)
                          ? 0
                          : currentIndex + 1);

        // �s�N�Z��������x�W�G���W���v�Z�B���ӂꂽ�ꍇ��1�ȏ��Ԃ�
        //  remaimLength�������Ă���
        double newRatio = getBezierRatioByLength(
            onePix, currentRatio, remainLength, Point1.pos, Point1.secondHandle,
            Point2.firstHandle, Point2.pos);

        // ���̃Z�O�����g�ł͕����_�����܂�Ȃ������ꍇ
        if (newRatio == 1.0) {
          currentCorrPos = (double)(currentIndex + 1);
          if (m_isClosed && currentCorrPos >= m_bezierPointAmount)
            currentCorrPos -= (double)m_bezierPointAmount;
          continue;
        } else {
          // �l���i�[
          currentCorrPos = (double)currentIndex + newRatio;
          break;
        }
      }
    }
  }

  // Open�ȃV�F�C�v�̏ꍇ�́A�Ō�̒[�_��ǉ�
  if (!m_isClosed) discreteCorrValues.push_back(cPList.last().value);

  return discreteCorrValues;
}

//--------------------------------------------------------
// ���݂̃t���[����Correnspondence(�Ή��_)�Ԃ𕪊������_�̃E�F�C�g�l�𓾂�
//--------------------------------------------------------
QList<double> ShapePair::getDiscreteCorrWeights(int frame, int fromTo) {
  QMutexLocker lock(&mutex_);
  // ���ʂ����߂���
  QList<double> discreteCorrWeights;
  CorrPointList cPList = getCorrPointList(frame, fromTo);
  int corrSegAmount = (m_isClosed) ? m_corrPointAmount : m_corrPointAmount - 1;
  for (int c = 0; c < corrSegAmount; c++) {
    double Corr1 = cPList.at(c).weight;
    double Corr2 =
        cPList.at((m_isClosed && c == m_corrPointAmount - 1) ? 0 : c + 1)
            .weight;

    // �����_���ɃE�F�C�g��ǉ����Ă���
    for (int p = 0; p < m_precision; p++) {
      double ratio = (double)p / (double)m_precision;
      // �i�[
      discreteCorrWeights.push_back(Corr1 * (1 - ratio) + Corr2 * ratio);
    }
  }

  // Open�ȃV�F�C�v�̏ꍇ�́A�Ō�̒[�_��ǉ�
  if (!m_isClosed) discreteCorrWeights.push_back(cPList.last().weight);

  return discreteCorrWeights;
}

//--------------------------------------------------------
// �x�W�G�l������W�l���v�Z����
//--------------------------------------------------------
QPointF ShapePair::getBezierPosFromValue(int frame, int fromTo,
                                         double bezierValue) {
  BezierPointList bPList = getBezierPointList(frame, fromTo);
  int segmentIndex       = (int)bezierValue;
  double ratio           = bezierValue - (double)segmentIndex;

  BezierPoint Point1 = bPList.at(segmentIndex);

  // ���傤��BezierPoint�̈ʒu�Ȃ�A���̍��W��Ԃ�
  if (ratio == 0.0) return Point1.pos;

  BezierPoint Point2 =
      bPList.at((m_isClosed && segmentIndex == m_bezierPointAmount - 1)
                    ? 0
                    : segmentIndex + 1);

  return calcBezier(ratio, Point1.pos, Point1.secondHandle, Point2.firstHandle,
                    Point2.pos);
}

//--------------------------------------------------------
// �w��x�W�G�l�ł̂P�x�W�G�l������̍��W�l�̕ω��ʂ�Ԃ�
//--------------------------------------------------------
double ShapePair::getBezierSpeedAt(int frame, int fromTo, double bezierValue,
                                   double distance) {
  auto modifyBezierPos = [&](double val) {
    if (isClosed()) {
      if (val >= (double)getBezierPointAmount())
        return val - (double)getBezierPointAmount();
      else if (val < 0.0)
        return val + (double)getBezierPointAmount();
    } else {
      if (val > (double)getBezierPointAmount())
        return (double)getBezierPointAmount();
      else if (val < 0.0)
        return 0.0;
    }
    return val;
  };

  double val1 = modifyBezierPos(bezierValue);
  double val2 = modifyBezierPos(bezierValue + distance);

  double d = std::abs((isClosed()) ? distance * 2.0 : val2 - val1);

  QVector2D p1(getBezierPosFromValue(frame, fromTo, val1));
  QVector2D p2(getBezierPosFromValue(frame, fromTo, val2));
  return (p1 - p2).length() / d;
}
//--------------------------------------------------------
// �w�肵���x�W�G�l�Ԃ̃s�N�Z��������Ԃ�
//--------------------------------------------------------
double ShapePair::getBezierLengthFromValueRange(const QPointF& onePix,
                                                int frame, int fromTo,
                                                double startVal,
                                                double endVal) {
  if (startVal == endVal) return 0.0;

  BezierPointList bPList = getBezierPointList(frame, fromTo);

  // ���ꂼ�ꂪ�������Ă���Z�O�����gID
  int segId1 = (int)startVal;
  int segId2 = (int)endVal;
  // �Ή��_�̃Z�O�����g���̏����ʒu
  double ratio1 = startVal - (double)segId1;
  double ratio2 = endVal - (double)segId2;

  // �Ή��_�ԋ�����ώZ����ϐ�
  double segLength = 0.0;

  // �Ή��_��������Ă���ꍇ(closed�ȃV�F�C�v�̏ꍇ�ɂ��蓾��)
  if (endVal < startVal) {
    // �Q�ɕ����Čv�Z����
    segLength =
        getBezierLengthFromValueRange(onePix, frame, fromTo, startVal,
                                      (double)m_bezierPointAmount - 0.000001) +
        getBezierLengthFromValueRange(onePix, frame, fromTo, 0.0, endVal);
  }
  // ���ʂ̏��ԂɑΉ��_������ł���Ƃ�
  else {
    // �����Z�O�����g��̋���
    if (segId1 == segId2) {
      BezierPoint Point1 = bPList.at(segId1);
      BezierPoint Point2 = bPList.at(
          (m_isClosed && segId1 == m_bezierPointAmount - 1) ? 0 : segId1 + 1);

      segLength =
          getBezierLength(ratio1, ratio2, onePix, Point1.pos,
                          Point1.secondHandle, Point2.firstHandle, Point2.pos);
    } else {
      for (int seg = segId1; seg <= segId2; seg++) {
        // ratio2���O�Ȃ�A���̃x�W�G�͌v�Z����K�v�Ȃ�
        if (seg == segId2 && ratio2 == 0.0) continue;

        BezierPoint Point1 = bPList.at(seg);
        BezierPoint Point2 = bPList.at(
            (m_isClosed && seg == m_bezierPointAmount - 1) ? 0 : seg + 1);
        // Start���܂ރx�W�G�̋���
        if (seg == segId1)
          segLength += getBezierLength(ratio1, 1.0, onePix, Point1.pos,
                                       Point1.secondHandle, Point2.firstHandle,
                                       Point2.pos);
        // End���܂ރx�W�G�̋���
        else if (seg == segId2)
          segLength += getBezierLength(0.0, ratio2, onePix, Point1.pos,
                                       Point1.secondHandle, Point2.firstHandle,
                                       Point2.pos);
        // �S���x�W�G���܂܂�Ă��镔���̋���
        else
          segLength += getBezierLength(onePix, Point1.pos, Point1.secondHandle,
                                       Point2.firstHandle, Point2.pos);
      }
    }
  }

  return segLength;
}

//--------------------------------------------------------
// Correnspondence(�Ή��_)�Ԃ����������邩�A�̕ύX
//--------------------------------------------------------

void ShapePair::setEdgeDensity(int prec) {
  if (m_precision == prec) return;
  m_precision = prec;
}

//--------------------------------------------------------
// �V�F�C�v�̃L�[�Ԃ̕�Ԃ̓x�����A�̕ύX
//--------------------------------------------------------

void ShapePair::setAnimationSmoothness(double smoothness) {
  if (m_animationSmoothness == smoothness) return;
  m_animationSmoothness = smoothness;
}

//--------------------------------------------------------
// �`��L�[�t���[������Ԃ�
//--------------------------------------------------------

int ShapePair::getFormKeyFrameAmount(int fromTo) {
  return m_formKeys[fromTo].getKeyCount();
}

//--------------------------------------------------------
// �Ή��_�L�[�t���[������Ԃ�
//--------------------------------------------------------

int ShapePair::getCorrKeyFrameAmount(int fromTo) {
  return m_corrKeys[fromTo].getKeyCount();
}

//--------------------------------------------------------
// �\�[�g���ꂽ�`��L�[�t���[���̃��X�g��Ԃ�
//--------------------------------------------------------

QList<int> ShapePair::getFormKeyFrameList(int fromTo) {
  QList<int> frameList = m_formKeys[fromTo].getData().keys();
  std::sort(frameList.begin(), frameList.end());
  return frameList;
}

//--------------------------------------------------------
// �\�[�g���ꂽ�Ή��_�L�[�t���[���̃��X�g��Ԃ�
//--------------------------------------------------------

QList<int> ShapePair::getCorrKeyFrameList(int fromTo) {
  QList<int> frameList = m_corrKeys[fromTo].getData().keys();
  std::sort(frameList.begin(), frameList.end());
  return frameList;
}

// frame �O��̃L�[�t���[���͈́i�O��̃L�[�͊܂܂Ȃ��j��Ԃ�
void ShapePair::getFormKeyRange(int& start, int& end, int frame, int fromTo) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // �O����
  start = frame;
  while (start - 1 >= 0 && !m_formKeys[fromTo].isKey(start - 1)) {
    --start;
  }
  // �����
  end = frame;
  while (end + 1 < project->getProjectFrameLength() &&
         !m_formKeys[fromTo].isKey(end + 1)) {
    ++end;
  }
}

void ShapePair::getCorrKeyRange(int& start, int& end, int frame, int fromTo) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // �O����
  start = frame;
  while (start - 1 >= 0 && !m_corrKeys[fromTo].isKey(start - 1)) {
    --start;
  }
  // �����
  end = frame;
  while (end + 1 < project->getProjectFrameLength() &&
         !m_corrKeys[fromTo].isKey(end + 1)) {
    ++end;
  }
}

//----------------------------------
// �ȉ��A�^�C�����C���ł̕\���p
//--------------------------------------------------------

// �W�J���Ă��邩�ǂ����𓥂܂��ĕ\���s����Ԃ�
int ShapePair::getRowCount() {
  // �u�V�F�C�v��S�ĕ\���v���[�h�̎�
  if (!Preferences::instance()->isShowSelectedShapesOnly()) return 5;

  int count = 1;
  for (const auto& isExp : m_isExpanded)
    if (isExp) count += 2;
  return count;
}

//--------------------------------------------------------
// �@�`��(�w�b�_����)
//--------------------------------------------------------

void ShapePair::drawTimeLineHead(QPainter& p, int& vpos, int width,
                                 int rowHeight, int& currentRow,
                                 int mouseOverRow, int mouseHPos,
                                 ShapePair* parentShape, ShapePair* nextShape,
                                 bool layerIsLocked) {
  int indent          = LayerIndent;
  int childIndent     = (m_isParent) ? 0 : indent * 2;
  bool isHighlightRow = (mouseOverRow == currentRow) && !layerIsLocked;
  bool isSelected =
      IwShapePairSelection::instance()->isSelected(OneShape(this, 0)) ||
      IwShapePairSelection::instance()->isSelected(OneShape(this, 1));
  // �܂��͎������g
  QRect rowRect(indent + childIndent, vpos, width - indent - childIndent,
                rowHeight);
  rowRect.adjust(0, 0, -1, 0);

  QColor rowColor = (m_isParent) ? QColor(50, 81, 51) : QColor(69, 93, 84);
  p.setPen(Qt::black);
  p.setBrush(rowColor);
  p.drawRect(rowRect);
  if (layerIsLocked) {
    p.fillRect(rowRect, QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
  }

  auto marginFor = [&](int height) { return (rowHeight - height) / 2; };

  // �V�F�C�v��Ƀ}�E�X����������n�C���C�g
  if (isHighlightRow) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 220, 50));

    // ��ԃR���g���[�����n�C���C�g
    if (mouseHPos > rowRect.right() - 20) {
      p.drawRect(rowRect.right() - 20, rowRect.center().y() - 8, 18, 18);
    }
    // �}�X�N���n�C���C�g
    else if (m_isParent && mouseHPos > rowRect.right() - 40) {
      p.drawRect(rowRect.right() - 40, rowRect.center().y() - 8, 18, 18);
    } else if (mouseHPos >= rowRect.left() + 23 &&
               mouseHPos < rowRect.left() + 43) {
      p.drawRect(rowRect.left() + 23, rowRect.center().y() - 8, 18, 18);
    } else {
      p.drawRect(rowRect);
    }
  }

  p.drawPixmap(rowRect.left() + 3, rowRect.center().y() - 8, 18, 18,
               m_isClosed ? QPixmap(":Resources/shape_close.svg")
                          : QPixmap(":Resources/shape_open.svg"));

  p.drawPixmap(rowRect.left() + 24, rowRect.center().y() - 7, 15, 15,
               m_isVisible ? QPixmap(":Resources/visible_on.svg")
                           : QPixmap(":Resources/visible_off.svg"));

  p.setPen((isSelected) ? Qt::yellow : Qt::white);
  p.drawText(rowRect.adjusted(45, 0, -20, 0), Qt::AlignVCenter, m_name);

  if (m_isParent) {
    p.drawPixmap(rowRect.right() - 40, rowRect.center().y() - 8, 18, 18,
                 (m_matteInfo.layerName.isEmpty())
                     ? QPixmap(":Resources/mask_off.svg")
                     : QPixmap(":Resources/mask_on.svg"));
  }

  p.drawPixmap(
      rowRect.right() - 20, rowRect.center().y() - 8, 18, 18,
      (m_animationSmoothness == 0.)   ? QPixmap(":Resources/interp_linear.svg")
      : (m_animationSmoothness == 1.) ? QPixmap(":Resources/interp_smooth.svg")
                                      : QPixmap(":Resources/interp_mixed.svg"));

  // �q�V�F�C�v�̐擪�Ɛe�V�F�C�v�����Ԑ���`��
  if (!m_isParent && parentShape) {
    QVector<QPoint> points = {
        QPoint(indent + childIndent / 2, vpos + rowHeight / 2),
        QPoint(indent + childIndent, vpos + rowHeight / 2),
    };
    // �c��
    points.append(QPoint(indent + childIndent / 2, vpos));
    if (nextShape && !nextShape->isParent())
      points.append(QPoint(indent + childIndent / 2, vpos + rowHeight));
    else
      points.append(QPoint(indent + childIndent / 2, vpos + rowHeight / 2));

    p.setPen(Qt::gray);
    p.drawLines(points);
  }

  vpos += rowHeight;
  currentRow++;

  // �L�[�t���[���̍s�w�b�_��`��
  if (Preferences::instance()->isShowSelectedShapesOnly() && !m_isExpanded[0] &&
      !m_isExpanded[1])
    return;

  QList<QString> formCorrList;
  formCorrList << tr("Form") << tr("Corr");
  QList<QString> fromToList;
  fromToList << tr("From ") << tr("To ");

  // �擪���炷
  rowRect.setLeft(CorrIndent);
  // rowRect.setLeft(indent * 4);

  bool doDrawConnectionLine =
      parentShape && nextShape && !nextShape->isParent();

  for (int fromTo = 0; fromTo < 2; fromTo++) {
    if (Preferences::instance()->isShowSelectedShapesOnly() &&
        !m_isExpanded[fromTo])
      continue;

    bool isOneShapeSelected =
        IwShapePairSelection::instance()->isSelected(OneShape(this, fromTo));
    bool isMouseOnLockButton =
        ((mouseOverRow == currentRow) || (mouseOverRow == currentRow + 1)) &&
        (mouseHPos >= LockPos && mouseHPos < PinPos) && !layerIsLocked;
    bool isMouseOnPinButton =
        ((mouseOverRow == currentRow) || (mouseOverRow == currentRow + 1)) &&
        (mouseHPos >= PinPos && mouseHPos < CorrIndent) && !layerIsLocked;

    QRect fcRowRect(rowRect);
    fcRowRect.translate(0, rowHeight);
    fcRowRect.setLeft(LockPos);
    fcRowRect.setBottom(fcRowRect.bottom() + rowHeight);
    rowColor = (fromTo == 0) ? QColor(100, 60, 50) : QColor(50, 60, 110);
    p.setPen(Qt::black);
    p.setBrush(rowColor);
    p.drawRect(fcRowRect);
    if (layerIsLocked) {
      p.fillRect(fcRowRect,
                 QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
    }
    if (m_isLocked[fromTo]) {
      QRect tmpRect(fcRowRect);
      tmpRect.setLeft(CorrIndent);
      p.fillRect(tmpRect, QBrush(QColor(128, 128, 128, 128), Qt::FDiagPattern));
    }

    // ���b�N�G���A�̕`��
    QRect lockRect(fcRowRect);
    lockRect.setRight(PinPos);
    if (isMouseOnLockButton) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(255, 255, 220, 50));
      p.drawRect(lockRect);
    }
    p.drawPixmap(lockRect.center().x() - 9, lockRect.center().y() - 9, 18, 18,
                 (m_isLocked[fromTo]) ? QPixmap(":Resources/lock_on.svg")
                                      : QPixmap(":Resources/lock.svg"));

    // �s���G���A�̕`��
    QRect pinRect(fcRowRect);
    pinRect.setLeft(PinPos);
    pinRect.setRight(CorrIndent);
    if (isMouseOnPinButton) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(255, 255, 220, 50));
      p.drawRect(pinRect);
    }
    p.drawPixmap(pinRect.center().x() - 9, pinRect.center().y() - 9, 18, 18,
                 (m_isPinned[fromTo]) ? QPixmap(":Resources/pin_on.svg")
                                      : QPixmap(":Resources/pin.svg"));

    for (int formCorr = 0; formCorr < 2; formCorr++) {
      isHighlightRow =
          (mouseOverRow == currentRow) && !layerIsLocked && !m_isLocked[fromTo];
      rowRect.translate(0, rowHeight);

      if (formCorr == 1) {
        rowColor = QColor(69, 68, 48);

        p.setPen(Qt::black);
        p.setBrush(rowColor);
        p.drawRect(rowRect);
        if (layerIsLocked) {
          p.fillRect(rowRect,
                     QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
        }
        if (m_isLocked[fromTo]) {
          p.fillRect(rowRect,
                     QBrush(QColor(128, 128, 128, 128), Qt::FDiagPattern));
        }
      }

      if (isOneShapeSelected) {
        qreal borderWidth = 4.;  // even number
        p.setPen(QPen(selectedColor, borderWidth, Qt::SolidLine, Qt::SquareCap,
                      Qt::MiterJoin));
        p.setBrush(Qt::NoBrush);
        int halfW = (int)(borderWidth / 2);
        p.drawRect(rowRect.adjusted(1 + halfW, 1 + halfW, -halfW, -halfW));
      }

      if (isHighlightRow && !isMouseOnLockButton && !isMouseOnPinButton) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 220, 50));
        p.drawRect(rowRect);
      }
      p.setPen(Qt::white);
      p.drawText(rowRect.adjusted(indent, 0, 0, 0), Qt::AlignVCenter,
                 fromToList.at(fromTo) + formCorrList.at(formCorr));

      currentRow++;
    }

    if (doDrawConnectionLine) {
      p.setPen(Qt::gray);
      p.drawLine(QPoint(indent + indent, vpos),
                 QPoint(indent + indent, vpos + rowHeight * 2));
    }

    vpos += rowHeight * 2;
  }
}

void ShapePair::drawTimeLine(QPainter& p, int& vpos, int /*width*/,
                             int fromFrame, int toFrame, int frameWidth,
                             int rowHeight, int& currentRow, int mouseOverRow,
                             double mouseOverFrameD, bool layerIsLocked,
                             bool layerIsVisibleInRender) {
  bool isHighlightRow = (mouseOverRow == currentRow) && !layerIsLocked;

  // �܂��͎������g
  QRect frameRect(0, vpos, frameWidth, rowHeight);

  QColor rowColor = (m_isParent) ? QColor(50, 81, 51) : QColor(69, 93, 84);

  IwProject* prj  = IwApp::instance()->getCurrentProject()->getProject();
  int frameLength = prj->getProjectFrameLength();

  // ���C���������_�����O�Ώۂ��ǂ���
  int targetShapeTag =
      prj->getRenderQueue()->currentOutputSettings()->getShapeTagId();
  bool showCacheState =
      m_isParent && layerIsVisibleInRender && isRenderTarget(targetShapeTag);

  // �e�t���[���ŁA���g�̂�����̂����`��
  for (int f = fromFrame; f <= toFrame; f++) {
    QRect tmpRect = frameRect.translated(frameWidth * f, 0);
    p.setPen(Qt::black);
    if (f < frameLength)
      p.setBrush(rowColor);
    else
      p.setBrush(QColor(255, 255, 255, 5));
    p.drawRect(tmpRect);

    if (isHighlightRow && (int)mouseOverFrameD == f)
      p.fillRect(tmpRect, QColor(255, 255, 220, 100));
    else if (layerIsLocked) {
      p.fillRect(tmpRect, QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
    }

    // show render cache state if the shape is render target
    if (showCacheState && f < frameLength) {
      bool isCached = IwTriangleCache::instance()->isValid(f, this);
      bool isRunning =
          IwApp::instance()->getCurrentProject()->isRunningPreview(f);
      QRect barRect = tmpRect;
      barRect.setTop(barRect.bottom() - 4);
      barRect.setBottom(barRect.bottom() - 1);
      p.fillRect(barRect, (isCached)    ? Qt::darkGreen
                          : (isRunning) ? Qt::darkYellow
                                        : Qt::darkRed);
    }
  }

  currentRow++;
  vpos += rowHeight;

  static const QPointF handlePoints[3] = {
      QPointF(0.0, 0.0),
      QPointF(-2.0, rowHeight / 2),
      QPointF(2.0, rowHeight / 2),
  };

  for (int fromTo = 0; fromTo < 2; fromTo++) {
    if (Preferences::instance()->isShowSelectedShapesOnly() &&
        !m_isExpanded[fromTo])
      continue;

    bool isSelected = false;
    int selFromTo   = 0;
    bool isForm     = true;
    if (IwApp::instance()->getCurrentSelection()->getSelection() ==
        IwTimeLineKeySelection::instance()) {
      OneShape selectedShape = IwTimeLineKeySelection::instance()->getShape();
      if (selectedShape.shapePairP == this) {
        isSelected = true;
        selFromTo  = selectedShape.fromTo;
        isForm     = IwTimeLineKeySelection::instance()->isFormKeySelected();
      }
    }

    // �L�[�t���[���́�
    QPolygon polygon;
    polygon << QPoint(5, 0) << QPoint(0, 5) << QPoint(-5, 0) << QPoint(0, -5);

    QPolygon selectionFrame;
    selectionFrame << QPoint(6, 0) << QPoint(0, 6) << QPoint(-6, 0)
                   << QPoint(0, -6);

    polygon.translate(frameWidth / 2, rowHeight / 2 + vpos);
    selectionFrame.translate(frameWidth / 2, rowHeight / 2 + vpos);

    for (int formCorr = 0; formCorr < 2; formCorr++) {
      isHighlightRow =
          (mouseOverRow == currentRow) && !layerIsLocked && !m_isLocked[fromTo];
      frameRect.translate(0, rowHeight);

      // �L�[�t���[���I������Ă��邩�ǂ���
      bool rowSelected = false;
      if (isSelected && selFromTo == fromTo && (formCorr == 0) == isForm)
        rowSelected = true;

      rowColor           = (formCorr == 1) ? QColor(69, 68, 48)
                           : (fromTo == 0) ? QColor(100, 60, 50)
                                           : QColor(50, 60, 110);
      QRect tmpFrameRect = frameRect.translated(fromFrame * frameWidth, 0);
      for (int f = fromFrame; f <= toFrame; f++) {
        if (f >= frameLength) break;

        p.setBrush(rowColor);
        p.setPen(Qt::black);
        p.drawRect(tmpFrameRect);
        if (layerIsLocked) {
          p.fillRect(tmpFrameRect,
                     QBrush(QColor(128, 128, 128, 128), Qt::BDiagPattern));
        }
        if (m_isLocked[fromTo]) {
          p.fillRect(tmpFrameRect,
                     QBrush(QColor(128, 128, 128, 128), Qt::FDiagPattern));
        }

        bool isSelectedCell = false;
        //----- �L�[�t���[���I���̂Ƃ����w�i�ɂ���
        if (rowSelected &&
            IwTimeLineKeySelection::instance()->isFrameSelected(f)) {
          p.setBrush(QColor(100, 153, 176, 128));
          p.setPen(Qt::NoPen);
          p.drawRect(tmpFrameRect);

          isSelectedCell = true;
        }
        //-----

        bool isKey;
        if (formCorr == 0)
          isKey = m_formKeys[fromTo].isKey(f);
        else
          isKey = m_corrKeys[fromTo].isKey(f);
        if (isKey) {
          if (isHighlightRow && (int)mouseOverFrameD == f)
            p.setBrush(Qt::yellow);
          else if (layerIsLocked || m_isLocked[fromTo])
            p.setBrush(Qt::lightGray);
          else
            p.setBrush(QColor(255, 128, 0, 255));

          p.setPen(Qt::black);
          p.drawPolygon(polygon.translated(f * frameWidth, 0));
        }

        if (KeyDragTool::instance()->isCurrentRow(OneShape(this, fromTo),
                                                  (formCorr == 0))) {
          if ((isSelectedCell && isKey &&
               !KeyDragTool::instance()->isDragging()) ||
              KeyDragTool::instance()->isDragDestination(f)) {
            p.setPen(Qt::white);
            p.setBrush(Qt::NoBrush);
            p.drawPolygon(selectionFrame.translated(f * frameWidth, 0));
          }
        }

        tmpFrameRect.translate(frameWidth, 0);
      }

      // �����`��
      QList<int> keyframes;
      if (formCorr == 0)
        keyframes = getFormKeyFrameList(fromTo);
      else
        keyframes = getCorrKeyFrameList(fromTo);
      // �Ō�̃L�[�̓X�L�b�v���Ă悢
      for (int kId = 0; kId < keyframes.size() - 1; kId++) {
        int curKey  = keyframes.at(kId);
        int nextKey = keyframes.at(kId + 1);
        // �͈͓����ǂ���
        if (nextKey <= fromFrame || toFrame <= curKey || nextKey == curKey + 1)
          continue;
        // �}�E�X�I�[�o�[���Ă��邩�A��������^�񒆂łȂ��Ƃ��Ƀn���h����`��
        double interp;
        if (formCorr == 0)
          interp = getBezierInterpolation(curKey, fromTo);
        else
          interp = getCorrInterpolation(curKey, fromTo);
        if (interp == 0.5 &&
            (!isHighlightRow || (int)mouseOverFrameD < curKey ||
             nextKey < (int)mouseOverFrameD))
          continue;

        // �����t���[���ɃX���C�_��`��
        p.setPen(Qt::white);
        int left     = (curKey + 1);
        int right    = nextKey;
        int lineVPos = frameRect.center().y();
        // ����
        p.drawLine(left * frameWidth, lineVPos, right * frameWidth, lineVPos);
        double handleFramePos = (double)left + interp * (double)(right - left);
        // �}�E�X���n���h����ɂ��邩�ǂ���
        if (isHighlightRow && std::abs(mouseOverFrameD - handleFramePos) < 0.1)
          p.setPen(Qt::cyan);
        else
          p.setPen(Qt::white);
        p.setBrush(Qt::NoBrush);
        // �n���h��
        p.save();
        p.translate(handleFramePos * frameWidth, lineVPos);
        p.drawPolygon(handlePoints, 3);
        p.restore();
      }

      polygon.translate(0, rowHeight);
      selectionFrame.translate(0, rowHeight);
      currentRow++;
    }
    vpos += rowHeight * 2;
  }
}

//--------------------------------------------------------
// �^�C�����C���N���b�N��
//--------------------------------------------------------
bool ShapePair::onTimeLineClick(int rowInShape, double frameD, bool ctrlPressed,
                                bool shiftPressed, Qt::MouseButton /*button*/) {
  // �ŏ��̍s�̓V�F�C�v�̍s�ŁA����͂Ƃ肠�������V
  if (rowInShape == 0) return false;
  bool fromShapeIsShown =
      (!Preferences::instance()->isShowSelectedShapesOnly() || m_isExpanded[0]);
  int fromTo  = (fromShapeIsShown && rowInShape <= 2) ? 0 : 1;
  bool isForm = (rowInShape % 2 == 1) ? true : false;
  int frame   = (double)frameD;

  if (m_isLocked[fromTo]) return false;

  IwTimeLineKeySelection::instance()->setShape(OneShape(this, fromTo), isForm);
  // �V�F�C�v���P�ƑI���ɂ���
  IwShapePairSelection::instance()->selectOneShape(OneShape(this, fromTo));

  // ��ԃn���h��������ł��邩�ǂ����̔���
  QList<int> keys;
  if (isForm)
    keys = getFormKeyFrameList(fromTo);
  else
    keys = getCorrKeyFrameList(fromTo);
  if (keys.count() >= 2 && !keys.contains(frame) && keys.first() < frame &&
      frame < keys.last()) {
    // �L�[��T��
    int prevKey = 0;
    for (auto key : keys) {
      if (key > frame) {
        if (key == prevKey + 1) break;
        double interp;
        if (isForm)
          interp = getBezierInterpolation(prevKey, fromTo);
        else
          interp = getCorrInterpolation(prevKey, fromTo);
        // �n���h���̃t���[���ʒu
        double handleFramePos =
            (double)(prevKey + 1) + interp * (double)(key - prevKey - 1);
        if (std::abs(frameD - handleFramePos) < 0.1) {
          IwTimeLineKeySelection::instance()->makeCurrent();
          InterpHandleDragTool::instance()->onClick(
              OneShape(this, fromTo), isForm, frameD - handleFramePos, prevKey,
              key);
          IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
          return true;
        }
        break;
      } else
        prevKey = key;
    }
  }

  if (!shiftPressed) {
    // �ʏ�N���b�N
    if (!ctrlPressed) {
      // �Z���̒P�ƑI���@���I���̃Z���FPress���@�I���ς̃Z���FRelease��
      if (!IwTimeLineKeySelection::instance()->isFrameSelected(frame)) {
        IwTimeLineKeySelection::instance()->selectNone();
        IwTimeLineKeySelection::instance()->selectFrame(frame);
      }
    }
    // �{Ctrl
    else {
      // ���̃Z�������I���Ȃ�ǉ� �I���ςȂ�I������͂���
      if (IwTimeLineKeySelection::instance()->isFrameSelected(frame))
        IwTimeLineKeySelection::instance()->unselectFrame(frame);
      else
        IwTimeLineKeySelection::instance()->selectFrame(frame);
    }
    IwTimeLineKeySelection::instance()->setRangeSelectionStartPos(frame);
  } else {
    IwTimeLineKeySelection::instance()->doRangeSelect(frame, ctrlPressed);
  }

  IwTimeLineKeySelection::instance()->makeCurrent();

  KeyDragTool::instance()->onClick(frame);

  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
  return true;
}

//--------------------------------------------------------
// ���N���b�N���̏��� row�͂��̃��C�����ŏォ��O�Ŏn�܂�s���B
// �ĕ`�悪�K�v�Ȃ�true��Ԃ�
//--------------------------------------------------------

bool ShapePair::onLeftClick(int row, int mouseHPos, QMouseEvent* e) {
  if (row >= getRowCount()) return false;

  int indent      = LayerIndent;
  int childIndent = (m_isParent) ? 0 : indent * 2;

  ViewSettings* vs =
      IwApp::instance()->getCurrentProject()->getProject()->getViewSettings();
  bool showAll = (!Preferences::instance()->isShowSelectedShapesOnly());

  if (row == 0) {
    // ��ԃR���g���[���̑���
    if (mouseHPos >= 180) {
      ProjectUtils::openInterpolationPopup(this, e->globalPos());
      return false;
    } else if (mouseHPos >= 160) {
      IwShapePairSelection::instance()->selectOneShape(OneShape(this, 0));
      IwShapePairSelection::instance()->addShape(OneShape(this, 1));
      ProjectUtils::openMaskInfoPopup();
      return false;
    } else if (mouseHPos >= indent + childIndent + 23 &&
               mouseHPos < indent + childIndent + 43) {
      ProjectUtils::switchShapeVisibility(this);
      return false;
    }

    bool selectionChanged = false;

    // �I������Ă��Ȃ��ꍇ�͑I���ɒǉ�
    // Selected: �s������Ă���ꍇ�̂� All:���fromTo����
    // fromTo����\���̏ꍇ���I������Ȃ��悤�ɂ���B
    if (showAll && !(e->modifiers() & Qt::ShiftModifier))
      IwShapePairSelection::instance()->selectNone();

    if ((showAll || isPinned(0)) && vs->isFromToVisible(0) &&
        !IwShapePairSelection::instance()->isSelected(OneShape(this, 0))) {
      IwShapePairSelection::instance()->addShape(OneShape(this, 0));
      selectionChanged = true;
    }
    if ((showAll || isPinned(1)) && vs->isFromToVisible(1) &&
        !IwShapePairSelection::instance()->isSelected(OneShape(this, 1))) {
      IwShapePairSelection::instance()->addShape(OneShape(this, 1));
      selectionChanged = true;
    }
    if (selectionChanged) {
      IwShapePairSelection::instance()->makeCurrent();
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
    }
    // }
    return selectionChanged;
  }
  // �L�[�t���[���������N���b�N�̏ꍇ�́AFromTo�Е��̃V�F�C�v�̂ݑI��
  else if (row < 5) {
    bool fromShapeIsShown = (showAll || m_isExpanded[0]);
    int fromTo            = (fromShapeIsShown && row <= 2) ? 0 : 1;

    if (mouseHPos >= LockPos && mouseHPos < PinPos) {
      ProjectUtils::switchLock(OneShape(this, fromTo));
      IwApp::instance()->getCurrentProject()->notifyProjectChanged();
      return true;
    }
    if (mouseHPos >= PinPos && mouseHPos < CorrIndent) {
      ProjectUtils::switchPin(OneShape(this, fromTo));
      IwApp::instance()->getCurrentProject()->notifyProjectChanged();
      return true;
    }

    if (!vs->isFromToVisible(fromTo) || m_isLocked[fromTo]) return false;
    IwShapePairSelection::instance()->makeCurrent();
    if (showAll && !(e->modifiers() & Qt::ShiftModifier))
      IwShapePairSelection::instance()->selectNone();

    // ��Q���N���b�N������FROM�A���Q�Ȃ�TO
    IwShapePairSelection::instance()->addShape(OneShape(this, fromTo));

    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
    return true;
  }
  // �ی�
  return false;
}

//--------------------------------------------------------
// From�V�F�C�v��To�V�F�C�v�ɃR�s�[
//--------------------------------------------------------

void ShapePair::copyFromShapeToToShape(double offset) {
  if (offset == 0.0) {
    m_formKeys[1] = m_formKeys[0];
    m_corrKeys[1] = m_corrKeys[0];
  } else {
    QMap<int, BezierPointList> formData = m_formKeys[0].getData();
    QMap<int, BezierPointList> newFormData;
    // �`��f�[�^�̊e�L�[�t���[���ɂ���
    QMap<int, BezierPointList>::const_iterator i = formData.constBegin();
    while (i != formData.constEnd()) {
      int frame              = i.key();
      BezierPointList bpList = i.value();
      for (int bp = 0; bp < bpList.size(); bp++) {
        bpList[bp].pos += QPointF(offset, offset);
        bpList[bp].firstHandle += QPointF(offset, offset);
        bpList[bp].secondHandle += QPointF(offset, offset);
      }
      newFormData[frame] = bpList;
      ++i;
    }

    m_formKeys[1].setData(newFormData);
    m_corrKeys[1] = m_corrKeys[0];
  }
}

bool ShapePair::isAtLeastOneShapeExpanded() {
  return !Preferences::instance()->isShowSelectedShapesOnly() ||
         m_isExpanded[0] | m_isExpanded[1];
}

//--------------------------------------------------------
// �Z�[�u/���[�h
//--------------------------------------------------------
void ShapePair::saveData(QXmlStreamWriter& writer) {
  // �V�F�C�v��
  writer.writeTextElement("name", m_name);
  // �Ή��_�Ԃ����������邩
  writer.writeTextElement("precision", QString::number(m_precision));
  // �V�F�C�v�����Ă��邩�ǂ���
  writer.writeTextElement("closed", (m_isClosed) ? "True" : "False");
  // �V�F�C�v���e���ǂ���
  writer.writeTextElement("parent", (m_isParent) ? "True" : "False");
  // tween �i�ۗ��j
  // �x�W�G�̃|�C���g��
  writer.writeTextElement("bPoints", QString::number(m_bezierPointAmount));
  // �Ή��_�̐�
  writer.writeTextElement("cPoints", QString::number(m_corrPointAmount));

  // �`��f�[�^
  writer.writeStartElement("FormKeysFrom");
  m_formKeys[0].saveData(writer);
  writer.writeEndElement();
  writer.writeStartElement("FormKeysTo");
  m_formKeys[1].saveData(writer);
  writer.writeEndElement();

  // �Ή��_�f�[�^
  writer.writeStartElement("CorrKeysFrom");
  m_corrKeys[0].saveData(writer);
  writer.writeEndElement();
  writer.writeStartElement("CorrKeysTo");
  m_corrKeys[1].saveData(writer);
  writer.writeEndElement();

  // ���b�N
  writer.writeTextElement("LockFrom", (m_isLocked[0]) ? "True" : "False");
  writer.writeTextElement("LockTo", (m_isLocked[1]) ? "True" : "False");

  // �s��
  writer.writeTextElement("PinnedFrom", (m_isPinned[0]) ? "True" : "False");
  writer.writeTextElement("PinnedTo", (m_isPinned[1]) ? "True" : "False");

  // �V�F�C�v�^�O
  auto listToString = [](const QList<int> list) {
    QString ret;
    for (auto val : list) {
      ret += QString::number(val);
      ret += ",";
    }
    if (!list.isEmpty()) ret.chop(1);
    return ret;
  };
  if (!m_shapeTag[0].isEmpty())
    writer.writeTextElement("ShapeTagFrom", listToString(m_shapeTag[0]));
  if (!m_shapeTag[1].isEmpty())
    writer.writeTextElement("ShapeTagTo", listToString(m_shapeTag[1]));

  // �V�F�C�v���\������Ă��邩�ǂ���
  writer.writeTextElement("visible", (m_isVisible) ? "True" : "False");
  // �V�F�C�v�̃L�[�Ԃ̕�Ԃ̓x����
  writer.writeTextElement("AnimationSmoothness",
                          QString::number(m_animationSmoothness));

  // �}�b�g���
  if (m_isParent) {
    writer.writeStartElement("MatteInfo");
    writer.writeTextElement("layerName", m_matteInfo.layerName);

    writer.writeStartElement("MatteColors");
    {
      for (auto color : m_matteInfo.colors) {
        writer.writeTextElement("color", color.name());
      }
    }
    writer.writeEndElement();

    writer.writeTextElement("tolerance",
                            QString::number(m_matteInfo.tolerance));
    writer.writeEndElement();
  }
}

//--------------------------------------------------------

void ShapePair::loadData(QXmlStreamReader& reader) {
  while (reader.readNextStartElement()) {
    // �V�F�C�v��
    if (reader.name() == "name") m_name = reader.readElementText();
    // �Ή��_�Ԃ����������邩
    else if (reader.name() == "precision")
      m_precision = reader.readElementText().toInt();

    // �V�F�C�v�����Ă��邩�ǂ���
    else if (reader.name() == "closed")
      m_isClosed = (reader.readElementText() == "True") ? true : false;

    // �V�F�C�v���e���ǂ���
    else if (reader.name() == "parent")
      m_isParent = (reader.readElementText() == "True") ? true : false;

    // tween �i�ۗ��j

    // �x�W�G�̃|�C���g��
    else if (reader.name() == "bPoints")
      m_bezierPointAmount = reader.readElementText().toInt();

    // �Ή��_�̐�
    else if (reader.name() == "cPoints")
      m_corrPointAmount = reader.readElementText().toInt();

    // �`��f�[�^
    else if (reader.name() == "FormKeysFrom")
      m_formKeys[0].loadData(reader);
    else if (reader.name() == "FormKeysTo")
      m_formKeys[1].loadData(reader);

    // �Ή��_�f�[�^
    else if (reader.name() == "CorrKeysFrom")
      m_corrKeys[0].loadData(reader);
    else if (reader.name() == "CorrKeysTo")
      m_corrKeys[1].loadData(reader);

    // ���b�N
    else if (reader.name() == "LockFrom")
      m_isLocked[0] = (reader.readElementText() == "True") ? true : false;
    else if (reader.name() == "LockTo")
      m_isLocked[1] = (reader.readElementText() == "True") ? true : false;

    // �s��
    else if (reader.name() == "PinnedFrom")
      m_isPinned[0] = (reader.readElementText() == "True") ? true : false;
    else if (reader.name() == "PinnedTo")
      m_isPinned[1] = (reader.readElementText() == "True") ? true : false;

    else if (reader.name() == "ShapeTagFrom") {
      QStringList list =
          reader.readElementText().split(QLatin1Char(','), Qt::SkipEmptyParts);
      m_shapeTag[0].clear();
      for (auto valStr : list) m_shapeTag[0].append(valStr.toInt());
    } else if (reader.name() == "ShapeTagTo") {
      QStringList list =
          reader.readElementText().split(QLatin1Char(','), Qt::SkipEmptyParts);
      m_shapeTag[1].clear();
      for (auto valStr : list) m_shapeTag[1].append(valStr.toInt());
    } else if (reader.name() == "visible")
      m_isVisible = (reader.readElementText() == "True") ? true : false;
    else if (reader.name() == "AnimationSmoothness")
      m_animationSmoothness = reader.readElementText().toDouble();

    // �}�b�g���
    else if (reader.name() == "MatteInfo") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "layerName")
          m_matteInfo.layerName = reader.readElementText();
        else if (reader.name() == "MatteColors") {
          m_matteInfo.colors.clear();
          while (reader.readNextStartElement()) {
            if (reader.name() == "color") {
              QColor color;
              color.setNamedColor(reader.readElementText());
              m_matteInfo.colors.append(color);
            } else
              reader.skipCurrentElement();
          }
        } else if (reader.name() == "tolerance")
          m_matteInfo.tolerance = reader.readElementText().toInt();
        else
          reader.skipCurrentElement();
      }
    }

    else
      reader.skipCurrentElement();
  }
}