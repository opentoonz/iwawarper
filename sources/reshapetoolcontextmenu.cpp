//------------------------------------------
// ReshapeToolContextMenu
// Reshape Tool の右クリックメニュー用クラス
//------------------------------------------

#include "reshapetoolcontextmenu.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"

#include "iwshapepairselection.h"
#include "iwundomanager.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"

#include "preferences.h"
#include "iwtrianglecache.h"

// #include <QWidget>
#include <QMenu>
#include <QAction>
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

ReshapeToolContextMenu::ReshapeToolContextMenu() : m_mode(CurrentPointMode) {
  // アクションの宣言
  m_lockPointsAct    = new QAction(tr("Lock Points"), this);
  m_cuspAct          = new QAction(tr("Cusp"), this);
  m_linearAct        = new QAction(tr("Linear"), this);
  m_smoothAct        = new QAction(tr("Smooth"), this);
  m_centerAct        = new QAction(tr("Center"), this);
  m_tweenPointAct    = new QAction(tr("Tween Point"), this);
  m_reverseShapesAct = new QAction(tr("Reverse Shapes"), this);

  // シグナル／スロット接続
  connect(m_lockPointsAct, SIGNAL(triggered()), this, SLOT(doLockPoints()));
  connect(m_cuspAct, SIGNAL(triggered()), this, SLOT(doCusp()));
  connect(m_linearAct, SIGNAL(triggered()), this, SLOT(doLinear()));
  connect(m_smoothAct, SIGNAL(triggered()), this, SLOT(doSmooth()));
  connect(m_centerAct, SIGNAL(triggered()), this, SLOT(doCenter()));
  connect(m_tweenPointAct, SIGNAL(triggered()), this, SLOT(doTween()));
  connect(m_reverseShapesAct, SIGNAL(triggered()), this, SLOT(doReverse()));
}
//------------------------------------------
void ReshapeToolContextMenu::init(IwShapePairSelection* selection,
                                  QPointF onePixelLength) {
  m_selection      = selection;
  m_onePixelLength = onePixelLength;
}

//------------------------------------------
ReshapeToolContextMenu* ReshapeToolContextMenu::instance() {
  static ReshapeToolContextMenu _instance;
  return &_instance;
}

//------------------------------------------
// このメニューの操作で影響を与えるポイントを更新
//------------------------------------------
void ReshapeToolContextMenu::updateEffectedPoints() {
  m_effectedPoints.clear();
  m_activePointShapes.clear();

  QList<QPair<OneShape, int>> activePointList =
      m_selection->getActivePointList();

  // 各点について
  for (int i = 0; i < activePointList.size(); i++) {
    QPair<OneShape, int> activePoint = activePointList.at(i);

    OneShape shape = activePoint.first;
    int pointIndex = activePoint.second;

    // ActivePointを含むシェイプのリストを追加
    if (!m_activePointShapes.contains(shape)) m_activePointShapes.append(shape);

    // モードによって、このメニューの操作で影響を与えるポイントを追加
    if (m_mode == SelectedShapeMode ||
        (m_mode == CurrentShapeMode && m_shape == shape))
      m_effectedPoints.append(qMakePair(shape, pointIndex));
  }

  // CurrentPointModeのときは、Selectionに関係なくクリックしたポイントに効果
  if (m_mode == CurrentPointMode)
    m_effectedPoints.append(qMakePair(m_shape, m_pointIndex));
}

//------------------------------------------
// アクションの有効／無効を切り替える
//------------------------------------------
void ReshapeToolContextMenu::enableActions() {
  m_lockPointsAct->setEnabled(canLockPoints());

  bool enable = canModifyHandles();
  m_cuspAct->setEnabled(enable);
  m_linearAct->setEnabled(enable);
  m_smoothAct->setEnabled(enable);
  m_centerAct->setEnabled(canCenter());

  // なにかCPが選択されていればOK
  m_tweenPointAct->setEnabled(!m_effectedPoints.isEmpty());
  // 何かシェイプが選択されていればOK
  m_reverseShapesAct->setEnabled(!m_selection->isEmpty());
}

