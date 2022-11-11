//--------------------------------------------------------
// Reshape Tool用の ドラッグツール
// コントロールポイント編集ツール
//--------------------------------------------------------

#include "pointdragtool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwlayerhandle.h"
#include "iwlayer.h"
#include "iwundomanager.h"
#include "transformdragtool.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"
#include "viewsettings.h"
#include "reshapetool.h"
#include "sceneviewer.h"
#include "iwtrianglecache.h"

#include <QTransform>
#include <QMouseEvent>

namespace {
// Currently the multi frame mode is not available
// MultiFrameモードがONかどうかを返す
bool isMultiFrameActivated() {
  return false;
  // return
  // CommandManager::instance()->getAction(VMI_MultiFrameMode)->isChecked();
}
};  // namespace

//--------------------------------------------------------

PointDragTool::PointDragTool() : m_undoEnabled(true) {
  m_project = IwApp::instance()->getCurrentProject()->getProject();
  m_layer   = IwApp::instance()->getCurrentLayer()->getLayer();
}

//--------------------------------------------------------
//--------------------------------------------------------
// 並行移動ツール
//--------------------------------------------------------

TranslatePointDragTool::TranslatePointDragTool(QSet<int>& points,
                                               QPointF& grabbedPointOrgPos,
                                               QPointF& onePix, OneShape shape,
                                               int pointIndex)
    : m_grabbedPointOrgPos(grabbedPointOrgPos)
    , m_onePixLength(onePix)
    , m_snapHGrid(-1)
    , m_snapVGrid(-1)
    , m_shape(shape)
    , m_pointIndex(pointIndex)
    , m_handleSnapped(false) {
  // 関わるシェイプをリストに格納する
  QSet<int>::const_iterator i = points.constBegin();
  while (i != points.constEnd()) {
    int name       = *i;
    OneShape shape = m_layer->getShapePairFromName(name);

    int pointIndex = (int)((name % 10000) / 10);

    // 新たなシェイプの場合、追加
    if (!m_shapes.contains(shape)) {
      m_shapes.append(shape);
      QList<int> list;
      list << pointIndex;
      m_pointsInShapes.append(list);
    } else {
      m_pointsInShapes[m_shapes.indexOf(shape)].append(pointIndex);
    }
    ++i;
  }

  // 現在のフレームを得る
  int frame = m_project->getViewFrame();
  // MultiFrameの情報を得る
  QList<int> multiFrameList = m_project->getMultiFrames();

  // 元の形状と、キーフレームだったかどうかを取得する
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);
    // キーフレームだったかどうか
    m_wasKeyFrame.push_back(shape.shapePairP->isFormKey(frame, shape.fromTo));
    // m_wasKeyFrame.push_back(m_shapes.at(s)->isFormKey(frame));

    QMap<int, BezierPointList> formKeyMap;
    // まず、編集中のフレーム
    formKeyMap[frame] =
        shape.shapePairP->getBezierPointList(frame, shape.fromTo);
    // 続いて、MultiFrameがONのとき、MultiFrameで、かつキーフレームを格納
    if (isMultiFrameActivated()) {
      for (int mf = 0; mf < multiFrameList.size(); mf++) {
        int mframe = multiFrameList.at(mf);
        // カレントフレームは飛ばす
        if (mframe == frame) continue;
        // キーフレームなら、登録
        if (shape.shapePairP->isFormKey(mframe, shape.fromTo))
          formKeyMap[mframe] =
              shape.shapePairP->getBezierPointList(mframe, shape.fromTo);
      }
    }

    // 元の形状の登録
    m_orgPoints.append(formKeyMap);
  }

  // シグナルをEmit
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//--------------------------------------------------------
// PenTool用
TranslatePointDragTool::TranslatePointDragTool(OneShape shape,
                                               QList<int> points,
                                               QPointF& onePix)
    : m_onePixLength(onePix) {
  m_shapes.push_back(shape);

  QList<int> list;
  for (auto pId : points) {
    list.push_back((int)(pId / 10));
  }
  m_pointsInShapes.push_back(list);
  m_grabbedPointOrgPos =
      shape.getBezierPointList(m_project->getViewFrame()).at(list.last()).pos;

  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  QMap<int, BezierPointList> formKeyMap;
  formKeyMap[frame] = m_shapes.at(0).shapePairP->getBezierPointList(
      frame, m_shapes.at(0).fromTo);
  m_orgPoints.push_back(formKeyMap);

  m_undoEnabled = false;
}

