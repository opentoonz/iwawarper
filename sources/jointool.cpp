//--------------------------------------------------------
// Join Tool
// 接続ツール。シェイプ２つをモーフィングのペアにする
//--------------------------------------------------------

#include "jointool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "sceneviewer.h"
#include "iwshape.h"
#include "viewsettings.h"
#include "iwundomanager.h"

#include <QMouseEvent>
#include <QMessageBox>
#include <iostream>

JoinTool::JoinTool()
    : IwTool("T_Join"), m_overShape(0), m_srcShape(0), m_dstShape(0) {}

//--------------------------------------------------------

JoinTool::~JoinTool() {}

//--------------------------------------------------------

bool JoinTool::leftButtonDown(const QPointF& pos, const QMouseEvent*) {
  // マウス上にシェイプが無ければスルー
  if (!m_overShape) return false;

  // 既に対応点を持つシェイプをクリックした場合、
  //  Joinを切るかどうかを聞くダイアログを出す
  if (m_overShape->getPartner()) {
    QMessageBox msgBox;
    msgBox.setText("Unjoin the Selected Shape?");
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.setIcon(QMessageBox::Question);
    int ret = msgBox.exec();

    switch (ret) {
    case QMessageBox::Ok:
      // Unjoin操作をここに入れる
      // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
      IwUndoManager::instance()->push(new UnjoinShapeUndo(m_overShape));
      return true;
      break;
    case QMessageBox::Cancel:
    default:
      return false;
      break;
    }
    // std::cout<<"ret = "<<ret<<std::endl;
  }

  // クリックしたら、そこからスタート
  m_startPos   = pos;
  m_currentPos = pos;
  m_srcShape   = m_overShape;

  return true;
}

//--------------------------------------------------------

bool JoinTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (m_srcShape) {
    m_currentPos = pos;

    // ここでDstShapeを拾えるかチェック
    int name = m_viewer->pick(e->pos());
    // マウスオーバーしているシェイプが有る場合、格納する
    IwShape* shape = project->getShapeFromName(name);
    if (shape) {
      // DstShapeたり得る条件：
      // 1, まだJoin相手がいない
      // 2, 対応点の個数が同じ
      // 3, isClosed が同じ（閉じている同士、開いている同士）
      if (!shape->getPartner() &&
          (m_srcShape->getCorrPointAmount() == shape->getCorrPointAmount()) &&
          (m_srcShape->isClosed() == shape->isClosed())) {
        // dstShapeを入れる
        m_dstShape = shape;
      } else {
        m_dstShape = 0;
      }
    }
    // マウスオーバーしているシェイプが無ければ、dstShapeを解除
    else {
      m_dstShape = 0;
    }

    // マウスが動いている時点で表示は更新する
    return true;
  }
  return false;
}

//--------------------------------------------------------

bool JoinTool::leftButtonUp(const QPointF&, const QMouseEvent*) {
  if (m_srcShape && m_dstShape) {
    // Join操作で行うこと
    // 1, お互いのm_partnerに相手のシェイプのポインタを入れる
    // 2, Dst側のシェイプのShapeTypeIdを変える
    // 3, 対応点間の分割数が違う場合、多い方に合わせる

    IwProject* project = IwApp::instance()->getCurrentProject()->getProject();

    // Undoに登録 同時にredoが呼ばれ、コマンドが実行される
    IwUndoManager::instance()->push(
        new JoinShapeUndo(m_srcShape, m_dstShape, project));
  }

  if (m_srcShape) m_srcShape = 0;
  if (m_dstShape) m_dstShape = 0;

  return false;
}

//--------------------------------------------------------

bool JoinTool::mouseMove(const QPointF& pos, const QMouseEvent* e) {
  if (!m_viewer) return false;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return false;

  int name = m_viewer->pick(e->pos());

  // マウスオーバーしているシェイプが有る場合、格納する
  IwShape* shape = project->getShapeFromName(name);
  // シェイプをクリックした場合
  if (shape) {
    if (shape == m_overShape)
      return false;
    else {
      m_overShape = shape;
      return true;
    }
  } else {
    if (m_overShape == 0)
      return false;
    else {
      m_overShape = 0;
      return true;
    }
  }
}

