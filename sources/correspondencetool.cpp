//--------------------------------------------------------
// Correspondence Tool
// 対応点編集ツール
//--------------------------------------------------------

#include "correspondencetool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwselectionhandle.h"
#include "iwproject.h"
#include "sceneviewer.h"
#include "iwundomanager.h"
#include "viewsettings.h"

#include "iwshapepairselection.h"
#include "iwlayerhandle.h"
#include "iwlayer.h"
#include "reshapetool.h"
#include "iwtrianglecache.h"

#include <QList>
#include <QPointF>
#include <QMouseEvent>

#include <math.h>

//--------------------------------------------------------
// 対応点ドラッグツール
//--------------------------------------------------------
CorrDragTool::CorrDragTool(OneShape shape, int corrPointId,
                           const QPointF& onePix)
    // CorrDragTool::CorrDragTool(IwShape* shape, int corrPointId)
    : m_shape(shape), m_corrPointId(corrPointId), m_onePixLength(onePix) {
  m_project = IwApp::instance()->getCurrentProject()->getProject();
  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  // 元の対応点情報と、キーフレームだったかどうかを取得する
  m_orgCorrs    = shape.shapePairP->getCorrPointList(frame, shape.fromTo);
  m_wasKeyFrame = shape.shapePairP->isCorrKey(frame, shape.fromTo);
}
//--------------------------------------------------------
void CorrDragTool::onClick(const QPointF& pos, const QMouseEvent*) {
  m_startPos = pos;
}
//--------------------------------------------------------
void CorrDragTool::onDrag(const QPointF& pos, const QMouseEvent* e) {
  // 値を格納するリスト
  CorrPointList newCorrs;

  // 現在のフレームを得る
  int frame         = m_project->getViewFrame();
  m_snappedCpId     = -1;
  m_snapTargetShape = OneShape();

  // Shiftが押されている場合、他の対応点も全部スライドさせる
  if (e->modifiers() & Qt::ShiftModifier) {
    // マウス位置に最近傍のベジエ座標を得る
    double nearestBezierPos =
        m_shape.shapePairP->getNearestBezierPos(pos, frame, m_shape.fromTo);

    if (e->modifiers() & Qt::ControlModifier) {
      doSnap(nearestBezierPos, frame);
    }

    // 差分を得る
    double dc = nearestBezierPos - m_orgCorrs.at(m_corrPointId).value;

    // 閉じたシェイプの場合、全部回す
    if (m_shape.shapePairP->isClosed()) {
      for (int c = 0; c < m_orgCorrs.size(); c++) {
        double tmp = m_orgCorrs.at(c).value + dc;
        // 値をクランプ
        if (tmp >= (double)m_shape.shapePairP->getBezierPointAmount())
          tmp -= (double)m_shape.shapePairP->getBezierPointAmount();
        if (tmp < 0.0)
          tmp += (double)m_shape.shapePairP->getBezierPointAmount();

        newCorrs.push_back({tmp, m_orgCorrs.at(c).weight});
      }
    }
    // 開いたシェイプの場合、端点以外をスライド。端についたらそこでストップ
    else {
      int cpAmount = m_shape.shapePairP->getCorrPointAmount();
      // 正方向に移動
      if (dc > 0)
        dc = std::min(dc, m_orgCorrs.at(cpAmount - 1).value -
                              m_orgCorrs.at(cpAmount - 2).value - 0.05);
      // 負方向に移動
      else
        dc = std::max(dc,
                      m_orgCorrs.at(0).value - m_orgCorrs.at(1).value + 0.05);

      newCorrs.push_back(m_orgCorrs.at(0));
      for (int c = 1; c < m_orgCorrs.size() - 1; c++) {
        double tmp = m_orgCorrs.at(c).value + dc;
        newCorrs.push_back({tmp, m_orgCorrs.at(c).weight});
      }
      newCorrs.push_back(m_orgCorrs.last());
    }
  }
  // Shiftが押されていなければ、選択点のみをスライドさせる
  else {
    int beforeIndex = (m_shape.shapePairP->isClosed() && m_corrPointId - 1 < 0)
                          ? m_shape.shapePairP->getCorrPointAmount() - 1
                          : m_corrPointId - 1;
    int afterIndex =
        (m_shape.shapePairP->isClosed() &&
         m_corrPointId + 1 >= m_shape.shapePairP->getCorrPointAmount())
            ? 0
            : m_corrPointId + 1;
    double rangeBefore = m_orgCorrs.at(beforeIndex).value;
    double beforeSpeed = m_shape.shapePairP->getBezierSpeedAt(
        frame, m_shape.fromTo, rangeBefore, 0.001);
    rangeBefore += std::min(0.01, 0.001 / beforeSpeed);
    // 値をクランプ
    if (rangeBefore >= (double)m_shape.shapePairP->getBezierPointAmount())
      rangeBefore -= (double)m_shape.shapePairP->getBezierPointAmount();

    double rangeAfter = m_orgCorrs.at(afterIndex).value;
    double afterSpeed = m_shape.shapePairP->getBezierSpeedAt(
        frame, m_shape.fromTo, rangeAfter, -0.001);
    rangeAfter -= std::min(0.01, 0.001 / afterSpeed);
    // 値をクランプ
    if (rangeAfter < 0.0)
      rangeAfter += (double)m_shape.shapePairP->getBezierPointAmount();

    // マウス位置に最近傍のベジエ座標を得る(範囲つき版)
    double dummy;
    double nearestBezierPos = m_shape.shapePairP->getNearestBezierPos(
        pos, frame, m_shape.fromTo, rangeBefore, rangeAfter, dummy);

    if (e->modifiers() & Qt::ControlModifier) {
      doSnap(nearestBezierPos, frame, rangeBefore, rangeAfter);
    }
    // 値を1つだけ変更
    newCorrs                      = m_orgCorrs;
    newCorrs[m_corrPointId].value = nearestBezierPos;
  }
  //----- 対応点キーフレームを格納
  m_shape.shapePairP->setCorrKey(frame, m_shape.fromTo, newCorrs);
}

