//--------------------------------------------------------
// Pen Tool
// ペンツール
//--------------------------------------------------------

#include "pentool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwselectionhandle.h"
#include "iwselection.h"
#include "pointdragtool.h"
#include "reshapetool.h"
#include "viewsettings.h"
#include "sceneviewer.h"
#include "iwundomanager.h"

#include "iwlayerhandle.h"
#include "iwlayer.h"
#include "iwshapepairselection.h"
#include "shapepair.h"
#include "iwtrianglecache.h"

#include <QMouseEvent>
#include <QMap>
#include <QMessageBox>

//--------------------------------------------------------

PenTool::PenTool() : IwTool("T_Pen"), m_editingShape(0), m_dragTool(0) {}

//--------------------------------------------------------

PenTool::~PenTool() {}

//--------------------------------------------------------

bool PenTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
  if (!m_viewer) return false;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return false;
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();

  if (!layer) return false;
  if (layer->isLocked()) {
    QMessageBox::critical(m_viewer, tr("Critical"),
                          tr("The current layer is locked."));
    return false;
  }

  // 現在のフレーム
  int frame = project->getViewFrame();

  // 選択を解除して、現在の点をアクティブな点に追加
  if (IwApp::instance()->getCurrentSelection()->getSelection())
    IwApp::instance()->getCurrentSelection()->getSelection()->selectNone();

  // まだシェイプが無い場合、新規作成
  if (!m_editingShape) {
    // ベジエポイント1つ
    BezierPointList bpList;
    BezierPoint bp = {pos, pos, pos};
    bpList << bp;

    // CorrPointList作る
    CorrPointList cpList;
    cpList << 0 << 0 << 0 << 0;

    m_editingShape = new ShapePair(frame, false, bpList, cpList, 0.01f);

    m_activePoints.clear();  // 保険
    m_activePoints.push_back(
        3);  // 10の位が0(コントロールポイント#0)、1の位が3(SecondHandle)

    m_dragTool = new TranslateHandleDragTool(OneShape(m_editingShape, 0),
                                             m_activePoints[0],
                                             m_viewer->getOnePixelLength());
    m_dragTool->onClick(pos, e);

    // ポイントを増やしたと同時に伸ばすハンドルは、
    //  Ctrlを押して伸ばしているかのようにふるまう。
    // すなわち、反対側のハンドルも点対称に伸びる。
    TranslateHandleDragTool* thdt =
        dynamic_cast<TranslateHandleDragTool*>(m_dragTool);
    thdt->setIsInitial();

    return true;
  }

  int name = m_viewer->pick(e->pos());

  // 現在編集中のカーブの何かをクリックした場合
  if ((int)(name / 10000) == PenTool::EDITING_SHAPE_ID) {
    // ポイントインデックスを得る
    int pointIndex = (int)((name % 10000) / 10);
    int handleId   = name % 10;
    // ポイントをクリックしたら
    if (handleId == 1) {
      // 既にポイントが3つ以上あり、先頭のポイントをクリックした場合、
      //  Closeなシェイプを作る
      if (m_editingShape->getBezierPointAmount() >= 3 && pointIndex == 0) {
        m_editingShape->setIsClosed(true);
        // 対応点の更新
        updateCorrPoints(0);
        updateCorrPoints(1);

        m_activePoints.clear();
        m_activePoints.push_back(1);  // 1はCP

        m_dragTool = new TranslatePointDragTool(OneShape(m_editingShape, 0),
                                                m_activePoints,
                                                m_viewer->getOnePixelLength());
        m_dragTool->onClick(pos, e);

        finishShape();
        return true;
      }
      // それ以外の場合、ポイントを選択する
      // 編集中のシェイプのポイント/ハンドルを掴むとReShapeモードになる
      if (!(e->modifiers() & Qt::ShiftModifier)) m_activePoints.clear();
      m_activePoints.push_back(pointIndex * 10 + 1);  // 1はCP

      m_dragTool = new TranslatePointDragTool(OneShape(m_editingShape, 0),
                                              m_activePoints,
                                              m_viewer->getOnePixelLength());
      m_dragTool->onClick(pos, e);
      return true;
    }
    // ハンドルをクリックしたらそのポイントだけを選択にする
    else if (handleId == 2 || handleId == 3) {
      m_activePoints.clear();
      m_activePoints.push_back(pointIndex * 10 + handleId);

      m_dragTool = new TranslateHandleDragTool(OneShape(m_editingShape, 0),
                                               m_activePoints[0],
                                               m_viewer->getOnePixelLength());
      m_dragTool->onClick(pos, e);

      // シグナルをエミット
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
    // シェイプをクリックしたら、+Altでポイントを追加する
    else {
      //+Altでポイントを追加する
      if (e->modifiers() & Qt::AltModifier) {
        // 指定位置にコントロールポイントを追加する
        m_editingShape->copyFromShapeToToShape();
        int newIndex = m_editingShape->addControlPoint(pos, frame, 0);
        IwTriangleCache::instance()->invalidateShapeCache(
            layer->getParentShape(m_editingShape));

        m_activePoints.clear();
        m_activePoints.push_back(newIndex * 10 + 1);  // 1はCP
        m_dragTool = new TranslatePointDragTool(OneShape(m_editingShape, 0),
                                                m_activePoints,
                                                m_viewer->getOnePixelLength());
        m_dragTool->onClick(pos, e);
        return true;
      }
      // それ以外はムシ
      return false;
    }
  }

  // それ以外の場合は、ポイントを延長追加する
  {
    QMap<int, BezierPointList> formData = m_editingShape->getFormData(0);
    BezierPointList bpList              = formData.value(frame);
    BezierPoint bp                      = {pos, pos, pos};
    bpList.push_back(bp);
    formData[frame] = bpList;
    m_editingShape->setFormData(formData, 0);

    // 対応点の更新
    updateCorrPoints(0);

    m_activePoints.clear();
    m_activePoints.push_back((bpList.size() - 1) * 10 + 3);  // 3はsecondHandle
  }

  m_dragTool = new TranslateHandleDragTool(OneShape(m_editingShape, 0),
                                           m_activePoints[0],
                                           m_viewer->getOnePixelLength());
  m_dragTool->onClick(pos, e);

  // ポイントを増やしたと同時に伸ばすハンドルは、
  //  Ctrlを押して伸ばしているかのようにふるまう。
  // すなわち、反対側のハンドルも点対称に伸びる。
  TranslateHandleDragTool* thdt =
      dynamic_cast<TranslateHandleDragTool*>(m_dragTool);
  thdt->setIsInitial();

  return true;
}

