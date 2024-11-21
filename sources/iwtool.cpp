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
  QColor cPointCol =
      ColorSettings::instance()->getQColor(Color_CorrPoint);  // �Ή��_
  QColor cActivePointCol = ColorSettings::instance()->getQColor(
      Color_ActiveCorr);  // �A�N�e�B�u�Ή��_
  QColor cInbtwnPointCol = ColorSettings::instance()->getQColor(
      Color_InbetweenCorr);  // �L�[�t���[���łȂ��Ή��_
  QColor cNumberCol = ColorSettings::instance()->getQColor(
      Color_CorrNumber);  // �Ή��_�E���̔ԍ�
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
    m_viewer->setColor(cPointCol);
  else
    m_viewer->setColor(cActivePointCol);

  // �Ή��_�̕����_�̃x�W�G���W�l�����߂�
  QList<double> discreteCorrValues =
      shape.shapePairP->getDiscreteCorrValues(onePix, frame, shape.fromTo);

  GLenum mode;
  if (shape.shapePairP->isClosed())
    mode = GL_LINE_LOOP;
  else
    mode = GL_LINE_STRIP;

  QVector3D* vert = new QVector3D[discreteCorrValues.size()];
  // �e�x�W�G���W���q��
  for (int d = 0; d < discreteCorrValues.size(); d++) {
    double value = discreteCorrValues.at(d);
    // �x�W�G�l������W�l���v�Z����
    QPointF tmpPos =
        shape.shapePairP->getBezierPosFromValue(frame, shape.fromTo, value);
    vert[d] = QVector3D(tmpPos.x(), tmpPos.y(), 0.0);
  }
  m_viewer->doDrawLine(mode, vert, discreteCorrValues.size());
  delete[] vert;

  // glEnd();

  //---�Ή��_�̕`��(���ɐ�����)
  // �Ή��_�̍��W���X�g�𓾂�
  QList<QPointF> corrPoints =
      shape.shapePairP->getCorrPointPositions(frame, shape.fromTo);
  // �E�F�C�g�̃��X�g������
  QList<double> corrWeights =
      shape.shapePairP->getCorrPointWeights(frame, shape.fromTo);

  // ���O
  int shapeName = layer->getNameFromShapePair(shape);

  for (int p = 0; p < corrPoints.size(); p++) {
    QPointF corrP = corrPoints.at(p);

    // �F���w��
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

    // �e�L�X�g�̕`��
    // �E�F�C�g��1����Ȃ��ꍇ�ɕ`�悷��
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
  QColor col = ColorSettings::instance()->getQColor(Color_CorrPoint);  // �Ή��_
  // �V�F�C�v���W�n�Ō����ڂP�s�N�Z���ɂȂ钷�����擾����
  QPointF onePix = m_viewer->getOnePixelLength();

  m_viewer->setColor(col);

  // �Ή��_�̕����_�̃x�W�G���W�l�����߂�
  QList<double> discreteCorrValues1 =
      shapePair->getDiscreteCorrValues(onePix, frame, 0);
  QList<double> discreteCorrValues2 =
      shapePair->getDiscreteCorrValues(onePix, frame, 1);

  m_viewer->setLineStipple(2, 0xCCCC);

  // �e�x�W�G���W���q��
  for (int d = 0; d < discreteCorrValues1.size(); d++) {
    double value1 = discreteCorrValues1.at(d);
    double value2 = discreteCorrValues2.at(d);
    // �x�W�G�l������W�l���v�Z����
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