//--------------------------------------------------------

void TranslatePointDragTool::onClick(const QPointF& pos, const QMouseEvent* e) {
  m_startPos = pos;
}

//--------------------------------------------------------
// 表示中のシェイプのポイントにスナップする
// 線にもスナップできるようにしたいが
// コントロールポイントにスナップした場合はindexを返す

int TranslatePointDragTool::calculateSnap(QPointF& pos) {
  int ret = -1;
  // 現在のフレーム
  int frame             = m_project->getViewFrame();
  IwLayer* currentLayer = IwApp::instance()->getCurrentLayer()->getLayer();

  double thresholdPixels = 10.0;

  QPointF thres_dist = thresholdPixels * m_onePixLength;

  // ロック情報を得る
  bool fromToVisible[2];
  for (int fromTo = 0; fromTo < 2; fromTo++)
    fromToVisible[fromTo] =
        m_project->getViewSettings()->isFromToVisible(fromTo);

  double minimumW    = 0.0;
  double minimumDist = (thres_dist.x() + thres_dist.y()) * 0.5;

  // 各レイヤについて
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    // レイヤを取得
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;

    bool isCurrent = (layer == currentLayer);
    // 現在のレイヤが非表示ならcontinue
    // ただしカレントレイヤの場合は表示する
    if (!isCurrent && !layer->isVisibleInViewer()) continue;

    for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
      ShapePair* shapePair = layer->getShapePair(sp);
      if (!shapePair) continue;
      if (!shapePair->isVisible()) continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // ロックされていて非表示ならスキップ
        if (!fromToVisible[fromTo]) continue;
        // ドラッグ中のシェイプならスキップ
        if (m_shapes.contains(OneShape(shapePair, fromTo))) continue;
        // バウンディングボックスに入っていなかったらスキップ
        QRectF bBox = shapePair->getBBox(frame, fromTo)
                          .adjusted(-thres_dist.x(), -thres_dist.y(),
                                    thres_dist.x(), thres_dist.y());
        if (!bBox.contains(pos)) continue;
        double dist;
        double w = shapePair->getNearestBezierPos(pos, frame, fromTo, dist);
        if (dist < minimumDist) {
          double pointBefore = std::floor(w);
          QPointF posBefore =
              shapePair->getBezierPosFromValue(frame, fromTo, pointBefore);

          double pointAfter = std::ceil(w);
          if (shapePair->isClosed() &&
              (int)pointAfter == shapePair->getBezierPointAmount())
            pointAfter = 0.0;
          QPointF posAfter =
              shapePair->getBezierPosFromValue(frame, fromTo, pointAfter);

          if (std::abs(posBefore.x() - pos.x()) < thres_dist.x() &&
              std::abs(posBefore.y() - pos.y()) < thres_dist.y()) {
            minimumW = pointBefore;
            ret      = (int)pointBefore;
          } else if (std::abs(posAfter.x() - pos.x()) < thres_dist.x() &&
                     std::abs(posAfter.y() - pos.y()) < thres_dist.y()) {
            minimumW = pointAfter;
            ret      = (int)minimumW;
          } else
            minimumW = w;
          minimumDist  = dist;
          m_snapTarget = OneShape(shapePair, fromTo);
        }
      }
    }
  }

  // シェイプにスナップしたとき
  if (m_snapTarget.shapePairP != nullptr) {
    pos = m_snapTarget.shapePairP->getBezierPosFromValue(
        frame, m_snapTarget.fromTo, minimumW);
    return ret;
  }

  // 次に、ガイドへのスナップを処理する
  IwProject::Guides hGuide = m_project->getHGuides();
  double minDist           = thres_dist.x();
  double snappedPos;
  for (int gId = 0; gId < hGuide.size(); gId++) {
    double g    = hGuide[gId];
    double dist = std::abs(pos.x() - g);
    if (dist <= minDist) {
      minDist     = dist;
      m_snapHGrid = gId;
      snappedPos  = g;
    }
  }
  if (m_snapHGrid >= 0) pos.setX(snappedPos);

  IwProject::Guides vGuide = m_project->getVGuides();
  minDist                  = thres_dist.y();
  for (int gId = 0; gId < vGuide.size(); gId++) {
    double g    = vGuide[gId];
    double dist = std::abs(pos.y() - g);
    if (dist <= minDist) {
      minDist     = dist;
      m_snapVGrid = gId;
      snappedPos  = g;
    }
  }
  if (m_snapVGrid >= 0) pos.setY(snappedPos);
  return ret;
}

