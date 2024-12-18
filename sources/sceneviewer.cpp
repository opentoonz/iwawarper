//----------------------------------
// SceneViewer
// 1�̃v���W�F�N�g�ɂ���1�̃G���A
//----------------------------------

#include "sceneviewer.h"

#include "iwproject.h"
#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwtoolhandle.h"
#include "iwselectionhandle.h"
#include "iwselection.h"
#include "iwtool.h"
#include "viewsettings.h"

#include "menubarcommand.h"
#include "menubarcommandids.h"

#include "ttoonzimage.h"
#include "trasterimage.h"
#include "tropcm.h"
#include "tpalette.h"

#include "iwimagecache.h"
#include "iwtrianglecache.h"
#include "cursormanager.h"

#include "iwrenderinstance.h"
#include "renderprogresspopup.h"
#include "mainwindow.h"

#include "iwshapepairselection.h"
#include "iwlayer.h"
#include "iwlayerhandle.h"
#include "shapepair.h"

#include "iocommand.h"
#include "ruler.h"
#include "rendersettings.h"
#include "projectutils.h"

#include <QPointF>
#include <QVector3D>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFramebufferObject>
#include <QMessageBox>
#include <QApplication>

#ifdef MACOSX
#include <GLUT/glut.h>
#endif

#include <QPainter>
#include <QMenu>
#include <QOpenGLTexture>
#include <QScreen>

#include "logger.h"
#include "outputsettings.h"
#include "keydragtool.h"

#define PRINT_LOG(message)                                                     \
  { Logger::Write(message); }

namespace {
bool isPremultiplied(TRaster32P ras) {
  for (int y = 0; y < ras->getLy(); y++) {
    TPixel32* pix = ras->pixels(y);
    for (int x = 0; x < ras->getLx(); x++, pix++) {
      if ((*pix).m == TPixel32::maxChannelValue) continue;
      if ((*pix).m < (*pix).r || (*pix).m < (*pix).g || (*pix).m < (*pix).b)
        return false;
    }
  }
  return true;
}
void adjustBrightnessContrast(TRaster32P ras, int brightness, int contrast) {
  auto truncate = [](int val) {
    return (val < 0) ? 0 : (val > 255) ? 255 : val;
  };
  // LUT������
  unsigned char lut[256];
  double contrastFactor =
      (259.0 * (double)(contrast + 255)) / (255.0 * (double)(259 - contrast));
  for (int c = 0; c < 256; c++) {
    lut[c] = (unsigned char)truncate((int)std::round(
        contrastFactor * (double)(c - 128) + 128.0 + (double)brightness));
  }
  for (int y = 0; y < ras->getLy(); y++) {
    TPixel32* pix = ras->pixels(y);
    for (int x = 0; x < ras->getLx(); x++, pix++) {
      (*pix).r = lut[(int)((*pix).r)];
      (*pix).g = lut[(int)((*pix).g)];
      (*pix).b = lut[(int)((*pix).b)];
    }
  }
}

// �n���h�����\������Ă�����n���h���D��
// ���� �|�C���g���\������Ă�����|�C���g
// �Ō�ɃV�F�C�v
bool isPoint(const int& id1, const int& id2) {
  if ((id1 % 100) == 0 && (id2 % 100) == 0)
    return (id1 % 10000) > (id2 % 10000);
  if ((id1 % 100) == 0) return false;
  if ((id2 % 100) == 0) return true;
  return (id1 % 100) < (id2 % 100);
}

void my_gluPickMatrix(QMatrix4x4& mat, GLdouble x, GLdouble y, GLdouble deltax,
                      GLdouble deltay, GLint viewport[4]) {
  if (deltax <= 0 || deltay <= 0) {
    return;
  }

  mat.translate((viewport[2] - 2 * (x - viewport[0])) / deltax,
                (viewport[3] - 2 * (y - viewport[1])) / deltay, 0);
  mat.scale(viewport[2] / deltax, viewport[3] / deltay, 1.0);
}

inline QVector4D colorToVector(QColor col) {
  return QVector4D(col.redF(), col.greenF(), col.blueF(), col.alphaF());
}

};  // namespace

Qt::KeyboardModifiers SceneViewer::m_modifiers = Qt::NoModifier;

SceneViewer::SceneViewer(QWidget* parent)
    : QOpenGLWidget(parent), m_isPanning(false), m_mouseButton(Qt::NoButton) {
  setAutoFillBackground(false);  // �����OFF�ɂ��邱�Ƃ�QPainter���g�p�ł���
  // �}�E�X�̓��������m����
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);

  // �V�O�i��/�X���b�g�ڑ�
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(viewFrameChanged()),
          this, SLOT(update()));
  // ViewSettings���ς������A�����̃v���W�F�N�g���J�����g�Ȃ�Aupdate
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(viewSettingsChanged()),
          this, SLOT(onViewSettingsChanged()));
  // �v���W�F�N�g�̓��e���ς������A�����̃v���W�F�N�g���J�����g�Ȃ�Aupdate
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(projectChanged()),
          this, SLOT(onViewSettingsChanged()));
  // ���C���̓��e���ς������A���ݕ\�����Ă���t���[���̓��e���ς�������̂݃e�N�X�`�����X�V
  connect(IwApp::instance()->getCurrentLayer(), SIGNAL(layerChanged(IwLayer*)),
          this, SLOT(onLayerChanged()));
  // �J�����g���C�����؂�ւ��ƁA�\�����X�V
  connect(IwApp::instance()->getCurrentLayer(), SIGNAL(layerSwitched()), this,
          SLOT(update()));
  //  �v���r���[�����������Ƃ��A���ݕ\�����̃t���[������
  //  �v���r���[�^�n�[�t���[�h�������ꍇ�͕\�����X�V����
  connect(IwApp::instance()->getCurrentProject(),
          SIGNAL(previewRenderFinished(int)), this,
          SLOT(onPreviewRenderFinished(int)));
  // �c�[�����؂�ւ������redraw
  connect(IwApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()), this,
          SLOT(update()));
  // �I�����؂�ւ������redraw
  connect(IwApp::instance()->getCurrentSelection(),
          SIGNAL(selectionChanged(IwSelection*)), this, SLOT(update()));

  //-
  // �Y�[���̃R�l�N�g�BsetCommandHandler���ƑΉ�����R�}���h���u��������Ă��܂��̂ŁA����Viewer�ɑΉ��ł��Ȃ�
  connect(CommandManager::instance()->getAction(MI_ZoomIn), SIGNAL(triggered()),
          this, SLOT(zoomIn()));
  connect(CommandManager::instance()->getAction(MI_ZoomOut),
          SIGNAL(triggered()), this, SLOT(zoomOut()));
  connect(CommandManager::instance()->getAction(MI_ActualSize),
          SIGNAL(triggered()), this, SLOT(zoomReset()));

  CommandManager::instance()->getAction(MI_ZoomIn)->setEnabled(true);
  CommandManager::instance()->getAction(MI_ZoomOut)->setEnabled(true);
  CommandManager::instance()->getAction(MI_ActualSize)->setEnabled(true);
}

//----------------------------------
SceneViewer::~SceneViewer() {}

//----------------------------------
// �o�^�e�N�X�`���̏���
//----------------------------------
void SceneViewer::deleteTextures() {
  // �e�N�X�`�����
  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;

  settings->clearAllTextures();
}

void SceneViewer::setProject(IwProject* prj) {
  if (m_project) {
    deleteTextures();
  }
  m_project = prj;
  update();
}