//--------------------------------------------------------

void CorrDragTool::doSnap(double& bezierPos, int frame, double rangeBefore,
                          double rangeAfter) {
  double snapCandidateBezierPos = std::round(bezierPos);
  if (rangeAfter > 0. && (snapCandidateBezierPos <= rangeBefore &&
                          snapCandidateBezierPos >= rangeAfter))
    return;

  if (m_shape.shapePairP->isClosed() &&
      (int)snapCandidateBezierPos == m_shape.shapePairP->getBezierPointAmount())
    snapCandidateBezierPos = 0.0;
  QPointF snapPoint = m_shape.shapePairP->getBezierPosFromValue(
      frame, m_shape.fromTo, snapCandidateBezierPos);
  QPointF currentPoint = m_shape.shapePairP->getBezierPosFromValue(
      frame, m_shape.fromTo, bezierPos);

  double thresholdPixels = 10.0;
  QPointF thres_dist     = thresholdPixels * m_onePixLength;
  double minimumDist     = (thres_dist.x() + thres_dist.y()) * 0.5;
  QPointF distVec        = currentPoint - snapPoint;
  if (distVec.x() * distVec.x() + distVec.y() * distVec.y() <
      minimumDist * minimumDist) {
    bezierPos     = snapCandidateBezierPos;
    m_snappedCpId = (int)snapCandidateBezierPos;
    return;
  }

  // 次に、他のシェイプとの交点へのスナップを試みる
  IwLayer* currentLayer = IwApp::instance()->getCurrentLayer()->getLayer();
  // ロック情報を得る
  bool fromToVisible[2];
  for (int fromTo = 0; fromTo < 2; fromTo++)
    fromToVisible[fromTo] =
        m_project->getViewSettings()->isFromToVisible(fromTo);
  OneShape tmpSnapTarget;
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
        // 自分自身のシェイプはスキップ
        if (m_shape.shapePairP == shapePair && m_shape.fromTo == fromTo)
          continue;
        // バウンディングボックスに入っていなかったらスキップ
        QRectF bBox = shapePair->getBBox(frame, fromTo)
                          .adjusted(-thres_dist.x(), -thres_dist.y(),
                                    thres_dist.x(), thres_dist.y());
        if (!bBox.contains(currentPoint)) continue;
        double dist;
        double w =
            shapePair->getNearestBezierPos(currentPoint, frame, fromTo, dist);
        if (dist < minimumDist) {
          minimumDist   = dist;
          tmpSnapTarget = OneShape(shapePair, fromTo);
        }
      }
    }
  }
  if (tmpSnapTarget.shapePairP) {
    // 近傍点の取得を繰り返して交点を出す
    double dist;
    double snappedBezierPos = bezierPos;
    snapPoint               = currentPoint;
    int itr                 = 0;
    while (itr < 10) {
      double w = tmpSnapTarget.shapePairP->getNearestBezierPos(
          snapPoint, frame, tmpSnapTarget.fromTo, dist);
      snapPoint = tmpSnapTarget.shapePairP->getBezierPosFromValue(
          frame, tmpSnapTarget.fromTo, w);
      snappedBezierPos = m_shape.shapePairP->getNearestBezierPos(
          snapPoint, frame, m_shape.fromTo, dist);
      snapPoint = m_shape.shapePairP->getBezierPosFromValue(
          frame, m_shape.fromTo, snappedBezierPos);

      // 交点とする
      if (dist < 0.001) {
        QPointF distVec = currentPoint - snapPoint;
        if (distVec.x() * distVec.x() + distVec.y() * distVec.y() <
            minimumDist * minimumDist) {
          bezierPos         = snappedBezierPos;
          m_snapTargetShape = tmpSnapTarget;
        }
        return;
      }

      itr++;
    }
  }
}