bool ReshapeToolContextMenu::canLockPoints() {
  // SelectedShapeModeではだめ（アンカーとなる点がどれか指定できないので）
  if (m_mode == SelectedShapeMode) return false;
  // CurrentShapeModeのとき、Activeな点が、
  else if (m_mode == CurrentShapeMode) {
    // 自分のシェイプを含む２つ以上にまたがっている必要
    if (m_activePointShapes.size() >= 2 &&
        m_activePointShapes.contains(m_shape))
      return true;
    else
      return false;
  }
  // CurrentPointModeのとき、Activeな点が、
  else {
    // 自分のシェイプ以外にもう１つ必要（本家とは仕様を変える）
    for (int s = 0; s < m_activePointShapes.size(); s++) {
      if (m_activePointShapes.at(s) != m_shape) return true;
    }
    return false;
  }
}

// Cusp, Linear, Smooth用
bool ReshapeToolContextMenu::canModifyHandles() {
  // CurrentPointModeならOK
  if (m_mode == CurrentPointMode) return true;
  // CurrentShapeModeの場合、ActiveなポイントがそのShapeに含まれている必要
  else if (m_mode == CurrentShapeMode)
    return m_activePointShapes.contains(m_shape);
  // SelectedShapeModeの場合、Activeなポイントが１つでもあればOK
  else
    return !m_effectedPoints.isEmpty();
}

bool ReshapeToolContextMenu::canCenter() {
  // 動かすポイントに、1つでも、
  // ① OpenShapeの端点以外の点があればOK。または、
  // ② CloseShapeで、前後が選択されていない点で
  // はさまれている点があればOK
  if (m_effectedPoints.isEmpty()) return false;

  for (int i = 0; i < m_effectedPoints.size(); i++) {
    OneShape shape = m_effectedPoints.at(i).first;
    int pId        = m_effectedPoints.at(i).second;

    // ① OpenShapeの端点以外の点があればOK。
    if (!shape.shapePairP->isClosed()) {
      if (pId != 0 && pId != shape.shapePairP->getBezierPointAmount() - 1)
        return true;
    }
    // ② CloseShapeで、前後が選択されていない点で
    // はさまれている点があればOK
    else {
      // 前の点を探す
      int beforePId = pId - 1;
      while (beforePId != pId) {
        // インデックスを一周させる
        if (beforePId == -1)
          beforePId = shape.shapePairP->getBezierPointAmount() - 1;

        // 一周してしまった場合は駄目なので次の操作点へ
        if (beforePId == pId) break;
        // 操作されるポイントでなければ発見
        if (!m_effectedPoints.contains(qMakePair(shape, beforePId))) break;
        // 次のポイントへ
        --beforePId;
      }
      // 一周してしまった場合は駄目なので次の操作点へ
      if (beforePId == pId) continue;

      // 後の点を探す
      int afterPId = pId + 1;
      while (afterPId != pId) {
        // インデックスを一周させる
        if (afterPId == shape.shapePairP->getBezierPointAmount()) afterPId = 0;
        // 一周してしまった場合は駄目なので次の操作点へ
        if (afterPId == pId) break;
        // 操作されるポイントでなければ発見
        if (!m_effectedPoints.contains(qMakePair(shape, afterPId))) break;
        // 次のポイントへ
        ++afterPId;
      }
      // 一周してしまった場合は駄目なので次の操作点へ
      if (afterPId == pId) continue;

      // 前後の点が違う場合は、Center操作が可能。
      // 同じ場合は次の操作点へ
      if (beforePId != afterPId) return true;
    }
  }
  // 全部見ても駄目だったらfalse
  return false;
}

//------------------------------------------
// メニューを開く
//------------------------------------------
void ReshapeToolContextMenu::openMenu(const QMouseEvent* /*e*/, OneShape shape,
                                      int pointIndex, int handleId,
                                      QMenu& menu) {
  m_shape      = shape;
  m_pointIndex = pointIndex;
  m_handleId   = handleId;

  // 右クリック位置にあわせ、モードを変える
  //  選択シェイプをクリックした場合
  //  ポイント／ハンドルをクリックしていたら CurrentPointMode
  //  それ以外なら、CurrentShapeMode
  if (shape.shapePairP && m_selection->isSelected(shape))
    m_mode = (handleId > 0) ? CurrentPointMode : CurrentShapeMode;
  // 選択シェイプ以外をクリックした場合
  else
    m_mode = SelectedShapeMode;

  // このメニューの操作で影響を与えるポイントを更新
  updateEffectedPoints();
  // ここで、アクションの有効／無効を切り替える
  enableActions();

  // モードの表示
  QString str;
  switch (m_mode) {
  case CurrentPointMode:
    str = tr("Current Point Mode");
    break;
  case CurrentShapeMode:
    str = tr("Current Shape Mode");
    break;
  case SelectedShapeMode:
    str = tr("Selected Shapes Mode");
    break;
  default:
    str = tr("Unknown Mode");
    break;
  }
  menu.addAction(new QAction(str, 0));

  menu.addSeparator();

  // menu.addAction(m_breakShapeAct);
  menu.addAction(m_lockPointsAct);

  menu.addSeparator();

  menu.addAction(m_cuspAct);
  menu.addAction(m_linearAct);
  menu.addAction(m_smoothAct);
  menu.addAction(m_centerAct);

  menu.addSeparator();

  menu.addAction(m_tweenPointAct);
  menu.addAction(m_reverseShapesAct);
}