//----------------------------------
void SceneViewer::initializeGL() {
  initializeOpenGLFunctions();

  connect(context(), &QOpenGLContext::aboutToBeDestroyed, this,
          &SceneViewer::cleanup);
  m_program_texture = new QOpenGLShaderProgram(this);
  m_program_texture->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                             ":/tex_vert.glsl");
  m_program_texture->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                             ":/tex_frag.glsl");
  m_program_texture->link();
  m_program_texture->bind();

  u_tex_matrix  = m_program_texture->uniformLocation("matrix");
  u_tex_texture = m_program_texture->uniformLocation("texture");
  u_tex_alpha   = m_program_texture->uniformLocation("alpha");
  // �}�b�g�֌W
  u_tex_matte_sw       = m_program_texture->uniformLocation("matte_sw");
  u_tex_matteImgSize   = m_program_texture->uniformLocation("matteImgSize");
  u_tex_workAreaSize   = m_program_texture->uniformLocation("workAreaSize");
  u_tex_matteTexture   = m_program_texture->uniformLocation("matteTexture");
  u_tex_matteColors    = m_program_texture->uniformLocation("matteColors");
  u_tex_matteTolerance = m_program_texture->uniformLocation("matteTolerance");
  u_tex_matteDilate    = m_program_texture->uniformLocation("matteDilate");

  // ���_�o�b�t�@�I�u�W�F�N�g
  if (m_vbo) {
    delete m_vbo;
    m_vbo = nullptr;
  }
  m_vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
  if (m_vbo->create())
    m_vbo->bind();
  else
    std::cout << "failed to bind QOpenGLBuffer" << std::endl;
  m_vbo->allocate(VertAmount * sizeof(MeshVertex));

  // ���_�C���f�b�N�X�I�u�W�F�N�g
  if (m_ibo) {
    delete m_ibo;
    m_ibo = nullptr;
  }
  m_ibo = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
  if (m_ibo->create())
    m_ibo->bind();
  else
    std::cout << "failed to bind IndexBuffer" << std::endl;
  m_ibo->allocate(VertAmount * sizeof(GLint));

  // Create Vertex Array Object
  if (m_vao) {
    delete m_vao;
    m_vao = 0;
  }
  // �z��o�b�t�@�I�u�W�F�N�g�����A�o�C���h
  m_vao = new QOpenGLVertexArrayObject;
  if (m_vao->create())
    m_vao->bind();
  else
    std::cout << "failed to bind QOpenGLVertexArrayObject" << std::endl;

  m_program_texture->enableAttributeArray(0);
  m_program_texture->enableAttributeArray(1);
  m_program_texture->setAttributeBuffer(
      0, GL_FLOAT, MeshVertex::positionOffset(), MeshVertex::PositionTupleSize,
      MeshVertex::stride());
  m_program_texture->setAttributeBuffer(1, GL_FLOAT, MeshVertex::uvOffset(),
                                        MeshVertex::UvTupleSize,
                                        MeshVertex::stride());

  m_vao->release();
  m_vbo->release();
  m_ibo->release();
  m_program_texture->release();

  //------

  m_program_meshLine = new QOpenGLShaderProgram(this);
  m_program_meshLine->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                              ":/line_vert.glsl");
  m_program_meshLine->addShaderFromSourceFile(QOpenGLShader::Geometry,
                                              ":/mesh_to_line_geom.glsl");
  m_program_meshLine->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                              ":/line_frag.glsl");
  m_program_meshLine->link();
  m_program_meshLine->bind();

  u_meshLine_matrix = m_program_meshLine->uniformLocation("matrix");
  u_meshLine_color  = m_program_meshLine->uniformLocation("color");

  m_vbo->bind();
  m_vao->bind();
  m_ibo->bind();

  m_program_meshLine->enableAttributeArray(0);
  m_program_meshLine->setAttributeBuffer(
      0, GL_FLOAT, MeshVertex::positionOffset(), MeshVertex::PositionTupleSize,
      MeshVertex::stride());

  m_vao->release();
  m_vbo->release();
  m_ibo->release();
  m_program_meshLine->release();

  //------

  // ���_�o�b�t�@�I�u�W�F�N�g
  if (m_line_vbo) {
    delete m_line_vbo;
    m_line_vbo = nullptr;
  }
  m_line_vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
  if (m_line_vbo->create())
    m_line_vbo->bind();
  else
    std::cout << "failed to bind QOpenGLBuffer" << std::endl;
  m_line_vbo->allocate(VertAmount * sizeof(QVector3D));

  // Create Vertex Array Object
  if (m_line_vao) {
    delete m_line_vao;
    m_line_vao = 0;
  }
  // �z��o�b�t�@�I�u�W�F�N�g�����A�o�C���h
  m_line_vao = new QOpenGLVertexArrayObject;
  if (m_line_vao->create())
    m_line_vao->bind();
  else
    std::cout << "failed to bind QOpenGLVertexArrayObject" << std::endl;

  m_program_line = new QOpenGLShaderProgram(this);
  m_program_line->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                          ":/line_vert.glsl");
  m_program_line->addShaderFromSourceFile(QOpenGLShader::Geometry,
                                          ":/stipple_line_geom.glsl");
  m_program_line->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                          ":/stipple_line_frag.glsl");
  m_program_line->link();
  m_program_line->bind();

  m_line_vbo->bind();
  m_line_vao->bind();

  u_line_matrix         = m_program_line->uniformLocation("matrix");
  u_line_color          = m_program_line->uniformLocation("color");
  u_line_viewportSize   = m_program_line->uniformLocation("viewportSize");
  u_line_stippleFactor  = m_program_line->uniformLocation("stippleFactor");
  u_line_stipplePattern = m_program_line->uniformLocation("stipplePattern");

  m_program_line->enableAttributeArray(0);
  m_program_line->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(QVector3D));

  m_line_vao->release();
  m_line_vbo->release();
  m_program_line->release();

  //------

  m_program_fill = new QOpenGLShaderProgram(this);
  m_program_fill->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                          ":/fill_vert.glsl");
  m_program_fill->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                          ":/fill_frag.glsl");
  m_program_fill->link();
  m_program_fill->bind();

  m_line_vbo->bind();
  m_line_vao->bind();

  u_fill_matrix = m_program_fill->uniformLocation("matrix");
  u_fill_color  = m_program_fill->uniformLocation("color");

  m_program_fill->enableAttributeArray(0);
  m_program_fill->setAttributeBuffer(0, GL_FLOAT, 0, 3, sizeof(QVector3D));

  m_line_vao->release();
  m_line_vbo->release();
  m_program_fill->release();

  //------

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
}

//----------------------------------
void SceneViewer::resizeGL(int w, int h) {
  glViewport(0, 0, w, h);
  m_viewProjMatrix = QMatrix4x4();
  m_viewProjMatrix.ortho(0, w, 0, h, -4000, 4000);

  m_modelMatrix.push(QMatrix4x4());
  m_modelMatrix.top().translate(w * 0.5, h * 0.5, 0);
}

//----------------------------------

void SceneViewer::paintGL() {
  if (m_p) delete m_p;
  m_p = new QPainter(this);
  m_p->beginNativePainting();
  // �w�i�̃N���A
  // �F�ݒ肩��F���E��
  double bgCol[3];
  ColorSettings::instance()->getColor(bgCol, Color_Background);
  glClearColor(bgCol[0], bgCol[1], bgCol[2], 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;

  m_modelMatrix.push(m_modelMatrix.top());
  // �J�����ړ�
  m_modelMatrix.top().scale(m_affine.m11(), m_affine.m22(), 1.0);
  m_modelMatrix.top().translate(m_affine.dx(), m_affine.dy(), 0.0);

  PRINT_LOG("paintGL")
  //// ���[�h���ꂽ�摜��\������
  if (settings->getImageMode() == ImageMode_Edit ||
      settings->getImageMode() == ImageMode_Half)
    drawImage();
  // �v���r���[�\�����[�h�̂Ƃ��̓v���r���[���ʂ�\��
  if (settings->getImageMode() == ImageMode_Preview ||
      settings->getImageMode() == ImageMode_Half)
    drawGLPreview();

  // ���[�N�G���A�̃��N��`��
  drawWorkArea();

  // �V�F�C�v��`���B���Ńc�[���̕`�������Ƃ�
  drawShapes();

  m_modelMatrix.pop();

  // ���݂̃��C����������

  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (layer) {
    renderText(10, 30, layer->getName(), QColor(Qt::yellow),
               QFont("Helvetica", 20, QFont::Normal));
  }

  m_p->endNativePainting();
  PRINT_LOG("paintGL End")
}

//----------------------------------

void SceneViewer::renderText(double x, double y, const QString& str,
                             const QColor fontColor, const QFont& font) {
  m_p->endNativePainting();
  m_p->setPen(fontColor);
  m_p->setFont(font);
  m_p->drawText(x, this->height() - y, str);
  m_p->beginNativePainting();
}

//----------------------------------

void SceneViewer::pushMatrix() { m_modelMatrix.push(m_modelMatrix.top()); }
void SceneViewer::popMatrix() { m_modelMatrix.pop(); }
void SceneViewer::translate(GLdouble x, GLdouble y, GLdouble z) {
  m_modelMatrix.top().translate(x, y, z);
}
void SceneViewer::scale(GLdouble x, GLdouble y, GLdouble z) {
  m_modelMatrix.top().scale(x, y, z);
}
void SceneViewer::setColor(const QColor& color) {
  m_program_line->setUniformValue(u_line_color, color);
}
void SceneViewer::doDrawLine(GLenum mode, QVector3D* verts, int vertCount) {
  m_program_line->setUniformValue(u_line_matrix,
                                  m_viewProjMatrix * m_modelMatrix.top());
  auto ptr = m_line_vbo->map(QOpenGLBuffer::WriteOnly);
  memcpy(ptr, verts, sizeof(QVector3D) * vertCount);
  m_line_vbo->unmap();
  glDrawArrays(mode, 0, vertCount);
}
void SceneViewer::doDrawFill(GLenum mode, QVector3D* verts, int vertCount,
                             QColor fillColor) {
  m_program_fill->bind();

  m_program_fill->setUniformValue(u_fill_color, fillColor);
  m_program_fill->setUniformValue(u_fill_matrix,
                                  m_viewProjMatrix * m_modelMatrix.top());

  m_line_vbo->bind();
  m_line_vao->bind();

  auto ptr = m_line_vbo->map(QOpenGLBuffer::WriteOnly);
  memcpy(ptr, verts, sizeof(QVector3D) * vertCount);
  m_line_vbo->unmap();

  glDrawArrays(mode, 0, vertCount);

  m_line_vao->release();
  m_line_vbo->release();

  m_program_line->bind();
  m_line_vbo->bind();
  m_line_vao->bind();
}
void SceneViewer::releaseBufferObjects() {
  m_line_vao->release();
  m_line_vbo->release();
  m_program_line->release();
}
void SceneViewer::bindBufferObjects() {
  m_program_line->bind();
  m_line_vbo->bind();
  m_line_vao->bind();
}

//----------------------------------
// ���[�h���ꂽ�摜��\������
//----------------------------------
void SceneViewer::drawImage() {
  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;

  int frame = settings->getFrame();

  // �f�ނ̕`��
  m_modelMatrix.push(m_modelMatrix.top());

  glEnable(GL_TEXTURE_2D);

  m_program_texture->bind();
  m_program_texture->setUniformValue(u_tex_matrix,
                                     m_viewProjMatrix * m_modelMatrix.top());
  m_program_texture->setUniformValue(u_tex_texture, 0);

  m_vao->bind();
  m_vbo->bind();

  double modeVal = (settings->getImageMode() == ImageMode_Half) ? 0.5 : 1.0;

  // ���C���������珇�ɍ������Ă���
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;
    // ���C������\����������continue;
    if (m_project->getLayer(lay)->isVisibleInViewer() == 0) continue;

    QString path = layer->getImageFilePath(frame);
    if (path.isEmpty()) continue;

    ViewSettings::LAYERIMAGE layerImage =
        getLayerImage(path, layer->brightness(), layer->contrast());

    if (!layerImage.size.isEmpty()) {
      layerImage.texture->bind();

      QSize texSize = layerImage.size;
      QPointF texCornerPos((double)texSize.width() / 2.0,
                           (double)texSize.height() / 2.0);

      // ������
      double visibleVal =
          (m_project->getLayer(lay)->isVisibleInViewer() == 1) ? 0.5 : 1.0;
      GLfloat chan = modeVal * visibleVal;
      m_program_texture->setUniformValue(u_tex_alpha, chan);
      m_program_texture->setUniformValue(u_tex_matte_sw, false);

      MeshVertex verts[4] = {
          MeshVertex(QVector3D(-texCornerPos.x(), -texCornerPos.y(), 0.),
                     QVector2D(0., 0.)),
          MeshVertex(QVector3D(-texCornerPos.x(), texCornerPos.y(), 0.),
                     QVector2D(0., 1.)),
          MeshVertex(QVector3D(texCornerPos.x(), -texCornerPos.y(), 0.),
                     QVector2D(1., 0.)),
          MeshVertex(QVector3D(texCornerPos.x(), texCornerPos.y(), 0.),
                     QVector2D(1., 1.))};

      auto ptr = m_vbo->map(QOpenGLBuffer::WriteOnly);
      memcpy(ptr, verts, sizeof(MeshVertex) * 4);
      m_vbo->unmap();
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      layerImage.texture->release();
    }
  }

  m_vao->release();
  m_vbo->release();
  m_program_texture->release();

  m_modelMatrix.pop();

  glDisable(GL_TEXTURE_2D);
}

