//--------------------------------------------------------
// Transform Tool用 の ドラッグツール
// シェイプ変形ツール
//--------------------------------------------------------
#include "transformdragtool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwundomanager.h"

#include "transformtool.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"

#include "iwlayerhandle.h"
#include "iwlayer.h"
#include "iwtrianglecache.h"

#include <QTransform>
#include <QMouseEvent>

namespace {
// MultiFrameモードがONかどうかを返す
bool isMultiFrameActivated() {
  return false;
  // return
  // CommandManager::instance()->getAction(VMI_MultiFrameMode)->isChecked();
}
};  // namespace

//--------------------------------------------------------

TransformDragTool::TransformDragTool(OneShape shape, TransformHandleId handleId,
                                     const QList<OneShape>& selectedShapes)
    : m_grabbingShape(shape), m_handleId(handleId), m_shapes(selectedShapes) {
  m_project = IwApp::instance()->getCurrentProject()->getProject();
  m_layer   = IwApp::instance()->getCurrentLayer()->getLayer();

  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  // MultiFrameの情報を得る
  QList<int> multiFrameList = m_project->getMultiFrames();

  // 元の形状と、キーフレームだったかどうかを取得する
  for (int s = 0; s < m_shapes.count(); s++) {
    // キーフレームだったかどうか
    m_wasKeyFrame.push_back(
        m_shapes.at(s).shapePairP->isFormKey(frame, m_shapes.at(s).fromTo));

    QMap<int, BezierPointList> formKeyMap;
    // まず、編集中のフレーム
    formKeyMap[frame] = m_shapes.at(s).shapePairP->getBezierPointList(
        frame, m_shapes.at(s).fromTo);
    // 続いて、MultiFrameがONのとき、MultiFrameで、かつキーフレームを格納
    if (isMultiFrameActivated()) {
      for (int mf = 0; mf < multiFrameList.size(); mf++) {
        int mframe = multiFrameList.at(mf);
        // カレントフレームは飛ばす
        if (mframe == frame) continue;
        // キーフレームなら、登録
        if (m_shapes.at(s).shapePairP->isFormKey(mframe, m_shapes.at(s).fromTo))
          formKeyMap[mframe] = m_shapes.at(s).shapePairP->getBezierPointList(
              mframe, m_shapes.at(s).fromTo);
      }
    }

    // 元の形状の登録
    m_orgPoints.append(formKeyMap);
  }

  // TransformOptionの情報を得るために、TransformToolへのポインタを控えておく
  m_tool = dynamic_cast<TransformTool*>(IwTool::getTool("T_Transform"));
}

//--------------------------------------------------------
// つかんだハンドルの反対側のハンドル位置を返す
//--------------------------------------------------------
QPointF TransformDragTool::getOppositeHandlePos(OneShape shape, int frame) {
  // シェイプが無い場合は今つかんでいるシェイプ
  if (!shape.shapePairP) shape = m_grabbingShape;
  // フレームが-1のときはカレントフレーム
  if (frame == -1) frame = m_project->getViewFrame();

  QRectF box = shape.shapePairP->getBBox(frame, shape.fromTo);

  // 基準の点を返す
  switch (m_handleId) {
  case Handle_BottomRight:
    return box.topLeft();
    break;
  case Handle_Right:
  case Handle_RightEdge:
    return QPointF(box.left(), box.center().y());
    break;
  case Handle_TopRight:
    return box.bottomLeft();
    break;
  case Handle_Top:
  case Handle_TopEdge:
    return QPointF(box.center().x(), box.bottom());
    break;
  case Handle_TopLeft:
    return box.bottomRight();
    break;
  case Handle_Left:
  case Handle_LeftEdge:
    return QPointF(box.right(), box.center().y());
    break;
  case Handle_BottomLeft:
    return box.topRight();
    break;
  case Handle_Bottom:
  case Handle_BottomEdge:
    return QPointF(box.center().x(), box.top());
    break;
  }
  // 保険
  return box.center();
}