//--------------------------------------------------------

void TranslatePointDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // 現在のフレームを得る
  int frame       = m_project->getViewFrame();
  m_handleSnapped = false;

  // 移動ベクトルを得る
  QPointF moveVec = pos - m_startPos;

  // Shiftが押されていたら移動量が大きい方のみ残す
  if (e->modifiers() & Qt::ShiftModifier) {
    // X方向に平行移動
    if (abs(moveVec.x()) > abs(moveVec.y())) moveVec.setY(0.0);
    // Y方向に平行移動
    else
      moveVec.setX(0.0);
  }

  // ここにスナップ処理を入れる
  m_snapTarget       = OneShape();
  m_snapHGrid        = -1;
  m_snapVGrid        = -1;
  int snapPointIndex = -1;
  if (e->modifiers() & Qt::ControlModifier) {
    QPointF newPos = m_startPos + moveVec;
    snapPointIndex = calculateSnap(newPos);
    if (m_snapTarget.shapePairP != nullptr || m_snapHGrid >= 0 ||
        m_snapVGrid >= 0) {
      moveVec = newPos - m_grabbedPointOrgPos;
    }
  }

  bool altPressed = (e->modifiers() & Qt::AltModifier);

  // 各コントロールポイントに移動を適用
  for (int s = 0; s < m_pointsInShapes.size(); s++) {
    QList<int> points = m_pointsInShapes[s];
    // シェイプを得る
    OneShape shape = m_shapes[s];

    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // これから変形を行う形状データ
      BezierPointList pointList = i.value();
      // 変形
      for (auto pointIndex : points) {
        pointList[pointIndex].pos += moveVec;
        pointList[pointIndex].firstHandle += moveVec;
        pointList[pointIndex].secondHandle += moveVec;
        // ハンドルも一緒にスナップする
        if (snapPointIndex >= 0 && altPressed && shape == m_shape &&
            pointIndex == m_pointIndex) {
          // どちらのハンドルに近づけるか、移動距離が近い方を選ぶ
          BezierPoint snapPoint =
              m_snapTarget.shapePairP
                  ->getBezierPointList(frame, m_snapTarget.fromTo)
                  .at(snapPointIndex);
          double len1 =
              (pointList[pointIndex].firstHandle - snapPoint.firstHandle)
                  .manhattanLength() +
              (pointList[pointIndex].secondHandle - snapPoint.secondHandle)
                  .manhattanLength();
          double len2 =
              (pointList[pointIndex].firstHandle - snapPoint.secondHandle)
                  .manhattanLength() +
              (pointList[pointIndex].secondHandle - snapPoint.firstHandle)
                  .manhattanLength();
          if (len1 < len2) {
            pointList[pointIndex].firstHandle  = snapPoint.firstHandle;
            pointList[pointIndex].secondHandle = snapPoint.secondHandle;
          } else {
            pointList[pointIndex].firstHandle  = snapPoint.secondHandle;
            pointList[pointIndex].secondHandle = snapPoint.firstHandle;
          }
          m_handleSnapped = true;
        }
      }

      // キーを格納
      shape.shapePairP->setFormKey(frame, shape.fromTo, pointList);

      ++i;
    }
  }
  // 更新シグナルをエミットする
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//--------------------------------------------------------

// Click時からマウス位置が変わっていなければfalseを返すようにする
bool TranslatePointDragTool::onRelease(const QPointF& pos, const QMouseEvent*) {
  // 動いていなければ false
  if (pos == m_startPos) return false;

  // PenToolの場合はここでreturn
  if (!m_undoEnabled) return true;

  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  QList<QMap<int, BezierPointList>> afterPoints;
  // 変形後のシェイプを取得
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // シェイプを取得
    OneShape shape = m_shapes.at(s);

    QList<int> frameList = m_orgPoints.at(s).keys();
    QMap<int, BezierPointList> afterFormKeyMap;
    for (int f = 0; f < frameList.size(); f++) {
      int tmpFrame = frameList.at(f);
      afterFormKeyMap[tmpFrame] =
          shape.shapePairP->getBezierPointList(tmpFrame, shape.fromTo);
    }

    afterPoints.push_back(afterFormKeyMap);
  }

  // Transform Tool用のUndoを使いまわす
  // Undoに登録 同時にredoが呼ばれるが、最初はフラグで回避する
  IwUndoManager::instance()->push(new TransformDragToolUndo(
      m_shapes, m_orgPoints, afterPoints, m_wasKeyFrame, m_project, frame));

  // 変更されるフレームをinvalidate
  for (auto shape : m_shapes) {
    int start, end;
    shape.shapePairP->getFormKeyRange(start, end, frame, shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, m_project->getParentShape(shape.shapePairP));
  }
  return true;
}