//----------------------------------
// �v���r���[���ʂ�`�� GL��
//----------------------------------
void SceneViewer::drawGLPreview() {
  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;

  int frame = settings->getFrame();

  // �f�ނ̕`��
  m_modelMatrix.push(m_modelMatrix.top());

  glEnable(GL_TEXTURE_2D);

  double modeVal = (settings->getImageMode() == ImageMode_Half) ? 0.5 : 1.0;

  QSize workAreaSize = m_project->getWorkAreaSize();
  QPointF texCornerPos((double)workAreaSize.width() / 2.0,
                       (double)workAreaSize.height() / 2.0);
  ResampleMode resampleMode = m_project->getRenderSettings()->getResampleMode();

  m_modelMatrix.top().translate(-texCornerPos.x(), -texCornerPos.y(), 0.);

  m_program_texture->bind();
  m_program_texture->setUniformValue(u_tex_texture, 0);
  m_program_texture->setUniformValue(u_tex_matteTexture, 1);
  m_program_texture->setUniformValue(u_tex_workAreaSize, workAreaSize);

  m_vao->bind();
  m_vbo->bind();

  int targetShapeTag =
      m_project->getRenderQueue()->currentOutputSettings()->getShapeTagId();

  IwTriangleCache::instance()->lock();
  // ���C���������珇�ɍ������Ă���
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;
    // ���C������\����������continue;
    if (m_project->getLayer(lay)->isVisibleInViewer() == 0) continue;

    QString path = layer->getImageFilePath(frame);
    if (path.isEmpty()) continue;
    ViewSettings::LAYERIMAGE layerImage =
        getLayerImage(path, layer->brightness(), layer->contrast());

    if (layerImage.size.isEmpty()) continue;

    double visibleVal =
        (m_project->getLayer(lay)->isVisibleInViewer() == 1) ? 0.5 : 1.0;
    GLfloat chan = modeVal * visibleVal;

    m_program_texture->setUniformValue(u_tex_alpha, chan);

    layerImage.texture->bind(0);

    if (resampleMode == AreaAverage)
      layerImage.texture->setMagnificationFilter(QOpenGLTexture::Linear);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // �V�F�C�v�������ꍇ�A�u�Q�Ɨp�v�V�F�C�v�Ɣ��f���ĕ`�悷��
    if (layer->getShapePairCount(true) == 0) {
      QSize layerSize = layerImage.size;
      QPointF layerCornerPos((double)layerSize.width() / 2.0,
                             (double)layerSize.height() / 2.0);
      double texClipMargin[2] = {0.0, 0.0};
      if (layerSize.width() > workAreaSize.width()) {
        texClipMargin[0] =
            (1.0 - ((double)workAreaSize.width() / (double)layerSize.width())) *
            0.5;
        layerCornerPos.setX((double)workAreaSize.width() / 2.0);
      }
      if (layerSize.height() > workAreaSize.height()) {
        texClipMargin[1] = (1.0 - ((double)workAreaSize.height() /
                                   (double)layerSize.height())) *
                           0.5;
        layerCornerPos.setY((double)workAreaSize.height() / 2.0);
      }

      m_modelMatrix.push(m_modelMatrix.top());
      m_modelMatrix.top().translate(texCornerPos.x(), texCornerPos.y(), 0.);
      // glPushMatrix();
      // glTranslated(texCornerPos.x(), texCornerPos.y(), 0);
      m_program_texture->setUniformValue(
          u_tex_matrix, m_viewProjMatrix * m_modelMatrix.top());
      m_program_texture->setUniformValue(u_tex_matte_sw, false);

      MeshVertex verts[4] = {
          MeshVertex(QVector3D(-layerCornerPos.x(), -layerCornerPos.y(), 0.),
                     QVector2D(texClipMargin[0], texClipMargin[1])),
          MeshVertex(QVector3D(-layerCornerPos.x(), layerCornerPos.y(), 0.),
                     QVector2D(texClipMargin[0], 1.0 - texClipMargin[1])),
          MeshVertex(QVector3D(layerCornerPos.x(), -layerCornerPos.y(), 0.),
                     QVector2D(1.0 - texClipMargin[0], texClipMargin[1])),
          MeshVertex(
              QVector3D(layerCornerPos.x(), layerCornerPos.y(), 0.),
              QVector2D(1.0 - texClipMargin[0], 1.0 - texClipMargin[1]))};

      auto ptr = m_vbo->map(QOpenGLBuffer::WriteOnly);
      memcpy(ptr, verts, sizeof(MeshVertex) * 4);
      m_vbo->unmap();
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      m_modelMatrix.pop();
    } else {
      // �V�F�C�v�������瑖�����Ă���
      for (int s = layer->getShapePairCount() - 1; s >= 0; s--) {
        ShapePair* shape = layer->getShapePair(s);
        if (!shape) continue;
        // �q�V�F�C�v�̂Ƃ����̃V�F�C�v��
        if (!shape->isParent()) continue;

        // �^�[�Q�b�g�łȂ��ꍇ�A���̃V�F�C�v��
        if (!shape->isRenderTarget(targetShapeTag)) continue;

        // �e�V�F�C�v�̂Ƃ��A�L���b�V�����`�F�b�N���ĕ`��
        if (!IwTriangleCache::instance()->isCached(frame, shape)) continue;

        m_program_texture->setUniformValue(
            u_tex_matrix, m_viewProjMatrix * m_modelMatrix.top());

        //---- �}�b�g���
        m_program_texture->setUniformValue(u_tex_matte_sw, false);
        ShapePair::MatteInfo matteInfo = shape->matteInfo();
        QOpenGLTexture* matteTexture   = nullptr;
        if (settings->isMatteApplied() && !matteInfo.layerName.isEmpty()) {
          IwLayer* matteLayer = m_project->getLayerByName(matteInfo.layerName);
          if (matteLayer) {
            QString mattePath = matteLayer->getImageFilePath(frame);
            if (!mattePath.isEmpty()) {
              ViewSettings::LAYERIMAGE matteLayerImage =
                  getLayerImage(mattePath, 0, 0);
              if (!matteLayerImage.size.isEmpty()) {
                matteLayerImage.texture->bind(1);
                matteTexture = matteLayerImage.texture;
                m_program_texture->setUniformValue(u_tex_matte_sw, true);
                m_program_texture->setUniformValue(u_tex_matteImgSize,
                                                   matteLayerImage.size);
                QVector4D colorVec[10];
                for (int i = 0; i < 10; i++) {
                  if (i < matteInfo.colors.size())
                    colorVec[i] = colorToVector(matteInfo.colors[i]);
                  else
                    colorVec[i] = colorToVector(QColor(Qt::transparent));
                }
                m_program_texture->setUniformValueArray(u_tex_matteColors,
                                                        colorVec, 10);
                m_program_texture->setUniformValue(
                    u_tex_matteTolerance,
                    (GLfloat)(matteInfo.tolerance) / 255.f);
                m_program_texture->setUniformValue(
                    u_tex_matteDilate,
                    m_project->getRenderSettings()->getMatteDilate());
              }
            }
          }
        }
        //----

        int vertCount  = IwTriangleCache::instance()->vertexCount(frame, shape);
        int pointCount = IwTriangleCache::instance()->pointCount(frame, shape);

        // ���T�͂�������I�I���_�ƃC���f�b�N�X�����i�[����ׂ�
        MeshVertex* vertex =
            IwTriangleCache::instance()->vertexData(frame, shape);
        int* ids = IwTriangleCache::instance()->idsData(frame, shape);

        auto vptr = m_vbo->map(QOpenGLBuffer::WriteOnly);
        memcpy(vptr, vertex, sizeof(MeshVertex) * pointCount);
        m_vbo->unmap();

        m_ibo->bind();
        auto iptr = m_ibo->map(QOpenGLBuffer::WriteOnly);
        memcpy(iptr, ids, sizeof(int) * vertCount);
        m_ibo->unmap();

        glDrawElements(GL_TRIANGLES, vertCount, GL_UNSIGNED_INT, (void*)0);

        if (matteTexture) matteTexture->release(1);

        // ���b�V����\��
        if (settings->isMeshVisible()) {
          m_program_meshLine->bind();
          m_program_meshLine->setUniformValue(
              u_meshLine_color, QColor::fromRgbF(0.5, 1.0, 0.0, 0.4));
          m_program_meshLine->setUniformValue(
              u_meshLine_matrix, m_viewProjMatrix * m_modelMatrix.top());

          glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
          glDrawElements(GL_TRIANGLES, vertCount, GL_UNSIGNED_INT, (void*)0);
          glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

          m_program_texture->bind();
        }

        m_ibo->release();
      }
    }

    if (resampleMode == AreaAverage)
      layerImage.texture->setMagnificationFilter(QOpenGLTexture::Nearest);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    layerImage.texture->release(0);
  }
  m_vao->release();
  m_vbo->release();
  m_program_texture->release();

  m_modelMatrix.pop();

  glDisable(GL_TEXTURE_2D);

  IwTriangleCache::instance()->unlock();
}