//--------------------------------------------------------
// シェイプの中心位置を返す
//--------------------------------------------------------
QPointF TransformDragTool::getCenterPos(OneShape shape, int frame) {
  // シェイプが無い場合は今つかんでいるシェイプ
  if (!shape.shapePairP) shape = m_grabbingShape;
  // フレームが-1のときはカレントフレーム
  if (frame == -1) frame = m_project->getViewFrame();

  QRectF box = shape.shapePairP->getBBox(frame, shape.fromTo);

  return box.center();
}

//--------------------------------------------------------

void TransformDragTool::onRelease(const QPointF&, const QMouseEvent*) {
  QList<QMap<int, BezierPointList>> afterPoints;
  // 変形後のシェイプを取得
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // シェイプを取得
    OneShape shape = m_shapes.at(s);
    // IwShape* shape = m_shapes.at(s);
    QList<int> frameList = m_orgPoints.at(s).keys();
    QMap<int, BezierPointList> afterFormKeyMap;
    for (int f = 0; f < frameList.size(); f++) {
      int tmpFrame = frameList.at(f);
      afterFormKeyMap[tmpFrame] =
          shape.shapePairP->getBezierPointList(tmpFrame, shape.fromTo);
    }

    afterPoints.push_back(afterFormKeyMap);
  }

  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  // Undoに登録 同時にredoが呼ばれるが、最初はフラグで回避する
  IwUndoManager::instance()->push(new TransformDragToolUndo(
      m_shapes, m_orgPoints, afterPoints, m_wasKeyFrame, m_project, frame));

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  // 変更されるフレームをinvalidate
  for (auto shape : m_shapes) {
    int start, end;
    shape.shapePairP->getFormKeyRange(start, end, frame, shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, m_project->getParentShape(shape.shapePairP));
  }
}

//--------------------------------------------------------

//--------------------------------------------------------
//--------------------------------------------------------
// ハンドルCtrlクリック   → 回転モード (＋Shiftで45度ずつ)
//--------------------------------------------------------

RotateDragTool::RotateDragTool(OneShape shape, TransformHandleId handleId,
                               const QList<OneShape>& selectedShapes,
                               const QPointF& onePix)
    : TransformDragTool(shape, handleId, selectedShapes)
    , m_onePixLength(onePix) {}

//--------------------------------------------------------

void RotateDragTool::onClick(const QPointF& pos, const QMouseEvent* e) {
  // 最初の角度を得る
  if (m_tool->IsRotateAroundCenter())
    m_anchorPos = getCenterPos();
  else
    m_anchorPos = getOppositeHandlePos();
  QPointF initVec = pos - m_anchorPos;

  m_initialAngle = atan2(initVec.x(), initVec.y());

  // 各シェイプ／フレームでの、回転中心の座標を格納
  // 各シェイプについて
  OneShape tmpShape = m_grabbingShape;

  int tmpFrame = m_project->getViewFrame();
  for (int s = 0; s < m_shapes.size(); s++) {
    // シェイプごとに回転中心を持つ場合、シェイプを切り替え
    if (m_tool->IsShapeIndependentTransforms()) tmpShape = m_shapes.at(s);

    QList<int> frames = m_orgPoints.at(s).keys();
    QMap<int, QPointF> centerPosMap;
    // 各フレームについて
    for (int f = 0; f < frames.size(); f++) {
      // フレームごとに回転中心が異なる場合、フレームを切り替え
      if (m_tool->IsFrameIndependentTransforms()) tmpFrame = frames.at(f);

      // 回転中心がBBoxの中心か、ハンドルの反対側か
      if (m_tool->IsRotateAroundCenter())
        centerPosMap[frames.at(f)] = getCenterPos(tmpShape, tmpFrame);
      else
        centerPosMap[frames.at(f)] = getOppositeHandlePos(tmpShape, tmpFrame);
    }
    // 回転中心を格納
    m_centerPos.append(centerPosMap);
  }
}

//--------------------------------------------------------

void RotateDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // 新たな回転角
  QPointF newVec = pos - m_anchorPos;

  double rotAngle = atan2(newVec.x(), newVec.y()) - m_initialAngle;

  double rotDegree = rotAngle * 180.0 / M_PI;

  // Shiftが押されていたら、45度ずつ回転
  if (e->modifiers() & Qt::ShiftModifier) {
    while (rotDegree < 0) rotDegree += 360.0;

    int pizzaIndex = (int)(rotDegree / 45.0);

    double amari = rotDegree - (double)pizzaIndex * 45.0;

    if (amari > 22.5f) pizzaIndex += 1;

    rotDegree = (double)pizzaIndex * 45.0;
  }

  // 各シェイプに変形を適用
  for (int s = 0; s < m_orgPoints.size(); s++) {
    // シェイプを得る
    OneShape shape = m_shapes.at(s);
    // IwShape* shape = m_shapes.at(s);
    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, QPointF> centerPosMap        = m_centerPos.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // これから変形を行う形状データ
      BezierPointList scaledBPointList = i.value();
      // 回転中心
      QPointF tmpCenter = centerPosMap.value(frame);
      // 回転に合わせて変形を適用していく
      QTransform affine;
      affine = affine.translate(tmpCenter.x(), tmpCenter.y());
      affine = affine.scale(m_onePixLength.x(), m_onePixLength.y());
      affine = affine.rotate(-rotDegree);
      affine = affine.scale(1.0 / m_onePixLength.x(), 1.0 / m_onePixLength.y());
      affine = affine.translate(-tmpCenter.x(), -tmpCenter.y());
      // 変形
      for (int bp = 0; bp < scaledBPointList.count(); bp++) {
        scaledBPointList[bp].pos = affine.map(scaledBPointList.at(bp).pos);
        scaledBPointList[bp].firstHandle =
            affine.map(scaledBPointList.at(bp).firstHandle);
        scaledBPointList[bp].secondHandle =
            affine.map(scaledBPointList.at(bp).secondHandle);
      }

      // キーを格納
      shape.shapePairP->setFormKey(frame, shape.fromTo, scaledBPointList);

      ++i;
    }
  }
}

//--------------------------------------------------------
//--------------------------------------------------------
// 縦横ハンドルAltクリック　  → Shear変形モード（＋Shiftで平行に変形）
//--------------------------------------------------------

ShearDragTool::ShearDragTool(OneShape shape, TransformHandleId handleId,
                             const QList<OneShape>& selectedShapes)
    : TransformDragTool(shape, handleId, selectedShapes) {}

//--------------------------------------------------------

void ShearDragTool::onClick(const QPointF& pos, const QMouseEvent*) {
  m_startPos = pos;

  // 各シェイプ／フレームでの、シアー変形中心の座標を格納
  // 各シェイプについて
  OneShape tmpShape = m_grabbingShape;
  int tmpFrame      = m_project->getViewFrame();
  for (int s = 0; s < m_shapes.size(); s++) {
    // シェイプごとに回転中心を持つ場合、シェイプを切り替え
    if (m_tool->IsShapeIndependentTransforms()) tmpShape = m_shapes.at(s);

    QList<int> frames = m_orgPoints.at(s).keys();
    QMap<int, QPointF> anchorPosMap;
    // 各フレームについて
    for (int f = 0; f < frames.size(); f++) {
      // フレームごとに回転中心が異なる場合、フレームを切り替え
      if (m_tool->IsFrameIndependentTransforms()) tmpFrame = frames.at(f);
      // ハンドルの反対側がシアー変形の中心
      anchorPosMap[frames.at(f)] = getOppositeHandlePos(tmpShape, tmpFrame);
    }
    // 回転中心を格納
    m_anchorPos.append(anchorPosMap);
  }

  // 変形のフラグを得る
  // 左右のハンドルをクリック：横に拡大縮小、縦にシアー変形
  // 上下のハンドルをクリック：縦に拡大縮小、横にシアー変形
  if (m_handleId == Handle_Right || m_handleId == Handle_Left)
    m_isVertical = true;
  else
    m_isVertical = false;

  if (m_handleId == Handle_Left || m_handleId == Handle_Top)
    m_isInv = true;
  else
    m_isInv = false;
}