//--------------------------------------------------------

void JoinTool::draw() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  // MouseOverしてるときは、マウス下のシェイプの対応点を表示。
  // 既に接続しているシェイプが有る場合は、それとのリンクぶりも表示
  if (m_overShape) {
    // 既に接続しているシェイプの場合
    // かつ、その相手が同じロールにいるとき
    ShapeRollId rollId = project->getViewSettings()->getShapeDisplayMode();
    if (m_overShape->getPartner() &&
        ((rollId == SHAPE_ROLL_AB) ||
         (m_overShape->getPartner()->getRollId() == rollId))) {
      drawJoinLine(m_overShape, m_overShape->getPartner());
      drawCorrLine(m_overShape->getPartner());
    }

    // std::cout<<"you have overShape"<<std::endl;
    drawCorrLine(m_overShape);
  }

  if (!m_srcShape) return;

  // シェイプをクリック→ドラッグしているときは、元のシェイプの対応点と、
  // ドラッグのスタート点から今のマウス位置までの直線を描画。
  if (m_srcShape != m_overShape) {
    drawCorrLine(m_srcShape);
  }

  glColor3f(1.0, 1.0, 1.0);
  glBegin(GL_LINE_STRIP);
  glVertex3f(m_startPos.x(), m_startPos.y(), 0.0);
  glVertex3f(m_currentPos.x(), m_currentPos.y(), 0.0);
  glEnd();

  if (!m_dstShape) return;

  // さらに、マウス下にシェイプが有る場合かつ、接続可能な場合は、
  // そのシェイプの対応点、および、対応点同士を結んだプレビュー線を表示する。
  if (m_dstShape != m_overShape) {
    drawCorrLine(m_dstShape);
  }

  // 対応点同士を繋ぐプレビュー線を描画する
  drawJoinPreviewLine();
}

//--------------------------------------------------------

int JoinTool::getCursorId(const QMouseEvent* e) {
  return ToolCursor::ArrowCursor;
}

//--------------------------------------------------------
// 対応点同士を繋ぐプレビュー線を描画する
//--------------------------------------------------------
void JoinTool::drawJoinPreviewLine() {
  // 2シェイプが無ければreturn(保険)
  if (!m_srcShape || !m_dstShape || m_srcShape == m_dstShape) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  // 表示フレームを得る
  int frame = project->getViewFrame();

  // 対応点の座標リストを得る
  QList<QPointF> corrPoints1 = m_srcShape->getCorrPointPositions(frame);
  QList<QPointF> corrPoints2 = m_dstShape->getCorrPointPositions(frame);

  glColor3f(1.0, 1.0, 1.0);

  glBegin(GL_LINES);

  for (int p = 0; p < corrPoints1.size(); p++) {
    QPointF corr1P = corrPoints1.at(p);
    QPointF corr2P = corrPoints2.at(p);

    glVertex3f(corr1P.x(), corr1P.y(), 0.0f);
    glVertex3f(corr2P.x(), corr2P.y(), 0.0f);

    glPopMatrix();
  }

  glEnd();
}

JoinTool joinTool;

//---------------------------------------------------
// 以下、Undoコマンドを登録
//---------------------------------------------------
JoinShapeUndo::JoinShapeUndo(IwShape* srcShape, IwShape* dstShape,
                             IwProject* prj)
    : m_project(prj), m_srcShape(srcShape), m_dstShape(dstShape) {
  // 対応点の精度を取っておく
  m_srcPrec = m_srcShape->getEdgeDensity();
  m_dstPrec = m_dstShape->getEdgeDensity();

  m_srcWasInAB = (m_srcShape->getRollId() == SHAPE_ROLL_AB);
  m_dstWasInAB = (m_dstShape->getRollId() == SHAPE_ROLL_AB);
}

JoinShapeUndo::~JoinShapeUndo() {}

