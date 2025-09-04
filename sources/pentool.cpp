//--------------------------------------------------------
// Pen Tool
// �y���c�[��
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

  // ���݂̃t���[��
  int frame = project->getViewFrame();

  // �I�����������āA���݂̓_���A�N�e�B�u�ȓ_�ɒǉ�
  if (IwApp::instance()->getCurrentSelection()->getSelection())
    IwApp::instance()->getCurrentSelection()->getSelection()->selectNone();

  // �܂��V�F�C�v�������ꍇ�A�V�K�쐬
  if (!m_editingShape) {
    // �x�W�G�|�C���g1��
    BezierPointList bpList;
    BezierPoint bp = {pos, pos, pos};
    bpList << bp;

    // CorrPointList���
    CorrPointList cpList;
    CorrPoint cp = {0., 1.};
    cpList << cp << cp << cp << cp;

    m_editingShape = new ShapePair(frame, false, bpList, cpList, 0.01f);

    m_activePoints.clear();  // �ی�
    m_activePoints.push_back(
        3);  // 10�̈ʂ�0(�R���g���[���|�C���g#0)�A1�̈ʂ�3(SecondHandle)

    m_dragTool = new TranslateHandleDragTool(OneShape(m_editingShape, 0),
                                             m_activePoints[0],
                                             m_viewer->getOnePixelLength());
    m_dragTool->onClick(pos, e);

    // �|�C���g�𑝂₵���Ɠ����ɐL�΂��n���h���́A
    //  Ctrl�������ĐL�΂��Ă��邩�̂悤�ɂӂ�܂��B
    // ���Ȃ킿�A���Α��̃n���h�����_�Ώ̂ɐL�т�B
    TranslateHandleDragTool* thdt =
        dynamic_cast<TranslateHandleDragTool*>(m_dragTool);
    thdt->setIsInitial();

    return true;
  }

  int name = m_viewer->pick(e->pos());

  // ���ݕҏW���̃J�[�u�̉������N���b�N�����ꍇ
  if ((int)(name / 10000) == PenTool::EDITING_SHAPE_ID) {
    // �|�C���g�C���f�b�N�X�𓾂�
    int pointIndex = (int)((name % 10000) / 10);
    int handleId   = name % 10;
    // �|�C���g���N���b�N������
    if (handleId == 1) {
      // ���Ƀ|�C���g��3�ȏ゠��A�擪�̃|�C���g���N���b�N�����ꍇ�A
      //  Close�ȃV�F�C�v�����
      if (m_editingShape->getBezierPointAmount() >= 3 && pointIndex == 0) {
        m_editingShape->setIsClosed(true);
        // �Ή��_�̍X�V
        updateCorrPoints(0);
        updateCorrPoints(1);

        m_activePoints.clear();
        m_activePoints.push_back(1);  // 1��CP

        m_dragTool = new TranslatePointDragTool(OneShape(m_editingShape, 0),
                                                m_activePoints,
                                                m_viewer->getOnePixelLength());
        m_dragTool->onClick(pos, e);

        finishShape();
        return true;
      }
      // ����ȊO�̏ꍇ�A�|�C���g��I������
      // �ҏW���̃V�F�C�v�̃|�C���g/�n���h����͂ނ�ReShape���[�h�ɂȂ�
      if (!(e->modifiers() & Qt::ShiftModifier)) m_activePoints.clear();
      m_activePoints.push_back(pointIndex * 10 + 1);  // 1��CP

      m_dragTool = new TranslatePointDragTool(OneShape(m_editingShape, 0),
                                              m_activePoints,
                                              m_viewer->getOnePixelLength());
      m_dragTool->onClick(pos, e);
      return true;
    }
    // �n���h�����N���b�N�����炻�̃|�C���g������I���ɂ���
    else if (handleId == 2 || handleId == 3) {
      m_activePoints.clear();
      m_activePoints.push_back(pointIndex * 10 + handleId);

      m_dragTool = new TranslateHandleDragTool(OneShape(m_editingShape, 0),
                                               m_activePoints[0],
                                               m_viewer->getOnePixelLength());
      m_dragTool->onClick(pos, e);

      // �V�O�i�����G�~�b�g
      IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return true;
    }
    // �V�F�C�v���N���b�N������A+Alt�Ń|�C���g��ǉ�����
    else {
      //+Alt�Ń|�C���g��ǉ�����
      if (e->modifiers() & Qt::AltModifier) {
        // �w��ʒu�ɃR���g���[���|�C���g��ǉ�����
        m_editingShape->copyFromShapeToToShape();
        int newIndex = m_editingShape->addControlPoint(pos, frame, 0);
        IwTriangleCache::instance()->invalidateShapeCache(
            layer->getParentShape(m_editingShape));

        m_activePoints.clear();
        m_activePoints.push_back(newIndex * 10 + 1);  // 1��CP
        m_dragTool = new TranslatePointDragTool(OneShape(m_editingShape, 0),
                                                m_activePoints,
                                                m_viewer->getOnePixelLength());
        m_dragTool->onClick(pos, e);
        return true;
      }
      // ����ȊO�̓��V
      return false;
    }
  }

  // ����ȊO�̏ꍇ�́A�|�C���g�������ǉ�����
  {
    QMap<int, BezierPointList> formData = m_editingShape->getFormData(0);
    BezierPointList bpList              = formData.value(frame);
    BezierPoint bp                      = {pos, pos, pos};
    bpList.push_back(bp);
    formData[frame] = bpList;
    m_editingShape->setFormData(formData, 0);

    // �Ή��_�̍X�V
    updateCorrPoints(0);

    m_activePoints.clear();
    m_activePoints.push_back((bpList.size() - 1) * 10 + 3);  // 3��secondHandle
  }

  m_dragTool = new TranslateHandleDragTool(OneShape(m_editingShape, 0),
                                           m_activePoints[0],
                                           m_viewer->getOnePixelLength());
  m_dragTool->onClick(pos, e);

  // �|�C���g�𑝂₵���Ɠ����ɐL�΂��n���h���́A
  //  Ctrl�������ĐL�΂��Ă��邩�̂悤�ɂӂ�܂��B
  // ���Ȃ킿�A���Α��̃n���h�����_�Ώ̂ɐL�т�B
  TranslateHandleDragTool* thdt =
      dynamic_cast<TranslateHandleDragTool*>(m_dragTool);
  thdt->setIsInitial();

  return true;
}