//--------------------------------------------------------

bool PenTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  // ReShapeモードのドラッグ操作
  if (m_dragTool) {
    m_dragTool->onDrag(pos, e);
    return true;
  }
  return false;
}

//--------------------------------------------------------

bool PenTool::leftButtonUp(const QPointF& pos, const QMouseEvent* e) {
  if (m_dragTool) {
    m_dragTool->onRelease(pos, e);
    delete m_dragTool;
    m_dragTool = 0;
    return true;
  }

  return false;
}

//--------------------------------------------------------

bool PenTool::rightButtonDown(const QPointF& pos, const QMouseEvent* e,
                              bool& canOpenMenu, QMenu& menu) {
  // シェイプの完成 うまくいったら、
  // 右クリックメニューは出さないので canOpenMenu = false を返す
  canOpenMenu = finishShape();
  return true;
}

//--------------------------------------------------------
// 編集中のシェイプを描く。コントロールポイントを描く。
// 選択ポイントは色を変え、ハンドルも描く
//--------------------------------------------------------
void PenTool::draw() {
  // シェイプが無ければreturn
  if (!m_editingShape) return;

  //---現在描画中のベジエを描画

  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // 現在のフレーム
  int frame = project->getViewFrame();

  glColor3d(1.0, 1.0, 1.0);

  GLdouble* vertexArray = m_editingShape->getVertexArray(frame, 0, project);

  // 名前付ける
  int name = PenTool::EDITING_SHAPE_ID * 10000;
  glPushName(name);

  // VertexArrayの有効化
  glEnableClientState(GL_VERTEX_ARRAY);
  // 1頂点は3つで構成、double型、オフセット０、データ元
  glVertexPointer(3, GL_DOUBLE, 0, vertexArray);
  // シェイプは開いている
  glDrawArrays(GL_LINE_STRIP, 0, m_editingShape->getVertexAmount(project));
  // VertexArrayの無効化
  glDisableClientState(GL_VERTEX_ARRAY);

  glPopName();

  // データを解放
  delete[] vertexArray;

  // コントロールポイントを描画

  // コントロールポイントの色を得ておく
  double cpColor[3], cpSelected[3];
  ColorSettings::instance()->getColor(cpColor, Color_CtrlPoint);
  ColorSettings::instance()->getColor(cpSelected, Color_ActiveCtrl);
  // それぞれのポイントについて、選択されていたらハンドル付きで描画
  // 選択されていなければ普通に四角を描く
  BezierPointList bPList = m_editingShape->getBezierPointList(frame, 0);
  for (int p = 0; p < bPList.size(); p++) {
    // 選択されている場合
    if (isPointActive(p)) {
      // ハンドル付きでコントロールポイントを描画する
      glColor3d(cpSelected[0], cpSelected[1], cpSelected[2]);
      ReshapeTool::drawControlPoint(OneShape(m_editingShape, 0), bPList, p,
                                    true, m_viewer->getOnePixelLength(),
                                    PenTool::EDITING_SHAPE_ID);
    }
    // 選択されていない場合
    else {
      // 単にコントロールポイントを描画する
      glColor3d(cpColor[0], cpColor[1], cpColor[2]);
      ReshapeTool::drawControlPoint(OneShape(m_editingShape, 0), bPList, p,
                                    false, m_viewer->getOnePixelLength(),
                                    PenTool::EDITING_SHAPE_ID);
    }
  }
}

