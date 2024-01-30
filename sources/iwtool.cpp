//-----------------------------------------------------------------------------
// IwTool
// ツールのクラス
//-----------------------------------------------------------------------------

#include "iwtool.h"

#include <QMap>
#include <QSet>

#include "iwapp.h"
#include "iwtoolhandle.h"
#include "menubarcommand.h"
#include "iwproject.h"
#include "iwprojecthandle.h"
#include "sceneviewer.h"

#include "iwlayer.h"
#include "iwlayerhandle.h"
#include "iwshapepairselection.h"

// toolTable : ツールを保存しておくテーブル
// toolNames : ツール名を保存しておくテーブル
namespace {
typedef QMap<QString, IwTool*> ToolTable;
ToolTable* toolTable     = 0;
QSet<QString>* toolNames = 0;
}  // namespace
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// ToolSelector
// コマンドとツール切り替えを繋ぐクラス
//-----------------------------------------------------------------------------
class ToolSelector {
  QString m_toolName;

public:
  ToolSelector(QString toolName) : m_toolName(toolName) {}
  // コマンドが呼ばれたら、この関数でカレントツールを切り替える
  void selectTool() {
    IwApp::instance()->getCurrentTool()->setTool(m_toolName);
  }
};
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// ツール名からツールを取得
//-----------------------------------------------------------------------------
IwTool* IwTool::getTool(QString toolName) {
  if (!toolTable) return 0;
  ToolTable::iterator it = toolTable->find(toolName);
  if (it == toolTable->end()) return 0;
  return *it;
}

//-----------------------------------------------------------------------------
// コンストラクタ
// toolTable, toolNames が無ければ作る
// toolNamesに名前を挿入
// ToolSelectorクラスをはさんでコマンドを登録
// toolTableにこのインスタンスを登録
//-----------------------------------------------------------------------------
IwTool::IwTool(QString toolName) : m_name(toolName) {
  // toolTable, toolNames が無ければ作る
  if (!toolTable) toolTable = new ToolTable();
  if (!toolNames) toolNames = new QSet<QString>();

  QString name = getName();
  // まだツールを登録していなかったら
  if (!toolNames->contains(name)) {
    toolNames->insert(name);
    ToolSelector* toolSelector = new ToolSelector(name);
    CommandManager::instance()->setHandler(
        name.toStdString().c_str(),
        new CommandHandlerHelper<ToolSelector>(toolSelector,
                                               &ToolSelector::selectTool));
  }
  // toolTableにこのインスタンスを登録
  toolTable->insert(name, this);
}