//--------------------------------------------------------
void CorrDragTool::onRelease(const QPointF& /*pos*/, const QMouseEvent*) {
  // 現在のフレームを得る
  int frame = m_project->getViewFrame();
  CorrPointList afterCorrs =
      m_shape.shapePairP->getCorrPointList(frame, m_shape.fromTo);

  bool somethingChanged = false;
  for (int c = 0; c < m_orgCorrs.size(); c++) {
    if (m_orgCorrs[c] == afterCorrs[c])
      continue;
    else {
      somethingChanged = true;
      break;
    }
  }

  if (!somethingChanged) return;

  // Undo登録 同時にredoが呼ばれるが、最初はフラグで回避する
  IwUndoManager::instance()->push(new CorrDragToolUndo(
      m_shape, m_orgCorrs, afterCorrs, m_wasKeyFrame, m_project, frame));

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();

  // 変更されるフレームをinvalidate
  int start, end;
  m_shape.shapePairP->getCorrKeyRange(start, end, frame, m_shape.fromTo);
  IwTriangleCache::instance()->invalidateCache(
      start, end, m_project->getParentShape(m_shape.shapePairP));
}

//--------------------------------------------------------

void CorrDragTool::draw() {
  int frame = m_project->getViewFrame();
  // 自身のシェイプのコントロールポイントにスナップ
  if (m_snappedCpId >= 0) {
    glColor3d(1.0, 0.0, 1.0);
    BezierPointList bPList =
        m_shape.shapePairP->getBezierPointList(frame, m_shape.fromTo);
    for (int p = 0; p < bPList.size(); p++) {
      ReshapeTool::drawControlPoint(m_shape, bPList, p, false, m_onePixLength,
                                    0, true);
      if (p == m_snappedCpId)
        ReshapeTool::drawControlPoint(m_shape, bPList, p, false,
                                      QPointF(3.0 * m_onePixLength));
    }
  }
  // 他シェイプとの交点にスナップの場合は、setSpecialShapeColorでスナップ対象の色を変える
  // else if (m_snapTargetShape.shapePairP) {}
}

//--------------------------------------------------------
// 対応点移動のUndo
//--------------------------------------------------------
CorrDragToolUndo::CorrDragToolUndo(OneShape shape,
                                   // IwShape* & shape,
                                   CorrPointList& beforeCorrs,
                                   CorrPointList& afterCorrs, bool& wasKeyFrame,
                                   IwProject* project, int frame)
    : m_project(project)
    , m_shape(shape)
    , m_beforeCorrs(beforeCorrs)
    , m_afterCorrs(afterCorrs)
    , m_wasKeyFrame(wasKeyFrame)
    , m_firstFlag(true)
    , m_frame(frame) {}