//---------------------------------------------------
// クリックしたシェイプ内のActiveな点を、
// 他のシェイプのActiveな点で最寄りのものにフィットさせる。
// フィットさせるかどうかのポイント間の距離の閾値はPreferenceで決める
// カーブの端点以外は、ハンドルも同じ位置にする
// CurrentPointModeのときは、選んだpointだけ操作。。
// MultiFrameモードに対応。
//---------------------------------------------------
void ReshapeToolContextMenu::doLockPoints() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // 現在のフレームを得る
  int frame = project->getViewFrame();

  // Lock可能なピクセル距離を得る
  int lockThres = Preferences::instance()->getLockThreshold();

  // 対象となるフレームのリストを作る
  QList<int> moveFrames;
  if (isMultiFrameActivated()) {
    // MultiFrameの情報を得る
    QList<int> multiFrameList = project->getMultiFrames();
    // カレントフレームが含まれていなかったら追加
    if (!multiFrameList.contains(frame)) multiFrameList.append(frame);

    for (int f = 0; f < multiFrameList.size(); f++) {
      if (m_shape.shapePairP->isFormKey(multiFrameList.at(f), m_shape.fromTo))
        moveFrames.append(multiFrameList.at(f));
    }
    // 対象フレームが1つも無かったらreturn
    if (moveFrames.isEmpty()) return;
  }
  // マルチがOFFのときは、カレントフレームがクリックしたシェイプの
  // 形状キーフレームでないと、何もしないでreturn。
  else {
    if (m_shape.shapePairP->isFormKey(frame, m_shape.fromTo))
      moveFrames.append(frame);
    else
      return;
  }

  // 移動対象となるポイントと、アンカーとなるポイントの
  // インデックスのリストを作る
  QList<int> movePoints;
  QList<QPair<OneShape, int>> anchorPoints;

  QList<QPair<OneShape, int>> activePointList =
      m_selection->getActivePointList();

  // 各アクティブ点について
  for (int ep = 0; ep < activePointList.size(); ep++) {
    QPair<OneShape, int> curPoint = activePointList.at(ep);

    // クリックしたシェイプに含まれるActive点なら、移動する対象
    // ただし、CurrentPointModeの場合は、クリックしている点のみが移動する
    if (curPoint.first == m_shape) {
      if (m_mode == CurrentPointMode && curPoint.second != m_pointIndex)
        continue;
      movePoints.append(curPoint.second);
    }
    // それ以外なら、アンカーとなる対象
    else
      anchorPoints.append(curPoint);
  }

  //----- ここから操作

  // Undo用 形状データをとっておく
  QMap<int, BezierPointList> beforePoints =
      m_shape.shapePairP->getFormData(m_shape.fromTo);

  // 操作対象となる各フレームについて
  for (int f = 0; f < moveFrames.size(); f++) {
    int curFrame = moveFrames.at(f);

    // このポイントを移動させていく
    BezierPointList bpList =
        m_shape.shapePairP->getBezierPointList(curFrame, m_shape.fromTo);

    // 既にほかの点に場所を取られているアンカー点のリスト
    QList<QPair<OneShape, int>> occupiedAnchors;

    // 移動対象となる各ポイントについて
    for (int p = 0; p < movePoints.size(); p++) {
      int curIndex = movePoints.at(p);
      // これから動かすポイント
      BezierPoint curPoint = bpList.at(curIndex);

      // 一番近いアンカー点
      QPair<OneShape, int> nearestAnchor;
      BezierPoint nearestBPoint;
      double nearestDistance2 = 100000000.0;

      // アンカーとなる各点について
      for (int a = 0; a < anchorPoints.size(); a++) {
        QPair<OneShape, int> curAnchor =
            anchorPoints.at(a);  // 対象となるポイントを得る

        // すでに別のポイントが場所を取っていたらスキップ
        if (occupiedAnchors.contains(curAnchor)) continue;

        BezierPoint anchorBP =
            curAnchor.first.shapePairP
                ->getBezierPointList(curFrame, curAnchor.first.fromTo)
                .at(curAnchor.second);

        // 移動する点とアンカー点との「表示上の」ピクセル距離を求める
        QPointF vec  = curPoint.pos - anchorBP.pos;
        vec          = QPointF(vec.x() / m_onePixelLength.x(),
                               vec.y() / m_onePixelLength.y());
        double dist2 = vec.x() * vec.x() + vec.y() * vec.y();

        // これまでの一番近い距離よりも近かったら更新
        if (dist2 < nearestDistance2) {
          nearestAnchor    = curAnchor;
          nearestBPoint    = anchorBP;
          nearestDistance2 = dist2;
        }
      }
      // 最近傍のアンカー点との距離が、Preferenceで決めた閾値より
      // 遠かったら、この点は動かさないでcontinue
      // 閾値 暫定50
      if (nearestDistance2 > lockThres * lockThres) continue;

      // 閾値より近ければ、位置をフィットさせる
      //  Openシェイプの端点以外の場合は、ハンドル位置も一致させる。
      //  なんか挙動が違うみたい？？端点関係ないかも
      else {
        bool isEndPoint =
            (!m_shape.shapePairP->isClosed()) &&
            (curIndex == 0 ||
             curIndex == m_shape.shapePairP->getBezierPointAmount() - 1);
        if (isEndPoint)  // 平行移動
        {
          QPointF moveVec = nearestBPoint.pos - bpList[curIndex].pos;
          bpList[curIndex].pos += moveVec;
          bpList[curIndex].firstHandle += moveVec;
          bpList[curIndex].secondHandle += moveVec;
        } else
          bpList.replace(curIndex, nearestBPoint);
        // 占有点に追加
        occupiedAnchors.append(nearestAnchor);
      }
    }

    // キーを格納
    m_shape.shapePairP->setFormKey(curFrame, m_shape.fromTo, bpList);
    // 変更されるフレームをinvalidate
    int start, end;
    m_shape.shapePairP->getFormKeyRange(start, end, curFrame, m_shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, project->getParentShape(m_shape.shapePairP));
  }

  // Undo用 操作後の形状データ
  QMap<int, BezierPointList> afterPoints =
      m_shape.shapePairP->getFormData(m_shape.fromTo);

  // Undo
  QList<OneShape> shapeList;
  QList<QMap<int, BezierPointList>> beforeList;
  QList<QMap<int, BezierPointList>> afterList;
  QList<bool> wasKey;
  shapeList.append(m_shape);
  beforeList.append(beforePoints);
  afterList.append(afterPoints);
  wasKey.append(true);
  IwUndoManager::instance()->push(new ReshapeContextMenuUndo(
      shapeList, beforeList, afterList, wasKey, frame, project));

  // 更新シグナルをエミットする
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
// Active点のハンドル長を０にして、鋭角にする
// カレントフレームがキーフレームでなかった場合、キーにする
//---------------------------------------------------
void ReshapeToolContextMenu::doCusp() { doModifyHandle(Cusp); }

