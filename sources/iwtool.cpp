//-----------------------------------------------------------------------------
// IwTool
// �c�[���̃N���X
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

// toolTable : �c�[����ۑ����Ă����e�[�u��
// toolNames : �c�[������ۑ����Ă����e�[�u��
namespace {
typedef QMap<QString, IwTool*> ToolTable;
ToolTable* toolTable     = 0;
QSet<QString>* toolNames = 0;
}  // namespace
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// ToolSelector
// �R�}���h�ƃc�[���؂�ւ����q���N���X
//-----------------------------------------------------------------------------
class ToolSelector {
  QString m_toolName;

public:
  ToolSelector(QString toolName) : m_toolName(toolName) {}
  // �R�}���h���Ă΂ꂽ��A���̊֐��ŃJ�����g�c�[����؂�ւ���
  void selectTool() {
    IwApp::instance()->getCurrentTool()->setTool(m_toolName);
  }
};
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// �c�[��������c�[�����擾
//-----------------------------------------------------------------------------
IwTool* IwTool::getTool(QString toolName) {
  if (!toolTable) return 0;
  ToolTable::iterator it = toolTable->find(toolName);
  if (it == toolTable->end()) return 0;
  return *it;
}

//-----------------------------------------------------------------------------
// �R���X�g���N�^
// toolTable, toolNames ��������΍��
// toolNames�ɖ��O��}��
// ToolSelector�N���X���͂���ŃR�}���h��o�^
// toolTable�ɂ��̃C���X�^���X��o�^
//-----------------------------------------------------------------------------
IwTool::IwTool(QString toolName) : m_name(toolName) {
  // toolTable, toolNames ��������΍��
  if (!toolTable) toolTable = new ToolTable();
  if (!toolNames) toolNames = new QSet<QString>();

  QString name = getName();
  // �܂��c�[����o�^���Ă��Ȃ�������
  if (!toolNames->contains(name)) {
    toolNames->insert(name);
    ToolSelector* toolSelector = new ToolSelector(name);
    CommandManager::instance()->setHandler(
        name.toStdString().c_str(),
        new CommandHandlerHelper<ToolSelector>(toolSelector,
                                               &ToolSelector::selectTool));
  }
  // toolTable�ɂ��̃C���X�^���X��o�^
  toolTable->insert(name, this);
}

//-----------------------------------------------------------------------------
// �w��V�F�C�v�̑Ή��_��`�悷��
//-----------------------------------------------------------------------------
void IwTool::drawCorrLine(OneShape shape)
// void IwTool::drawCorrLine( IwShape* shape )
{
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // �V�F�C�v��������Ȃ������玟�ցi�ی��j
  if (!shape.shapePairP) return;

  // �\���t���[���𓾂�
  int frame = project->getViewFrame();

  // �Ή��_�\���p�̐F�𓾂�
  double cPointCol[3];        // �Ή��_
  double cActivePointCol[3];  // �A�N�e�B�u�Ή��_
  double cInbtwnPointCol[3];  // �L�[�t���[���łȂ��Ή��_
  double cNumberCol[3];       // �Ή��_�E���̔ԍ�
  ColorSettings::instance()->getColor(cPointCol, Color_CorrPoint);
  ColorSettings::instance()->getColor(cActivePointCol, Color_ActiveCorr);
  ColorSettings::instance()->getColor(cInbtwnPointCol, Color_InbetweenCorr);
  ColorSettings::instance()->getColor(cNumberCol, Color_CorrNumber);
  // �V�F�C�v���W�n�Ō����ڂP�s�N�Z���ɂȂ钷�����擾����
  QPointF onePix = m_viewer->getOnePixelLength();
  // �����p�̃t�H���g�̎w��
  QFont f("MS PGothic");
  f.setPointSize(13);

  // CorrespondenceTool �g�p���A�Ή��_���I������Ă���ꍇ�͑Ή��_����
  QPair<OneShape, int> activeCorrPoint =
      IwShapePairSelection::instance()->getActiveCorrPoint();
  bool corrpointIsActive =
      (activeCorrPoint.first.shapePairP && activeCorrPoint.second >= 0);

  //---�����_���q������`��
  if (corrpointIsActive)
    glColor3d(cPointCol[0], cPointCol[1], cPointCol[2]);
  else
    glColor3d(cActivePointCol[0], cActivePointCol[1], cActivePointCol[2]);

  // �Ή��_�̕����_�̃x�W�G���W�l�����߂�
  QList<double> discreteCorrValues =
      shape.shapePairP->getDiscreteCorrValues(onePix, frame, shape.fromTo);

  if (shape.shapePairP->isClosed())
    glBegin(GL_LINE_LOOP);
  else
    glBegin(GL_LINE_STRIP);

  // �e�x�W�G���W���q��
  for (int d = 0; d < discreteCorrValues.size(); d++) {
    double value = discreteCorrValues.at(d);
    // �x�W�G�l������W�l���v�Z����
    QPointF tmpPos =
        shape.shapePairP->getBezierPosFromValue(frame, shape.fromTo, value);
    glVertex3d(tmpPos.x(), tmpPos.y(), 0.0);
  }

  glEnd();

  //---�Ή��_�̕`��(���ɐ�����)
  // �Ή��_�̍��W���X�g�𓾂�
  QList<QPointF> corrPoints =
      shape.shapePairP->getCorrPointPositions(frame, shape.fromTo);
  // �E�F�C�g�̃��X�g������
  QList<double> corrWeights =
      shape.shapePairP->getCorrPointWeights(frame, shape.fromTo);

  // ���O
  int shapeName = layer->getNameFromShapePair(shape);
  // int shapeName = project->getNameFromShape(shape);

  for (int p = 0; p < corrPoints.size(); p++) {
    QPointF corrP = corrPoints.at(p);

    // �F���w��
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

    // �e�L�X�g�̕`��
    // �E�F�C�g��1����Ȃ��ꍇ�ɕ`�悷��
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
// Join���Ă���V�F�C�v���q���Ή�����`��
//-----------------------------------------------------------------------------
void IwTool::drawJoinLine(ShapePair* shapePair) {
  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return;

  // �V�F�C�v��������Ȃ�������return�i�ی��j
  // �V�F�C�v���v���W�F�N�g�ɑ����Ă��Ȃ�������return�i�ی��j
  if (!shapePair || !layer->contains(shapePair)) return;

  // �\���t���[���𓾂�
  int frame = project->getViewFrame();

  // �Ή��_�\���p�̐F�𓾂�
  double col[3];  // �Ή��_
  ColorSettings::instance()->getColor(col, Color_CorrPoint);
  // �V�F�C�v���W�n�Ō����ڂP�s�N�Z���ɂȂ钷�����擾����
  QPointF onePix = m_viewer->getOnePixelLength();

  glColor3d(col[0], col[1], col[2]);

  // �Ή��_�̕����_�̃x�W�G���W�l�����߂�
  QList<double> discreteCorrValues1 =
      shapePair->getDiscreteCorrValues(onePix, frame, 0);
  QList<double> discreteCorrValues2 =
      shapePair->getDiscreteCorrValues(onePix, frame, 1);

  glEnable(GL_LINE_STIPPLE);
  glLineStipple(2, 0xCCCC);

  glBegin(GL_LINES);

  // �e�x�W�G���W���q��
  for (int d = 0; d < discreteCorrValues1.size(); d++) {
    double value1 = discreteCorrValues1.at(d);
    double value2 = discreteCorrValues2.at(d);
    // �x�W�G�l������W�l���v�Z����
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