bool TranslatePointDragTool::setSpecialShapeColor(OneShape shape) {
  return (shape == m_snapTarget);
}

bool TranslatePointDragTool::setSpecialGridColor(int gId, bool isVertical) {
  return gId == ((isVertical) ? m_snapVGrid : m_snapHGrid);
}

void TranslatePointDragTool::draw(QPointF& onePixelLength) {
  if (!m_snapTarget.shapePairP) return;
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // 表示フレームを得る
  int frame = project->getViewFrame();

  if (m_handleSnapped) {
    BezierPointList bPList =
        m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);

    glColor3d(1.0, 0.5, 1.0);
    ReshapeTool::drawControlPoint(m_shape, bPList, m_pointIndex, true,
                                  onePixelLength);
  } else {
    BezierPointList bPList =
        m_snapTarget.shapePairP->getBezierPointList(frame, m_snapTarget.fromTo);

    glColor3d(1.0, 0.0, 1.0);
    for (int p = 0; p < bPList.size(); p++) {
      ReshapeTool::drawControlPoint(m_snapTarget, bPList, p, false,
                                    onePixelLength);
    }
  }
}

//--------------------------------------------------------
//--------------------------------------------------------
// ハンドル操作ツール
//--------------------------------------------------------

TranslateHandleDragTool::TranslateHandleDragTool(int name, QPointF& onePix)
    : m_onePixLength(onePix), m_isInitial(false) {
  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  m_shape = m_layer->getShapePairFromName(name);

  m_pointIndex  = (int)((name % 10000) / 10);
  m_handleIndex = name % 10;  // 2 : firstHandle, 3 : secondHandle

  m_orgPoints   = m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);
  m_wasKeyFrame = m_shape.shapePairP->isFormKey(frame, m_shape.fromTo);
}

//--------------------------------------------------------

TranslateHandleDragTool::TranslateHandleDragTool(OneShape shape, int pointIndex,
                                                 int handleIndex,
                                                 QPointF& onePix)
    : m_onePixLength(onePix)
    , m_shape(shape)
    , m_pointIndex(pointIndex)
    , m_handleIndex(handleIndex)
    , m_isInitial(false) {
  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  m_orgPoints   = m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);
  m_wasKeyFrame = m_shape.shapePairP->isFormKey(frame, m_shape.fromTo);
}

//--------------------------------------------------------

// PenTool用
TranslateHandleDragTool::TranslateHandleDragTool(OneShape shape, int name,
                                                 QPointF& onePix)
    : m_onePixLength(onePix), m_shape(shape), m_isInitial(false) {
  m_pointIndex  = (int)(name / 10);
  m_handleIndex = name % 10;  // 2 : firstHandle, 3 : secondHandle

  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  m_orgPoints = m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);

  m_undoEnabled = false;
}

//--------------------------------------------------------

void TranslateHandleDragTool::onClick(const QPointF& pos, const QMouseEvent*) {
  m_startPos = pos;
}

//--------------------------------------------------------

void TranslateHandleDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  BezierPoint point = m_orgPoints[m_pointIndex];

  QPointF touchedPoint =
      (m_handleIndex == 2) ? point.firstHandle : point.secondHandle;

  // 元のベクトル
  QPointF orgVec = touchedPoint - point.pos;

  // 新しい位置とアングル
  // 変移
  QPointF moveVec = pos - m_startPos;
  // 新しい位置
  QPointF newHandlePoint = touchedPoint + moveVec;
  // 新しいベクトル
  QPointF newVec = newHandlePoint - point.pos;

  m_snapCandidates.clear();
  m_isHandleSnapped = false;

  // 修飾キーに合わせてポイントを編集する
  //  Altキー : 反対側のハンドルに影響しない
  if (e->modifiers() & Qt::AltModifier) {
    // ここで、位置が一致している他のシェイプの頂点があった場合、
    // ハンドルのスナップを可能にする
    if (e->modifiers() & Qt::ControlModifier)
      calculateHandleSnap(point.pos, newHandlePoint);

    if (m_handleIndex == 2)  // firstHandleだけ動かす
      point.firstHandle = newHandlePoint;
    else  // secondHandleだけ動かす
      point.secondHandle = newHandlePoint;
  }
  // Shiftキー：角度変えず、伸ばすだけ
  else if (e->modifiers() & Qt::ShiftModifier && !orgVec.isNull()) {
    double orgLength =
        std::sqrt(orgVec.x() * orgVec.x() + orgVec.y() * orgVec.y());
    double newLength =
        std::sqrt(newVec.x() * newVec.x() + newVec.y() * newVec.y());

    QTransform affine;
    affine = affine.translate(point.pos.x(), point.pos.y());
    affine = affine.scale(newLength / orgLength, newLength / orgLength);
    affine = affine.translate(-point.pos.x(), -point.pos.y());

    if (m_handleIndex == 2)
      point.firstHandle = affine.map(point.firstHandle);
    else
      point.secondHandle = affine.map(point.secondHandle);
  }
  // Ctrlキー：反対側も点対称方向に伸ばす
  // m_isInitial : PenTool用。ポイントを増やしたと同時に伸ばすハンドルは、
  // Ctrlを押して伸ばしているかのようにふるまう。
  else if (e->modifiers() & Qt::ControlModifier || m_isInitial) {
    if (m_handleIndex == 2) {
      point.firstHandle  = newHandlePoint;
      point.secondHandle = point.pos - newVec;
    } else {
      point.secondHandle = newHandlePoint;
      point.firstHandle  = point.pos - newVec;
    }
  }
  // 修飾キーなし：反対側は同期して回転
  else {
    double orgAngle = (orgVec.isNull())
                          ? 0.0
                          : std::atan2(orgVec.y() / m_onePixLength.y(),
                                       orgVec.x() / m_onePixLength.x());
    double newAngle = (newVec.isNull())
                          ? 0.0
                          : std::atan2(newVec.y() / m_onePixLength.y(),
                                       newVec.x() / m_onePixLength.x());

    QTransform affine;
    affine = affine.translate(point.pos.x(), point.pos.y());
    affine = affine.scale(m_onePixLength.x(), m_onePixLength.y());
    affine = affine.rotateRadians(newAngle - orgAngle);
    affine = affine.scale(1.0 / m_onePixLength.x(), 1.0 / m_onePixLength.y());
    affine = affine.translate(-point.pos.x(), -point.pos.y());

    if (m_handleIndex == 2) {
      point.firstHandle  = newHandlePoint;
      point.secondHandle = affine.map(point.secondHandle);
    } else {
      point.secondHandle = newHandlePoint;
      point.firstHandle  = affine.map(point.firstHandle);
    }
  }

  BezierPointList newBPList = m_orgPoints;

  newBPList.replace(m_pointIndex, point);

  // キーを格納
  m_shape.shapePairP->setFormKey(frame, m_shape.fromTo, newBPList);
  // 更新シグナルをエミットする
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();

  // 変更されるフレームをinvalidate
  int start, end;
  m_shape.shapePairP->getFormKeyRange(start, end, frame, m_shape.fromTo);
  IwTriangleCache::instance()->invalidateCache(
      start, end, m_project->getParentShape(m_shape.shapePairP));
}

//--------------------------------------------------------

bool TranslateHandleDragTool::onRelease(const QPointF& pos,
                                        const QMouseEvent* e) {
  if (pos == m_startPos) return false;

  // PenToolの場合return
  if (!m_undoEnabled) return true;

  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  QList<OneShape> shapes;
  // QList<IwShape*> shapes;
  shapes.push_back(m_shape);

  QList<QMap<int, BezierPointList>> beforePoints;
  QMap<int, BezierPointList> bpMap;
  bpMap[frame] = m_orgPoints;
  beforePoints.push_back(bpMap);

  QList<QMap<int, BezierPointList>> afterPoints;
  QMap<int, BezierPointList> afterBpMap;
  afterBpMap[frame] =
      m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);
  // afterBpMap[frame] = m_shape->getBezierPointList(frame);
  afterPoints.push_back(afterBpMap);

  QList<bool> wasKey;
  wasKey.push_back(m_wasKeyFrame);

  // Transform Tool用のUndoを使いまわす
  // Undoに登録 同時にredoが呼ばれるが、最初はフラグで回避する
  IwUndoManager::instance()->push(new TransformDragToolUndo(
      shapes, beforePoints, afterPoints, wasKey, m_project, frame));
  return true;
}

