//-----------------------------------------------------------------------------
// KeyContainer クラス
//  キーフレームを持つデータを扱うテンプレートクラス
//	‐シェイプ情報を扱う FormKey
//	‐Correspondence（対応点）情報を扱う CorrKey
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
// 補間の計算 ベジエの各点の補間
void linearInterPolate(BezierPointList &beforeKey, BezierPointList &afterKey,
                       double afterRatio, BezierPointList &result) {
  double beforeRatio = 1.0 - afterRatio;
  // 各ベジエポイントについて
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

// 補間の計算 ベジエの各点の補間 スプライン補間
void splineInterpolate(BezierPointList &key0, BezierPointList &key1,
                       BezierPointList &key2, BezierPointList &key3,
                       double afterRatio, BezierPointList &result,
                       int segmentId, double smoothness) {
  double beforeRatio = 1.0 - afterRatio;
  // 各ベジエポイントについて
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
    // ハンドルは線形補間する
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

// 補間の計算 Correspondence点の補間
// Openな線の場合、単純に補間
void simple_InterPolate(CorrPointList &beforeKey, CorrPointList &afterKey,
                        double afterRatio, CorrPointList &result) {
  double beforeRatio = 1.0 - afterRatio;

  for (int c = 0; c < beforeKey.count(); c++) {
    double beforeVal    = beforeKey.at(c).value;
    double afterVal     = afterKey.at(c).value;
    double beforeWeight = beforeKey.at(c).weight;
    double afterWeight  = afterKey.at(c).weight;
    double beforeDepth  = beforeKey.at(c).depth;
    double afterDepth   = afterKey.at(c).depth;

    // 補間
    double tmpVal = (beforeVal == afterVal)
                        ? beforeVal
                        : beforeRatio * beforeVal + afterRatio * afterVal;
    double tmpWeight =
        (beforeWeight == afterWeight)
            ? beforeWeight
            : beforeRatio * beforeWeight + afterRatio * afterWeight;

    double tmpDepth = (beforeDepth == afterDepth)
                          ? beforeDepth
                          : beforeRatio * beforeDepth + afterRatio * afterDepth;

    result.push_back({tmpVal, tmpWeight, tmpDepth});
  }
}

// 交差チェック
bool checkIntersect(CorrPointList &beforeKey, QList<double> &tmpAfterPos) {
  // それぞれの点同士のペアについて、BeforeとAfterの位置関係の正負が逆転していたらNG

  for (int p = 0; p < beforeKey.count() - 1; p++) {
    for (int q = p + 1; q < beforeKey.count(); q++) {
      bool bef = (beforeKey[q].value - beforeKey[p].value) >= 0;
      bool aft = (tmpAfterPos[q] - tmpAfterPos[p]) >= 0;

      // XORを取り、正ならNGなのでfalseをreturn
      if (bef ^ aft) return false;
    }
  }

  return true;
}

// 補間の計算 Correspondence点の補間 Closedなシェイプの場合
// 各点について、シェイプのとり得る補間の移動方向は、右回り、左回りの2通り。
// しかし、点の順番が交差してはいけない。
// n個の対応点について、２^n通りの移動方法が考えられる。それらを探索して、
// 点の順序関係が変わらない移動方法を見つける
void interPolate(CorrPointList &beforeKey, CorrPointList &afterKey,
                 double afterRatio, CorrPointList &result, double maxValue) {
  // まずは、すべての点が直近のルートで移動した場合に交差が起きないかテストする
  QList<double> bestAfterPos;
  QList<double> tmpIdealAfterPos;
  for (int c = 0; c < beforeKey.count(); c++) {
    double beforeVal = beforeKey.at(c).value;
    double afterVal  = afterKey.at(c).value;

    // 右方向、左方向の、ループをほぐした座標値
    double rightMovePos, leftMovePos;
    if (afterVal >= beforeVal) {
      leftMovePos  = afterVal;
      rightMovePos = afterVal - maxValue;
    } else {
      rightMovePos = afterVal;
      leftMovePos  = afterVal + maxValue;
    }
    // 近い方を登録
    double rDistance = std::abs(rightMovePos - beforeVal);
    double lDistance = std::abs(leftMovePos - beforeVal);
    if (rDistance < lDistance)
      tmpIdealAfterPos.push_back(rightMovePos);
    else
      tmpIdealAfterPos.push_back(leftMovePos);
  }

  // これで交差が起きなければ探索終了
  if (checkIntersect(beforeKey, tmpIdealAfterPos)) {
    bestAfterPos = tmpIdealAfterPos;
  }
  // ここから大規模な探索をする。
  // TODO: 処理が肥大化するので要効率化
  else {
    // 各点の移動で、右回り、左回りの移動のパターンで、点の動きが交差しないものを探索する
    QList<QPair<double, double>>
        afterPos;  //<左,
                   // 右>に移動した場合の座標値(ループをほぐしたもの)を入れる
    for (int c = 0; c < beforeKey.count(); c++) {
      double beforeVal = beforeKey.at(c).value;
      double afterVal  = afterKey.at(c).value;

      // 右方向、左方向の、ループをほぐした座標値
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

    // 各点の移動方向 右回り、左回りの組み合わせを総当りして
    // 点の移動が交差しないものを探す→点の位置関係が変わらないもの
    // 組み合わせ数
    unsigned int amount = 1 << beforeKey.count();

    QList<QPair<unsigned int, double>> idLengthList;
    for (unsigned int k = 0; k < amount; k++) {
      QList<double> tmpAfterPos;
      // 仮のAfter位置に、現在の組み合わせを入れてみる
      for (int c = 0; c < beforeKey.count(); c++) {
        bool useLeftPos = ((k / (1 << c)) % 2 == 0);

        tmpAfterPos.push_back((useLeftPos) ? afterPos[c].first
                                           : afterPos[c].second);
      }
      double moveLength = 0.0;
      for (int p = 0; p < beforeKey.count(); p++) {
        moveLength += std::abs(tmpAfterPos[p] - beforeKey[p].value);
      }
      idLengthList.push_back(QPair<unsigned int, double>(k, moveLength));
    }

    // 移動距離順にソート
    auto lengthLessThan = [](const QPair<unsigned int, double> &val1,
                             const QPair<unsigned int, double> &val2) {
      return val1.second < val2.second;
    };
    std::sort(idLengthList.begin(), idLengthList.end(), lengthLessThan);

    for (auto idLengthPair : idLengthList) {
      QList<double> tmpAfterPos;
      int k = idLengthPair.first;
      // 仮のAfter位置に、現在の組み合わせを入れてみる
      for (int c = 0; c < beforeKey.count(); c++) {
        bool useLeftPos = ((k / (1 << c)) % 2 == 0);

        tmpAfterPos.push_back((useLeftPos) ? afterPos[c].first
                                           : afterPos[c].second);
      }

      // 交差チェック OKなら、各点の総移動距離の最も短いもので置き換え
      if (checkIntersect(beforeKey, tmpAfterPos)) {
        bestAfterPos = tmpAfterPos;
        break;
      }
    }
  }

  double beforeRatio = 1.0 - afterRatio;

  for (int c = 0; c < beforeKey.count(); c++) {
    double beforeVal = beforeKey[c].value;
    double afterVal  = bestAfterPos[c];
    // 補間
    double tmpVal = beforeVal * beforeRatio + afterVal * afterRatio;

    if (tmpVal >= maxValue) tmpVal -= maxValue;
    if (tmpVal < 0.0) tmpVal += maxValue;

    double beforeWeight = beforeKey[c].weight;
    double afterWeight  = afterKey[c].weight;
    double tmpWeight    = beforeWeight * beforeRatio + afterWeight * afterRatio;

    double beforeDepth = beforeKey[c].depth;
    double afterDepth  = afterKey[c].depth;
    double tmpDepth    = beforeDepth * beforeRatio + afterDepth * afterRatio;

    result.push_back({tmpVal, tmpWeight, tmpDepth});
  }
}

};  // namespace

//-----------------------------------------------------------------------------
// キーフレーム情報を追加する。以前にデータがある場合、消去して置き換える
//-----------------------------------------------------------------------------
template <class T>
void KeyContainer<T>::setKeyData(int frame, T data) {
  // insertコマンドは、既に同じKeyにデータが有った場合、置き換えられる
  m_data.insert(frame, data);
}

//-----------------------------------------------------------------------------
// 指定されたフレームがキーフレームかどうかを返す
//-----------------------------------------------------------------------------
template <class T>
bool KeyContainer<T>::isKey(int frame) {
  return m_data.contains(frame);
}

//-----------------------------------------------------------------------------
// frame より後で（frameを含まない）一番近いキーフレームを返す。
// 無ければ -1を返す
//-----------------------------------------------------------------------------
template <class T>
int KeyContainer<T>::nextKey(int frame) {
  auto afterKey = m_data.upperBound(frame);
  if (afterKey == m_data.end()) return -1;
  return afterKey.key();
}

//-----------------------------------------------------------------------------
// frameがKeyならframeを返し、そうでなければframeより前で一番近いキーフレームを返す。
// 無ければ -1を返す
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
// 指定したフレームのキーフレーム情報を消す
//-----------------------------------------------------------------------------
template <class T>
void KeyContainer<T>::removeKeyData(int frame) {
  m_data.remove(frame);
  m_interpolation.remove(frame);
}

//-----------------------------------------------------------------------------
// 指定されたフレームの値を返す。(特殊化する)
// キーフレームが無い場合は初期値。
// キーフレームが１つの場合はその値。
// キーフレームの場合はその値。
// キーフレームではない場合は補間して返す。
//-----------------------------------------------------------------------------
template <>
BezierPointList KeyContainer<BezierPointList>::getData(int frame, int,
                                                       double smoothness) {
  // キーフレームが無い場合(ありえないハズ)
  if (m_data.isEmpty()) {
    BezierPointList list;
    return list;
  }
  // キーフレームが１つの場合はその値。
  else if (m_data.count() == 1)
    return m_data.begin().value();
  // キーフレームの場合はその値。
  else if (m_data.contains(frame))
    return m_data.value(frame);
  else {
    // 直近で1つ上のキーフレームを得る
    QMap<int, BezierPointList>::iterator afterKey = m_data.lowerBound(frame);

    // もし、afterKeyが無い場合、
    // それは、キーフレーム範囲の外側ということ。
    // 補間せずに、お尻のキーフレームの値を返せばよい。
    if (afterKey == m_data.end()) return (afterKey - 1).value();
    // もし、afterKeyが先頭のアイテムのとき
    // これまた、キーフレーム範囲の外側ということ。
    // 補間せずに、頭のキーフレームの値を返せばよい
    else if (afterKey == m_data.begin())
      return afterKey.value();
    // キー間のセグメントの場合、補間する
    else {
      int beforeFrame = (afterKey - 1).key();
      int afterFrame  = afterKey.key();

      // afterキーの重み
      double afterRatio =
          (double)(frame - beforeFrame) / (double)(afterFrame - beforeFrame);

      // 詰め方を調整する
      if (m_interpolation.contains(beforeFrame)) {
        double interp = m_interpolation.value(beforeFrame);
        if (interp < 0.5) {
          double gamma = std::log(interp) / std::log(0.5);
          afterRatio   = std::pow(afterRatio, gamma);
        } else {  // 0.5より大きい場合、グラフを反転して計算
          double gamma = std::log(1.0 - interp) / std::log(0.5);
          afterRatio   = 1.0 - std::pow((1.0 - afterRatio), gamma);
        }
      }

      // 補間値を返す
      BezierPointList list;

      // キーが４つ以上無ければLinear
      if (smoothness == 0. || getKeyCount() < 4)
        linearInterPolate((afterKey - 1).value(), afterKey.value(), afterRatio,
                          list);
      else {  // Spline補間
        // ひとつめのセグメントの場合
        if (afterKey - 1 == m_data.begin())
          splineInterpolate((afterKey - 1).value(), afterKey.value(),
                            (afterKey + 1).value(), (afterKey + 2).value(),
                            afterRatio, list, 0, smoothness);
        // 最後のセグメントの場合
        else if (afterKey + 1 == m_data.end())
          splineInterpolate((afterKey - 3).value(), (afterKey - 2).value(),
                            (afterKey - 1).value(), afterKey.value(),
                            afterRatio, list, 2, smoothness);
        else  // 中間のセグメントの場合
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
  // キーフレームが無い場合(ありえないハズ)
  if (m_data.isEmpty()) {
    CorrPointList list;
    return list;
  }
  // キーフレームが１つの場合はその値。
  else if (m_data.count() == 1)
    return m_data.begin().value();
  // キーフレームの場合はその値。
  else if (m_data.contains(frame))
    return m_data.value(frame);
  else {
    // 直近で1つ上のキーフレームを得る
    QMap<int, CorrPointList>::iterator afterKey = m_data.lowerBound(frame);

    // もし、afterKeyが無い場合、
    // それは、キーフレーム範囲の外側ということ。
    // 補間せずに、お尻のキーフレームの値を返せばよい。
    if (afterKey == m_data.end()) return (afterKey - 1).value();
    // もし、afterKeyが先頭のアイテムのとき
    // これまた、キーフレーム範囲の外側ということ。
    // 補間せずに、頭のキーフレームの値を返せばよい
    else if (afterKey == m_data.begin())
      return afterKey.value();
    // キー間のセグメントの場合、補間する
    else {
      int beforeFrame = (afterKey - 1).key();
      int afterFrame  = afterKey.key();

      // afterキーの重み
      double afterRatio =
          (double)(frame - beforeFrame) / (double)(afterFrame - beforeFrame);

      // 詰め方を調整する
      if (m_interpolation.contains(beforeFrame)) {
        double gamma =
            std::log(m_interpolation.value(beforeFrame)) / std::log(0.5);
        afterRatio = std::pow(afterRatio, 1.0 / gamma);
      }

      // 補間値を返す
      CorrPointList list;
      // もし、先頭と最後のCorr点の値が変わっていないなら、Openな線として、単純な補間をする
      if ((afterKey - 1).value().first().value ==
              afterKey.value().first().value &&
          (afterKey - 1).value().last().value ==
              afterKey.value().last().value &&
          (afterKey - 1).value().first().value == 0.0 &&
          (afterKey - 1).value().last().value == (double)maxValue)
        simple_InterPolate((afterKey - 1).value(), afterKey.value(), afterRatio,
                           list);
      // それ以外の場合は、Closedな線として、複雑な補間をする
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
// セーブ/ロード(特殊化する)
//-----------------------------------------------------------------------------
template <>
void KeyContainer<BezierPointList>::saveData(QXmlStreamWriter &writer) {
  QMap<int, BezierPointList>::const_iterator i = m_data.constBegin();
  while (i != m_data.constEnd()) {
    writer.writeStartElement("FormKey");

    // キーフレームは１足していることに注意。ロード時に１引くこと
    writer.writeAttribute("frame", QString::number(i.key() + 1));

    // 補間値
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
      // Save時に１足したので、ロード時に１引く
      int frame = reader.attributes().value("frame").toString().toInt() - 1;

      // 補間値
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
      // キーフレームの格納
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

    // キーフレームは１足していることに注意。ロード時に１引くこと
    writer.writeAttribute("frame", QString::number(i.key() + 1));

    // 補間値
    if (m_interpolation.contains(i.key()))
      writer.writeAttribute("interpolation",
                            QString::number(m_interpolation.value(i.key())));

    QString str;
    CorrPointList cpList = i.value();
    for (int cp = 0; cp < cpList.size(); cp++) {
      if (cp != 0) str.append(", ");

      str.append(QString::number(cpList.at(cp).value));
    }
    writer.writeTextElement("corrPoint", str);

    // ウェイトを保存
    str.clear();
    for (int cp = 0; cp < cpList.size(); cp++) {
      if (cp != 0) str.append(", ");

      str.append(QString::number(cpList.at(cp).weight));
    }
    writer.writeTextElement("corrWeights", str);

    // デプスを保存
    str.clear();
    for (int cp = 0; cp < cpList.size(); cp++) {
      if (cp != 0) str.append(", ");

      str.append(QString::number(cpList.at(cp).depth));
    }
    writer.writeTextElement("corrDepths", str);

    writer.writeEndElement();
    ++i;
  }
}

template <>
void KeyContainer<CorrPointList>::loadData(QXmlStreamReader &reader) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "CorrKey") {
      // Save時に１足したので、ロード時に１引く
      int frame = reader.attributes().value("frame").toString().toInt() - 1;

      // 補間値
      if (reader.attributes().hasAttribute("interpolation")) {
        double interp =
            reader.attributes().value("interpolation").toString().toDouble();
        m_interpolation.insert(frame, interp);
      }

      CorrPointList cpList;

      while (reader.readNextStartElement())  // corrPointに入る
      {
        if (reader.name() == "corrPoint") {
          QStringList list = reader.readElementText().split(", ");
          for (int c = 0; c < list.size(); c++)
            cpList.append({list.at(c).toDouble(), 1.0});
        } else if (reader.name() == "corrWeights") {
          assert(!cpList.isEmpty());
          QStringList list = reader.readElementText().split(", ");
          for (int c = 0; c < list.size(); c++)
            cpList[c].weight = list.at(c).toDouble();
        } else if (reader.name() == "corrDepths") {
          assert(!cpList.isEmpty());
          QStringList list = reader.readElementText().split(", ");
          for (int c = 0; c < list.size(); c++)
            cpList[c].depth = list.at(c).toDouble();
        } else
          reader.skipCurrentElement();
      }
      // キーフレームの格納
      setKeyData(frame, cpList);
    } else
      reader.skipCurrentElement();
  }
}

// VisualC で、テンプレートの実装をCPPに書きたいとき、
// 以下のように、実体化したいクラスを明文化する必要
template class KeyContainer<BezierPointList>;
template class KeyContainer<CorrPointList>;
