//-----------------------------------------------------------------------------
// KeyContainer �N���X
//  �L�[�t���[�������f�[�^�������e���v���[�g�N���X
//	�]�V�F�C�v�������� FormKey
//	�]Correspondence�i�Ή��_�j�������� CorrKey
//-----------------------------------------------------------------------------

#include "keycontainer.h"

#include <QPair>
#include <math.h>
#include <iostream>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QStringList>
#include <QRegExp>

#include <QMatrix4x4>
#include <QVector2D>

namespace {
// ��Ԃ̌v�Z �x�W�G�̊e�_�̕��
void linearInterPolate(BezierPointList &beforeKey, BezierPointList &afterKey,
                       double afterRatio, BezierPointList &result) {
  double beforeRatio = 1.0 - afterRatio;
  // �e�x�W�G�|�C���g�ɂ���
  for (int p = 0; p < beforeKey.size(); p++) {
    BezierPoint beforePoint = beforeKey.at(p);
    BezierPoint afterPoint  = afterKey.at(p);

    BezierPoint resultPoint;

    resultPoint.pos =
        beforePoint.pos * beforeRatio + afterPoint.pos * afterRatio;
    resultPoint.firstHandle = beforePoint.firstHandle * beforeRatio +
                              afterPoint.firstHandle * afterRatio;
    resultPoint.secondHandle = beforePoint.secondHandle * beforeRatio +
                               afterPoint.secondHandle * afterRatio;

    result.push_back(resultPoint);
  }
}

// ��Ԃ̌v�Z �x�W�G�̊e�_�̕�� �X�v���C�����
void splineInterpolate(BezierPointList &key0, BezierPointList &key1,
                       BezierPointList &key2, BezierPointList &key3,
                       double afterRatio, BezierPointList &result,
                       int segmentId, double smoothness) {
  double beforeRatio = 1.0 - afterRatio;
  // �e�x�W�G�|�C���g�ɂ���
  for (int p = 0; p < key0.size(); p++) {
    BezierPoint keyP[4] = {key0.at(p), key1.at(p), key2.at(p), key3.at(p)};

    double u[4];
    u[0] = 0.;
    u[1] = u[0] + QVector2D(keyP[1].pos - keyP[0].pos).length();
    u[2] = u[1] + QVector2D(keyP[2].pos - keyP[1].pos).length();
    u[3] = u[2] + QVector2D(keyP[3].pos - keyP[2].pos).length();

    QMatrix4x4 mat(u[0] * u[0] * u[0], u[0] * u[0], u[0], 1.,
                   u[1] * u[1] * u[1], u[1] * u[1], u[1], 1.,
                   u[2] * u[2] * u[2], u[2] * u[2], u[2], 1.,
                   u[3] * u[3] * u[3], u[3] * u[3], u[3], 1.);

    bool ok;
    mat = mat.inverted(&ok);
    if (!ok) {
      BezierPoint resultPoint;
      resultPoint.pos = keyP[segmentId].pos * beforeRatio +
                        keyP[segmentId + 1].pos * afterRatio;
      resultPoint.firstHandle = keyP[segmentId].firstHandle * beforeRatio +
                                keyP[segmentId + 1].firstHandle * afterRatio;
      resultPoint.secondHandle = keyP[segmentId].secondHandle * beforeRatio +
                                 keyP[segmentId + 1].secondHandle * afterRatio;
      result.push_back(resultPoint);
      continue;
    }

    QPointF coeff[4];
    for (int i = 0; i < 4; i++) {
      coeff[i] = mat(i, 0) * keyP[0].pos + mat(i, 1) * keyP[1].pos +
                 mat(i, 2) * keyP[2].pos + mat(i, 3) * keyP[3].pos;
    }

    double before_u = u[segmentId];
    double after_u  = u[segmentId + 1];
    double cur_u    = before_u * beforeRatio + after_u * afterRatio;

    BezierPoint resultPoint;
    resultPoint.pos = QPointF(
        cur_u * cur_u * cur_u * coeff[0].x() + cur_u * cur_u * coeff[1].x() +
            cur_u * coeff[2].x() + coeff[3].x(),
        cur_u * cur_u * cur_u * coeff[0].y() + cur_u * cur_u * coeff[1].y() +
            cur_u * coeff[2].y() + coeff[3].y());
    if (smoothness < 1.0) {
      QPointF linearPos = keyP[segmentId].pos * beforeRatio +
                          keyP[segmentId + 1].pos * afterRatio;
      resultPoint.pos =
          resultPoint.pos * smoothness + linearPos * (1. - smoothness);
    }
    // �n���h���͐��`��Ԃ���
    QPointF firstHandleVec =
        (keyP[segmentId].firstHandle - keyP[segmentId].pos) * beforeRatio +
        (keyP[segmentId + 1].firstHandle - keyP[segmentId + 1].pos) *
            afterRatio;
    resultPoint.firstHandle = resultPoint.pos + firstHandleVec;
    QPointF secondHandleVec =
        (keyP[segmentId].secondHandle - keyP[segmentId].pos) * beforeRatio +
        (keyP[segmentId + 1].secondHandle - keyP[segmentId + 1].pos) *
            afterRatio;
    resultPoint.secondHandle = resultPoint.pos + secondHandleVec;

    result.push_back(resultPoint);
  }
}

// ��Ԃ̌v�Z Correspondence�_�̕��
// Open�Ȑ��̏ꍇ�A�P���ɕ��
void simple_InterPolate(CorrPointList &beforeKey, CorrPointList &afterKey,
                        double afterRatio, CorrPointList &result) {
  double beforeRatio = 1.0 - afterRatio;

  for (int c = 0; c < beforeKey.count(); c++) {
    double beforeVal = beforeKey.at(c);
    double afterVal  = afterKey.at(c);

    // ���
    double tmpVal = (beforeVal == afterVal)
                        ? beforeVal
                        : beforeRatio * beforeVal + afterRatio * afterVal;

    result.push_back(tmpVal);
  }
}

// �����`�F�b�N
bool checkIntersect(CorrPointList &beforeKey, QList<double> &tmpAfterPos) {
  // ���ꂼ��̓_���m�̃y�A�ɂ��āABefore��After�̈ʒu�֌W�̐������t�]���Ă�����NG

  for (int p = 0; p < beforeKey.count() - 1; p++) {
    for (int q = p + 1; q < beforeKey.count(); q++) {
      bool bef = (beforeKey[q] - beforeKey[p]) >= 0;
      bool aft = (tmpAfterPos[q] - tmpAfterPos[p]) >= 0;

      // XOR�����A���Ȃ�NG�Ȃ̂�false��return
      if (bef ^ aft) return false;
    }
  }

  return true;
}

// ��Ԃ̌v�Z Correspondence�_�̕�� Closed�ȃV�F�C�v�̏ꍇ
// �e�_�ɂ��āA�V�F�C�v�̂Ƃ蓾���Ԃ̈ړ������́A�E���A������2�ʂ�B
// �������A�_�̏��Ԃ��������Ă͂����Ȃ��B
// n�̑Ή��_�ɂ��āA�Q^n�ʂ�̈ړ����@���l������B������T�����āA
// �_�̏����֌W���ς��Ȃ��ړ����@��������
void interPolate(CorrPointList &beforeKey, CorrPointList &afterKey,
                 double afterRatio, CorrPointList &result, double maxValue) {
  // �܂��́A���ׂĂ̓_�����߂̃��[�g�ňړ������ꍇ�Ɍ������N���Ȃ����e�X�g����
  QList<double> bestAfterPos;
  QList<double> tmpIdealAfterPos;
  for (int c = 0; c < beforeKey.count(); c++) {
    double beforeVal = beforeKey.at(c);
    double afterVal  = afterKey.at(c);

    // �E�����A�������́A���[�v���ق��������W�l
    double rightMovePos, leftMovePos;
    if (afterVal >= beforeVal) {
      leftMovePos  = afterVal;
      rightMovePos = afterVal - maxValue;
    } else {
      rightMovePos = afterVal;
      leftMovePos  = afterVal + maxValue;
    }
    // �߂�����o�^
    double rDistance = std::abs(rightMovePos - beforeVal);
    double lDistance = std::abs(leftMovePos - beforeVal);
    if (rDistance < lDistance)
      tmpIdealAfterPos.push_back(rightMovePos);
    else
      tmpIdealAfterPos.push_back(leftMovePos);
  }

  // ����Ō������N���Ȃ���ΒT���I��
  if (checkIntersect(beforeKey, tmpIdealAfterPos)) {
    bestAfterPos = tmpIdealAfterPos;
  }
  // ���������K�͂ȒT��������B
  // TODO: ��������剻����̂ŗv������
  else {
    // �e�_�̈ړ��ŁA�E���A�����̈ړ��̃p�^�[���ŁA�_�̓������������Ȃ����̂�T������
    QList<QPair<double, double>>
        afterPos;  //<��,
                   // �E>�Ɉړ������ꍇ�̍��W�l(���[�v���ق���������)������
    for (int c = 0; c < beforeKey.count(); c++) {
      double beforeVal = beforeKey.at(c);
      double afterVal  = afterKey.at(c);

      // �E�����A�������́A���[�v���ق��������W�l
      double rightMovePos, leftMovePos;
      if (afterVal >= beforeVal) {
        leftMovePos  = afterVal;
        rightMovePos = afterVal - maxValue;
      } else {
        rightMovePos = afterVal;
        leftMovePos  = afterVal + maxValue;
      }

      afterPos.push_back(QPair<double, double>(leftMovePos, rightMovePos));
    }

    // �e�_�̈ړ����� �E���A�����̑g�ݍ��킹�𑍓��肵��
    // �_�̈ړ����������Ȃ����̂�T�����_�̈ʒu�֌W���ς��Ȃ�����
    // �g�ݍ��킹��
    unsigned int amount = 1 << beforeKey.count();

    QList<QPair<unsigned int, double>> idLengthList;
    for (unsigned int k = 0; k < amount; k++) {
      QList<double> tmpAfterPos;
      // ����After�ʒu�ɁA���݂̑g�ݍ��킹�����Ă݂�
      for (int c = 0; c < beforeKey.count(); c++) {
        bool useLeftPos = ((k / (1 << c)) % 2 == 0);

        tmpAfterPos.push_back((useLeftPos) ? afterPos[c].first
                                           : afterPos[c].second);
      }
      double moveLength = 0.0;
      for (int p = 0; p < beforeKey.count(); p++) {
        moveLength += std::abs(tmpAfterPos[p] - beforeKey[p]);
      }
      idLengthList.push_back(QPair<unsigned int, double>(k, moveLength));
    }

    // �ړ��������Ƀ\�[�g
    auto lengthLessThan = [](const QPair<unsigned int, double> &val1,
                             const QPair<unsigned int, double> &val2) {
      return val1.second < val2.second;
    };
    std::sort(idLengthList.begin(), idLengthList.end(), lengthLessThan);

    for (auto idLengthPair : idLengthList) {
      QList<double> tmpAfterPos;
      int k = idLengthPair.first;
      // ����After�ʒu�ɁA���݂̑g�ݍ��킹�����Ă݂�
      for (int c = 0; c < beforeKey.count(); c++) {
        bool useLeftPos = ((k / (1 << c)) % 2 == 0);

        tmpAfterPos.push_back((useLeftPos) ? afterPos[c].first
                                           : afterPos[c].second);
      }

      // �����`�F�b�N OK�Ȃ�A�e�_�̑��ړ������̍ł��Z�����̂Œu������
      if (checkIntersect(beforeKey, tmpAfterPos)) {
        bestAfterPos = tmpAfterPos;
        break;
      }
    }
  }

  double beforeRatio = 1.0 - afterRatio;

  for (int c = 0; c < beforeKey.count(); c++) {
    double beforeVal = beforeKey[c];
    double afterVal  = bestAfterPos[c];
    // ���
    double tmpVal = beforeVal * beforeRatio + afterVal * afterRatio;

    if (tmpVal >= maxValue) tmpVal -= maxValue;
    if (tmpVal < 0.0) tmpVal += maxValue;

    result.push_back(tmpVal);
  }
}

};  // namespace

