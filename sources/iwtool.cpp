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
  QColor cPointCol =
      ColorSettings::instance()->getQColor(Color_CorrPoint);  // 対応点
  QColor cActivePointCol = ColorSettings::instance()->getQColor(
      Color_ActiveCorr);  // アクティブ対応点
  QColor cInbtwnPointCol = ColorSettings::instance()->getQColor(
      Color_InbetweenCorr);  // キーフレームでない対応点
  QColor cNumberCol = ColorSettings::instance()->getQColor(
      Color_CorrNumber);  // 対応点右下の番号
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
    m_viewer->setColor(cPointCol);
  else
    m_viewer->setColor(cActivePointCol);

  // 対応点の分割点のベジエ座標値を求める
  QList<double> discreteCorrValues =
      shape.shapePairP->getDiscreteCorrValues(onePix, frame, shape.fromTo);

  GLenum mode;
  if (shape.shapePairP->isClosed())
    mode = GL_LINE_LOOP;
  else
    mode = GL_LINE_STRIP;

  QVector3D* vert = new QVector3D[discreteCorrValues.size()];
  // 各ベジエ座標を繋ぐ
  for (int d = 0; d < discreteCorrValues.size(); d++) {
    double value = discreteCorrValues.at(d);
    // ベジエ値から座標値を計算する
    QPointF tmpPos =
        shape.shapePairP->getBezierPosFromValue(frame, shape.fromTo, value);
    vert[d] = QVector3D(tmpPos.x(), tmpPos.y(), 0.0);
  }
  m_viewer->doDrawLine(mode, vert, discreteCorrValues.size());
  delete[] vert;

  // glEnd();

  //---対応点の描画(横に数字も)
  // 対応点の座標リストを得る
  QList<QPointF> corrPoints =
      shape.shapePairP->getCorrPointPositions(frame, shape.fromTo);
  // ウェイトのリストも得る
  QList<double> corrWeights =
      shape.shapePairP->getCorrPointWeights(frame, shape.fromTo);

  // 名前
  int shapeName = layer->getNameFromShapePair(shape);

  for (int p = 0; p < corrPoints.size(); p++) {
    QPointF corrP = corrPoints.at(p);

    // 色を指定
    if (IwShapePairSelection::instance()->isActiveCorrPoint(shape, p))
      m_viewer->setColor(cActivePointCol);
    else if (shape.shapePairP->isCorrKey(frame, shape.fromTo))
      m_viewer->setColor(cPointCol);
    else
      m_viewer->setColor(cInbtwnPointCol);

    m_viewer->pushMatrix();

    glPushName(shapeName + p * 10 + 1);

    m_viewer->translate(corrP.x(), corrP.y(), 0.0);
    m_viewer->scale(onePix.x(), onePix.y(), 1.0);

    static QVector3D vert[4] = {
        QVector3D(2.0, -2.0, 0.0), QVector3D(2.0, 2.0, 0.0),
        QVector3D(-2.0, 2.0, 0.0), QVector3D(-2.0, -2.0, 0.0)};

    m_viewer->doDrawLine(GL_LINE_LOOP, vert, 4);
    glPopName();

    // テキストの描画
    // ウェイトが1じゃない場合に描画する
    double weight = corrWeights.at(p);
    if (weight != 1.) {
      m_viewer->releaseBufferObjects();
      QMatrix4x4 mat = m_viewer->modelMatrix();
      QPointF pos    = mat.map(QPointF(0., 0.));
      m_viewer->renderText(pos.x() + 5.0, pos.y() - 15.0,
                           QString::number(weight), cNumberCol,
                           QFont("Helvetica", 10, QFont::Normal));
      m_viewer->bindBufferObjects();
    }
    m_viewer->popMatrix();
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
  QColor col = ColorSettings::instance()->getQColor(Color_CorrPoint);  // 対応点
  // シェイプ座標系で見た目１ピクセルになる長さを取得する
  QPointF onePix = m_viewer->getOnePixelLength();

  m_viewer->setColor(col);

  // 対応点の分割点のベジエ座標値を求める
  QList<double> discreteCorrValues1 =
      shapePair->getDiscreteCorrValues(onePix, frame, 0);
  QList<double> discreteCorrValues2 =
      shapePair->getDiscreteCorrValues(onePix, frame, 1);

  m_viewer->setLineStipple(2, 0xCCCC);

  // 各ベジエ座標を繋ぐ
  for (int d = 0; d < discreteCorrValues1.size(); d++) {
    double value1 = discreteCorrValues1.at(d);
    double value2 = discreteCorrValues2.at(d);
    // ベジエ値から座標値を計算する
    QPointF tmpPos1 = shapePair->getBezierPosFromValue(frame, 0, value1);
    QPointF tmpPos2 = shapePair->getBezierPosFromValue(frame, 1, value2);

    col.setAlphaF((d % shapePair->getEdgeDensity() == 0) ? 1. : .5);
    m_viewer->setColor(col);
    QVector3D vert[2] = {QVector3D(tmpPos1), QVector3D(tmpPos2)};
    m_viewer->doDrawLine(GL_LINE_STRIP, vert, 2);
  }

  m_viewer->setLineStipple(1, 0xFFFF);
}

//-----------------------------------------------------------------------------

void IwTool::onDeactivate() { IwApp::instance()->showMessageOnStatusBar(""); }