//--------------------------------------------------------

bool PenTool::leftButtonDrag(const QPointF& pos, const QMouseEvent* e) {
  // ReShape���[�h�̃h���b�O����
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

bool PenTool::rightButtonDown(const QPointF& /*pos*/, const QMouseEvent* /*e*/,
                              bool& canOpenMenu, QMenu& /*menu*/) {
  // �V�F�C�v�̊��� ���܂���������A
  // �E�N���b�N���j���[�͏o���Ȃ��̂� canOpenMenu = false ��Ԃ�
  canOpenMenu = finishShape();
  return true;
}

//--------------------------------------------------------
// �ҏW���̃V�F�C�v��`���B�R���g���[���|�C���g��`���B
// �I���|�C���g�͐F��ς��A�n���h�����`��
//--------------------------------------------------------
void PenTool::draw() {
  // �V�F�C�v���������return
  if (!m_editingShape) return;

  //---���ݕ`�撆�̃x�W�G��`��

  if (!m_viewer) return;

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // ���݂̃t���[��
  int frame = project->getViewFrame();

  m_viewer->setColor(QColor(Qt::white));

  QVector3D* vertexArray = m_editingShape->getVertexArray(frame, 0, project);

  // ���O�t����
  int name = PenTool::EDITING_SHAPE_ID * 10000;
  m_viewer->pushName(name);

  m_viewer->doDrawLine(GL_LINE_STRIP, vertexArray,
                       m_editingShape->getVertexAmount(project));

  m_viewer->popName();

  // �f�[�^�����
  delete[] vertexArray;

  // �R���g���[���|�C���g��`��

  // �R���g���[���|�C���g�̐F�𓾂Ă���
  QColor cpColor    = ColorSettings::instance()->getQColor(Color_CtrlPoint);
  QColor cpSelected = ColorSettings::instance()->getQColor(Color_ActiveCtrl);
  // ���ꂼ��̃|�C���g�ɂ��āA�I������Ă�����n���h���t���ŕ`��
  // �I������Ă��Ȃ���Ε��ʂɎl�p��`��
  BezierPointList bPList = m_editingShape->getBezierPointList(frame, 0);
  for (int p = 0; p < bPList.size(); p++) {
    // �I������Ă���ꍇ
    if (isPointActive(p)) {
      // �n���h���t���ŃR���g���[���|�C���g��`�悷��
      m_viewer->setColor(cpSelected);
      ReshapeTool::drawControlPoint(
          m_viewer, OneShape(m_editingShape, 0), bPList, p, true,
          m_viewer->getOnePixelLength(), PenTool::EDITING_SHAPE_ID);
    }
    // �I������Ă��Ȃ��ꍇ
    else {
      // �P�ɃR���g���[���|�C���g��`�悷��
      m_viewer->setColor(cpColor);
      ReshapeTool::drawControlPoint(
          m_viewer, OneShape(m_editingShape, 0), bPList, p, false,
          m_viewer->getOnePixelLength(), PenTool::EDITING_SHAPE_ID);
    }
  }
}

//--------------------------------------------------------
// �Ή��_���X�V����
//--------------------------------------------------------
void PenTool::updateCorrPoints(int fromTo) {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // ���݂̃t���[��
  int frame = project->getViewFrame();

  int pointAmount = m_editingShape->getBezierPointAmount();

  CorrPointList cpList;

  if (m_editingShape->isClosed()) {
    for (int c = 0; c < 4; c++) {
      cpList.push_back({(double)(pointAmount) * (double)c / 4.0, 1.0});
    }
  } else {
    for (int c = 0; c < 4; c++) {
      cpList.push_back({(double)(pointAmount - 1) * (double)c / 3.0, 1.0});
    }
  }

  m_editingShape->setCorrKey(frame, fromTo, cpList);
}

//--------------------------------------------------------
// �V�F�C�v������������ ���܂���������A
// �E�N���b�N���j���[�͏o���Ȃ��̂�false��Ԃ�
//--------------------------------------------------------
bool PenTool::finishShape() {
  if (!m_editingShape) return true;
  // �_��1�Ȃ疳��
  if (m_editingShape->getBezierPointAmount() < 2) {
    delete m_editingShape;
    m_editingShape = 0;
    return true;
  }

  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  IwLayer* layer     = IwApp::instance()->getCurrentLayer()->getLayer();

  // �����ŁA������΂����From�V�F�C�v�̓��e��To�V�F�C�v�ɃR�s�[����
  m_editingShape->copyFromShapeToToShape(0.01);
  if (m_editingShape->isClosed()) m_editingShape->setIsParent(true);

  // Undo�ɓo�^ ������redo���Ă΂�A�R�}���h�����s�����
  IwUndoManager::instance()->push(
      new CreatePenShapeUndo(m_editingShape, project, layer));

  // �V�F�C�v���v���W�F�N�g�ɓn�����̂ŁA�|�C���^�����Z�b�g
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

  // ���ݕҏW���̃J�[�u�̉������N���b�N�����ꍇ
  if ((int)(name / 10000) == PenTool::EDITING_SHAPE_ID) {
    // �|�C���g�C���f�b�N�X�𓾂�
    int pointIndex = (int)((name % 10000) / 10);

    // �n���h�����N���b�N���ꂽ���`�F�b�N����iname�̉��ꌅ�j
    int handleId = name % 10;
    if (handleId) {
      // ���Ƀ|�C���g��3�ȏ゠��A�擪�̃|�C���g���N���b�N�����ꍇ�A
      //  Close�ȃV�F�C�v�����
      if (handleId == 1 && m_editingShape->getBezierPointAmount() >= 3 &&
          pointIndex == 0)
        return ToolCursor::BlackArrowCloseShapeCursor;

      return ToolCursor::BlackArrowCursor;
    }
    // �R���g���[���|�C���g�ǉ����[�h�̏���
    else if (e->modifiers() & Qt::AltModifier)
      return ToolCursor::BlackArrowAddCursor;
  }

  return ToolCursor::ArrowCursor;
}

//--------------------------------------------------------
// �c�[�������̂Ƃ��A�`�撆�̃V�F�C�v����������m�肷��
//--------------------------------------------------------
void PenTool::onDeactivate() {
  //(�����r���̂Ƃ�)�V�F�C�v�̊���
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
// �ȉ��AUndo�R�}���h��o�^
//---------------------------------------------------
CreatePenShapeUndo::CreatePenShapeUndo(ShapePair* shape, IwProject* prj,
                                       IwLayer* layer)
    : m_shape(shape), m_project(prj), m_layer(layer) {
  // �V�F�C�v��}������C���f�b�N�X�́A�V�F�C�v���X�g�̍Ō�
  m_index = m_layer->getShapePairCount();
}

CreatePenShapeUndo::~CreatePenShapeUndo() {
  // �V�F�C�v��delete
  delete m_shape;
}

void CreatePenShapeUndo::undo() {
  // �V�F�C�v����������
  m_layer->deleteShapePair(m_index);

  // �������ꂪ�J�����g�Ȃ�A�V�O�i�����G�~�b�g
  if (m_project->isCurrent())
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
}

void CreatePenShapeUndo::redo() {
  // �V�F�C�v���v���W�F�N�g�ɒǉ�
  m_layer->addShapePair(m_shape, m_index);

  // �������ꂪ�J�����g�Ȃ�A
  if (m_project->isCurrent()) {
    // �쐬�����V�F�C�v��I����Ԃɂ���
    IwShapePairSelection::instance()->selectOneShape(OneShape(m_shape, 0));
    // �V�O�i�����G�~�b�g
    IwApp::instance()->getCurrentProject()->notifyProjectChanged(true);
  }
}