//--------------------------------------------------------

void ShearDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // 各シェイプに変形を適用
  for (int s = 0; s < m_orgPoints.size(); s++) {
    // シェイプを得る
    OneShape shape = m_shapes.at(s);

    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, QPointF> anchorPosMap        = m_anchorPos.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // これから変形を行う形状データ
      BezierPointList shearedBPointList = i.value();
      // シアー変形中心
      QPointF tmpAnchor = anchorPosMap.value(frame);

      QTransform affine;

      QPointF scale(1.0, 1.0);
      if (!(e->modifiers() & Qt::ShiftModifier)) {
        if (m_isVertical)
          scale.setX((pos.x() - tmpAnchor.x()) /
                     (m_startPos.x() - tmpAnchor.x()));
        else
          scale.setY((pos.y() - tmpAnchor.y()) /
                     (m_startPos.y() - tmpAnchor.y()));
      }

      // シアー位置
      QPointF shear(0.0, 0.0);
      if (m_isVertical) {
        shear.setY(pos.y() - m_startPos.y());
        shear.setY(shear.y() / (pos.x() - tmpAnchor.x()));
      } else {
        shear.setX(pos.x() - m_startPos.x());
        shear.setX(shear.x() / (pos.y() - tmpAnchor.y()));
      }

      affine = affine.translate(tmpAnchor.x(), tmpAnchor.y());
      affine = affine.shear(shear.x(), shear.y());
      affine = affine.scale(scale.x(), scale.y());
      affine = affine.translate(-tmpAnchor.x(), -tmpAnchor.y());

      // 変形
      for (int bp = 0; bp < shearedBPointList.count(); bp++) {
        shearedBPointList[bp].pos = affine.map(shearedBPointList.at(bp).pos);
        shearedBPointList[bp].firstHandle =
            affine.map(shearedBPointList.at(bp).firstHandle);
        shearedBPointList[bp].secondHandle =
            affine.map(shearedBPointList.at(bp).secondHandle);
      }

      // キーを格納
      shape.shapePairP->setFormKey(frame, shape.fromTo, shearedBPointList);

      ++i;
    }
  }
}

//--------------------------------------------------------
//--------------------------------------------------------
// 斜めハンドルAltクリック　  → 台形変形モード（＋Shiftで対称変形）
//--------------------------------------------------------

TrapezoidDragTool::TrapezoidDragTool(OneShape shape, TransformHandleId handleId,
                                     const QList<OneShape>& selectedShapes)
    : TransformDragTool(shape, handleId, selectedShapes) {}

//--------------------------------------------------------

void TrapezoidDragTool::onClick(const QPointF& pos, const QMouseEvent*) {
  m_startPos = pos;

  // 各シェイプ／フレームでの、台形変形の基準となるバウンディングボックスを格納
  // 各シェイプについて
  OneShape tmpShape = m_grabbingShape;
  int tmpFrame      = m_project->getViewFrame();
  for (int s = 0; s < m_shapes.size(); s++) {
    // シェイプごとに基準バウンディングボックスが異なる場合、シェイプを切り替え
    if (m_tool->IsShapeIndependentTransforms()) tmpShape = m_shapes.at(s);

    QList<int> frames = m_orgPoints.at(s).keys();
    QMap<int, QRectF> bBoxMap;
    // 各フレームについて
    for (int f = 0; f < frames.size(); f++) {
      // フレームごとに基準バウンディングボックスが異なる場合、フレームを切り替え
      if (m_tool->IsFrameIndependentTransforms()) tmpFrame = frames.at(f);

      bBoxMap[frames.at(f)] =
          tmpShape.shapePairP->getBBox(tmpFrame, tmpShape.fromTo);
    }
    // 基準バウンディングボックスを格納
    m_initialBox.append(bBoxMap);
  }
}

//--------------------------------------------------------

void TrapezoidDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // 各頂点の変移ベクトルを用意
  QPointF bottomRightVec(0.0, 0.0);
  QPointF topRightVec(0.0, 0.0);
  QPointF topLeftVec(0.0, 0.0);
  QPointF bottomLeftVec(0.0, 0.0);

  bool isShiftPressed = (e->modifiers() & Qt::ShiftModifier);
  // ハンドルに合わせて変移ベクトルを移動
  switch (m_handleId) {
  case Handle_BottomRight:
    bottomRightVec = pos - m_startPos;
    if (isShiftPressed)
      bottomLeftVec = QPointF(-bottomRightVec.x(), bottomRightVec.y());
    break;
  case Handle_TopRight:
    topRightVec = pos - m_startPos;
    if (isShiftPressed)
      bottomRightVec = QPointF(topRightVec.x(), -topRightVec.y());
    break;
  case Handle_TopLeft:
    topLeftVec = pos - m_startPos;
    if (isShiftPressed) topRightVec = QPointF(-topLeftVec.x(), topLeftVec.y());
    break;
  case Handle_BottomLeft:
    bottomLeftVec = pos - m_startPos;
    if (isShiftPressed)
      topLeftVec = QPointF(bottomLeftVec.x(), -bottomLeftVec.y());
    break;
  }

  // 各シェイプの各ポイントについて、４つの変移ベクトルの線形補間を足すことで移動させる

  // 各シェイプに変形を適用
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // シェイプを得る
    OneShape shape = m_shapes.at(s);
    // IwShape* shape = m_shapes.at(s);
    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, QRectF> bBoxMap              = m_initialBox.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // これから変形を行う形状データ
      BezierPointList trapezoidedBPointList = i.value();
      // 変形の基準バウンディングボックス
      QRectF tmpBBox = bBoxMap.value(frame);
      // 変形
      for (int bp = 0; bp < trapezoidedBPointList.count(); bp++) {
        trapezoidedBPointList[bp].pos =
            map(tmpBBox, bottomRightVec, topRightVec, topLeftVec, bottomLeftVec,
                trapezoidedBPointList[bp].pos);
        trapezoidedBPointList[bp].firstHandle =
            map(tmpBBox, bottomRightVec, topRightVec, topLeftVec, bottomLeftVec,
                trapezoidedBPointList[bp].firstHandle);
        trapezoidedBPointList[bp].secondHandle =
            map(tmpBBox, bottomRightVec, topRightVec, topLeftVec, bottomLeftVec,
                trapezoidedBPointList[bp].secondHandle);
      }

      // キーを格納
      shape.shapePairP->setFormKey(frame, shape.fromTo, trapezoidedBPointList);

      ++i;
    }
  }
}

//--------------------------------------------------------
// ４つの変移ベクトルの線形補間を足すことで移動させる
//--------------------------------------------------------

QPointF TrapezoidDragTool::map(const QRectF& bBox,
                               const QPointF& bottomRightVec,
                               const QPointF& topRightVec,
                               const QPointF& topLeftVec,
                               const QPointF& bottomLeftVec,
                               QPointF& targetPoint) {
  double hRatio = (targetPoint.x() - bBox.left()) / bBox.width();
  double vRatio = (targetPoint.y() - bBox.top()) / bBox.height();

  QPointF moveVec = (1.0 - hRatio) * (1.0 - vRatio) * topLeftVec +
                    (1.0 - hRatio) * vRatio * bottomLeftVec +
                    hRatio * (1.0 - vRatio) * topRightVec +
                    hRatio * vRatio * bottomRightVec;

  return targetPoint + moveVec;
}

//--------------------------------------------------------
// ハンドル普通のクリック → 拡大／縮小モード
//--------------------------------------------------------

ScaleDragTool::ScaleDragTool(OneShape shape, TransformHandleId handleId,
                             const QList<OneShape>& selectedShapes)
    : TransformDragTool(shape, handleId, selectedShapes)
    , m_scaleHorizontal(true)
    , m_scaleVertical(true) {
  if (m_handleId == Handle_Right || m_handleId == Handle_Left ||
      m_handleId == Handle_RightEdge || m_handleId == Handle_LeftEdge)
    m_scaleVertical = false;
  else if (m_handleId == Handle_Top || m_handleId == Handle_Bottom ||
           m_handleId == Handle_TopEdge || m_handleId == Handle_BottomEdge)
    m_scaleHorizontal = false;
}