//--------------------------------------------------------
void CorrDragToolUndo::undo() {
  // シェイプに元の対応点情報を格納
  // キーフレームだった場合、対応点情報を差し替え
  if (m_wasKeyFrame)
    m_shape.shapePairP->setCorrKey(m_frame, m_shape.fromTo, m_beforeCorrs);
  // キーフレームじゃなかった場合、キーを消去
  else
    m_shape.shapePairP->removeCorrKey(m_frame, m_shape.fromTo);

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();

    // 変更されるフレームをinvalidate
    int start, end;
    m_shape.shapePairP->getCorrKeyRange(start, end, m_frame, m_shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, m_project->getParentShape(m_shape.shapePairP));
  }
}
//--------------------------------------------------------
void CorrDragToolUndo::redo() {
  // Undo登録時にはredoを行わないよーにする
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }
  // シェイプに対応点をセット
  m_shape.shapePairP->setCorrKey(m_frame, m_shape.fromTo, m_afterCorrs);

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // 変更されるフレームをinvalidate
    int start, end;
    m_shape.shapePairP->getCorrKeyRange(start, end, m_frame, m_shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, m_project->getParentShape(m_shape.shapePairP));
  }
}
//--------------------------------------------------------

//--------------------------------------------------------
// +Altで対応点の追加
//--------------------------------------------------------

void CorrespondenceTool::addCorrPoint(const QPointF& pos, OneShape shape)
// void CorrespondenceTool::addCorrPoint(const QPointF & pos, IwShape * shape)
{
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // 現在のフレームを得る
  int frame = project->getViewFrame();

  AddCorrPointUndo* undo = new AddCorrPointUndo(shape, project);

  bool ok = shape.shapePairP->addCorrPoint(pos, frame, shape.fromTo);

  if (!ok) {
    delete undo;
    return;
  }

  // OKならUNDO格納
  undo->setAfterData();

  // Undo登録 同時にredoが呼ばれるが、最初はフラグで回避する
  IwUndoManager::instance()->push(undo);

  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  // invalidate cache
  IwTriangleCache::instance()->invalidateShapeCache(
      project->getParentShape(shape.shapePairP));
}

//--------------------------------------------------------
// 対応点追加のUndo
//--------------------------------------------------------

AddCorrPointUndo::AddCorrPointUndo(OneShape shape, IwProject* project)
    : m_project(project), m_shape(shape), m_partnerShape(0), m_firstFlag(true) {
  m_beforeData = shape.shapePairP->getCorrData(shape.fromTo);
  // if(shape->getPartner())
  {
    m_partnerShape = OneShape(shape.shapePairP, (shape.fromTo + 1) % 2);
    m_partnerBeforeData =
        m_partnerShape.shapePairP->getCorrData(m_partnerShape.fromTo);
  }
}

void AddCorrPointUndo::setAfterData() {
  m_afterData = m_shape.shapePairP->getCorrData(m_shape.fromTo);
  if (m_partnerShape.shapePairP)
    m_partnerAfterData =
        m_partnerShape.shapePairP->getCorrData(m_partnerShape.fromTo);
}

void AddCorrPointUndo::undo() {
  // シェイプに対応点データを戻す
  m_shape.shapePairP->setCorrData(m_beforeData, m_shape.fromTo);
  if (m_partnerShape.shapePairP)
    m_partnerShape.shapePairP->setCorrData(m_partnerBeforeData,
                                           m_partnerShape.fromTo);

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // invalidate cache
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

void AddCorrPointUndo::redo() {
  // Undo登録時にはredoを行わないよーにする
  if (m_firstFlag) {
    m_firstFlag = false;
    return;
  }

  // シェイプに対応点データを戻す
  m_shape.shapePairP->setCorrData(m_afterData, m_shape.fromTo);
  if (m_partnerShape.shapePairP)
    m_partnerShape.shapePairP->setCorrData(m_partnerAfterData,
                                           m_partnerShape.fromTo);

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    // invalidate cache
    IwTriangleCache::instance()->invalidateShapeCache(
        m_project->getParentShape(m_shape.shapePairP));
  }
}

//--------------------------------------------------------
// 　ツール本体
//--------------------------------------------------------