//-----------------------------------------------------------------------------
// 指定シェイプの対応点を描画する
//-----------------------------------------------------------------------------
void IwTool::drawCorrLine(OneShape shape)
// void IwTool::drawCorrLine( IwShape* shape )
{
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // シェイプが見つからなかったら次へ（保険）
  if (!shape.shapePairP) return;

  // 表示フレームを得る
  int frame = project->getViewFrame();

  // 対応点表示用の色を得る
  double cPointCol[3];        // 対応点
  double cActivePointCol[3];  // アクティブ対応点
  double cInbtwnPointCol[3];  // キーフレームでない対応点
  double cNumberCol[3];       // 対応点右下の番号
  ColorSettings::instance()->getColor(cPointCol, Color_CorrPoint);
  ColorSettings::instance()->getColor(cActivePointCol, Color_ActiveCorr);
  ColorSettings::instance()->getColor(cInbtwnPointCol, Color_InbetweenCorr);
  ColorSettings::instance()->getColor(cNumberCol, Color_CorrNumber);
  // シェイプ座標系で見た目１ピクセルになる長さを取得する
  QPointF onePix = m_viewer->getOnePixelLength();
  // 文字用のフォントの指定
  QFont f("MS PGothic");
  f.setPointSize(13);

  // CorrespondenceTool 使用時、対応点が選択されている場合は対応点消去
  QPair<OneShape, int> activeCorrPoint =
      IwShapePairSelection::instance()->getActiveCorrPoint();
  bool corrpointIsActive =
      (activeCorrPoint.first.shapePairP && activeCorrPoint.second >= 0);

  //---分割点を繋ぐ線を描く
  if (corrpointIsActive)
    glColor3d(cPointCol[0], cPointCol[1], cPointCol[2]);
  else
    glColor3d(cActivePointCol[0], cActivePointCol[1], cActivePointCol[2]);

  // 対応点の分割点のベジエ座標値を求める
  QList<double> discreteCorrValues =
      shape.shapePairP->getDiscreteCorrValues(onePix, frame, shape.fromTo);

  if (shape.shapePairP->isClosed())
    glBegin(GL_LINE_LOOP);
  else
    glBegin(GL_LINE_STRIP);

  // 各ベジエ座標を繋ぐ
  for (int d = 0; d < discreteCorrValues.size(); d++) {
    double value = discreteCorrValues.at(d);
    // ベジエ値から座標値を計算する
    QPointF tmpPos =
        shape.shapePairP->getBezierPosFromValue(frame, shape.fromTo, value);
    glVertex3d(tmpPos.x(), tmpPos.y(), 0.0);
  }

  glEnd();

  //---対応点の描画(横に数字も)
  // 対応点の座標リストを得る
  QList<QPointF> corrPoints =
      shape.shapePairP->getCorrPointPositions(frame, shape.fromTo);
  // ウェイトのリストも得る
  QList<double> corrWeights =
      shape.shapePairP->getCorrPointWeights(frame, shape.fromTo);

  // 名前
  int shapeName = layer->getNameFromShapePair(shape);
  // int shapeName = project->getNameFromShape(shape);

  for (int p = 0; p < corrPoints.size(); p++) {
    QPointF corrP = corrPoints.at(p);

    // 色を指定
    if (IwShapePairSelection::instance()->isActiveCorrPoint(shape, p))
      glColor3d(cActivePointCol[0], cActivePointCol[1], cActivePointCol[2]);
    else if (shape.shapePairP->isCorrKey(frame, shape.fromTo))
      glColor3d(cPointCol[0], cPointCol[1], cPointCol[2]);
    else
      glColor3d(cInbtwnPointCol[0], cInbtwnPointCol[1], cInbtwnPointCol[2]);

    glPushMatrix();
    glPushName(shapeName + p * 10 + 1);
    glTranslated(corrP.x(), corrP.y(), 0.0);
    glScaled(onePix.x(), onePix.y(), 1.0);
    glBegin(GL_LINE_LOOP);
    glVertex3d(2.0, -2.0, 0.0);
    glVertex3d(2.0, 2.0, 0.0);
    glVertex3d(-2.0, 2.0, 0.0);
    glVertex3d(-2.0, -2.0, 0.0);
    glEnd();
    glPopName();

    // テキストの描画
    // ウェイトが1じゃない場合に描画する
    double weight = corrWeights.at(p);
    if (weight != 1.) {
      glColor3d(cNumberCol[0], cNumberCol[1], cNumberCol[2]);
      GLdouble model[16];
      glGetDoublev(GL_MODELVIEW_MATRIX, model);
      m_viewer->renderText(model[12] + 5.0, model[13] - 15.0,
                           QString::number(weight),
                           QFont("Helvetica", 10, QFont::Normal));
    }
    glPopMatrix();
  }
}

//-----------------------------------------------------------------------------
// Joinしているシェイプを繋ぐ対応線を描く
//-----------------------------------------------------------------------------
void IwTool::drawJoinLine(ShapePair* shapePair) {
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // シェイプが見つからなかったらreturn（保険）
  // シェイプがプロジェクトに属していなかったらreturn（保険）
  if (!shapePair || !layer->contains(shapePair)) return;

  // 表示フレームを得る
  int frame = project->getViewFrame();

  // 対応点表示用の色を得る
  double col[3];  // 対応点
  ColorSettings::instance()->getColor(col, Color_CorrPoint);
  // シェイプ座標系で見た目１ピクセルになる長さを取得する
  QPointF onePix = m_viewer->getOnePixelLength();

  glColor3d(col[0], col[1], col[2]);

  // 対応点の分割点のベジエ座標値を求める
  QList<double> discreteCorrValues1 =
      shapePair->getDiscreteCorrValues(onePix, frame, 0);
  QList<double> discreteCorrValues2 =
      shapePair->getDiscreteCorrValues(onePix, frame, 1);

  glEnable(GL_LINE_STIPPLE);
  glLineStipple(2, 0xCCCC);

  glBegin(GL_LINES);

  // 各ベジエ座標を繋ぐ
  for (int d = 0; d < discreteCorrValues1.size(); d++) {
    double value1 = discreteCorrValues1.at(d);
    double value2 = discreteCorrValues2.at(d);
    // ベジエ値から座標値を計算する
    QPointF tmpPos1 = shapePair->getBezierPosFromValue(frame, 0, value1);
    QPointF tmpPos2 = shapePair->getBezierPosFromValue(frame, 1, value2);

    glColor4d(col[0], col[1], col[2],
              (d % shapePair->getEdgeDensity() == 0) ? 1. : .5);
    glVertex3d(tmpPos1.x(), tmpPos1.y(), 0.0);
    glVertex3d(tmpPos2.x(), tmpPos2.y(), 0.0);
  }

  glEnd();
  glDisable(GL_LINE_STIPPLE);
}

//-----------------------------------------------------------------------------

void IwTool::onDeactivate() { IwApp::instance()->showMessageOnStatusBar(""); }