//-----------------------------------------------------------------------------
// �L�[�t���[������ǉ�����B�ȑO�Ƀf�[�^������ꍇ�A�������Ēu��������
//-----------------------------------------------------------------------------
template <class T>
void KeyContainer<T>::setKeyData(int frame, T data) {
  // insert�R�}���h�́A���ɓ���Key�Ƀf�[�^���L�����ꍇ�A�u����������
  m_data.insert(frame, data);
}

//-----------------------------------------------------------------------------
// �w�肳�ꂽ�t���[�����L�[�t���[�����ǂ�����Ԃ�
//-----------------------------------------------------------------------------
template <class T>
bool KeyContainer<T>::isKey(int frame) {
  return m_data.contains(frame);
}

//-----------------------------------------------------------------------------
// frame ����Łiframe���܂܂Ȃ��j��ԋ߂��L�[�t���[����Ԃ��B
// ������� -1��Ԃ�
//-----------------------------------------------------------------------------
template <class T>
int KeyContainer<T>::nextKey(int frame) {
  auto afterKey = m_data.upperBound(frame);
  if (afterKey == m_data.end()) return -1;
  return afterKey.key();
}

//-----------------------------------------------------------------------------
// frame��Key�Ȃ�frame��Ԃ��A�����łȂ����frame���O�ň�ԋ߂��L�[�t���[����Ԃ��B
// ������� -1��Ԃ�
//-----------------------------------------------------------------------------
template <class T>
int KeyContainer<T>::belongingKey(int frame) {
  if (m_data.isEmpty()) return -1;
  if (isKey(frame)) return frame;
  auto afterKey = m_data.upperBound(frame);
  if (afterKey == m_data.end()) return m_data.lastKey();
  if (afterKey == m_data.begin()) return -1;
  return (afterKey - 1).key();
}