//--------------------------------------------------------

void ScaleDragTool::onClick(const QPointF& pos, const QMouseEvent* e) {
  m_startPos = pos;
  // 拡大縮小の基準位置
  if (m_tool->IsScaleAroundCenter())
    m_anchorPos = getCenterPos();
  else
    m_anchorPos = getOppositeHandlePos();

  // 各シェイプ／フレームでの、拡大縮小の中心の座標を格納
  // 各シェイプについて
  OneShape tmpShape = m_grabbingShape;
  int tmpFrame      = m_project->getViewFrame();
  for (int s = 0; s < m_shapes.size(); s++) {
    // シェイプごとに拡大縮小の中心を持つ場合、シェイプを切り替え
    if (m_tool->IsShapeIndependentTransforms()) tmpShape = m_shapes.at(s);

    QList<int> frames = m_orgPoints.at(s).keys();
    QMap<int, QPointF> centerPosMap;
    // 各フレームについて
    for (int f = 0; f < frames.size(); f++) {
      // フレームごとに回転中心が異なる場合、フレームを切り替え
      if (m_tool->IsFrameIndependentTransforms()) tmpFrame = frames.at(f);

      // 回転中心がBBoxの中心か、ハンドルの反対側か
      if (m_tool->IsScaleAroundCenter())
        centerPosMap[frames.at(f)] = getCenterPos(tmpShape, tmpFrame);
      else
        centerPosMap[frames.at(f)] = getOppositeHandlePos(tmpShape, tmpFrame);
    }
    // 回転中心を格納
    m_scaleCenter.append(centerPosMap);
  }
}

//--------------------------------------------------------

void ScaleDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // 縦横の変形倍率を計算する
  QPointF orgVector = pos - m_anchorPos;
  QPointF dstVector = m_startPos - m_anchorPos;

  QPointF scaleRatio =
      QPointF(orgVector.x() / dstVector.x(), orgVector.y() / dstVector.y());

  // ハンドルによって倍率を制限する
  if (!m_scaleVertical)
    scaleRatio.setY((e->modifiers() & Qt::ShiftModifier) ? scaleRatio.x()
                                                         : 1.0);
  else if (!m_scaleHorizontal)
    scaleRatio.setX((e->modifiers() & Qt::ShiftModifier) ? scaleRatio.y()
                                                         : 1.0);
  else {
    if (e->modifiers() & Qt::ShiftModifier) {
      double largerScale = (abs(scaleRatio.x()) > abs(scaleRatio.y()))
                               ? scaleRatio.x()
                               : scaleRatio.y();
      scaleRatio         = QPointF(largerScale, largerScale);
    }
  }

  // TODO: 倍率が０に近すぎる場合はreturn

  // 各シェイプに変形を適用
  for (int s = 0; s < m_orgPoints.size(); s++) {
    // シェイプを取得
    OneShape shape = m_shapes.at(s);

    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, QPointF> scaleCenterMap      = m_scaleCenter.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // これから変形を行う形状データ
      BezierPointList scaledBPointList = i.value();
      // 拡大縮小の中心
      QPointF tmpCenter = scaleCenterMap.value(frame);

      // 倍率に合わせて変形を適用していく
      QTransform affine;
      affine = affine.translate(tmpCenter.x(), tmpCenter.y());
      affine = affine.scale(scaleRatio.x(), scaleRatio.y());
      affine = affine.translate(-tmpCenter.x(), -tmpCenter.y());
      // 変形
      for (int bp = 0; bp < scaledBPointList.count(); bp++) {
        scaledBPointList[bp].pos = affine.map(scaledBPointList.at(bp).pos);
        scaledBPointList[bp].firstHandle =
            affine.map(scaledBPointList.at(bp).firstHandle);
        scaledBPointList[bp].secondHandle =
            affine.map(scaledBPointList.at(bp).secondHandle);
      }
      // キーを格納
      shape.shapePairP->setFormKey(frame, shape.fromTo, scaledBPointList);

      ++i;
    }
  }
}