CorrespondenceTool::CorrespondenceTool()
    : IwTool("T_Correspondence")
    , m_selection(IwShapePairSelection::instance())
    , m_dragTool(nullptr)
    , m_ctrlPressed(false) {}

//--------------------------------------------------------

CorrespondenceTool::~CorrespondenceTool() {}

//--------------------------------------------------------

bool CorrespondenceTool::leftButtonDown(const QPointF& pos,
                                        const QMouseEvent* e) {
  if (!m_viewer) return false;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return false;

  int name = m_viewer->pick(e->pos());

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return false;

  OneShape shape = layer->getShapePairFromName(name);

  // パートナーの選択が不可能かどうか
  bool partnerIsVisible =
      project->getViewSettings()->isFromToVisible((shape.fromTo == 0) ? 1 : 0);

  // シェイプがクリックされていた場合
  // ハンドルがクリックされていたら→ドラッグツールへ
  // ＋Shiftかつ、未選択なら追加選択
  // ＋Altかつ、選択ずみなら対応点追加
  // それ以外の場合、シェイプを単独選択
  if (shape.shapePairP) {
    m_selection->makeCurrent();
    // ハンドルがクリックされていたら→ドラッグツールへ
    // ハンドルの名前は下一桁に1が入っておる
    if ((name % 10) != 0) {
      int corrPointId = (int)(name / 10) % 1000;
      m_selection->setActiveCorrPoint(shape, corrPointId);
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();

      // オープンなシェイプで、端点は動かせない
      if (!shape.shapePairP->isClosed() &&
          (corrPointId == 0 ||
           corrPointId == shape.shapePairP->getCorrPointAmount() - 1))
        return true;

      // ドラッグツールを作る
      m_dragTool =
          new CorrDragTool(shape, corrPointId, m_viewer->getOnePixelLength());
      m_dragTool->onClick(pos, e);
      return true;
    }
    // ＋Shiftかつ、未選択なら追加選択
    else if (e->modifiers() & Qt::ShiftModifier) {
      if (m_selection->isSelected(shape))
        return false;
      else {
        m_selection->addShape(shape);
        // もし、そのシェイプにパートナーがあり、未選択なら追加する
        if (!m_selection->isSelected(shape.getPartner()) && partnerIsVisible)
          m_selection->addShape(shape.getPartner());
      }
    }
    // ＋Altかつ、選択ずみなら対応点追加
    else if (e->modifiers() & Qt::AltModifier &&
             m_selection->isSelected(shape)) {
      // 対応点追加コマンド
      addCorrPoint(pos, shape);

    }
    // それ以外の場合、シェイプを単独選択
    else {
      m_selection->selectOneShape(shape);
      // もし、そのシェイプにパートナーがあり、未選択なら追加する
      if (!m_selection->isSelected(shape.getPartner()) && partnerIsVisible)
        m_selection->addShape(shape.getPartner());
    }
    // シグナルをエミット
    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
    return true;
  }
  // 選択の解除
  else {
    m_selection->selectNone();
    // シグナルをエミット
    IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
    return true;
  }
}

//--------------------------------------------------------

bool CorrespondenceTool::leftButtonDrag(const QPointF& pos,
                                        const QMouseEvent* e) {
  if (m_dragTool) {
    m_viewer->setFocus();
    m_dragTool->onDrag(pos, e);
    return true;
  }

  return false;
}

//--------------------------------------------------------

bool CorrespondenceTool::leftButtonUp(const QPointF& pos,
                                      const QMouseEvent* e) {
  if (m_dragTool) {
    m_dragTool->onRelease(pos, e);
    delete m_dragTool;
    m_dragTool = 0;
    return true;
  }

  return false;
}

//--------------------------------------------------------

void CorrespondenceTool::leftButtonDoubleClick(const QPointF&,
                                               const QMouseEvent*) {}

//--------------------------------------------------------

