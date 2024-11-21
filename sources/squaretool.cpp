//--------------------------------------------------------
// Square Tool
// ��`�`��c�[��
// �쐬��͂��̃V�F�C�v���J�����g�ɂȂ�
// �J�����g�V�F�C�v�̕ό`���ł���iTransformTool�Ɠ����@�\�j
//--------------------------------------------------------

#include "squaretool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwselectionhandle.h"
#include "viewsettings.h"
#include "iwundomanager.h"

#include "transformtool.h"
#include "sceneviewer.h"

#include "iwlayerhandle.h"
#include "shapepair.h"
#include "iwlayer.h"

#include "iwshapepairselection.h"

#include <QMouseEvent>
#include <QMessageBox>

SquareTool::SquareTool() : IwTool("T_Square"), m_startDefined(false) {}

//--------------------------------------------------------
bool SquareTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer) return false;
  if (layer->isLocked()) {
    QMessageBox::critical(m_viewer, tr("Critical"),
                          tr("The current layer is locked."));
    return false;
  }

  // Transform���[�h�ɓ������
  // �����V�F�C�v���I������Ă���
  if (!IwShapePairSelection::instance()->isEmpty()) {
    // �I���V�F�C�v�̂ǂ������N���b�N���Ă�����TrasformTool�����s����
    IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
    if (!project) return false;
    int name = m_viewer->pick(e->pos());

    OneShape shape = layer->getShapePairFromName(name);
    if (shape.shapePairP &&
        IwShapePairSelection::instance()->isSelected(shape)) {
      return getTool("T_Transform")->leftButtonDown(pos, e);
    }
  }

  // ����ȊO�̏ꍇ�ASquareTool�����s
  m_startPos     = pos;
  m_startDefined = true;

  // �V�F�C�v�̑I������������
  IwShapePairSelection::instance()->selectNone();

  return false;
}
//--------------------------------------------------------
bool SquareTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  // Transform���[�h��������Ȃ�
  if (!m_startDefined) {
    return getTool("T_Transform")->leftButtonDrag(pos, e);
  }

  m_endPos = pos;

  return true;
}
//--------------------------------------------------------
bool SquareTool::leftButtonUp(const QPointF& pos, const QMouseEvent* e) {
  // Transform���[�h��������Ȃ�
  if (!m_startDefined) {
    return getTool("T_Transform")->leftButtonUp(pos, e);
  }

  // �܂��A2�_�ō��Rect�����A���K������
  QRectF rect(m_startPos, pos);
  rect = rect.normalized();

  // �����A�������ɒ[�ɏ������ꍇ��return
  if (rect.width() < 0.001 || rect.height() < 0.001) {
    m_startDefined = false;
    return true;
  }

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  IwLayer* layer     = IwApp::instance()->getCurrentLayer()->getLayer();

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(
      new CreateSquareShapeUndo(rect, project, layer));

  m_startDefined = false;

  return true;
}
//--------------------------------------------------------
void SquareTool::leftButtonDoubleClick(const QPointF&, const QMouseEvent*) {}
//--------------------------------------------------------
void SquareTool::draw() {
  if (!m_startDefined) {
    getTool("T_Transform")->draw();
    return;
  }

  // �܂��A2�_�ō��Rect�����A���K������
  QRectF rect(m_startPos, m_endPos);
  rect = rect.normalized();

  // �����A�������ɒ[�ɏ������ꍇ��return
  if (rect.width() < 0.001 || rect.height() < 0.001) {
    return;
  }

  GLint src, dst;
  bool isEnabled = glIsEnabled(GL_BLEND);
  glGetIntegerv(GL_BLEND_SRC, &src);
  glGetIntegerv(GL_BLEND_DST, &dst);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

  m_viewer->pushMatrix();
  m_viewer->setColor(QColor::fromRgbF(0.8, 1.0, 0.8));

  QVector3D vert[4] = {QVector3D(rect.bottomRight()),
                       QVector3D(rect.topRight()), QVector3D(rect.topLeft()),
                       QVector3D(rect.bottomLeft())};

  m_viewer->doDrawLine(GL_LINE_LOOP, vert, 4);

  m_viewer->popMatrix();

  if (!isEnabled) glDisable(GL_BLEND);
  glBlendFunc(src, dst);
}
//--------------------------------------------------------

int SquareTool::getCursorId(const QMouseEvent* e) {
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (!layer || layer->isLocked()) {
    IwApp::instance()->showMessageOnStatusBar(
        tr("Current layer is locked or not exist."));
    return ToolCursor::ForbiddenCursor;
  }

  QString statusStr;

  if (!IwShapePairSelection::instance()->isEmpty()) {
    // Transform�̃J�[�\��ID���܂��擾
    int cursorID = getTool("T_Transform")->getCursorId(e);
    // �����n���h��������ł���ꍇ�́A���̃J�[�\���ɂ���
    if (cursorID != ToolCursor::ArrowCursor) return cursorID;
    statusStr += tr("[Drag handles] to transform selected shape. ");
  }

  statusStr += tr("[Drag] to create a new square shape.");
  IwApp::instance()->showMessageOnStatusBar(statusStr);
  return ToolCursor::SquareCursor;
}
//--------------------------------------------------------

SquareTool squareTool;

//---------------------------------------------------
// �ȉ��AUndo�R�}���h��o�^
//---------------------------------------------------

CreateSquareShapeUndo::CreateSquareShapeUndo(QRectF& rect, IwProject* prj,
                                             IwLayer* layer)
    : m_project(prj), m_layer(layer) {
  // rect�ɍ��킹�ăV�F�C�v�����
  BezierPointList bpList;
  BezierPoint p[4] = {
      {rect.topRight(), rect.topRight(), rect.topRight()},
      {rect.bottomRight(), rect.bottomRight(), rect.bottomRight()},
      {rect.bottomLeft(), rect.bottomLeft(), rect.bottomLeft()},
      {rect.topLeft(), rect.topLeft(), rect.topLeft()}};

  bpList << p[0] << p[1] << p[2] << p[3];

  CorrPointList cpList;
  CorrPoint c[4] = {{0., 1.}, {1., 1.}, {2., 1.}, {3., 1.}};
  cpList << c[0] << c[1] << c[2] << c[3];

  // ���݂̃t���[��
  int frame = m_project->getViewFrame();

  m_shapePair = new ShapePair(frame, true, bpList, cpList, 0.01);
  m_shapePair->setName(QObject::tr("Rectangle"));

  // �V�F�C�v��}������C���f�b�N�X�́A�V�F�C�v���X�g�̍Ō�
  m_index = m_layer->getShapePairCount();
}

CreateSquareShapeUndo::~CreateSquareShapeUndo() {
  // �V�F�C�v��delete
  delete m_shapePair;
}

void CreateSquareShapeUndo::undo() {
  // �V�F�C�v����������
  m_layer->deleteShapePair(m_index);

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void CreateSquareShapeUndo::redo() {
  // �V�F�C�v���v���W�F�N�g�ɒǉ�
  m_layer->addShapePair(m_shapePair, m_index);

  // �������ꂪ�J�����g�Ȃ�A
  if (m_project->isCurrent()) {
    // �쐬�����V�F�C�v��I����Ԃɂ���
    IwShapePairSelection::instance()->selectOneShape(OneShape(m_shapePair, 0));
    IwShapePairSelection::instance()->addShape(OneShape(m_shapePair, 1));
    // �V�O�i�����G�~�b�g
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}