//---------------------------------------------------
// Active点のハンドルを、隣のポイントへの線分の1/4位置にする
// カレントフレームがキーフレームでなかった場合、キーにする
//---------------------------------------------------
void ReshapeToolContextMenu::doLinear() { doModifyHandle(Linear); }

//---------------------------------------------------
// Active点のハンドルを、前後のCPを結ぶ線分に平行で長さはそれぞれ1／4となるようにする
// カレントフレームがキーフレームでなかった場合、キーにする
//---------------------------------------------------
void ReshapeToolContextMenu::doSmooth() { doModifyHandle(Smooth); }

//---------------------------------------------------
// Active点のハンドルを、前後の非Active点を等分に割る位置に移動する。
// ハンドルの点は、点の間の距離の1／3の位置にする
// カレントフレームがキーフレームでなかった場合、キーにする
//---------------------------------------------------
void ReshapeToolContextMenu::doCenter() { doModifyHandle(Center); }

void ReshapeToolContextMenu::doModifyHandle(ModifyHandleActId actId) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // 現在のフレームを得る
  int frame = project->getViewFrame();

  // 対象となる点を、シェイプごとに並び替える
  QList<OneShape> shapes;
  QList<QList<int>> indicesList;

  // このメニューの操作で影響を与えるポイント
  for (int ep = 0; ep < m_effectedPoints.size(); ep++) {
    QPair<OneShape, int> pointPair = m_effectedPoints.at(ep);

    OneShape sh = pointPair.first;
    // シェイプが無ければ登録
    if (!shapes.contains(sh)) {
      shapes.append(sh);
      indicesList.append(QList<int>());
    }
    // ポイントを登録
    indicesList[shapes.indexOf(sh)].append(pointPair.second);
  }

  // 対象となるフレームのリストを作る
  QList<int> moveFrames;
  if (isMultiFrameActivated()) moveFrames.append(project->getMultiFrames());
  if (!moveFrames.contains(frame)) moveFrames.append(frame);

  // 各シェイプが、カレントフレームで形状キーだったかどうか
  QList<bool> wasKeyFrame;
  // シェイプごとの、編集されたフレームごとの元の形状のリスト
  QList<QMap<int, BezierPointList>> beforePoints;
  // シェイプごとの、編集されたフレームごとの操作後の形状のリスト
  QList<QMap<int, BezierPointList>> afterPoints;

  // 各対象シェイプについて
  for (int s = 0; s < shapes.size(); s++) {
    OneShape curShape = shapes.at(s);

    // 元の形状を格納
    beforePoints.append(curShape.shapePairP->getFormData(curShape.fromTo));

    // 操作前にカレントフレームで形状キーだったかどうか
    wasKeyFrame.append(curShape.shapePairP->isFormKey(frame, curShape.fromTo));

    // このシェイプのアクティブ点のリスト
    QList<int> cpIndices = indicesList.at(s);

    // 各対象フレームついて
    for (int f = 0; f < moveFrames.size(); f++) {
      int curFrame = moveFrames.at(f);
      // カレントフレームでなく、かつキーフレームでなければcontinue
      if (curFrame != frame &&
          !curShape.shapePairP->isFormKey(curFrame, curShape.fromTo))
        continue;

      // このフレームでのベジエ形状データを取得
      BezierPointList bpList =
          curShape.shapePairP->getBezierPointList(curFrame, curShape.fromTo);

      // 各アクティブポイントについて
      for (int i = 0; i < cpIndices.size(); i++) {
        int curIndex = cpIndices.at(i);
        // アクションに応じて処理を分ける
        switch (actId) {
          // 鋭角化
        case Cusp: {
          // ハンドル長を０にする
          QPointF cpPos                 = bpList.at(curIndex).pos;
          bpList[curIndex].firstHandle  = cpPos;
          bpList[curIndex].secondHandle = cpPos;
        } break;

          // ハンドルを隣のCPへのベクトルの1/4位置へ
        case Linear: {
          QPointF cpPos = bpList.at(curIndex).pos;
          // 前のハンドル : Openなシェイプで始点の場合はスキップ
          if (curShape.shapePairP->isClosed() || curIndex != 0) {
            int beforeIndex =
                (curIndex == 0)
                    ? curShape.shapePairP->getBezierPointAmount() - 1
                    : curIndex - 1;
            // 隣へのベクトル
            QPointF vec = bpList.at(beforeIndex).pos - cpPos;
            vec *= 0.25f;
            bpList[curIndex].firstHandle = cpPos + vec;
          }
          // 後ろのハンドル : Openなシェイプで終点の場合はスキップ
          if (curShape.shapePairP->isClosed() ||
              curIndex != curShape.shapePairP->getBezierPointAmount() - 1) {
            int afterIndex =
                (curIndex == curShape.shapePairP->getBezierPointAmount() - 1)
                    ? 0
                    : curIndex + 1;
            // 隣へのベクトル
            QPointF vec = bpList.at(afterIndex).pos - cpPos;
            vec *= 0.25f;
            bpList[curIndex].secondHandle = cpPos + vec;
          }
        } break;

          // ハンドルを前後の点を結ぶ線分と平行に、長さはそれぞれ1/4に
        case Smooth: {
          QPointF cpPos = bpList.at(curIndex).pos;
          // OpenShapeで始点の場合 次の点との中点までの長さにする
          if (!curShape.shapePairP->isClosed() && curIndex == 0) {
            QPointF vec = bpList.at(1).pos - cpPos;
            vec *= 0.5f;
            bpList[curIndex].firstHandle  = cpPos - vec;
            bpList[curIndex].secondHandle = cpPos + vec;
          }
          // OpenShapeで終点の場合
          else if (!curShape.shapePairP->isClosed() &&
                   curIndex ==
                       curShape.shapePairP->getBezierPointAmount() - 1) {
            QPointF vec = bpList.at(curIndex - 1).pos - cpPos;
            vec *= 0.5f;
            bpList[curIndex].firstHandle  = cpPos + vec;
            bpList[curIndex].secondHandle = cpPos - vec;
          }
          // 前後に点がある場合
          else {
            int beforeIndex =
                (curIndex == 0)
                    ? curShape.shapePairP->getBezierPointAmount() - 1
                    : curIndex - 1;
            int afterIndex =
                (curIndex == curShape.shapePairP->getBezierPointAmount() - 1)
                    ? 0
                    : curIndex + 1;
            QPointF vec =
                bpList.at(afterIndex).pos - bpList.at(beforeIndex).pos;
            vec *= 0.25f;
            bpList[curIndex].firstHandle  = cpPos - vec;
            bpList[curIndex].secondHandle = cpPos + vec;
          }
        } break;

          // 前後の非Active点の間を等分する
        case Center:
          // 長くなりそうなので逃がす
          doCenter_Imp(curShape, curIndex, bpList);
          break;
        }
      }

      // 形状データを戻す
      curShape.shapePairP->setFormKey(curFrame, curShape.fromTo, bpList);
      // 変更されるフレームをinvalidate
      int start, end;
      curShape.shapePairP->getFormKeyRange(start, end, curFrame,
                                           curShape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, project->getParentShape(curShape.shapePairP));
    }

    // 操作後の形状を格納
    afterPoints.append(curShape.shapePairP->getFormData(curShape.fromTo));
  }

  // Undoを登録
  IwUndoManager::instance()->push(new ReshapeContextMenuUndo(
      shapes, beforePoints, afterPoints, wasKeyFrame, frame, project));

  // シグナルをエミット
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------
// doCenterの実体
//---------------------------------------------------
void ReshapeToolContextMenu::doCenter_Imp(OneShape shape, int index,
                                          BezierPointList& bpList) {
  int beforeIndex, afterIndex;
  int beforeSegCount = 0;
  int afterSegCount  = 0;
  // ① OpenShapeの場合
  if (!shape.shapePairP->isClosed()) {
    // 前の点を探す
    beforeIndex = index;
    while (1) {
      if (beforeIndex == 0 || !m_selection->isPointActive(shape, beforeIndex))
        break;

      beforeIndex--;
      beforeSegCount += 3;
    }
    // 後の点を探す
    afterIndex = index;
    while (1) {
      if (afterIndex == shape.shapePairP->getBezierPointAmount() - 1 ||
          !m_selection->isPointActive(shape, afterIndex))
        break;

      afterIndex++;
      afterSegCount += 3;
    }
  }
  // ② CloseShapeの場合
  else {
    // 前の点を探す
    beforeIndex = index;
    while (1) {
      beforeIndex--;
      // インデックスのループ
      if (beforeIndex == -1)
        beforeIndex = shape.shapePairP->getBezierPointAmount() - 1;
      beforeSegCount += 3;
      // 端点が見つかったらbreak
      if (!m_selection->isPointActive(shape, beforeIndex)) break;
      // 一周しちゃったらreturn
      if (beforeIndex == index) return;
    }
    // 後の点を探す
    afterIndex = index;
    while (1) {
      afterIndex++;
      // インデックスのループ
      if (afterIndex == shape.shapePairP->getBezierPointAmount())
        afterIndex = 0;
      afterSegCount += 3;
      // 端点が見つかったらbreak
      if (!m_selection->isPointActive(shape, afterIndex)) break;
      // 一周しちゃったらreturn
      if (afterIndex == index) return;
    }
    // 前の点と後の点が同じ点だった場合、return
    if (beforeIndex == afterIndex) return;
  }
  // 保険
  if (beforeSegCount + afterSegCount == 0) return;
  // 前後のアンカー点を等分する
  QPointF beforePos = bpList.at(beforeIndex).pos;
  QPointF afterPos  = bpList.at(afterIndex).pos;
  QPointF vec       = afterPos - beforePos;
  vec /= (beforeSegCount + afterSegCount);
  bpList[index].pos          = beforePos + vec * beforeSegCount;
  bpList[index].firstHandle  = bpList[index].pos - vec;
  bpList[index].secondHandle = bpList[index].pos + vec;
}