void SceneViewer::setLineStipple(GLint factor, GLushort pattern) {
  GLfloat p[16];
  int i;
  for (i = 0; i < 16; i++) {
    p[i] = (pattern & (1 << i)) ? 1.0f : 0.0f;
  }
  m_program_line->setUniformValueArray(u_line_stipplePattern, p, 16, 1);
  m_program_line->setUniformValue(u_line_stippleFactor, GLfloat(factor));
}

//----------------------------------
// ���[�N�G���A�̃��N��`��
//----------------------------------
void SceneViewer::drawWorkArea() {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ���[�N�G���A�T�C�Y����Ȃ�return
  if (workAreaSize.isEmpty()) return;

  m_modelMatrix.push(m_modelMatrix.top());
  m_modelMatrix.top().translate(-(double)workAreaSize.width() / 2.0,
                                -(double)workAreaSize.height() / 2.0, 0.0);
  m_modelMatrix.top().scale((double)workAreaSize.width(),
                            (double)workAreaSize.height(), 1.0);

  m_program_line->bind();
  m_line_vao->bind();
  m_line_vbo->bind();

  m_program_line->setUniformValue(u_line_matrix,
                                  m_viewProjMatrix * m_modelMatrix.top());

  GLfloat viewport[4];
  glGetFloatv(GL_VIEWPORT, viewport);
  m_program_line->setUniformValue(u_line_viewportSize,
                                  QSizeF(viewport[2], viewport[3]));

  ViewSettings* settings = m_project->getViewSettings();

  if (settings->getImageMode() == ImageMode_Preview ||
      settings->getImageMode() == ImageMode_Half) {
    m_program_line->setUniformValue(u_line_color,
                                    QColor::fromRgbF(1.0, 0.545, 0.392));
    setLineStipple(2, 0x00FF);
  } else {
    m_program_line->setUniformValue(u_line_color,
                                    QColor::fromRgbF(1.0, 0.5, 0.5));
    setLineStipple(2, 0xAAAA);
  }

  QVector3D verts[4]{QVector3D(1.0, 0.0, 0.0), QVector3D(1.0, 1.0, 0.0),
                     QVector3D(0.0, 1.0, 0.0), QVector3D(0.0, 0.0, 0.0)};

  auto ptr = m_line_vbo->map(QOpenGLBuffer::WriteOnly);
  memcpy(ptr, verts, sizeof(QVector3D) * 4);
  m_line_vbo->unmap();
  glDrawArrays(GL_LINE_LOOP, 0, 4);

  m_line_vao->release();
  m_line_vbo->release();
  m_program_line->release();

  m_modelMatrix.pop();
}

//----------------------------------
// �V�F�C�v��`��
//----------------------------------
void SceneViewer::drawShapes() {
  auto drawOneShape = [&](const OneShape& shape, int frame) {
    QVector3D* vertexArray =
        shape.shapePairP->getVertexArray(frame, shape.fromTo, m_project);

    auto ptr       = m_line_vbo->map(QOpenGLBuffer::WriteOnly);
    int vertAmount = shape.shapePairP->getVertexAmount(m_project);
    memcpy(ptr, vertexArray, sizeof(QVector3D) * vertAmount);
    m_line_vbo->unmap();
    if (shape.shapePairP->isClosed())
      glDrawArrays(GL_LINE_LOOP, 0, vertAmount);
    else
      glDrawArrays(GL_LINE_STRIP, 0, vertAmount);

    delete[] vertexArray;
  };

  PRINT_LOG("drawShapes start")
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ���[�N�G���A�T�C�Y����Ȃ�return
  if (workAreaSize.isEmpty()) return;

  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  // ���[�N�G���A�̍��������_�A���[�N�G���A�̏c���T�C�Y���P�ɐ��K���������W�n�ɕϊ�����
  m_modelMatrix.push(m_modelMatrix.top());
  m_modelMatrix.top().translate(-(double)workAreaSize.width() / 2.0,
                                -(double)workAreaSize.height() / 2.0, 0.0);
  m_modelMatrix.top().scale((double)workAreaSize.width(),
                            (double)workAreaSize.height(), 1.0);

  m_program_line->bind();
  m_line_vao->bind();
  m_line_vbo->bind();

  m_program_line->setUniformValue(u_line_matrix,
                                  m_viewProjMatrix * m_modelMatrix.top());
  GLfloat viewport[4];
  glGetFloatv(GL_VIEWPORT, viewport);
  m_program_line->setUniformValue(u_line_viewportSize,
                                  QSizeF(viewport[2], viewport[3]));

  setLineStipple(1, 0xFFFF);

  // �J�����g���C���̏ꍇ�͐F��ς���
  IwLayer* currentLayer = IwApp::instance()->getCurrentLayer()->getLayer();

  // �J�����g�c�[�����擾
  IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
  //---- ���b�N���𓾂�
  bool fromToVisible[2];
  for (int fromTo = 0; fromTo < 2; fromTo++)
    fromToVisible[fromTo] =
        m_project->getViewSettings()->isFromToVisible(fromTo);
  //----

  // �I�j�I���X�L���𓾂�
  QSet<int> onionFrames = m_project->getOnionFrames();
  int onionRange        = m_project->getViewSettings()->getOnionRange();
  int projectLength     = m_project->getProjectFrameLength();

  // �e���C���ɂ���
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    PRINT_LOG("  Layer" + std::to_string(lay))

    // ���C�����擾
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;

    bool isCurrent = (layer == currentLayer);
    // ���݂̃��C������\���Ȃ�continue
    // �������J�����g���C���̏ꍇ�͕\������
    if (!isCurrent && !layer->isVisibleInViewer()) continue;

    // �I���V�F�C�v���ď�ʂɕ`���Ă݂�
    QList<OneShape> selectedShapes;

    for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
      PRINT_LOG("   Shape" + std::to_string(sp))

      ShapePair* shapePair = layer->getShapePair(sp);
      if (!shapePair) continue;

      // ��\���Ȃ�conitnue
      if (!shapePair->isVisible()) continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // 140128 ���b�N����Ă������\���ɂ���
        if (!fromToVisible[fromTo]) continue;

        OneShape oneShape(shapePair, fromTo);
        bool isSelected =
            IwShapePairSelection::instance()->isSelected(oneShape);

        // �I���V�F�C�v�͌�񂵂ɂ���
        if (isCurrent && isSelected) {
          selectedShapes.append(oneShape);
          continue;
        }

        // ���O�̃v�b�V��
        if (!shapePair->isLocked(fromTo) && !layer->isLocked())
          glPushName(layer->getNameFromShapePair(oneShape));

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        QColor lineColor = QColor::fromRgbF(0.7, 0.7, 0.7, 0.5);
        if (isCurrent) {
          if (isSelected)
            lineColor = (fromTo == 0) ? QColor::fromRgbF(1.0, 0.7, 0.0)
                                      : QColor::fromRgbF(0.0, 0.7, 1.0);
          // ���b�N����Ă����甖������
          else if (shapePair->isLocked(fromTo) || layer->isLocked())
            lineColor = (fromTo == 0) ? QColor::fromRgbF(0.7, 0.2, 0.2, 0.7)
                                      : QColor::fromRgbF(0.2, 0.2, 0.7, 0.7);
          else
            lineColor = (fromTo == 0) ? QColor::fromRgbF(1.0, 0.0, 0.0)
                                      : QColor::fromRgbF(0.0, 0.0, 1.0);
        }
        // ReshapeTool�����CorrTool��Snap�Ώۂ̃V�F�C�v
        if (tool && tool->setSpecialShapeColor(oneShape))
          lineColor = QColor::fromRgbF(1.0, 0.0, 1.0);

        m_program_line->setUniformValue(u_line_color, lineColor);

        drawOneShape(oneShape, frame);

        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        // ���O�̃|�b�v
        if (!shapePair->isLocked(fromTo) && !layer->isLocked()) glPopName();
      }
    }

    // �I���V�F�C�v���ď�ʂɕ`���Ă݂�
    for (auto oneShape : selectedShapes) {
      // �I�j�I���V�F�C�v�̕`��
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      if (!onionFrames.isEmpty()) {
        QColor lineColor;
        lineColor = (oneShape.fromTo == 0)
                        ? QColor::fromRgbF(0.8, 0.7, 0.2, 0.8)
                        : QColor::fromRgbF(0.2, 0.7, 0.8, 0.8);
        m_program_line->setUniformValue(u_line_color, lineColor);

        foreach (const int& onionFrame, onionFrames) {
          if (onionFrame == frame) continue;
          drawOneShape(oneShape, onionFrame);
        }
      }
      // relative onion skin
      if (onionRange > 0) {
        int df = (onionRange >= 50) ? 2 : 1;
        for (int relativeOf = frame - onionRange;
             relativeOf <= frame + onionRange; relativeOf += df) {
          if (relativeOf >= projectLength) break;
          if (relativeOf < 0) continue;
          if (relativeOf == frame) continue;
          if (onionFrames.contains(relativeOf)) continue;
          double ratio =
              1.0 - (double)(std::abs(relativeOf - frame)) / (double)onionRange;
          double alpha = 0.8 * ratio + 0.1 * (1.0 - ratio);
          QColor lineColor;
          lineColor = (oneShape.fromTo == 0)
                          ? QColor::fromRgbF(0.8, 0.7, 0.2, alpha)
                          : QColor::fromRgbF(0.2, 0.7, 0.8, alpha);
          m_program_line->setUniformValue(u_line_color, lineColor);

          drawOneShape(oneShape, relativeOf);
        }
      }

      if (!oneShape.shapePairP->isLocked(oneShape.fromTo) && !layer->isLocked())
        glPushName(layer->getNameFromShapePair(oneShape));

      QColor lineColor;
      if (oneShape.fromTo == 0)
        lineColor = QColor::fromRgbF(1.0, 0.7, 0.0);
      else
        lineColor = QColor::fromRgbF(0.0, 0.7, 1.0);
      // ���̂Ƃ���ReshapeTool��Snap�Ώۂ̃V�F�C�v�̂�
      if (tool && tool->setSpecialShapeColor(oneShape))
        lineColor = QColor::fromRgbF(1.0, 0.0, 1.0);
      m_program_line->setUniformValue(u_line_color, lineColor);

      drawOneShape(oneShape, frame);

      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

      // ���O�̃|�b�v
      if (!oneShape.shapePairP->isLocked(oneShape.fromTo) && !layer->isLocked())
        glPopName();
    }
  }

  // �V�F�C�v�̕�ԃn���h���̃h���b�O���́A�O��̃L�[�t���[���Ԃ̃V�F�C�v���I�j�I���X�L���ŕ\������
  if (InterpHandleDragTool::instance()->isDragging()) {
    OneShape shape;
    bool isForm;
    int key, nextKey;
    InterpHandleDragTool::instance()->getInfo(shape, isForm, key, nextKey);
    if (isForm) {
      m_program_line->setUniformValue(u_line_color,
                                      QColor::fromRgbF(1.0, 0.0, 1.0));
      for (int f = key; f <= nextKey; f++) {
        drawOneShape(shape, f);
      }
    }
  }

  PRINT_LOG("  Draw Guides")
  // �K�C�h��`��
  if (m_hRuler->getGuideCount() > 0 || m_vRuler->getGuideCount() > 0) {
    setLineStipple(1, 0xAAAA);
    // �`��͈�
    double minV = m_vRuler->posToValue(QPoint(0, 0));
    double maxV = m_vRuler->posToValue(QPoint(0, height()));
    QColor lineColor;
    for (int gId = 0; gId < m_hRuler->getGuideCount(); gId++) {
      if (tool && tool->setSpecialGridColor(gId, false))
        lineColor = QColor::fromRgbF(1.0, 0.0, 1.0);
      else
        lineColor = QColor::fromRgbF(0.7, 0.7, 0.7);
      m_program_line->setUniformValue(u_line_color, lineColor);
      double g = m_hRuler->getGuide(gId);

      auto ptr = m_line_vbo->map(QOpenGLBuffer::WriteOnly);
      QVector3D verts[2]{QVector3D(g, minV, 0.0), QVector3D(g, maxV, 0.0)};
      memcpy(ptr, verts, sizeof(QVector3D) * 2);
      m_line_vbo->unmap();
      glDrawArrays(GL_LINES, 0, 2);
    }

    minV = m_hRuler->posToValue(QPoint(0, 0));
    maxV = m_hRuler->posToValue(QPoint(width(), 0));
    for (int gId = 0; gId < m_vRuler->getGuideCount(); gId++) {
      if (tool && tool->setSpecialGridColor(gId, false))
        lineColor = QColor::fromRgbF(1.0, 0.0, 1.0);
      else
        lineColor = QColor::fromRgbF(0.7, 0.7, 0.7);
      m_program_line->setUniformValue(u_line_color, lineColor);
      double g = m_vRuler->getGuide(gId);

      auto ptr = m_line_vbo->map(QOpenGLBuffer::WriteOnly);
      QVector3D verts[2]{QVector3D(minV, g, 0.0), QVector3D(maxV, g, 0.0)};
      memcpy(ptr, verts, sizeof(QVector3D) * 2);
      m_line_vbo->unmap();
      glDrawArrays(GL_LINES, 0, 2);
    }
  }

  PRINT_LOG("  Draw Tools")
  // �c�[�����擾�ł��Ȃ�������return
  // �v���W�F�N�g���J�����g����Ȃ��Ă�return
  if (tool && m_project->isCurrent()) {
    // �c�[���ɂ��̃r���[�A�̃|�C���^��n��
    tool->setViewer(this);
    // �c�[���̕`��
    tool->draw();
  }

  m_line_vao->release();
  m_line_vbo->release();
  m_program_line->release();

  m_modelMatrix.pop();
  PRINT_LOG("drawShapes end")
}