//-----------------------------------------------------------------------------
// �w�肵���t���[���̃L�[�t���[����������
//-----------------------------------------------------------------------------
template <class T>
void KeyContainer<T>::removeKeyData(int frame) {
  m_data.remove(frame);
  m_interpolation.remove(frame);
}

//-----------------------------------------------------------------------------
// �w�肳�ꂽ�t���[���̒l��Ԃ��B(���ꉻ����)
// �L�[�t���[���������ꍇ�͏����l�B
// �L�[�t���[�����P�̏ꍇ�͂��̒l�B
// �L�[�t���[���̏ꍇ�͂��̒l�B
// �L�[�t���[���ł͂Ȃ��ꍇ�͕�Ԃ��ĕԂ��B
//-----------------------------------------------------------------------------
template <>
BezierPointList KeyContainer<BezierPointList>::getData(int frame, int,
                                                       double smoothness) {
  // �L�[�t���[���������ꍇ(���肦�Ȃ��n�Y)
  if (m_data.isEmpty()) {
    BezierPointList list;
    return list;
  }
  // �L�[�t���[�����P�̏ꍇ�͂��̒l�B
  else if (m_data.count() == 1)
    return m_data.begin().value();
  // �L�[�t���[���̏ꍇ�͂��̒l�B
  else if (m_data.contains(frame))
    return m_data.value(frame);
  else {
    // ���߂�1��̃L�[�t���[���𓾂�
    QMap<int, BezierPointList>::iterator afterKey = m_data.lowerBound(frame);

    // �����AafterKey�������ꍇ�A
    // ����́A�L�[�t���[���͈͂̊O���Ƃ������ƁB
    // ��Ԃ����ɁA���K�̃L�[�t���[���̒l��Ԃ��΂悢�B
    if (afterKey == m_data.end()) return (afterKey - 1).value();
    // �����AafterKey���擪�̃A�C�e���̂Ƃ�
    // ����܂��A�L�[�t���[���͈͂̊O���Ƃ������ƁB
    // ��Ԃ����ɁA���̃L�[�t���[���̒l��Ԃ��΂悢
    else if (afterKey == m_data.begin())
      return afterKey.value();
    // �L�[�Ԃ̃Z�O�����g�̏ꍇ�A��Ԃ���
    else {
      int beforeFrame = (afterKey - 1).key();
      int afterFrame  = afterKey.key();

      // after�L�[�̏d��
      double afterRatio =
          (double)(frame - beforeFrame) / (double)(afterFrame - beforeFrame);

      // �l�ߕ��𒲐�����
      if (m_interpolation.contains(beforeFrame)) {
        double interp = m_interpolation.value(beforeFrame);
        if (interp < 0.5) {
          double gamma = std::log(interp) / std::log(0.5);
          afterRatio   = std::pow(afterRatio, gamma);
        } else {  // 0.5���傫���ꍇ�A�O���t�𔽓]���Čv�Z
          double gamma = std::log(1.0 - interp) / std::log(0.5);
          afterRatio   = 1.0 - std::pow((1.0 - afterRatio), gamma);
        }
      }

      // ��Ԓl��Ԃ�
      BezierPointList list;

      // �L�[���S�ȏ㖳�����Linear
      if (smoothness == 0. || getKeyCount() < 4)
        linearInterPolate((afterKey - 1).value(), afterKey.value(), afterRatio,
                          list);
      else {  // Spline���
        // �ЂƂ߂̃Z�O�����g�̏ꍇ
        if (afterKey - 1 == m_data.begin())
          splineInterpolate((afterKey - 1).value(), afterKey.value(),
                            (afterKey + 1).value(), (afterKey + 2).value(),
                            afterRatio, list, 0, smoothness);
        // �Ō�̃Z�O�����g�̏ꍇ
        else if (afterKey + 1 == m_data.end())
          splineInterpolate((afterKey - 3).value(), (afterKey - 2).value(),
                            (afterKey - 1).value(), afterKey.value(),
                            afterRatio, list, 2, smoothness);
        else  // ���Ԃ̃Z�O�����g�̏ꍇ
          splineInterpolate((afterKey - 2).value(), (afterKey - 1).value(),
                            afterKey.value(), (afterKey + 1).value(),
                            afterRatio, list, 1, smoothness);
      }

      return list;
    }
  }
}