//---------------------------------------------------
// キーフレーム上にあるActive点を、前後のキーフレームの中割位置に移動させる
// マルチフレーム無効
//---------------------------------------------------
void ReshapeToolContextMenu::doTween() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // 現在のフレームを得る
  int frame = project->getViewFrame();

  // 対象となる点を、シェイプごとに並び替える
  QList<OneShape> shapes;
  QList<QList<int>> indicesList;

  // このメニューの操作で影響を与えるポイント
  for (int ep = 0; ep < m_effectedPoints.size(); ep++) {
    QPair<OneShape, int> pointPair = m_effectedPoints.at(ep);
    // QPair<IwShape*, int> pointPair = m_effectedPoints.at(ep);

    OneShape sh = pointPair.first;
    // IwShape* sh = pointPair.first;
    // シェイプが無ければ登録
    if (!shapes.contains(sh)) {
      shapes.append(sh);
      indicesList.append(QList<int>());
    }
    // ポイントを登録
    indicesList[shapes.indexOf(sh)].append(pointPair.second);
  }

  // シェイプごとの、編集されたフレームごとの元の形状のリスト
  QList<QMap<int, BezierPointList>> beforePoints;
  // シェイプごとの、編集されたフレームごとの操作後の形状のリスト
  QList<QMap<int, BezierPointList>> afterPoints;
  // キーフレームだったかどうか。すべてtrue
  QList<bool> wasKeyFrame;

  // 各対象シェイプについて
  for (int s = 0; s < shapes.size(); s++) {
    OneShape curShape = shapes.at(s);

    // キーフレームでなければcontinue
    if (!curShape.shapePairP->isFormKey(frame, curShape.fromTo)) continue;
    // このシェイプの形状キーフレームのリスト
    QList<int> formKeyFrameList =
        curShape.shapePairP->getFormKeyFrameList(curShape.fromTo);
    // キーフレームが最初／最後のキーフレームならcontinue
    int keyPos = formKeyFrameList.indexOf(frame);
    if (keyPos == 0 || keyPos == formKeyFrameList.size() - 1) continue;

    // 元の形状を格納
    beforePoints.append(curShape.shapePairP->getFormData(curShape.fromTo));
    // キーフレームだったかどうか。全てtrue
    wasKeyFrame.append(true);

    //---------------------------------
    // 前後のキーを補間
    // 前後キーフレームから、補間の割合を求める
    double interpRatio = (double)(frame - formKeyFrameList.at(keyPos - 1)) /
                         (double)(formKeyFrameList.at(keyPos + 1) -
                                  formKeyFrameList.at(keyPos - 1));
    BezierPointList beforeBPList = curShape.shapePairP->getBezierPointList(
        formKeyFrameList.at(keyPos - 1), curShape.fromTo);
    BezierPointList afterBPList = curShape.shapePairP->getBezierPointList(
        formKeyFrameList.at(keyPos + 1), curShape.fromTo);
    //---------------------------------
    // このフレームでのベジエ形状データを取得
    BezierPointList bpList =
        curShape.shapePairP->getBezierPointList(frame, curShape.fromTo);

    // このシェイプのアクティブ点のリスト
    QList<int> cpIndices = indicesList.at(s);
    // 各アクティブポイントについて
    for (int i = 0; i < cpIndices.size(); i++) {
      int curIndex = cpIndices.at(i);

      bpList[curIndex].pos =
          beforeBPList.at(curIndex).pos * (1.0 - interpRatio) +
          afterBPList.at(curIndex).pos * interpRatio;
      bpList[curIndex].firstHandle =
          beforeBPList.at(curIndex).firstHandle * (1.0 - interpRatio) +
          afterBPList.at(curIndex).firstHandle * interpRatio;
      bpList[curIndex].secondHandle =
          beforeBPList.at(curIndex).secondHandle * (1.0 - interpRatio) +
          afterBPList.at(curIndex).secondHandle * interpRatio;
    }
    // キーを格納
    curShape.shapePairP->setFormKey(frame, curShape.fromTo, bpList);

    // 操作後の形状を格納
    afterPoints.append(curShape.shapePairP->getFormData(curShape.fromTo));
  }

  // Undoを登録
  IwUndoManager::instance()->push(new ReshapeContextMenuUndo(
      shapes, beforePoints, afterPoints, wasKeyFrame, frame, project));

  // シグナルをエミット
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  for (auto shape : shapes) {
    // 変更されるフレームをinvalidate
    int start, end;
    shape.shapePairP->getFormKeyRange(start, end, frame, shape.fromTo);
    IwTriangleCache::instance()->invalidateCache(
        start, end, project->getParentShape(shape.shapePairP));
  }
}