//----------------------------------
// �e�N�X�`���C���f�b�N�X�𓾂�B������΃��[�h���ēo�^
//----------------------------------
ViewSettings::LAYERIMAGE SceneViewer::getLayerImage(QString& path,
                                                    int brightness,
                                                    int contrast) {
  // �����̃v���W�F�N�g���J�����g�v���W�F�N�g����Ȃ����return
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project)
    return ViewSettings::LAYERIMAGE();
  ViewSettings* settings = m_project->getViewSettings();
  int shrink             = m_project->getRenderSettings()->getImageShrink();

  // ���ɕۑ����Ă���΂����Ԃ�
  if (settings->hasTexture(path)) {
    if (settings->getTextureId(path).brightness == brightness &&
        settings->getTextureId(path).contrast == contrast &&
        settings->getTextureId(path).shrink == shrink)
      return settings->getTextureId(path);
    // brightness contrast ���قȂ�ꍇ�͍Čv�Z
    else
      settings->releaseTexture(path);
  }

  TImageP img;
  // �L���b�V������Ă�����A�����Ԃ�
  if (IwImageCache::instance()->isCached(path))
    img = IwImageCache::instance()->get(path);
  else
    img = IoCmd::loadImage(path);

  // �摜�����[�h�ł�����A�L���b�V���ɓ����
  if (img.getPointer()) {
    std::cout << "load success" << std::endl;
    IwImageCache::instance()->add(path, img);
  } else
    return ViewSettings::LAYERIMAGE();

  TRaster32P ras;
  TRasterImageP ri = (TRasterImageP)img;
  TToonzImageP ti  = (TToonzImageP)img;
  if (ri) {
    TRasterP tmpRas = ri->getRaster();
    // pixelSize = 4 �Ȃ� 8bpc�A8 �Ȃ� 16bpc
    // 16bit�Ȃ�8bit�ɕϊ�
    if (tmpRas->getPixelSize() == 8) {
      ras = TRaster32P(tmpRas->getWrap(), tmpRas->getLy());
      TRop::convert(ras, tmpRas);
    } else
      ras = tmpRas;

    // Premultiply����Ă��邩����
    if (!isPremultiplied(ras)) TRop::premultiply(ras);

  } else if (ti) {
    TRasterCM32P rascm;
    rascm = ti->getRaster();
    ras   = TRaster32P(rascm->getWrap(), rascm->getLy());

    TRop::convert(ras, rascm, ti->getPalette());
  }
  // �`��������Ȃ����continue
  else {
    return ViewSettings::LAYERIMAGE();
  }

  ViewSettings::LAYERIMAGE layerImage;

  layerImage.size = QSize(ras->getWrap(), ras->getLy());
  std::cout << "texSize = " << layerImage.size.width() << " x "
            << layerImage.size.height() << std::endl;

  if (shrink != 1) {
    TRaster32P resized_ras =
        TRaster32P(ras->getWrap() / shrink, ras->getLy() / shrink);
    TRop::over(resized_ras, ras,
               TScale(1.0 / (double)shrink, 1.0 / (double)shrink));
    ras               = resized_ras;
    layerImage.shrink = shrink;
  }

  // brightness contrast computation
  if (brightness != 0 || contrast != 0) {
    ras = ras->clone();
    adjustBrightnessContrast(ras, brightness, contrast);
    layerImage.brightness = brightness;
    layerImage.contrast   = contrast;
  }

  layerImage.texture = new QOpenGLTexture(QImage(
      ras->getRawData(), ras->getWrap(), ras->getLy(), QImage::Format_ARGB32));

  // �e�N�X�`���Ɋe��v���p�e�B������
  // glBindTexture(GL_TEXTURE_2D, texName);
  // �e�N�X�`�����E�F
  // static const GLfloat border[] = { 0.0f, 0.0f, 0.0f, 0.0f };
  // glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
  // �ŊO�������E�F�ɂ���
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  // �g�債�Ă���Ԃ��Ȃ�
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // �k������Ƃ��̓��j�A���
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  layerImage.texture->setBorderColor(Qt::transparent);
  layerImage.texture->setWrapMode(QOpenGLTexture::DirectionS,
                                  QOpenGLTexture::ClampToBorder);
  layerImage.texture->setWrapMode(QOpenGLTexture::DirectionT,
                                  QOpenGLTexture::ClampToBorder);
  layerImage.texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
  layerImage.texture->setMagnificationFilter(QOpenGLTexture::Nearest);
  layerImage.texture->bind();

  settings->storeTextureId(path, layerImage);
  return layerImage;
}