template <>
CorrPointList KeyContainer<CorrPointList>::getData(int frame, int maxValue,
                                                   double) {
  // �L�[�t���[���������ꍇ(���肦�Ȃ��n�Y)
  if (m_data.isEmpty()) {
    CorrPointList list;
    return list;
  }
  // �L�[�t���[�����P�̏ꍇ�͂��̒l�B
  else if (m_data.count() == 1)
    return m_data.begin().value();
  // �L�[�t���[���̏ꍇ�͂��̒l�B
  else if (m_data.contains(frame))
    return m_data.value(frame);
  else {
    // ���߂�1��̃L�[�t���[���𓾂�
    QMap<int, CorrPointList>::iterator afterKey = m_data.lowerBound(frame);

    // �����AafterKey�������ꍇ�A
    // ����́A�L�[�t���[���͈͂̊O���Ƃ������ƁB
    // ��Ԃ����ɁA���K�̃L�[�t���[���̒l��Ԃ��΂悢�B
    if (afterKey == m_data.end()) return (afterKey - 1).value();
    // �����AafterKey���擪�̃A�C�e���̂Ƃ�
    // ����܂��A�L�[�t���[���͈͂̊O���Ƃ������ƁB
    // ��Ԃ����ɁA���̃L�[�t���[���̒l��Ԃ��΂悢
    else if (afterKey == m_data.begin())
      return afterKey.value();
    // �L�[�Ԃ̃Z�O�����g�̏ꍇ�A��Ԃ���
    else {
      int beforeFrame = (afterKey - 1).key();
      int afterFrame  = afterKey.key();

      // after�L�[�̏d��
      double afterRatio =
          (double)(frame - beforeFrame) / (double)(afterFrame - beforeFrame);

      // �l�ߕ��𒲐�����
      if (m_interpolation.contains(beforeFrame)) {
        double gamma =
            std::log(m_interpolation.value(beforeFrame)) / std::log(0.5);
        afterRatio = std::pow(afterRatio, 1.0 / gamma);
      }

      // ��Ԓl��Ԃ�
      CorrPointList list;
      // �����A�擪�ƍŌ��Corr�_�̒l���ς���Ă��Ȃ��Ȃ�AOpen�Ȑ��Ƃ��āA�P���ȕ�Ԃ�����
      if ((afterKey - 1).value().first() == afterKey.value().first() &&
          (afterKey - 1).value().last() == afterKey.value().last() &&
          (afterKey - 1).value().first() == 0.0 &&
          (afterKey - 1).value().last() == (double)maxValue)
        simple_InterPolate((afterKey - 1).value(), afterKey.value(), afterRatio,
                           list);
      // ����ȊO�̏ꍇ�́AClosed�Ȑ��Ƃ��āA���G�ȕ�Ԃ�����
      else
        interPolate((afterKey - 1).value(), afterKey.value(), afterRatio, list,
                    (double)maxValue);

      return list;
    }
  }
}

