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

#include <GL/GLU.h>

#include <QPainter>
#include <QMenu>
#include <QOpenGLTexture>

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
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);

  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
}

//----------------------------------
void SceneViewer::resizeGL(int w, int h) {
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, w, 0, h, -4000, 4000);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslated(w * 0.5, h * 0.5, 0);
}

//----------------------------------

void SceneViewer::paintGL() {
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

  glPushMatrix();
  // �J�����ړ�
  glScaled(m_affine.m11(), m_affine.m22(), 1.0);
  glTranslated(m_affine.dx(), m_affine.dy(), 0.0);

  PRINT_LOG("paintGL")
  // ���[�h���ꂽ�摜��\������
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

  glPopMatrix();

  // ���݂̃��C����������
  IwLayer* layer = IwApp::instance()->getCurrentLayer()->getLayer();
  if (layer) {
    glColor3d(1.0, 1.0, 0.0);
    renderText(10, 30, layer->getName(), QFont("Helvetica", 20, QFont::Normal));
  }
  PRINT_LOG("paintGL End")
}

//----------------------------------

void SceneViewer::renderText(double x, double y, const QString& str,
                             const QFont& font) {
  GLdouble glColor[4];
  glGetDoublev(GL_CURRENT_COLOR, glColor);
  QColor fontColor;
  fontColor.setRgbF(glColor[0], glColor[1], glColor[2], glColor[3]);

  // Render text
  QPainter painter(this);
  painter.setPen(fontColor);
  painter.setFont(font);
  painter.drawText(x, this->height() - y, str);
  painter.end();
}

//----------------------------------
// ���[�h���ꂽ�摜��\������
//----------------------------------
void SceneViewer::drawImage() {
  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;

  int frame = settings->getFrame();

  // �f�ނ̕`��

  glPushMatrix();

  glEnable(GL_TEXTURE_2D);

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
      GLuint texName = (GLuint)layerImage.texture->textureId();
      glBindTexture(GL_TEXTURE_2D, texName);

      QSize texSize = layerImage.size;
      QPointF texCornerPos((double)texSize.width() / 2.0,
                           (double)texSize.height() / 2.0);

      // ������
      double visibleVal =
          (m_project->getLayer(lay)->isVisibleInViewer() == 1) ? 0.5 : 1.0;
      double chan = modeVal * visibleVal;
      glColor4d(chan, chan, chan, chan);
      glBegin(GL_POLYGON);
      glTexCoord2d(0.0, 0.0);
      glVertex2d(-texCornerPos.x(), -texCornerPos.y());
      glTexCoord2d(0.0, 1.0);
      glVertex2d(-texCornerPos.x(), texCornerPos.y());
      glTexCoord2d(1.0, 1.0);
      glVertex2d(texCornerPos.x(), texCornerPos.y());
      glTexCoord2d(1.0, 0.0);
      glVertex2d(texCornerPos.x(), -texCornerPos.y());

      glEnd();
    }
  }
  glPopMatrix();

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

  glPushMatrix();

  glEnable(GL_TEXTURE_2D);

  double modeVal = (settings->getImageMode() == ImageMode_Half) ? 0.5 : 1.0;

  QSize workAreaSize = m_project->getWorkAreaSize();
  QPointF texCornerPos((double)workAreaSize.width() / 2.0,
                       (double)workAreaSize.height() / 2.0);

  glTranslated(-texCornerPos.x(), -texCornerPos.y(), 0);

  int targetShapeTag = m_project->getOutputSettings()->getShapeTagId();

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

    if (!layerImage.size.isEmpty()) {
      double visibleVal =
          (m_project->getLayer(lay)->isVisibleInViewer() == 1) ? 0.5 : 1.0;
      double chan = modeVal * visibleVal;
      glColor4d(chan, chan, chan, chan);

      GLuint texName = (GLuint)layerImage.texture->textureId();
      glBindTexture(GL_TEXTURE_2D, texName);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      // �V�F�C�v�������ꍇ�A�u�Q�Ɨp�v�V�F�C�v�Ɣ��f���ĕ`�悷��
      if (layer->getShapePairCount(true) == 0) {
        QSize layerSize = layerImage.size;
        QPointF layerCornerPos((double)layerSize.width() / 2.0,
                               (double)layerSize.height() / 2.0);
        double texClipMargin[2] = {0.0, 0.0};
        if (layerSize.width() > workAreaSize.width()) {
          texClipMargin[0] = (1.0 - ((double)workAreaSize.width() /
                                     (double)layerSize.width())) *
                             0.5;
          layerCornerPos.setX((double)workAreaSize.width() / 2.0);
        }
        if (layerSize.height() > workAreaSize.height()) {
          texClipMargin[1] = (1.0 - ((double)workAreaSize.height() /
                                     (double)layerSize.height())) *
                             0.5;
          layerCornerPos.setY((double)workAreaSize.height() / 2.0);
        }
        glPushMatrix();
        glTranslated(texCornerPos.x(), texCornerPos.y(), 0);
        glBegin(GL_QUADS);
        glTexCoord2d(texClipMargin[0], texClipMargin[1]);
        glVertex2d(-layerCornerPos.x(), -layerCornerPos.y());
        glTexCoord2d(texClipMargin[0], 1.0 - texClipMargin[1]);
        glVertex2d(-layerCornerPos.x(), layerCornerPos.y());
        glTexCoord2d(1.0 - texClipMargin[0], 1.0 - texClipMargin[1]);
        glVertex2d(layerCornerPos.x(), layerCornerPos.y());
        glTexCoord2d(1.0 - texClipMargin[0], texClipMargin[1]);
        glVertex2d(layerCornerPos.x(), -layerCornerPos.y());
        glEnd();
        glPopMatrix();
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
          int vertCount =
              IwTriangleCache::instance()->vertexCount(frame, shape);
          Vertex* vertex =
              IwTriangleCache::instance()->vertexData(frame, shape);
          int* ids = IwTriangleCache::instance()->idsData(frame, shape);
          // Vertices vertices = IwTriangleCache::instance()->data(frame,
          // shape);

          glVertexPointer(3, GL_DOUBLE, sizeof(Vertex), vertex);
          glTexCoordPointer(2, GL_DOUBLE, sizeof(Vertex), vertex[0].uv);

          glEnableClientState(GL_VERTEX_ARRAY);
          glEnableClientState(GL_TEXTURE_COORD_ARRAY);

          glDrawElements(GL_TRIANGLES, vertCount, GL_UNSIGNED_INT, ids);

          // ���b�V����\��
          if (settings->isMeshVisible()) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4d(0.5, 1.0, 0.0, 0.2);
            glDisable(GL_TEXTURE_2D);
            for (int t = 0; t < vertCount / 3; t++)
              glDrawElements(GL_LINE_LOOP, 3, GL_UNSIGNED_INT, &ids[t * 3]);
            glEnable(GL_TEXTURE_2D);
            glColor4d(chan, chan, chan, chan);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
          }
        }
      }

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
  }
  glPopMatrix();

  glDisable(GL_TEXTURE_2D);

  IwTriangleCache::instance()->unlock();
}