//----------------------------------
// �����v���r���[���ʂ̉摜���L���b�V������Ă�����A
// ������g���ăe�N�X�`�������B�e�N�X�`���C���f�b�N�X��Ԃ�
//----------------------------------
unsigned int SceneViewer::setPreviewTexture() {
  // �����̃v���W�F�N�g���J�����g�v���W�F�N�g����Ȃ����return
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project)
    return 0;

  ViewSettings* settings = m_project->getViewSettings();

  // �\������t���[�����擾
  int viewFrame = m_project->getViewFrame();

  // �L���b�V���̃G�C���A�X�𓾂�
  QString cacheAlias =
      QString("PRJ%1_PREVIEW_FRAME%2").arg(m_project->getId()).arg(viewFrame);

  // �L���b�V������Ă��Ȃ�������return
  if (!IwImageCache::instance()->isCached(cacheAlias)) return 0;

  // �摜���擾
  TImageP img = IwImageCache::instance()->get(cacheAlias);

  if (!img.getPointer()) return 0;

  TRasterP ras;
  TRasterImageP ri = (TRasterImageP)img;
  if (ri) {
    TRasterP tmpRas = ri->getRaster();
    // pixelSize = 4 �Ȃ� 8bpc�A8 �Ȃ� 16bpc
    // 16bit�Ȃ�8bit�ɕϊ�
    if (tmpRas->getPixelSize() == 8) {
      ras = TRaster32P(tmpRas->getWrap(), tmpRas->getLy());
      TRop::convert(ras, tmpRas);
    } else
      ras = tmpRas;
  } else
    return 0;

  // ras->lock();

  QSize texSize(ras->getWrap(), ras->getLy());
  QOpenGLTexture* texture =
      new QOpenGLTexture(QImage(ras->getRawData(), texSize.width(),
                                texSize.height(), QImage::Format_ARGB32));
  GLuint texName = texture->textureId();

  // �e�N�X�`���Ɋe��v���p�e�B������
  glBindTexture(GL_TEXTURE_2D, texName);
  // �[���J��Ԃ��Ȃ�
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  // �g�債�Ă���Ԃ��Ȃ�
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // �k������Ƃ��̓��j�A���
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  settings->setPreviewTextureId(viewFrame, (unsigned int)texName);

  return (unsigned int)texName;
}

//----------------------------------
// �J�����g���C���؂�ւ��R�}���h��o�^����
//----------------------------------
void SceneViewer::addContextMenus(QMenu& menu) {
  // ���C��������ꍇ
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (prj && prj->getLayerCount() > 0) {
    if (!menu.actions().isEmpty()) menu.addSeparator();
    QMenu* changeLayerMenu = new QMenu(tr("Change Current Layer"));
    for (int lay = 0; lay < prj->getLayerCount(); lay++) {
      QAction* tmpAct = new QAction(prj->getLayer(lay)->getName(), 0);
      tmpAct->setData(lay);  // ���C���C���f�b�N�X���i�[
      connect(tmpAct, SIGNAL(triggered()), this, SLOT(onChangeCurrentLayer()));
      changeLayerMenu->addAction(tmpAct);
    }
    menu.addMenu(changeLayerMenu);

    menu.addSeparator();
  }

  menu.addAction(CommandManager::instance()->getAction(MI_InsertImages));

  bool separatorAdded = false;
  if (!IwShapePairSelection::instance()->isEmpty()) {
    menu.addSeparator();
    menu.addAction(CommandManager::instance()->getAction(MI_ToggleShapeLock));
    separatorAdded = true;
  }
  if (ProjectUtils::hasLockedShape()) {
    if (!separatorAdded) menu.addSeparator();
    QAction* unlockAct = menu.addAction(tr("Unlock All Shapes"));
    QObject::connect(unlockAct, &QAction::triggered,
                     []() { ProjectUtils::unlockAllShapes(); });
  }
  if (!IwShapePairSelection::instance()->isEmpty()) {
    menu.addAction(CommandManager::instance()->getAction(MI_ToggleVisibility));
  }
}

//----------------------------------

void SceneViewer::updateRulers() {
  if (m_hRuler) m_hRuler->update();
  if (m_vRuler) m_vRuler->update();
}

//----------------------------------
// ViewSettings���ς������
// �����̃v���W�F�N�g���J�����g�Ȃ�Aupdate
//----------------------------------
void SceneViewer::onViewSettingsChanged() {
  // �����̃v���W�F�N�g���J�����g�v���W�F�N�g����Ȃ����return
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project) return;

  repaint();
  update();
}

//----------------------------------
// �T�C�Y����
//----------------------------------
void SceneViewer::zoomIn() {
  // �����̃v���W�F�N�g���J�����g�v���W�F�N�g����Ȃ����return
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project) return;

  int zoomStep = m_project->getViewSettings()->getZoomStep();
  if (zoomStep >= 5) return;

  QPointF delta(m_mousePos.x() - width() / 2, -m_mousePos.y() + height() / 2);
  delta /= m_affine.determinant();

  m_project->getViewSettings()->setZoomStep(zoomStep + 1);

  m_affine = m_affine.translate(delta.x(), delta.y())
                 .scale(2.0, 2.0)
                 .translate(-delta.x() * 0.75, -delta.y() * 0.75);
  // m_affine = m_affine.scale(2.0,2.0);
  update();
  updateRulers();

  // �^�C�g���o�[�X�V
  IwApp::instance()->getMainWindow()->updateTitle();
}
//----------------------------------
void SceneViewer::zoomOut() {
  // �����̃v���W�F�N�g���J�����g�v���W�F�N�g����Ȃ����return
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project) return;

  int zoomStep = m_project->getViewSettings()->getZoomStep();
  if (zoomStep <= -3) return;

  QPointF delta(m_mousePos.x() - width() / 2, -m_mousePos.y() + height() / 2);
  delta /= m_affine.determinant();

  m_project->getViewSettings()->setZoomStep(zoomStep - 1);

  m_affine = m_affine.translate(delta.x(), delta.y()).scale(0.5, 0.5);
  update();
  updateRulers();

  // �^�C�g���o�[�X�V
  IwApp::instance()->getMainWindow()->updateTitle();
}
//----------------------------------
void SceneViewer::zoomReset() {
  // �����̃v���W�F�N�g���J�����g�v���W�F�N�g����Ȃ����return
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project) return;

  m_project->getViewSettings()->setZoomStep(0);

  m_affine.reset();
  update();
  updateRulers();

  // �^�C�g���o�[�X�V
  IwApp::instance()->getMainWindow()->updateTitle();
}

//----------------------------------
// �J�����g���C����ς���
//----------------------------------
void SceneViewer::onChangeCurrentLayer() {
  int layerIndex = qobject_cast<QAction*>(sender())->data().toInt();
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  if (layerIndex < 0 || layerIndex >= prj->getLayerCount()) return;
  IwApp::instance()->getCurrentLayer()->setLayer(prj->getLayer(layerIndex));
}

//----------------------------------
// �}�E�X�ʒu����IwaWarper�̍��W�n�ɕϊ�����
//----------------------------------
inline QPointF SceneViewer::convertMousePosToIWCoord(const QPointF& mousePos) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  double iwx = ((mousePos.x() - (double)width() * 0.5) / m_affine.m11() -
                m_affine.m31()) /
                   (double)workAreaSize.width() +
               0.5;
  double iwy = (((double)height() * 0.5 - mousePos.y()) / m_affine.m22() -
                m_affine.m32()) /
                   (double)workAreaSize.height() +
               0.5;

  return QPointF(iwx, iwy);
}

//----------------------------------
// ���h���b�O�Ńp������
//----------------------------------
void SceneViewer::mouseMoveEvent(QMouseEvent* e) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ���[�N�G���A�T�C�Y����Ȃ�return
  if (workAreaSize.isEmpty()) return;

  // ���{�^���h���b�O�̏ꍇ�A�p������
  if ((e->buttons() & Qt::MiddleButton) != 0 &&
      m_mouseButton == Qt::MiddleButton && m_isPanning) {
    // �X�N���[���o�[���X���C�h
    QPoint dp = e->pos() - m_mousePos;
    // �}�E�X�̓����ƍ��W�n�͏㉺�t
    m_affine.translate((double)dp.x() / m_affine.m11() / m_affine.m11(),
                       -(double)dp.y() / m_affine.m22() / m_affine.m22());
    m_mousePos = e->pos();
    update();
    updateRulers();
    return;
  }
  // ���{�^���h���b�O�̏ꍇ�A�c�[���̋@�\�����s
  // �N���b�N�̓E�B���h�E���ōs���Ă��Ȃ��Ƃ����Ȃ�
  else if ((e->buttons() & Qt::LeftButton) != 0 &&
           m_mouseButton == Qt::LeftButton) {
    // �J�����g�c�[�����擾
    IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
    // �c�[�����擾�ł��Ȃ�������return
    // �v���W�F�N�g���J�����g����Ȃ��Ă�return
    if (!tool || !m_project->isCurrent()) return;

    // �}�E�X�ʒu����IwaWarper�̍��W�n�ɕϊ�����
    QPointF erPos = convertMousePosToIWCoord(e->localPos());
    // �c�[���̋@�\�����s
    bool redrawViewer = tool->leftButtonDrag(erPos, e);
    if (redrawViewer) update();
    // Zoom��Viewer���S�ɂ���Ƃ��Ɏg��
    m_mousePos = e->pos();
    return;
  }
  // �}�E�X��������Ă��Ȃ��ꍇ�A�}�E�X�J�[�\�����X�V
  else {
    // �J�����g�c�[�����擾
    IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
    // �c�[�����擾�ł��Ȃ�������return
    // �v���W�F�N�g���J�����g����Ȃ��Ă�return
    if (!tool || !m_project->isCurrent()) return;

    // �}�E�X�ʒu����IwaWarper�̍��W�n�ɕϊ�����
    QPointF erPos = convertMousePosToIWCoord(e->localPos());
    // QSize workAreaSize = m_project->getWorkAreaSize();
    // std::cout << "pixelPos = " << erPos.x() * (double)workAreaSize.width()
    //   << ", " << erPos.y() * (double)workAreaSize.height() << std::endl;
    // �c�[���̋@�\�����s
    bool redrawViewer = tool->mouseMove(erPos, e);
    if (redrawViewer) update();

    setToolCursor(this, tool->getCursorId(e));

    // Zoom��Viewer���S�ɂ���Ƃ��Ɏg��
    m_mousePos = e->pos();
  }
}