//---------------------------------------------------
// 選択シェイプの全てのキーフレームの、ベジエ点の順番を逆にする
// マルチフレーム無効
//---------------------------------------------------
void ReshapeToolContextMenu::doReverse() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // 現在のフレームを得る
  int frame = project->getViewFrame();

  QList<OneShape> shapes = m_selection->getShapes();

  // シェイプごとの、編集されたフレームごとの元の形状のリスト
  QList<QMap<int, BezierPointList>> beforePoints;
  // シェイプごとの、編集されたフレームごとの操作後の形状のリスト
  QList<QMap<int, BezierPointList>> afterPoints;
  // キーフレームだったかどうか。すべてtrue
  QList<bool> wasKeyFrame;

  // 各対象シェイプについて
  for (int s = 0; s < shapes.size(); s++) {
    OneShape curShape = shapes.at(s);

    wasKeyFrame.append(true);

    // 元の形状を格納
    beforePoints.append(curShape.shapePairP->getFormData(curShape.fromTo));

    QMap<int, BezierPointList> formData =
        curShape.shapePairP->getFormData(curShape.fromTo);
    QMap<int, BezierPointList>::iterator i = formData.begin();
    while (i != formData.end()) {
      BezierPointList bpList = i.value();
      // OpenなシェイプかClosaなシェイプかで挙動が違う
      // Closeなら、ポイント#1（始点の位置は固定）
      if (curShape.shapePairP->isClosed()) {
        for (int j = 1; j < (int)((bpList.size() - 1) / 2) + 1; j++)
          bpList.swapItemsAt(j, bpList.size() - j);
      }
      // Openなら、端から要素を交換していく
      else {
        for (int j = 0; j < (int)(bpList.size() / 2); j++)
          bpList.swapItemsAt(j, bpList.size() - 1 - j);
      }

      // ハンドル位置を入れ替える
      for (int p = 0; p < bpList.size(); p++) {
        QPointF first          = bpList[p].firstHandle;
        bpList[p].firstHandle  = bpList[p].secondHandle;
        bpList[p].secondHandle = first;
      }

      formData[i.key()] = bpList;
      ++i;
    }

    afterPoints.append(formData);
    curShape.shapePairP->setFormData(formData, curShape.fromTo);
  }
  // Undoを登録
  IwUndoManager::instance()->push(new ReshapeContextMenuUndo(
      shapes, beforePoints, afterPoints, wasKeyFrame, frame, project));

  // シグナルをエミット
  IwApp::instance()->getCurrentProject()->notifyProjectChanged();
  for (auto shape : shapes) {
    IwTriangleCache::instance()->invalidateShapeCache(
        project->getParentShape(shape.shapePairP));
  }
}