//--------------------------------------------------------
// シェイプのハンドル以外をクリック  → 移動モード（＋Shiftで平行移動）
//--------------------------------------------------------

TranslateDragTool::TranslateDragTool(OneShape shape, TransformHandleId handleId,
                                     const QList<OneShape>& selectedShapes)
    : TransformDragTool(shape, handleId, selectedShapes) {}

//--------------------------------------------------------

void TranslateDragTool::onClick(const QPointF& pos, const QMouseEvent*) {
  m_startPos = pos;
}

//--------------------------------------------------------

void TranslateDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
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

  // 移動に合わせ変形を適用していく
  QTransform affine;
  affine = affine.translate(moveVec.x(), moveVec.y());

  // 各シェイプに変形を適用
  for (int s = 0; s < m_orgPoints.count(); s++) {
    // シェイプを得る
    OneShape shape = m_shapes.at(s);
    // IwShape* shape = m_shapes.at(s);
    QMap<int, BezierPointList> formKeyMap  = m_orgPoints.at(s);
    QMap<int, BezierPointList>::iterator i = formKeyMap.begin();
    while (i != formKeyMap.end()) {
      int frame = i.key();
      // これから変形を行う形状データ
      BezierPointList translatedBPointList = i.value();
      // 変形
      for (int bp = 0; bp < translatedBPointList.count(); bp++) {
        translatedBPointList[bp].pos =
            affine.map(translatedBPointList.at(bp).pos);
        translatedBPointList[bp].firstHandle =
            affine.map(translatedBPointList.at(bp).firstHandle);
        translatedBPointList[bp].secondHandle =
            affine.map(translatedBPointList.at(bp).secondHandle);
      }

      // キーを格納
      shape.shapePairP->setFormKey(frame, shape.fromTo, translatedBPointList);
      ++i;
    }
  }
}

//---------------------------------------------------
// 以下、Undoコマンド
//---------------------------------------------------

TransformDragToolUndo::TransformDragToolUndo(
    QList<OneShape>& shapes, QList<QMap<int, BezierPointList>>& beforePoints,
    QList<QMap<int, BezierPointList>>& afterPoints, QList<bool>& wasKeyFrame,
    IwProject* project, int frame)
    : m_project(project)
    , m_shapes(shapes)
    , m_beforePoints(beforePoints)
    , m_afterPoints(afterPoints)
    , m_wasKeyFrame(wasKeyFrame)
    , m_frame(frame)
    , m_firstFlag(true) {}

//---------------------------------------------------

TransformDragToolUndo::~TransformDragToolUndo() {}

//---------------------------------------------------

void TransformDragToolUndo::undo() {
  // 各シェイプにポイントをセット
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    // 操作前はキーフレームでなかった場合、キーを消去
    if (!m_wasKeyFrame.at(s))
      shape.shapePairP->removeFormKey(m_frame, shape.fromTo);
    else {
      QMap<int, BezierPointList> beforeFormKeys = m_beforePoints.at(s);
      QMap<int, BezierPointList>::iterator i    = beforeFormKeys.begin();
      while (i != beforeFormKeys.end()) {
        shape.shapePairP->setFormKey(i.key(), shape.fromTo, i.value());
        ++i;
      }
    }
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // 変更されるフレームをinvalidate
    for (auto shape : m_shapes) {
      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }
}

//---------------------------------------------------

void TransformDragToolUndo::redo() {
  // Undo登録時にはredoを行わないよーにする
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  // 各シェイプにポイントをセット
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    QMap<int, BezierPointList> afterFormKeys = m_afterPoints.at(s);
    QMap<int, BezierPointList>::iterator i   = afterFormKeys.begin();
    while (i != afterFormKeys.end()) {
      shape.shapePairP->setFormKey(i.key(), shape.fromTo, i.value());
      ++i;
    }
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();

    // 変更されるフレームをinvalidate
    for (auto shape : m_shapes) {
      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }
}