namespace {
double marume(double val) { return (abs(val) < 0.00001) ? 0.0 : val; }
};  // namespace

//-----------------------------------------------------------------------------
// �Z�[�u/���[�h(���ꉻ����)
//-----------------------------------------------------------------------------
template <>
void KeyContainer<BezierPointList>::saveData(QXmlStreamWriter &writer) {
  QMap<int, BezierPointList>::const_iterator i = m_data.constBegin();
  while (i != m_data.constEnd()) {
    writer.writeStartElement("FormKey");

    // �L�[�t���[���͂P�����Ă��邱�Ƃɒ��ӁB���[�h���ɂP��������
    writer.writeAttribute("frame", QString::number(i.key() + 1));

    // ��Ԓl
    if (m_interpolation.contains(i.key()))
      writer.writeAttribute("interpolation",
                            QString::number(m_interpolation.value(i.key())));

    BezierPointList bpList = i.value();
    for (int bp = 0; bp < bpList.size(); bp++) {
      QString str = QString("(%1, %2), (%3, %4), (%5, %6)")
                        .arg(bpList.at(bp).firstHandle.x())
                        .arg(bpList.at(bp).firstHandle.y())
                        .arg(bpList.at(bp).pos.x())
                        .arg(bpList.at(bp).pos.y())
                        .arg(bpList.at(bp).secondHandle.x())
                        .arg(bpList.at(bp).secondHandle.y());
      writer.writeTextElement("bezierPoint", str);
    }

    writer.writeEndElement();
    ++i;
  }
}
template <>
void KeyContainer<BezierPointList>::loadData(QXmlStreamReader &reader) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "FormKey") {
      // Save���ɂP�������̂ŁA���[�h���ɂP����
      int frame = reader.attributes().value("frame").toString().toInt() - 1;

      // ��Ԓl
      if (reader.attributes().hasAttribute("interpolation")) {
        double interp =
            reader.attributes().value("interpolation").toString().toDouble();
        m_interpolation.insert(frame, interp);
      }

      BezierPointList bpList;
      while (reader.readNextStartElement()) {
        if (reader.name() == "bezierPoint") {
          QString str = reader.readElementText();
          QStringList list2 =
              str.split(QRegExp("[\\s+(),]"), Qt::SkipEmptyParts);
          QList<double> list;
          for (int s = 0; s < list2.size(); s++) {
            list.append((double)list2.at(s).toDouble());
          }

          if (list.size() != 6)
            std::cout << "Warning : bezierPoint has no 6 values!" << std::endl;
          else {
            BezierPoint bp = {QPointF(list.at(2), list.at(3)),   // pos
                              QPointF(list.at(0), list.at(1)),   // first
                              QPointF(list.at(4), list.at(5))};  // second
            bpList.append(bp);
          }
        } else
          reader.skipCurrentElement();
      }
      // �L�[�t���[���̊i�[
      setKeyData(frame, bpList);
    } else
      reader.skipCurrentElement();
  }
}
//-----------------------------------------------------------------------------
template <>
void KeyContainer<CorrPointList>::saveData(QXmlStreamWriter &writer) {
  QMap<int, CorrPointList>::const_iterator i = m_data.constBegin();
  while (i != m_data.constEnd()) {
    writer.writeStartElement("CorrKey");

    // �L�[�t���[���͂P�����Ă��邱�Ƃɒ��ӁB���[�h���ɂP��������
    writer.writeAttribute("frame", QString::number(i.key() + 1));

    // ��Ԓl
    if (m_interpolation.contains(i.key()))
      writer.writeAttribute("interpolation",
                            QString::number(m_interpolation.value(i.key())));

    QString str;
    CorrPointList cpList = i.value();
    for (int cp = 0; cp < cpList.size(); cp++) {
      if (cp != 0) str.append(", ");

      str.append(QString::number(cpList.at(cp)));
    }
    writer.writeTextElement("corrPoint", str);

    writer.writeEndElement();
    ++i;
  }
}

template <>
void KeyContainer<CorrPointList>::loadData(QXmlStreamReader &reader) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "CorrKey") {
      // Save���ɂP�������̂ŁA���[�h���ɂP����
      int frame = reader.attributes().value("frame").toString().toInt() - 1;

      // ��Ԓl
      if (reader.attributes().hasAttribute("interpolation")) {
        double interp =
            reader.attributes().value("interpolation").toString().toDouble();
        m_interpolation.insert(frame, interp);
      }

      CorrPointList cpList;

      while (reader.readNextStartElement())  // corrPoint�ɓ���
      {
        if (reader.name() == "corrPoint") {
          QStringList list = reader.readElementText().split(", ");
          for (int c = 0; c < list.size(); c++)
            cpList.append(list.at(c).toDouble());
        } else
          reader.skipCurrentElement();
      }
      // �L�[�t���[���̊i�[
      setKeyData(frame, cpList);
    } else
      reader.skipCurrentElement();
  }
}

// VisualC �ŁA�e���v���[�g�̎�����CPP�ɏ��������Ƃ��A
// �ȉ��̂悤�ɁA���̉��������N���X�𖾕�������K�v
template class KeyContainer<BezierPointList>;
template class KeyContainer<CorrPointList>;