// 対応点と分割された対応シェイプ、シェイプ同士の連結を表示する
void CorrespondenceTool::draw() {
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // 表示フレームを得る
  int frame = project->getViewFrame();

  // 選択シェイプが無かったらreturn
  if (m_selection->isEmpty()) return;

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  QList<ShapePair*>
      alreadyDrawnLinkList;  // 既に対応線を描いたシェイプを記録しておく

  bool isSnapping = (m_dragTool != nullptr) && m_ctrlPressed;
  // join lines are drawn when from and to shapes are both drawn
  bool isJoinLinesVisible = project->getViewSettings()->isFromToVisible(0) &&
                            project->getViewSettings()->isFromToVisible(1);

  // 各選択シェイプの輪郭を描く
  for (int s = 0; s < m_selection->getCount(); s++) {
    // シェイプを取得
    OneShape shape = m_selection->getShape(s);

    drawCorrLine(shape);

    // 対応線の描画
    if (isJoinLinesVisible &&
        !alreadyDrawnLinkList.contains(shape.shapePairP) && !isSnapping) {
      drawJoinLine(shape.shapePairP);

      alreadyDrawnLinkList.push_back(shape.shapePairP);
    }
  }

  // スナップ操作中はコントロールポイントを描く。対応線は描かない
  if (isSnapping) {
    m_dragTool->draw();
  }
}

//--------------------------------------------------------

int CorrespondenceTool::getCursorId(const QMouseEvent* e) {
  if (!m_viewer) return ToolCursor::ArrowCursor;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return ToolCursor::ArrowCursor;

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return ToolCursor::ArrowCursor;

  int name      = m_viewer->pick(e->pos());
  m_ctrlPressed = (e->modifiers() & Qt::ControlModifier);

  OneShape shape = layer->getShapePairFromName(name);
  if (m_selection->isEmpty()) {
    IwApp::instance()->showMessageOnStatusBar(tr("[Click shape] to select."));
    return ToolCursor::ArrowCursor;
  }

  IwApp::instance()->showMessageOnStatusBar(
      tr("[Drag corr point] to move. [+Shift] to move all points. [+Ctrl] with "
         "snapping. [Alt + Click] to insert a new corr point."));

  // 選択シェイプじゃなかったら普通のカーソル
  if (!shape.shapePairP || !m_selection->isSelected(shape))
    return ToolCursor::ArrowCursor;

  // ハンドルがクリックされていたら→ドラッグツールへ
  // ハンドルの名前は下一桁に1が入っておる
  if ((name % 10) != 0) return ToolCursor::BlackArrowCursor;

  // ポイント以外で、Altが押されていたら、対応点追加のカーソル
  if (e->modifiers() & Qt::AltModifier) return ToolCursor::BlackArrowAddCursor;

  return ToolCursor::ArrowCursor;
}

//--------------------------------------------------------
// アクティブ時、Joinしているシェイプの片方だけが選択されていたら、
// もう片方も選択する
//--------------------------------------------------------

void CorrespondenceTool::onActivate() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // ただし、Lockされていない場合。そこで、Lockの情報を得る
  bool fromToVisible[2];
  for (int ft = 0; ft < 2; ft++)
    fromToVisible[ft] = project->getViewSettings()->isFromToVisible(ft);

  // 選択シェイプが無かったらreturn
  if (m_selection->isEmpty()) return;

  QList<OneShape> shapeToBeSelected;
  // 各選択シェイプについて
  for (int s = 0; s < m_selection->getCount(); s++) {
    // シェイプを取得
    OneShape shape = m_selection->getShape(s);
    if (!shape.shapePairP) continue;
    // 選択されていなければリストに格納
    // ロックされていたら加えない
    if (!m_selection->isSelected(shape.getPartner()) &&
        fromToVisible[(shape.fromTo == 0) ? 1 : 0])
      shapeToBeSelected.push_back(shape.getPartner());
  }

  // リストが空ならreturn
  if (shapeToBeSelected.isEmpty()) return;
  // シェイプを選択に追加
  for (int p = 0; p < shapeToBeSelected.size(); p++)
    m_selection->addShape(shapeToBeSelected.at(p));
}

//--------------------------------------------------------
// アクティブな対応点をリセットする

void CorrespondenceTool::onDeactivate() {
  m_selection->setActiveCorrPoint(OneShape(), -1);
  IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
}

//--------------------------------------------------------

bool CorrespondenceTool::setSpecialShapeColor(OneShape shape) {
  return (m_dragTool && m_dragTool->isSnapTargetShape(shape));
}

//--------------------------------------------------------
CorrespondenceTool correspondenceTool;