//--------------------------------------------------------
// 対応点を更新する
//--------------------------------------------------------
void PenTool::updateCorrPoints(int fromTo) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // 現在のフレーム
  int frame = project->getViewFrame();

  int pointAmount = m_editingShape->getBezierPointAmount();

  CorrPointList cpList;

  if (m_editingShape->isClosed()) {
    for (int c = 0; c < 4; c++) {
      cpList.push_back((double)(pointAmount) * (double)c / 4.0);
    }
  } else {
    for (int c = 0; c < 4; c++) {
      cpList.push_back((double)(pointAmount - 1) * (double)c / 3.0);
    }
  }

  m_editingShape->setCorrKey(frame, fromTo, cpList);
}

//--------------------------------------------------------
// シェイプを完成させる うまくいったら、
// 右クリックメニューは出さないのでfalseを返す
//--------------------------------------------------------
bool PenTool::finishShape() {
  if (!m_editingShape) return true;
  // 点が1つなら無効
  if (m_editingShape->getBezierPointAmount() < 2) {
    delete m_editingShape;
    m_editingShape = 0;
    return true;
  }

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  IwLayer* layer     = IwApp::instance()->getCurrentLayer()->getLayer();

  // ここで、作ったばかりのFromシェイプの内容をToシェイプにコピーする
  m_editingShape->copyFromShapeToToShape(0.01);
  if (m_editingShape->isClosed()) m_editingShape->setIsParent(true);

  // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
  IwUndoManager::instance()->push(
      new CreatePenShapeUndo(m_editingShape, project, layer));

  // シェイプをプロジェクトに渡したので、ポインタをリセット
  m_editingShape = 0;
  return false;
}

//--------------------------------------------------------

int PenTool::getCursorId(const QMouseEvent* e) {
  if (!m_viewer) return ToolCursor::ArrowCursor;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return ToolCursor::ArrowCursor;

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer || layer->isLocked()) {
    IwApp::instance()->showMessageOnStatusBar(
        tr("Current layer is locked or not exist."));
    return ToolCursor::ForbiddenCursor;
  }

  IwApp::instance()->showMessageOnStatusBar(
      tr("[Click] to add point. [+ Drag] to extend handles. [Right click] to "
         "finish shape.  [Alt + Click] to insert a new point."));

  int name = m_viewer->pick(e->pos());

  // 現在編集中のカーブの何かをクリックした場合
  if ((int)(name / 10000) == PenTool::EDITING_SHAPE_ID) {
    // ポイントインデックスを得る
    int pointIndex = (int)((name % 10000) / 10);

    // ハンドルがクリックされたかチェックする（nameの下一桁）
    int handleId = name % 10;
    if (handleId) {
      // 既にポイントが3つ以上あり、先頭のポイントをクリックした場合、
      //  Closeなシェイプを作る
      if (handleId == 1 && m_editingShape->getBezierPointAmount() >= 3 &&
          pointIndex == 0)
        return ToolCursor::BlackArrowCloseShapeCursor;

      return ToolCursor::BlackArrowCursor;
    }
    // コントロールポイント追加モードの条件
    else if (e->modifiers() & Qt::AltModifier)
      return ToolCursor::BlackArrowAddCursor;
  }

  return ToolCursor::ArrowCursor;
}

//--------------------------------------------------------
// ツール解除のとき、描画中のシェイプがあったら確定する
//--------------------------------------------------------
void PenTool::onDeactivate() {
  //(書き途中のとき)シェイプの完成
  finishShape();
}

//--------------------------------------------------------

bool PenTool::setSpecialShapeColor(OneShape shape) {
  if (m_dragTool) return m_dragTool->setSpecialShapeColor(shape);
  return false;
}

bool PenTool::setSpecialGridColor(int gId, bool isVertical) {
  if (m_dragTool) return m_dragTool->setSpecialGridColor(gId, isVertical);
  return false;
}

PenTool penTool;

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------
CreatePenShapeUndo::CreatePenShapeUndo(ShapePair* shape, IwProject* prj,
                                       IwLayer* layer)
    : m_shape(shape), m_project(prj), m_layer(layer) {
  // シェイプを挿入するインデックスは、シェイプリストの最後
  m_index = m_layer->getShapePairCount();
}

CreatePenShapeUndo::~CreatePenShapeUndo() {
  // シェイプをdelete
  delete m_shape;
}

void CreatePenShapeUndo::undo() {
  // シェイプを消去する
  m_layer->deleteShapePair(m_index);

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void CreatePenShapeUndo::redo() {
  // シェイプをプロジェクトに追加
  m_layer->addShapePair(m_shape, m_index);

  // もしこれがカレントなら、
  if (m_project->isCurrent()) {
    // 作成したシェイプを選択状態にする
    IwShapePairSelection::instance()->selectOneShape(OneShape(m_shape, 0));
    // シグナルをエミット
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}