//----------------------------------
// ���N���b�N�Ńp�����[�h�ɂ���
//----------------------------------
void SceneViewer::mousePressEvent(QMouseEvent* e) {
  // ���{�^���̏ꍇ�A�p�����[�h�Ɉڍs
  if (e->button() == Qt::MiddleButton) {
    // �}�E�X�ʒu���i�[
    m_mousePos = e->pos();
    // �p�����[�h��ON
    m_isPanning = true;

    m_mouseButton = Qt::MiddleButton;
    return;
  }
  // use tool functions when left or right click
  else if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton) {
    // �J�����g�c�[�����擾
    IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
    // �c�[�����擾�ł��Ȃ�������return
    // �v���W�F�N�g���J�����g����Ȃ��Ă�return
    if (!tool || !m_project->isCurrent()) return;

    // �E�N���b�N�̏ꍇ�A�J�����g���C���؂�ւ����j���[���o��
    if (!IwApp::instance()->getCurrentLayer()->getLayer()) {
      if (e->button() == Qt::RightButton) {
        QMenu menu(0);
        // �J�����g���C���؂�ւ��R�}���h��o�^����
        addContextMenus(menu);
        // ���j���[�ɃA�N�V�������o�^����Ă�����
        if (!menu.actions().isEmpty()) menu.exec(e->globalPos());
      }
      return;
    }

    // �}�E�X�ʒu����IwaWarper�̍��W�n�ɕϊ�����
    QPointF erPos = convertMousePosToIWCoord(e->localPos());
    // �c�[���̋@�\�����s
    bool redrawViewer;
    if (e->button() == Qt::LeftButton)
      redrawViewer = tool->leftButtonDown(erPos, e);
    else  // �E�{�^���̏ꍇ
    {
      bool canOpenMenu = true;
      QMenu menu(0);
      redrawViewer = tool->rightButtonDown(erPos, e, canOpenMenu, menu);
      // �J�����g���C����؂�ւ��郁�j���[��ǉ�����
      if (canOpenMenu) {
        // �J�����g���C���؂�ւ��R�}���h��o�^����
        addContextMenus(menu);
        // ���j���[�ɃA�N�V�������o�^����Ă�����
        if (!menu.actions().isEmpty()) menu.exec(e->globalPos());
      }
    }

    if (redrawViewer) update();

    m_mouseButton = e->button();
  }
}

//----------------------------------
// �p�����[�h���N���A����
//----------------------------------
void SceneViewer::mouseReleaseEvent(QMouseEvent* e) {
  // �p�����[�h��OFF
  m_isPanning = false;

  // ���{�^���������[�X�����ꍇ�A�}�E�X�������[�X����
  if (m_mouseButton == Qt::LeftButton) {
    // �J�����g�c�[�����擾
    IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
    // �c�[�����擾�ł��Ȃ�������return
    // �v���W�F�N�g���J�����g����Ȃ��Ă�return
    if (!tool || !m_project->isCurrent()) {
      m_mouseButton = Qt::NoButton;
      return;
    }

    // �}�E�X�ʒu����IwaWarper�̍��W�n�ɕϊ�����
    QPointF erPos = convertMousePosToIWCoord(e->localPos());
    // �c�[���̋@�\�����s
    bool redrawViewer = tool->leftButtonUp(erPos, e);
    if (redrawViewer) update();
  }

  // �}�E�X�{�^������������
  m_mouseButton = Qt::NoButton;
}

//----------------------------------
// �p�����[�h���N���A����
//----------------------------------
void SceneViewer::leaveEvent(QEvent* /*e*/) {
  // �p�����[�h��OFF
  m_isPanning = false;
}

//----------------------------------
// �C���L�[���N���A����
//----------------------------------
void SceneViewer::enterEvent(QEvent*) { m_modifiers = Qt::NoModifier; }

//----------------------------------
// �C���L�[�������ƃJ�[�\����ς���
//----------------------------------
void SceneViewer::keyPressEvent(QKeyEvent* e) {
  // �v���W�F�N�g���J�����g����Ȃ����return
  if (!m_project->isCurrent()) return;

  // �J�����g�c�[�����擾
  IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
  // �c�[�����擾�ł��Ȃ�������return
  if (!tool) return;

  int key = e->key();

  // �c�[�����V���[�g�J�b�g�L�[���g���Ă��邩�ǂ����H
  // ���̂Ƃ���ATransformTool��ReshapeTool��Ctrl�{���L�[���g���Ă���
  bool ret = tool->keyDown(key, (e->modifiers() & Qt::ControlModifier),
                           (e->modifiers() & Qt::ShiftModifier),
                           (e->modifiers() & Qt::AltModifier));
  if (ret) {
    e->accept();
    update();
    return;
  }

  //----- �ȉ��A�c�[�����L�[���E��Ȃ������ꍇ

  if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
      key == Qt::Key_AltGr) {
    if (key == Qt::Key_Shift)
      m_modifiers |= Qt::ShiftModifier;
    else if (key == Qt::Key_Control) {
      m_modifiers |= Qt::ControlModifier;
    } else if (key == Qt::Key_Alt || key == Qt::Key_AltGr)
      m_modifiers |= Qt::AltModifier;

    QMouseEvent mouseEvent(QEvent::MouseMove, m_mousePos, Qt::NoButton,
                           Qt::NoButton, m_modifiers);

    setToolCursor(this, tool->getCursorId(&mouseEvent));
  }
}

//----------------------------------
void SceneViewer::keyReleaseEvent(QKeyEvent* e) {
  // �v���W�F�N�g���J�����g����Ȃ����return
  if (!m_project->isCurrent()) return;

  int key = e->key();

  if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
      key == Qt::Key_AltGr) {
    // �J�����g�c�[�����擾
    IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
    // �c�[�����擾�ł��Ȃ�������return
    if (!tool) return;

    if (key == Qt::Key_Shift)
      m_modifiers &= ~Qt::ShiftModifier;
    else if (key == Qt::Key_Control) {
      m_modifiers &= ~Qt::ControlModifier;
      // std::cout<<"Ctrl pressed"<<std::endl;
    } else if (key == Qt::Key_Alt || key == Qt::Key_AltGr)
      m_modifiers &= ~Qt::AltModifier;

    QMouseEvent mouseEvent(QEvent::MouseMove, m_mousePos, Qt::NoButton,
                           Qt::NoButton, m_modifiers);

    setToolCursor(this, tool->getCursorId(&mouseEvent));
  }
}

//----------------------------------

void SceneViewer::wheelEvent(QWheelEvent* event) {
  int delta = 0;
  switch (event->source()) {
  case Qt::MouseEventNotSynthesized: {
    delta = event->angleDelta().y();
  }
  case Qt::MouseEventSynthesizedBySystem: {
    QPoint numPixels  = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8;
    if (!numPixels.isNull()) {
      delta = event->pixelDelta().y();
    } else if (!numDegrees.isNull()) {
      QPoint numSteps = numDegrees / 15;
      delta           = numSteps.y();
    }
    break;
  }

  default:  // Qt::MouseEventSynthesizedByQt,
            // Qt::MouseEventSynthesizedByApplication
  {
    std::cout << "not supported event: Qt::MouseEventSynthesizedByQt, "
                 "Qt::MouseEventSynthesizedByApplication"
              << std::endl;
    break;
  }

  }  // end switch

  if (delta > 0)
    zoomIn();
  else if (delta < 0)
    zoomOut();
  event->accept();
}

