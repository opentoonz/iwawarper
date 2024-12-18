//----------------------------------
// SceneViewer
// 1つのプロジェクトについて1つのエリア
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
  // LUTをつくる
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

// ハンドルが表示されていたらハンドル優先
// 次に ポイントが表示されていたらポイント
// 最後にシェイプ
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
  setAutoFillBackground(false);  // これをOFFにすることでQPainterが使用できる
  // マウスの動きを感知する
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);

  // シグナル/スロット接続
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(viewFrameChanged()),
          this, SLOT(update()));
  // ViewSettingsが変わったら、自分のプロジェクトがカレントなら、update
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(viewSettingsChanged()),
          this, SLOT(onViewSettingsChanged()));
  // プロジェクトの内容が変わったら、自分のプロジェクトがカレントなら、update
  connect(IwApp::instance()->getCurrentProject(), SIGNAL(projectChanged()),
          this, SLOT(onViewSettingsChanged()));
  // レイヤの内容が変わったら、現在表示しているフレームの内容が変わった時のみテクスチャを更新
  connect(IwApp::instance()->getCurrentLayer(), SIGNAL(layerChanged(IwLayer*)),
          this, SLOT(onLayerChanged()));
  // カレントレイヤが切り替わると、表示を更新
  connect(IwApp::instance()->getCurrentLayer(), SIGNAL(layerSwitched()), this,
          SLOT(update()));
  //  プレビューが完成したとき、現在表示中のフレームかつ
  //  プレビュー／ハーフモードだった場合は表示を更新する
  connect(IwApp::instance()->getCurrentProject(),
          SIGNAL(previewRenderFinished(int)), this,
          SLOT(onPreviewRenderFinished(int)));
  // ツールが切り替わったらredraw
  connect(IwApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()), this,
          SLOT(update()));
  // 選択が切り替わったらredraw
  connect(IwApp::instance()->getCurrentSelection(),
          SIGNAL(selectionChanged(IwSelection*)), this, SLOT(update()));

  //-
  // ズームのコネクト。setCommandHandlerだと対応するコマンドが置き換わってしまうので、複数Viewerに対応できない
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
// 登録テクスチャの消去
//----------------------------------
void SceneViewer::deleteTextures() {
  // テクスチャ解放
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
  // マット関係
  u_tex_matte_sw       = m_program_texture->uniformLocation("matte_sw");
  u_tex_matteImgSize   = m_program_texture->uniformLocation("matteImgSize");
  u_tex_workAreaSize   = m_program_texture->uniformLocation("workAreaSize");
  u_tex_matteTexture   = m_program_texture->uniformLocation("matteTexture");
  u_tex_matteColors    = m_program_texture->uniformLocation("matteColors");
  u_tex_matteTolerance = m_program_texture->uniformLocation("matteTolerance");
  u_tex_matteDilate    = m_program_texture->uniformLocation("matteDilate");

  // 頂点バッファオブジェクト
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

  // 頂点インデックスオブジェクト
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
  // 配列バッファオブジェクトを作り、バインド
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

  // 頂点バッファオブジェクト
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
  // 配列バッファオブジェクトを作り、バインド
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
  // 背景のクリア
  // 色設定から色を拾う
  double bgCol[3];
  ColorSettings::instance()->getColor(bgCol, Color_Background);
  glClearColor(bgCol[0], bgCol[1], bgCol[2], 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;

  m_modelMatrix.push(m_modelMatrix.top());
  // カメラ移動
  m_modelMatrix.top().scale(m_affine.m11(), m_affine.m22(), 1.0);
  m_modelMatrix.top().translate(m_affine.dx(), m_affine.dy(), 0.0);

  PRINT_LOG("paintGL")
  //// ロードされた画像を表示する
  if (settings->getImageMode() == ImageMode_Edit ||
      settings->getImageMode() == ImageMode_Half)
    drawImage();
  // プレビュー表示モードのときはプレビュー結果を表示
  if (settings->getImageMode() == ImageMode_Preview ||
      settings->getImageMode() == ImageMode_Half)
    drawGLPreview();

  // ワークエリアのワクを描く
  drawWorkArea();

  // シェイプを描く。中でツールの描画もやっとる
  drawShapes();

  m_modelMatrix.pop();

  // 現在のレイヤ名を書く

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
// ロードされた画像を表示する
//----------------------------------
void SceneViewer::drawImage() {
  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;

  int frame = settings->getFrame();

  // 素材の描画
  m_modelMatrix.push(m_modelMatrix.top());

  glEnable(GL_TEXTURE_2D);

  m_program_texture->bind();
  m_program_texture->setUniformValue(u_tex_matrix,
                                     m_viewProjMatrix * m_modelMatrix.top());
  m_program_texture->setUniformValue(u_tex_texture, 0);

  m_vao->bind();
  m_vbo->bind();

  double modeVal = (settings->getImageMode() == ImageMode_Half) ? 0.5 : 1.0;

  // レイヤを下から順に合成していく
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;
    // レイヤが非表示だったらcontinue;
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

      // 半透明
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
// プレビュー結果を描く GL版
//----------------------------------
void SceneViewer::drawGLPreview() {
  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;

  int frame = settings->getFrame();

  // 素材の描画
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
  // レイヤを下から順に合成していく
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;
    // レイヤが非表示だったらcontinue;
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

    // シェイプが無い場合、「参照用」シェイプと判断して描画する
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
      // シェイプを下から走査していく
      for (int s = layer->getShapePairCount() - 1; s >= 0; s--) {
        ShapePair* shape = layer->getShapePair(s);
        if (!shape) continue;
        // 子シェイプのとき次のシェイプへ
        if (!shape->isParent()) continue;

        // ターゲットでない場合、次のシェイプへ
        if (!shape->isRenderTarget(targetShapeTag)) continue;

        // 親シェイプのとき、キャッシュをチェックして描画
        if (!IwTriangleCache::instance()->isCached(frame, shape)) continue;

        m_program_texture->setUniformValue(
            u_tex_matrix, m_viewProjMatrix * m_modelMatrix.top());

        //---- マット情報
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

        // 来週はここから！！頂点とインデックス情報を格納するべし
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

        // メッシュを表示
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
// ワークエリアのワクを描く
//----------------------------------
void SceneViewer::drawWorkArea() {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ワークエリアサイズが空ならreturn
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
// シェイプを描く
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
  // ワークエリアサイズが空ならreturn
  if (workAreaSize.isEmpty()) return;

  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  // ワークエリアの左下を原点、ワークエリアの縦横サイズを１に正規化した座標系に変換する
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

  // カレントレイヤの場合は色を変える
  IwLayer* currentLayer = IwApp::instance()->getCurrentLayer()->getLayer();

  // カレントツールを取得
  IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
  //---- ロック情報を得る
  bool fromToVisible[2];
  for (int fromTo = 0; fromTo < 2; fromTo++)
    fromToVisible[fromTo] =
        m_project->getViewSettings()->isFromToVisible(fromTo);
  //----

  // オニオンスキンを得る
  QSet<int> onionFrames = m_project->getOnionFrames();
  int onionRange        = m_project->getViewSettings()->getOnionRange();
  int projectLength     = m_project->getProjectFrameLength();

  // 各レイヤについて
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    PRINT_LOG("  Layer" + std::to_string(lay))

    // レイヤを取得
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;

    bool isCurrent = (layer == currentLayer);
    // 現在のレイヤが非表示ならcontinue
    // ただしカレントレイヤの場合は表示する
    if (!isCurrent && !layer->isVisibleInViewer()) continue;

    // 選択シェイプを再上面に描いてみる
    QList<OneShape> selectedShapes;

    for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
      PRINT_LOG("   Shape" + std::to_string(sp))

      ShapePair* shapePair = layer->getShapePair(sp);
      if (!shapePair) continue;

      // 非表示ならconitnue
      if (!shapePair->isVisible()) continue;

      for (int fromTo = 0; fromTo < 2; fromTo++) {
        // 140128 ロックされていたら非表示にする
        if (!fromToVisible[fromTo]) continue;

        OneShape oneShape(shapePair, fromTo);
        bool isSelected =
            IwShapePairSelection::instance()->isSelected(oneShape);

        // 選択シェイプは後回しにする
        if (isCurrent && isSelected) {
          selectedShapes.append(oneShape);
          continue;
        }

        // 名前のプッシュ
        if (!shapePair->isLocked(fromTo) && !layer->isLocked())
          glPushName(layer->getNameFromShapePair(oneShape));

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        QColor lineColor = QColor::fromRgbF(0.7, 0.7, 0.7, 0.5);
        if (isCurrent) {
          if (isSelected)
            lineColor = (fromTo == 0) ? QColor::fromRgbF(1.0, 0.7, 0.0)
                                      : QColor::fromRgbF(0.0, 0.7, 1.0);
          // ロックされていたら薄くする
          else if (shapePair->isLocked(fromTo) || layer->isLocked())
            lineColor = (fromTo == 0) ? QColor::fromRgbF(0.7, 0.2, 0.2, 0.7)
                                      : QColor::fromRgbF(0.2, 0.2, 0.7, 0.7);
          else
            lineColor = (fromTo == 0) ? QColor::fromRgbF(1.0, 0.0, 0.0)
                                      : QColor::fromRgbF(0.0, 0.0, 1.0);
        }
        // ReshapeToolおよびCorrToolのSnap対象のシェイプ
        if (tool && tool->setSpecialShapeColor(oneShape))
          lineColor = QColor::fromRgbF(1.0, 0.0, 1.0);

        m_program_line->setUniformValue(u_line_color, lineColor);

        drawOneShape(oneShape, frame);

        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        // 名前のポップ
        if (!shapePair->isLocked(fromTo) && !layer->isLocked()) glPopName();
      }
    }

    // 選択シェイプを再上面に描いてみる
    for (auto oneShape : selectedShapes) {
      // オニオンシェイプの描画
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
      // 今のところReshapeToolのSnap対象のシェイプのみ
      if (tool && tool->setSpecialShapeColor(oneShape))
        lineColor = QColor::fromRgbF(1.0, 0.0, 1.0);
      m_program_line->setUniformValue(u_line_color, lineColor);

      drawOneShape(oneShape, frame);

      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

      // 名前のポップ
      if (!oneShape.shapePairP->isLocked(oneShape.fromTo) && !layer->isLocked())
        glPopName();
    }
  }

  // シェイプの補間ハンドルのドラッグ中は、前後のキーフレーム間のシェイプをオニオンスキンで表示する
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
  // ガイドを描く
  if (m_hRuler->getGuideCount() > 0 || m_vRuler->getGuideCount() > 0) {
    setLineStipple(1, 0xAAAA);
    // 描画範囲
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
  // ツールが取得できなかったらreturn
  // プロジェクトがカレントじゃなくてもreturn
  if (tool && m_project->isCurrent()) {
    // ツールにこのビューアのポインタを渡す
    tool->setViewer(this);
    // ツールの描画
    tool->draw();
  }

  m_line_vao->release();
  m_line_vbo->release();
  m_program_line->release();

  m_modelMatrix.pop();
  PRINT_LOG("drawShapes end")
}

//----------------------------------
// テクスチャインデックスを得る。無ければロードして登録
//----------------------------------
ViewSettings::LAYERIMAGE SceneViewer::getLayerImage(QString& path,
                                                    int brightness,
                                                    int contrast) {
  // 自分のプロジェクトがカレントプロジェクトじゃなければreturn
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project)
    return ViewSettings::LAYERIMAGE();
  ViewSettings* settings = m_project->getViewSettings();
  int shrink             = m_project->getRenderSettings()->getImageShrink();

  // 既に保存していればそれを返す
  if (settings->hasTexture(path)) {
    if (settings->getTextureId(path).brightness == brightness &&
        settings->getTextureId(path).contrast == contrast &&
        settings->getTextureId(path).shrink == shrink)
      return settings->getTextureId(path);
    // brightness contrast が異なる場合は再計算
    else
      settings->releaseTexture(path);
  }

  TImageP img;
  // キャッシュされていたら、それを返す
  if (IwImageCache::instance()->isCached(path))
    img = IwImageCache::instance()->get(path);
  else
    img = IoCmd::loadImage(path);

  // 画像がロードできたら、キャッシュに入れる
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
    // pixelSize = 4 なら 8bpc、8 なら 16bpc
    // 16bitなら8bitに変換
    if (tmpRas->getPixelSize() == 8) {
      ras = TRaster32P(tmpRas->getWrap(), tmpRas->getLy());
      TRop::convert(ras, tmpRas);
    } else
      ras = tmpRas;

    // Premultiplyされているか判定
    if (!isPremultiplied(ras)) TRop::premultiply(ras);

  } else if (ti) {
    TRasterCM32P rascm;
    rascm = ti->getRaster();
    ras   = TRaster32P(rascm->getWrap(), rascm->getLy());

    TRop::convert(ras, rascm, ti->getPalette());
  }
  // 形式が合わなければcontinue
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

  // テクスチャに各種プロパティを入れる
  // glBindTexture(GL_TEXTURE_2D, texName);
  // テクスチャ境界色
  // static const GLfloat border[] = { 0.0f, 0.0f, 0.0f, 0.0f };
  // glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
  // 最外周を境界色にする
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  // 拡大しても補間しない
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // 縮小するときはリニア補間
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
// もしプレビュー結果の画像がキャッシュされていたら、
// それを使ってテクスチャを作る。テクスチャインデックスを返す
//----------------------------------
unsigned int SceneViewer::setPreviewTexture() {
  // 自分のプロジェクトがカレントプロジェクトじゃなければreturn
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project)
    return 0;

  ViewSettings* settings = m_project->getViewSettings();

  // 表示するフレームを取得
  int viewFrame = m_project->getViewFrame();

  // キャッシュのエイリアスを得る
  QString cacheAlias =
      QString("PRJ%1_PREVIEW_FRAME%2").arg(m_project->getId()).arg(viewFrame);

  // キャッシュされていなかったらreturn
  if (!IwImageCache::instance()->isCached(cacheAlias)) return 0;

  // 画像を取得
  TImageP img = IwImageCache::instance()->get(cacheAlias);

  if (!img.getPointer()) return 0;

  TRasterP ras;
  TRasterImageP ri = (TRasterImageP)img;
  if (ri) {
    TRasterP tmpRas = ri->getRaster();
    // pixelSize = 4 なら 8bpc、8 なら 16bpc
    // 16bitなら8bitに変換
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

  // テクスチャに各種プロパティを入れる
  glBindTexture(GL_TEXTURE_2D, texName);
  // 端を繰り返さない
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  // 拡大しても補間しない
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // 縮小するときはリニア補間
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  settings->setPreviewTextureId(viewFrame, (unsigned int)texName);

  return (unsigned int)texName;
}

//----------------------------------
// カレントレイヤ切り替えコマンドを登録する
//----------------------------------
void SceneViewer::addContextMenus(QMenu& menu) {
  // レイヤがある場合
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (prj && prj->getLayerCount() > 0) {
    if (!menu.actions().isEmpty()) menu.addSeparator();
    QMenu* changeLayerMenu = new QMenu(tr("Change Current Layer"));
    for (int lay = 0; lay < prj->getLayerCount(); lay++) {
      QAction* tmpAct = new QAction(prj->getLayer(lay)->getName(), 0);
      tmpAct->setData(lay);  // レイヤインデックスを格納
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
// ViewSettingsが変わったら
// 自分のプロジェクトがカレントなら、update
//----------------------------------
void SceneViewer::onViewSettingsChanged() {
  // 自分のプロジェクトがカレントプロジェクトじゃなければreturn
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project) return;

  repaint();
  update();
}

//----------------------------------
// サイズ調整
//----------------------------------
void SceneViewer::zoomIn() {
  // 自分のプロジェクトがカレントプロジェクトじゃなければreturn
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

  // タイトルバー更新
  IwApp::instance()->getMainWindow()->updateTitle();
}
//----------------------------------
void SceneViewer::zoomOut() {
  // 自分のプロジェクトがカレントプロジェクトじゃなければreturn
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project) return;

  int zoomStep = m_project->getViewSettings()->getZoomStep();
  if (zoomStep <= -3) return;

  QPointF delta(m_mousePos.x() - width() / 2, -m_mousePos.y() + height() / 2);
  delta /= m_affine.determinant();

  m_project->getViewSettings()->setZoomStep(zoomStep - 1);

  m_affine = m_affine.translate(delta.x(), delta.y()).scale(0.5, 0.5);
  update();
  updateRulers();

  // タイトルバー更新
  IwApp::instance()->getMainWindow()->updateTitle();
}
//----------------------------------
void SceneViewer::zoomReset() {
  // 自分のプロジェクトがカレントプロジェクトじゃなければreturn
  if (IwApp::instance()->getCurrentProject()->getProject() != m_project) return;

  m_project->getViewSettings()->setZoomStep(0);

  m_affine.reset();
  update();
  updateRulers();

  // タイトルバー更新
  IwApp::instance()->getMainWindow()->updateTitle();
}

//----------------------------------
// カレントレイヤを変える
//----------------------------------
void SceneViewer::onChangeCurrentLayer() {
  int layerIndex = qobject_cast<QAction*>(sender())->data().toInt();
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;
  if (layerIndex < 0 || layerIndex >= prj->getLayerCount()) return;
  IwApp::instance()->getCurrentLayer()->setLayer(prj->getLayer(layerIndex));
}

//----------------------------------
// マウス位置からIwaWarperの座標系に変換する
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
// 中ドラッグでパンする
//----------------------------------
void SceneViewer::mouseMoveEvent(QMouseEvent* e) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ワークエリアサイズが空ならreturn
  if (workAreaSize.isEmpty()) return;

  // 中ボタンドラッグの場合、パンする
  if ((e->buttons() & Qt::MiddleButton) != 0 &&
      m_mouseButton == Qt::MiddleButton && m_isPanning) {
    // スクロールバーをスライド
    QPoint dp = e->pos() - m_mousePos;
    // マウスの動きと座標系は上下逆
    m_affine.translate((double)dp.x() / m_affine.m11() / m_affine.m11(),
                       -(double)dp.y() / m_affine.m22() / m_affine.m22());
    m_mousePos = e->pos();
    update();
    updateRulers();
    return;
  }
  // 左ボタンドラッグの場合、ツールの機能を実行
  // クリックはウィンドウ内で行われていないといけない
  else if ((e->buttons() & Qt::LeftButton) != 0 &&
           m_mouseButton == Qt::LeftButton) {
    // カレントツールを取得
    IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
    // ツールが取得できなかったらreturn
    // プロジェクトがカレントじゃなくてもreturn
    if (!tool || !m_project->isCurrent()) return;

    // マウス位置からIwaWarperの座標系に変換する
    QPointF erPos = convertMousePosToIWCoord(e->localPos());
    // ツールの機能を実行
    bool redrawViewer = tool->leftButtonDrag(erPos, e);
    if (redrawViewer) update();
    // ZoomをViewer中心にするときに使う
    m_mousePos = e->pos();
    return;
  }
  // マウスが押されていない場合、マウスカーソルを更新
  else {
    // カレントツールを取得
    IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
    // ツールが取得できなかったらreturn
    // プロジェクトがカレントじゃなくてもreturn
    if (!tool || !m_project->isCurrent()) return;

    // マウス位置からIwaWarperの座標系に変換する
    QPointF erPos = convertMousePosToIWCoord(e->localPos());
    // QSize workAreaSize = m_project->getWorkAreaSize();
    // std::cout << "pixelPos = " << erPos.x() * (double)workAreaSize.width()
    //   << ", " << erPos.y() * (double)workAreaSize.height() << std::endl;
    // ツールの機能を実行
    bool redrawViewer = tool->mouseMove(erPos, e);
    if (redrawViewer) update();

    setToolCursor(this, tool->getCursorId(e));

    // ZoomをViewer中心にするときに使う
    m_mousePos = e->pos();
  }
}

//----------------------------------
// 中クリックでパンモードにする
//----------------------------------
void SceneViewer::mousePressEvent(QMouseEvent* e) {
  // 中ボタンの場合、パンモードに移行
  if (e->button() == Qt::MiddleButton) {
    // マウス位置を格納
    m_mousePos = e->pos();
    // パンモードをON
    m_isPanning = true;

    m_mouseButton = Qt::MiddleButton;
    return;
  }
  // use tool functions when left or right click
  else if (e->button() == Qt::LeftButton || e->button() == Qt::RightButton) {
    // カレントツールを取得
    IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
    // ツールが取得できなかったらreturn
    // プロジェクトがカレントじゃなくてもreturn
    if (!tool || !m_project->isCurrent()) return;

    // 右クリックの場合、カレントレイヤ切り替えメニューを出す
    if (!IwApp::instance()->getCurrentLayer()->getLayer()) {
      if (e->button() == Qt::RightButton) {
        QMenu menu(0);
        // カレントレイヤ切り替えコマンドを登録する
        addContextMenus(menu);
        // メニューにアクションが登録されていたら
        if (!menu.actions().isEmpty()) menu.exec(e->globalPos());
      }
      return;
    }

    // マウス位置からIwaWarperの座標系に変換する
    QPointF erPos = convertMousePosToIWCoord(e->localPos());
    // ツールの機能を実行
    bool redrawViewer;
    if (e->button() == Qt::LeftButton)
      redrawViewer = tool->leftButtonDown(erPos, e);
    else  // 右ボタンの場合
    {
      bool canOpenMenu = true;
      QMenu menu(0);
      redrawViewer = tool->rightButtonDown(erPos, e, canOpenMenu, menu);
      // カレントレイヤを切り替えるメニューを追加する
      if (canOpenMenu) {
        // カレントレイヤ切り替えコマンドを登録する
        addContextMenus(menu);
        // メニューにアクションが登録されていたら
        if (!menu.actions().isEmpty()) menu.exec(e->globalPos());
      }
    }

    if (redrawViewer) update();

    m_mouseButton = e->button();
  }
}

//----------------------------------
// パンモードをクリアする
//----------------------------------
void SceneViewer::mouseReleaseEvent(QMouseEvent* e) {
  // パンモードをOFF
  m_isPanning = false;

  // 左ボタンをリリースした場合、マウスをリリースする
  if (m_mouseButton == Qt::LeftButton) {
    // カレントツールを取得
    IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
    // ツールが取得できなかったらreturn
    // プロジェクトがカレントじゃなくてもreturn
    if (!tool || !m_project->isCurrent()) {
      m_mouseButton = Qt::NoButton;
      return;
    }

    // マウス位置からIwaWarperの座標系に変換する
    QPointF erPos = convertMousePosToIWCoord(e->localPos());
    // ツールの機能を実行
    bool redrawViewer = tool->leftButtonUp(erPos, e);
    if (redrawViewer) update();
  }

  // マウスボタンを解除する
  m_mouseButton = Qt::NoButton;
}

//----------------------------------
// パンモードをクリアする
//----------------------------------
void SceneViewer::leaveEvent(QEvent* /*e*/) {
  // パンモードをOFF
  m_isPanning = false;
}

//----------------------------------
// 修飾キーをクリアする
//----------------------------------
void SceneViewer::enterEvent(QEvent*) { m_modifiers = Qt::NoModifier; }

//----------------------------------
// 修飾キーを押すとカーソルを変える
//----------------------------------
void SceneViewer::keyPressEvent(QKeyEvent* e) {
  // プロジェクトがカレントじゃなければreturn
  if (!m_project->isCurrent()) return;

  // カレントツールを取得
  IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
  // ツールが取得できなかったらreturn
  if (!tool) return;

  int key = e->key();

  // ツールがショートカットキーを使っているかどうか？
  // 今のところ、TransformToolとReshapeToolでCtrl＋矢印キーを使っている
  bool ret = tool->keyDown(key, (e->modifiers() & Qt::ControlModifier),
                           (e->modifiers() & Qt::ShiftModifier),
                           (e->modifiers() & Qt::AltModifier));
  if (ret) {
    e->accept();
    update();
    return;
  }

  //----- 以下、ツールがキーを拾わなかった場合

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
  // プロジェクトがカレントじゃなければreturn
  if (!m_project->isCurrent()) return;

  int key = e->key();

  if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
      key == Qt::Key_AltGr) {
    // カレントツールを取得
    IwTool* tool = IwApp::instance()->getCurrentTool()->getTool();
    // ツールが取得できなかったらreturn
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
// クリック位置の描画アイテム全てのインデックスのリストを返す
//----------------------------------
QList<int> SceneViewer::pickAll(const QPoint& pos) {
  makeCurrent();

  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);

  // 返されるセレクションデータに用いる配列を指定する
  GLuint selectBuffer[512];
  // GLdouble mat[16];
  QList<int> keepList;

  QList<int> pickRanges = {5, 10, 20};

  int devPixRatio = screen()->devicePixelRatio();

  for (auto pickRange : pickRanges) {
    glSelectBuffer(512, selectBuffer);
    // アプリケーションをセレクションモードに指定する
    glRenderMode(GL_SELECT);

    QMatrix4x4 viewProjMat = m_viewProjMatrix;
    QMatrix4x4 pickMat;
    // ピック領域の指定。この範囲に描画を制限する
    my_gluPickMatrix(
        pickMat, pos.x() * devPixRatio, (height() - pos.y()) * devPixRatio,
        pickRange * devPixRatio, pickRange * devPixRatio, viewport);

    m_viewProjMatrix = pickMat * m_viewProjMatrix;

    m_modelMatrix.push(m_modelMatrix.top());
    // ネームスタックをクリアして空にする
    glInitNames();
    // 描画
    paintGL();
    m_modelMatrix.pop();

    m_viewProjMatrix = viewProjMat;

    // セレクションモードを抜ける。同時に、セレクションのヒット数が返る。
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
      if (pointPicked)  // 何かポイントが拾えたら即座に返す
        return tmpList;
      else if (keepList.isEmpty())  // シェイプのみ拾えた場合はキープしておく
        keepList = tmpList;
    }
  }

  // シェイプのみ拾えた場合か、何も拾えなかった場合はこちら
  return keepList;
}

//----------------------------------
// クリック位置の描画アイテムのインデックスを返す
//----------------------------------
int SceneViewer::pick(const QPoint& pos) {
  QList<int> list = pickAll(pos);
  if (list.isEmpty())
    return 0;
  else {
    std::sort(list.begin(), list.end(), isPoint);  // 点を優先する
    return list.first();
  }
}

//----------------------------------
// シェイプ座標上で、表示上１ピクセルの幅がどれくらいになるかを返す
//----------------------------------
QPointF SceneViewer::getOnePixelLength() {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ワークエリアサイズが空ならreturn
  if (workAreaSize.isEmpty()) return QPointF();

  return QPointF(1.0 / m_affine.m11() / (double)workAreaSize.width(),
                 1.0 / m_affine.m22() / (double)workAreaSize.height());
}

//----------------------------------
// プレビューが完成したとき、現在表示中のフレームかつ
// プレビュー／ハーフモードだった場合は表示を更新する
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
// シーケンスの内容が変わったら、現在表示しているフレームの内容が変わった時のみテクスチャを更新
// 全フレーム更新の場合は start, end に-1が入る
// あるフレーム以降全て更新の場合は、endのみ-1が入る
//----------------------------------
void SceneViewer::onLayerChanged() {
  // 自分のプロジェクトがカレントプロジェクトじゃなければreturn
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

  // 計算範囲を求める
  // 何フレーム計算するか
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
      // フォルダ作る
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

  // プログレスバー作る
  RenderProgressPopup progressPopup(project);
  int frameAmount =
      (int)((saveRange.endFrame - saveRange.startFrame) / saveRange.stepFrame) +
      1;

  // 各フレームについて
  QList<int> frames;
  for (int i = 0; i < frameAmount; i++) {
    // フレームを求める
    int frame = saveRange.startFrame + i * saveRange.stepFrame;
    frames.append(frame);
  }

  progressPopup.open();

  for (auto frame : frames) {
    if (progressPopup.isCanceled()) break;

    // 出力サイズを取得
    QSize outputSize;
    if (sizeId < 0)
      outputSize = workAreaSize;
    else {
      IwLayer* layer = project->getLayer(sizeId);
      QString path   = layer->getImageFilePath(frame);
      if (path.isEmpty()) {  // 空のレイヤはスキップする
        progressPopup.onFrameFinished();
        continue;
      }
      ViewSettings* vs = project->getViewSettings();
      if (vs->hasTexture(path))
        outputSize = vs->getTextureId(path).size;
      else {
        TImageP img;
        // キャッシュされていたら、それを返す
        if (IwImageCache::instance()->isCached(path))
          img = IwImageCache::instance()->get(path);
        else
          img = IoCmd::loadImage(path);
        if (img.getPointer())
          outputSize = QSize(img->raster()->getLx(), img->raster()->getLy());
        else {  // 画像が読み込めなければスキップ
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

    //---- ロック情報を得る
    bool fromToVisible[2];
    for (int fromTo = 0; fromTo < 2; fromTo++)
      fromToVisible[fromTo] =
          project->getViewSettings()->isFromToVisible(fromTo);
    //----

    // 各レイヤについて
    for (int lay = project->getLayerCount() - 1; lay >= 0; lay--) {
      // レイヤを取得
      IwLayer* layer = project->getLayer(lay);
      if (!layer) continue;

      if (!layer->isVisibleInViewer()) continue;

      for (int sp = 0; sp < layer->getShapePairCount(); sp++) {
        ShapePair* shapePair = layer->getShapePair(sp);
        if (!shapePair) continue;

        // 非表示ならconitnue
        if (!shapePair->isVisible()) continue;

        for (int fromTo = 0; fromTo < 2; fromTo++) {
          // 140128 ロックされていたら非表示にする
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