//---------------------------------------------------
// 以下、Undoコマンド
//---------------------------------------------------

//---------------------------------------------------
// ReshapeContextMenuUndo 共通で使う
//---------------------------------------------------

ReshapeContextMenuUndo::ReshapeContextMenuUndo(
    QList<OneShape>& shapes, QList<QMap<int, BezierPointList>>& beforePoints,
    QList<QMap<int, BezierPointList>>& afterPoints, QList<bool>& wasKeyFrame,
    int frame, IwProject* project)
    : m_shapes(shapes)
    , m_beforePoints(beforePoints)
    , m_afterPoints(afterPoints)
    , m_wasKeyFrame(wasKeyFrame)
    , m_frame(frame)
    , m_project(project)
    , m_firstFlag(true) {}

void ReshapeContextMenuUndo::undo() {
  // 各シェイプにポイントをセット
  for (int s = 0; s < m_shapes.count(); s++) {
    OneShape shape = m_shapes.at(s);

    // 操作前はキーフレームでなかった場合、キーを消去
    if (!m_wasKeyFrame.at(s))
      shape.shapePairP->removeFormKey(m_frame, shape.fromTo);

    QMap<int, BezierPointList> beforeFormKeys = m_beforePoints.at(s);
    QMap<int, BezierPointList>::iterator i    = beforeFormKeys.begin();
    while (i != beforeFormKeys.end()) {
      // ポイントを差し替え
      shape.shapePairP->setFormKey(i.key(), shape.fromTo, i.value());
      ++i;
    }
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent()) {
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
    for (auto shape : m_shapes) {
      // 変更されるフレームをinvalidate
      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }
}

void ReshapeContextMenuUndo::redo() {
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
    for (auto shape : m_shapes) {
      // 変更されるフレームをinvalidate
      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, m_frame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, m_project->getParentShape(shape.shapePairP));
    }
  }
}