void JoinShapeUndo::undo() {
  // Undo操作
  // 1, m_partnerを解除
  m_srcShape->setPartner(0);
  m_dstShape->setPartner(0);

  // 2, Dst側のシェイプのShapeTypeIdを元に戻す
  m_dstShape->setShapeType(SourceType);

  // 3, 対応点間の分割数を元に戻す
  m_dstShape->setEdgeDensity(m_dstPrec);
  m_srcShape->setEdgeDensity(m_srcPrec);

  // 4, シェイプの属するロールが以前ABロールだった場合、そーする
  if (m_srcWasInAB) m_srcShape->setRollId(SHAPE_ROLL_AB);
  if (m_dstWasInAB) m_dstShape->setRollId(SHAPE_ROLL_AB);

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void JoinShapeUndo::redo() {
  // Join操作で行うこと
  // 1, お互いのm_partnerに相手のシェイプのポインタを入れる
  m_srcShape->setPartner(m_dstShape);
  m_dstShape->setPartner(m_srcShape);

  // 2, Dst側のシェイプのShapeTypeIdを変える
  m_dstShape->setShapeType(DestType);

  // 3, 対応点間の分割数が違う場合、多い方に合わせる
  if (m_srcPrec != m_dstPrec) {
    if (m_srcPrec > m_dstPrec)
      m_dstShape->setEdgeDensity(m_srcPrec);
    else
      m_srcShape->setEdgeDensity(m_dstPrec);
  }

  // 4, シェイプの属するロールがABロールだった場合、AまたはBに移動する
  // 両方ABの場合、SRCをA,DSTをBとする
  if (m_srcWasInAB && m_dstWasInAB) {
    m_srcShape->setRollId(SHAPE_ROLL_A);
    m_dstShape->setRollId(SHAPE_ROLL_B);
  }
  // それ以外の場合、ABロールに属するシェイプを、相手とは別のロールに移動させる
  else if (m_srcWasInAB)
    m_srcShape->setRollId((m_dstShape->getRollId() == SHAPE_ROLL_A)
                              ? SHAPE_ROLL_B
                              : SHAPE_ROLL_A);
  else if (m_dstWasInAB)
    m_dstShape->setRollId((m_srcShape->getRollId() == SHAPE_ROLL_A)
                              ? SHAPE_ROLL_B
                              : SHAPE_ROLL_A);

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

//---------------------------------------------------

UnjoinShapeUndo::UnjoinShapeUndo(IwShape* shape)
    : m_project(IwApp::instance()->getCurrentProject()->getProject()) {
  switch (shape->getShapeType()) {
  case SourceType:
    m_srcShape = shape;
    m_dstShape = shape->getPartner();
    break;
  case DestType:
  default:
    m_srcShape = shape->getPartner();
    m_dstShape = shape;
    break;
  }
}

UnjoinShapeUndo::~UnjoinShapeUndo() {}

void UnjoinShapeUndo::undo() {
  // Join操作で行うこと
  // 1, お互いのm_partnerに相手のシェイプのポインタを入れる
  m_srcShape->setPartner(m_dstShape);
  m_dstShape->setPartner(m_srcShape);

  // 2, Dst側のシェイプのShapeTypeIdを変える
  m_dstShape->setShapeType(DestType);

  // 3, 対応点間の分割数が違う場合、多い方に合わせる
  int srcPrec = m_srcShape->getEdgeDensity();
  int dstPrec = m_dstShape->getEdgeDensity();
  if (srcPrec != dstPrec) {
    if (srcPrec > dstPrec)
      m_dstShape->setEdgeDensity(srcPrec);
    else
      m_srcShape->setEdgeDensity(dstPrec);
  }

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}

void UnjoinShapeUndo::redo() {
  // 1, m_partnerを解除
  m_srcShape->setPartner(0);
  m_dstShape->setPartner(0);

  // 2, Dst側のシェイプのShapeTypeIdを元に戻す
  m_dstShape->setShapeType(SourceType);

  // もしこれがカレントなら、シグナルをエミット
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged();
}