//--------------------------------------------------------

void TranslateHandleDragTool::calculateHandleSnap(const QPointF pointPos,
                                                  QPointF& handlePos) {
  // 現在のフレーム
  int frame             = m_project->getViewFrame();
  IwLayer* currentLayer = IwApp::instance()->getCurrentLayer()->getLayer();

  double thresholdPixels = 10.0;

  QPointF thres_dist = thresholdPixels * m_onePixLength;

  // ロック情報を得る
  bool fromToVisible[2];
  for (int fromTo = 0; fromTo < 2; fromTo++)
    fromToVisible[fromTo] =
        m_project->getViewSettings()->isFromToVisible(fromTo);

  double minimumDist  = (thres_dist.x() + thres_dist.y()) * 0.5;
  double minimumDist2 = minimumDist * minimumDist;

  QPointF tmpSnappedPos;

  // 各レイヤについて
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    // レイヤを取得
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;

    bool isCurrent = (layer == currentLayer);
    // 現在のレイヤが非表示ならcontinue
    // ただしカレントレイヤの場合は表示する
    if (!isCurrent && !layer->isVisibleInViewer()) continue;

    for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
      ShapePair* shapePair = layer->getShapePair(sp);
      if (!shapePair) continue;
      if (!shapePair->isVisible()) continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // ロックされていて非表示ならスキップ
        if (!fromToVisible[fromTo]) continue;
        // ドラッグ中のシェイプならスキップ
        if (m_shape == OneShape(shapePair, fromTo)) continue;

        BezierPointList bpList = shapePair->getBezierPointList(frame, fromTo);
        for (int index = 0; index < bpList.size(); index++) {
          BezierPoint bp = bpList[index];
          // ポイント位置が一致していなければ continue
          if ((pointPos - bp.pos).manhattanLength() > 0.0001) continue;
          // 候補に入れる
          m_snapCandidates.append({OneShape(shapePair, fromTo), index});

          double dist2;
          // ハンドル１つめ
          QPointF VecToFirstHandle(bp.firstHandle - handlePos);
          dist2 = QPointF::dotProduct(VecToFirstHandle, VecToFirstHandle);
          if (dist2 < minimumDist2) {
            tmpSnappedPos     = bp.firstHandle;
            minimumDist2      = dist2;
            m_isHandleSnapped = true;
          }
          // ハンドル２つめ
          QPointF VecToSecondHandle(bp.secondHandle - handlePos);
          dist2 = QPointF::dotProduct(VecToSecondHandle, VecToSecondHandle);
          if (dist2 < minimumDist2) {
            tmpSnappedPos     = bp.secondHandle;
            minimumDist2      = dist2;
            m_isHandleSnapped = true;
          }
        }
      }
    }
  }

  if (m_isHandleSnapped) handlePos = tmpSnappedPos;
}

//--------------------------------------------------------

void TranslateHandleDragTool::draw(QPointF& onePixelLength) {
  if (m_snapCandidates.isEmpty()) return;
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // 表示フレームを得る
  int frame = project->getViewFrame();

  glColor3d(1.0, 0.0, 1.0);
  for (auto snapCandidate : m_snapCandidates) {
    BezierPointList bPList = snapCandidate.shape.shapePairP->getBezierPointList(
        frame, snapCandidate.shape.fromTo);
    ReshapeTool::drawControlPoint(snapCandidate.shape, bPList,
                                  snapCandidate.pointIndex, true,
                                  onePixelLength);
  }

  if (m_isHandleSnapped) {
    BezierPointList bPList =
        m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);

    glColor3d(1.0, 0.5, 1.0);
    glLineWidth(1.5);
    ReshapeTool::drawControlPoint(m_shape, bPList, m_pointIndex, true,
                                  onePixelLength);
    glLineWidth(1);
  }
}