//----------------------------------
// �N���b�N�ʒu�̕`��A�C�e���S�ẴC���f�b�N�X�̃��X�g��Ԃ�
//----------------------------------
QList<int> SceneViewer::pickAll(const QPoint& pos) {
  makeCurrent();

  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);

  // �Ԃ����Z���N�V�����f�[�^�ɗp����z����w�肷��
  GLuint selectBuffer[512];
  // GLdouble mat[16];
  QList<int> keepList;

  QList<int> pickRanges = {5, 10, 20};

  int devPixRatio = screen()->devicePixelRatio();

  for (auto pickRange : pickRanges) {
    glSelectBuffer(512, selectBuffer);
    // �A�v���P�[�V�������Z���N�V�������[�h�Ɏw�肷��
    glRenderMode(GL_SELECT);

    QMatrix4x4 viewProjMat = m_viewProjMatrix;
    QMatrix4x4 pickMat;
    // �s�b�N�̈�̎w��B���͈̔͂ɕ`��𐧌�����
    my_gluPickMatrix(
        pickMat, pos.x() * devPixRatio, (height() - pos.y()) * devPixRatio,
        pickRange * devPixRatio, pickRange * devPixRatio, viewport);

    m_viewProjMatrix = pickMat * m_viewProjMatrix;

    m_modelMatrix.push(m_modelMatrix.top());
    // �l�[���X�^�b�N���N���A���ċ�ɂ���
    glInitNames();
    // �`��
    paintGL();
    m_modelMatrix.pop();

    m_viewProjMatrix = viewProjMat;

    // �Z���N�V�������[�h�𔲂���B�����ɁA�Z���N�V�����̃q�b�g�����Ԃ�B
    int hitCount = glRenderMode(GL_RENDER);
    GLuint* p    = selectBuffer;
    QList<int> tmpList;
    bool pointPicked = false;
    for (int i = 0; i < hitCount; ++i) {
      GLuint nameCount = *p++;
      GLuint zmin      = *p++;
      GLuint zmax      = *p++;
      if (nameCount > 0) {
        GLuint name = *p;
        if (name % 10000 > 0) pointPicked = true;
        tmpList.append(name);
      }
      p += nameCount;
    }
    if (!tmpList.isEmpty()) {
      if (pointPicked)  // �����|�C���g���E�����瑦���ɕԂ�
        return tmpList;
      else if (keepList.isEmpty())  // �V�F�C�v�̂ݏE�����ꍇ�̓L�[�v���Ă���
        keepList = tmpList;
    }
  }

  // �V�F�C�v�̂ݏE�����ꍇ���A�����E���Ȃ������ꍇ�͂�����
  return keepList;
}

//----------------------------------
// �N���b�N�ʒu�̕`��A�C�e���̃C���f�b�N�X��Ԃ�
//----------------------------------
int SceneViewer::pick(const QPoint& pos) {
  QList<int> list = pickAll(pos);
  if (list.isEmpty())
    return 0;
  else {
    std::sort(list.begin(), list.end(), isPoint);  // �_��D�悷��
    return list.first();
  }
}

//----------------------------------
// �V�F�C�v���W��ŁA�\����P�s�N�Z���̕����ǂꂭ�炢�ɂȂ邩��Ԃ�
//----------------------------------
QPointF SceneViewer::getOnePixelLength() {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ���[�N�G���A�T�C�Y����Ȃ�return
  if (workAreaSize.isEmpty()) return QPointF();

  return QPointF(1.0 / m_affine.m11() / (double)workAreaSize.width(),
                 1.0 / m_affine.m22() / (double)workAreaSize.height());
}

//----------------------------------
// �v���r���[�����������Ƃ��A���ݕ\�����̃t���[������
// �v���r���[�^�n�[�t���[�h�������ꍇ�͕\�����X�V����
//----------------------------------
void SceneViewer::onPreviewRenderFinished(int frame) {
  if (!m_project) return;
  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;
  if (frame != settings->getFrame()) return;
  if (settings->getImageMode() == ImageMode_Edit) return;
  update();
}

//----------------------------------
// �V�[�P���X�̓��e���ς������A���ݕ\�����Ă���t���[���̓��e���ς�������̂݃e�N�X�`�����X�V
// �S�t���[���X�V�̏ꍇ�� start, end ��-1������
// ����t���[���ȍ~�S�čX�V�̏ꍇ�́Aend�̂�-1������
//----------------------------------
void SceneViewer::onLayerChanged() {
  // �����̃v���W�F�N�g���J�����g�v���W�F�N�g����Ȃ����return
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project) return;
  update();
}

void SceneViewer::doShapeRender() {
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  auto drawOneShape = [&](const OneShape& shape, int frame) {
    QVector3D* vertexArray =
        shape.shapePairP->getVertexArray(frame, shape.fromTo, m_project);

    auto ptr       = m_line_vbo->map(QOpenGLBuffer::WriteOnly);
    int vertAmount = shape.shapePairP->getVertexAmount(m_project);
    memcpy(ptr, vertexArray, sizeof(QVector3D) * vertAmount);
    m_line_vbo->unmap();
    if (shape.shapePairP->isClosed())
      glDrawArrays(GL_LINE_LOOP, 0, vertAmount);
    else
      glDrawArrays(GL_LINE_STRIP, 0, vertAmount);

    delete[] vertexArray;
  };

  // �v�Z�͈͂����߂�
  // ���t���[���v�Z���邩
  OutputSettings* settings = project->getRenderQueue()->currentOutputSettings();
  OutputSettings::SaveRange saveRange = settings->getSaveRange();
  QString fileName                    = settings->getShapeImageFileName();
  int sizeId                          = settings->getShapeImageSizeId();
  QSize workAreaSize                  = project->getWorkAreaSize();

  if (settings->getDirectory().isEmpty()) {
    QMessageBox::warning(0, QObject::tr("Output Settings Error"),
                         QObject::tr("Output directory is not set."));
    return;
  }

  QDir dir(settings->getDirectory());
  if (!dir.exists()) {
    QMessageBox::StandardButton ret = QMessageBox::question(
        0, QObject::tr("Do you want to create folder?"),
        QString("The folder %1 does not exist.\nDo you want to create it?")
            .arg(settings->getDirectory()),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (ret == QMessageBox::Yes) {
      std::cout << "yes" << std::endl;
      // �t�H���_���
      bool ok = dir.mkpath(dir.path());
      if (!ok) {
        QMessageBox::critical(
            0, QObject::tr("Failed to create folder."),
            QString("Failed to create folder %1.").arg(dir.path()));
        return;
      }
    } else
      return;
  }

  if (saveRange.endFrame == -1)
    saveRange.endFrame = project->getProjectFrameLength() - 1;

  // �v���O���X�o�[���
  RenderProgressPopup progressPopup(project);
  int frameAmount =
      (int)((saveRange.endFrame - saveRange.startFrame) / saveRange.stepFrame) +
      1;

  // �e�t���[���ɂ���
  QList<int> frames;
  for (int i = 0; i < frameAmount; i++) {
    // �t���[�������߂�
    int frame = saveRange.startFrame + i * saveRange.stepFrame;
    frames.append(frame);
  }

  progressPopup.open();

  for (auto frame : frames) {
    if (progressPopup.isCanceled()) break;

    // �o�̓T�C�Y���擾
    QSize outputSize;
    if (sizeId < 0)
      outputSize = workAreaSize;
    else {
      IwLayer* layer = project->getLayer(sizeId);
      QString path   = layer->getImageFilePath(frame);
      if (path.isEmpty()) {  // ��̃��C���̓X�L�b�v����
        progressPopup.onFrameFinished();
        continue;
      }
      ViewSettings* vs = project->getViewSettings();
      if (vs->hasTexture(path))
        outputSize = vs->getTextureId(path).size;
      else {
        TImageP img;
        // �L���b�V������Ă�����A�����Ԃ�
        if (IwImageCache::instance()->isCached(path))
          img = IwImageCache::instance()->get(path);
        else
          img = IoCmd::loadImage(path);
        if (img.getPointer())
          outputSize = QSize(img->raster()->getLx(), img->raster()->getLy());
        else {  // �摜���ǂݍ��߂Ȃ���΃X�L�b�v
          progressPopup.onFrameFinished();
          continue;
        }
      }
    }

    makeCurrent();

    QOpenGLFramebufferObject fbo(outputSize);
    fbo.bind();

    glViewport(0, 0, outputSize.width(), outputSize.height());
    QMatrix4x4 projMatrix = QMatrix4x4();
    projMatrix.ortho(0, outputSize.width(), 0, outputSize.height(), -4000,
                     4000);
    QMatrix4x4 modelMatrix = QMatrix4x4();
    modelMatrix.translate((double)outputSize.width() / 2.0,
                          (double)outputSize.height() / 2.0, 0.0);
    modelMatrix.translate(-(double)workAreaSize.width() / 2.0,
                          -(double)workAreaSize.height() / 2.0, 0.0);
    modelMatrix.scale((double)workAreaSize.width(),
                      (double)workAreaSize.height(), 1.0);

    m_program_line->bind();

    m_line_vao->bind();
    m_line_vbo->bind();

    m_program_line->setUniformValue(u_line_matrix, projMatrix * modelMatrix);
    m_program_line->setUniformValue(u_line_viewportSize, QSizeF(outputSize));
    setLineStipple(1, 0xFFFF);

    //---- ���b�N���𓾂�
    bool fromToVisible[2];
    for (int fromTo = 0; fromTo < 2; fromTo++)
      fromToVisible[fromTo] =
          project->getViewSettings()->isFromToVisible(fromTo);
    //----

    // �e���C���ɂ���
    for (int lay = project->getLayerCount() - 1; lay >= 0; lay--) {
      // ���C�����擾
      IwLayer* layer = project->getLayer(lay);
      if (!layer) continue;

      if (!layer->isVisibleInViewer()) continue;

      for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
        ShapePair* shapePair = layer->getShapePair(sp);
        if (!shapePair) continue;

        // ��\���Ȃ�conitnue
        if (!shapePair->isVisible()) continue;

        for (int fromTo = 0; fromTo < 2; fromTo++) {
          // 140128 ���b�N����Ă������\���ɂ���
          if (!fromToVisible[fromTo]) continue;

          OneShape oneShape(shapePair, fromTo);

          glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

          m_program_line->setUniformValue(
              u_line_color, (fromTo == 0) ? QColor(Qt::red) : QColor(Qt::blue));

          drawOneShape(oneShape, frame);

          glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
      }
    }

    // glPopMatrix();

    fbo.release();

    QImage img = fbo.toImage();

    m_line_vao->release();
    m_line_vbo->release();
    m_program_line->release();

    QString path = project->getOutputPath(
        frame, QString("[dir]/%1.[num].[ext]").arg(fileName));
    path.chop(3);
    path += "png";

    img.save(path);

    progressPopup.onFrameFinished();
    qApp->processEvents();
  }

  progressPopup.close();
}