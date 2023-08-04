//--------------------------------------------------------
// Circle Tool
// �~�`�`��c�[��
// �쐬��͂��̃V�F�C�v���J�����g�ɂȂ�
// �J�����g�V�F�C�v�̕ό`���ł���iTransformTool�Ɠ����@�\�j
//--------------------------------------------------------

#include "circletool.h"

#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"
#include "iwselectionhandle.h"
#include "viewsettings.h"
#include "iwundomanager.h"
#include "preferences.h"

#include "transformtool.h"
#include "sceneviewer.h"

#include "iwlayerhandle.h"
#include "shapepair.h"
#include "iwlayer.h"

#include "iwshapepairselection.h"

#include <QMouseEvent>
#include <QMessageBox>

CircleTool::CircleTool() : IwTool("T_Circle"), m_startDefined(false) {}

//--------------------------------------------------------
bool CircleTool::leftButtonDown(const QPointF& pos, const QMouseEvent* e) {
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

  // ����ȊO�̏ꍇ�ACircleTool�����s
  m_startPos     = pos;
  m_startDefined = true;

  // �V�F�C�v�̑I������������
  IwShapePairSelection::instance()->selectNone();

  return false;
}
//--------------------------------------------------------
bool CircleTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  // Transform���[�h��������Ȃ�
  if (!m_startDefined) {
    return getTool("T_Transform")->leftButtonDrag(pos, e);
  }

  m_endPos = pos;

  return true;
}
//--------------------------------------------------------
bool CircleTool::leftButtonUp(const QPointF& pos, const QMouseEvent* e) {
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
      new CreateCircleShapeUndo(rect, project, layer));

  m_startDefined = false;

  return true;
}
//--------------------------------------------------------
void CircleTool::leftButtonDoubleClick(const QPointF&, const QMouseEvent*) {}

//--------------------------------------------------------
void CircleTool::draw() {
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

  glPushMatrix();
  glColor3d(0.8, 1.0, 0.8);

  // �`����RECT�Ɏ��܂�悤�Ɉړ�
  glTranslated(rect.center().x(), rect.center().y(), 0.0);
  glScaled(rect.width() / 2.0, rect.height() / 2.0, 1.0);

  // �x�W�G�̕����������߂�
  // 1��4�Z�O�����g�Ȃ̂ŁA1�X�e�b�v������΁^(2*bezierPrec)
  int bezierPrec = Preferences::instance()->getBezierActivePrecision();
  // ���S(0,0), ���a1�̉~��`��
  glBegin(GL_LINE_LOOP);
  // �p�x�̕ϕ�
  double dTheta = M_PI / (double)(bezierPrec * 2);
  double theta  = 0.0;
  for (int t = 0; t < 4 * bezierPrec; t++) {
    glVertex3d(cosf(theta), sinf(theta), 0.0);
    theta += dTheta;
  }
  glEnd();

  if (!isEnabled) glDisable(GL_BLEND);
  glBlendFunc(src, dst);

  glPopMatrix();
}
//--------------------------------------------------------

int CircleTool::getCursorId(const QMouseEvent* e) {
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

  statusStr += tr("[Drag] to create a new circle shape.");
  IwApp::instance()->showMessageOnStatusBar(statusStr);
  return ToolCursor::CircleCursor;
}
//--------------------------------------------------------

CircleTool circleTool;

//---------------------------------------------------
// �ȉ��AUndo�R�}���h��o�^
//---------------------------------------------------

CreateCircleShapeUndo::CreateCircleShapeUndo(QRectF& rect, IwProject* prj,
                                             IwLayer* layer)
    : m_project(prj), m_layer(layer) {
  // �~�ɋߎ������V�F�C�v����邽�߂̃n���h����
  double handleRatio = 0.5522847;

  // �e�R���g���[���|�C���g�ʒu
  QPointF p0(rect.left(), rect.center().y());
  QPointF p1(rect.center().x(), rect.top());
  QPointF p2(rect.right(), rect.center().y());
  QPointF p3(rect.center().x(), rect.bottom());

  // �n���h���i���āj
  QPointF vertHandle(0.0, rect.height() * 0.5 * handleRatio);
  // �n���h���i�悱�j
  QPointF horizHandle(rect.width() * 0.5 * handleRatio, 0.0);

  // rect�ɍ��킹�ăV�F�C�v�����
  BezierPointList bpList;
  BezierPoint p[4] = {{p0, p0 + vertHandle, p0 - vertHandle},
                      {p1, p1 - horizHandle, p1 + horizHandle},
                      {p2, p2 - vertHandle, p2 + vertHandle},
                      {p3, p3 + horizHandle, p3 - horizHandle}};

  bpList << p[0] << p[1] << p[2] << p[3];

  CorrPointList cpList;
  CorrPoint c[4] = {{1.5, 1.0}, {2.5, 1.0}, {3.5, 1.0}, {0.5, 1.0}};
  cpList << c[0] << c[1] << c[2] << c[3];

  // ���݂̃t���[��
  int frame = m_project->getViewFrame();

  m_shapePair = new ShapePair(frame, true, bpList, cpList, 0.01);
  m_shapePair->setName(QObject::tr("Circle"));

  // �V�F�C�v��}������C���f�b�N�X�́A�V�F�C�v���X�g�̍Ō�
  m_index = m_layer->getShapePairCount();
}

CreateCircleShapeUndo::~CreateCircleShapeUndo() {
  // �V�F�C�v��delete
  delete m_shapePair;
}

void CreateCircleShapeUndo::undo() {
  // �V�F�C�v����������
  m_layer->deleteShapePair(m_index);

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void CreateCircleShapeUndo::redo() {
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