//----------------------------------
// ���[�N�G���A�̃��N��`��
//----------------------------------
void SceneViewer::drawWorkArea() {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ���[�N�G���A�T�C�Y����Ȃ�return
  if (workAreaSize.isEmpty()) return;

  glPushMatrix();
  glTranslated(-(double)workAreaSize.width() / 2.0,
               -(double)workAreaSize.height() / 2.0, 0.0);
  glScaled((double)workAreaSize.width(), (double)workAreaSize.height(), 1.0);

  ViewSettings* settings = m_project->getViewSettings();
  glEnable(GL_LINE_STIPPLE);
  if (settings->getImageMode() == ImageMode_Preview ||
      settings->getImageMode() == ImageMode_Half) {
    glColor3d(1.0, 0.545, 0.392);
    glLineStipple(2, 0x00FF);
  } else {
    glColor3d(1.0, 0.5, 0.5);
    glLineStipple(2, 0xAAAA);
  }

  glBegin(GL_LINE_LOOP);
  glVertex3d(1.0, 0.0, 0.0);
  glVertex3d(1.0, 1.0, 0.0);
  glVertex3d(0.0, 1.0, 0.0);
  glVertex3d(0.0, 0.0, 0.0);
  glEnd();

  glDisable(GL_LINE_STIPPLE);

  glPopMatrix();
}

//----------------------------------
// �V�F�C�v��`��
//----------------------------------
void SceneViewer::drawShapes() {
  auto drawOneShape = [&](const OneShape& shape, int frame) {
    GLdouble* vertexArray =
        shape.shapePairP->getVertexArray(frame, shape.fromTo, m_project);
    // �V�F�C�v�����Ă���ꍇ
    if (shape.shapePairP->isClosed()) {
      glBegin(GL_LINE_LOOP);
      GLdouble* v_p = vertexArray;
      for (int v = 0; v < shape.shapePairP->getVertexAmount(m_project); v++) {
        glVertex3d(v_p[0], v_p[1], v_p[2]);
        v_p += 3;
      }
      glEnd();
    }
    // �V�F�C�v���J���Ă���ꍇ
    else {
      glBegin(GL_LINE_STRIP);
      GLdouble* v_p = vertexArray;
      for (int v = 0; v < shape.shapePairP->getVertexAmount(m_project); v++) {
        glVertex3d(v_p[0], v_p[1], v_p[2]);
        v_p += 3;
      }
      glEnd();
    }
    // �f�[�^�����
    delete[] vertexArray;
  };

  PRINT_LOG("drawShapes start")
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ���[�N�G���A�T�C�Y����Ȃ�return
  if (workAreaSize.isEmpty()) return;

  // ���݂̃t���[���𓾂�
  int frame = m_project->getViewFrame();

  // ���[�N�G���A�̍��������_�A���[�N�G���A�̏c���T�C�Y���P�ɐ��K���������W�n�ɕϊ�����
  glPushMatrix();
  glTranslated(-(double)workAreaSize.width() / 2.0,
               -(double)workAreaSize.height() / 2.0, 0.0);
  glScaled((double)workAreaSize.width(), (double)workAreaSize.height(), 1.0);

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
        glColor4d(0.7, 0.7, 0.7, 0.5);
        if (isCurrent) {
          if (isSelected)
            (fromTo == 0) ? glColor3d(1.0, 0.7, 0.0) : glColor3d(0.0, 0.7, 1.0);
          // ���b�N����Ă����甖������
          else if (shapePair->isLocked(fromTo) || layer->isLocked())
            (fromTo == 0) ? glColor4d(0.7, 0.2, 0.2, 0.7)
                          : glColor4d(0.2, 0.2, 0.7, 0.7);
          else
            (fromTo == 0) ? glColor3d(1.0, 0.0, 0.0) : glColor3d(0.0, 0.0, 1.0);
        }

        // ���̂Ƃ���ReshapeTool��Snap�Ώۂ̃V�F�C�v�̂�
        if (tool && tool->setSpecialShapeColor(oneShape))
          glColor3d(1.0, 0.0, 1.0);

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
        (oneShape.fromTo == 0) ? glColor4d(0.8, 0.7, 0.2, 0.8)
                               : glColor4d(0.2, 0.7, 0.8, 0.8);
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
          (oneShape.fromTo == 0) ? glColor4d(0.8, 0.7, 0.2, alpha)
                                 : glColor4d(0.2, 0.7, 0.8, alpha);
          drawOneShape(oneShape, relativeOf);
        }
      }

      if (!oneShape.shapePairP->isLocked(oneShape.fromTo) && !layer->isLocked())
        glPushName(layer->getNameFromShapePair(oneShape));

      if (oneShape.fromTo == 0)
        glColor3d(1.0, 0.7, 0.0);
      else
        glColor3d(0.0, 0.7, 1.0);

      // ���̂Ƃ���ReshapeTool��Snap�Ώۂ̃V�F�C�v�̂�
      if (tool && tool->setSpecialShapeColor(oneShape))
        glColor3d(1.0, 0.0, 1.0);

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
      glColor3d(1.0, 0.0, 1.0);
      for (int f = key; f <= nextKey; f++) {
        drawOneShape(shape, f);
      }
    }
  }

  PRINT_LOG("  Draw Guides")
  // �K�C�h��`��
  if (m_hRuler->getGuideCount() > 0 || m_vRuler->getGuideCount() > 0) {
    glLineStipple(1, 0xAAAA);
    glEnable(GL_LINE_STIPPLE);
    glBegin(GL_LINES);
    // �`��͈�
    double minV = m_vRuler->posToValue(QPoint(0, 0));
    double maxV = m_vRuler->posToValue(QPoint(0, height()));
    for (int gId = 0; gId < m_hRuler->getGuideCount(); gId++) {
      if (tool && tool->setSpecialGridColor(gId, false))
        glColor3d(1.0, 0.0, 1.0);
      else
        glColor3d(0.7, 0.7, 0.7);
      double g = m_hRuler->getGuide(gId);
      glVertex2d(g, minV);
      glVertex2d(g, maxV);
    }

    minV = m_hRuler->posToValue(QPoint(0, 0));
    maxV = m_hRuler->posToValue(QPoint(width(), 0));
    for (int gId = 0; gId < m_vRuler->getGuideCount(); gId++) {
      if (tool && tool->setSpecialGridColor(gId, true))
        glColor3d(1.0, 0.0, 1.0);
      else
        glColor3d(0.7, 0.7, 0.7);
      double g = m_vRuler->getGuide(gId);
      glVertex2d(minV, g);
      glVertex2d(maxV, g);
    }
    glEnd();
    glDisable(GL_LINE_STIPPLE);
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

  glPopMatrix();
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
	else if(e->button() == Qt::LeftButton || e->button() == Qt::RightButton)
  {
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
void SceneViewer::leaveEvent(QEvent* e) {
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
  GLdouble mat[16];
  QList<int> keepList;

  QList<int> pickRanges = {5, 10, 20};

  for (auto pickRange : pickRanges) {
    glSelectBuffer(512, selectBuffer);
    // �A�v���P�[�V�������Z���N�V�������[�h�Ɏw�肷��
    glRenderMode(GL_SELECT);

    glMatrixMode(GL_PROJECTION);
    glGetDoublev(GL_PROJECTION_MATRIX, mat);
    glPushMatrix();
    glLoadIdentity();
    // �s�b�N�̈�̎w��B���͈̔͂ɕ`��𐧌�����
    gluPickMatrix(pos.x(), height() - pos.y(), pickRange, pickRange, viewport);
    glMultMatrixd(mat);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    // �l�[���X�^�b�N���N���A���ċ�ɂ���
    glInitNames();

    // �`��
    paintGL();
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    // int ret = 0;
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
