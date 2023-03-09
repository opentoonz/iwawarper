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

#ifdef _WIN32
#include <GL/GLU.h>
#endif
#ifdef MACOSX
#include <GLUT/glut.h>
#endif

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

void my_gluPickMatrix(GLfloat x, GLfloat y, GLfloat width, GLfloat height,
                      GLint viewport[4]) {
  float sx, sy;
  float tx, ty;
  GLfloat mp[4][4] = {{0.f}};

  sx = viewport[2] / width;
  sy = viewport[3] / height;
  tx = (viewport[2] + 2.0f * (viewport[0] - x)) / width;
  ty = (viewport[3] + 2.0f * (viewport[1] - y)) / height;

  mp[0][0] = sx;
  mp[0][3] = tx;
  mp[1][1] = sy;
  mp[1][3] = ty;

  glMultMatrixf((GLfloat*)&mp);
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

  glPushMatrix();
  // カメラ移動
  glScaled(m_affine.m11(), m_affine.m22(), 1.0);
  glTranslated(m_affine.dx(), m_affine.dy(), 0.0);

  PRINT_LOG("paintGL")
  // ロードされた画像を表示する
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

  glPopMatrix();

  // 現在のレイヤ名を書く
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
// ロードされた画像を表示する
//----------------------------------
void SceneViewer::drawImage() {
  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;

  int frame = settings->getFrame();

  // 素材の描画

  glPushMatrix();

  glEnable(GL_TEXTURE_2D);

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
      GLuint texName = (GLuint)layerImage.texture->textureId();
      glBindTexture(GL_TEXTURE_2D, texName);

      QSize texSize = layerImage.size;
      QPointF texCornerPos((double)texSize.width() / 2.0,
                           (double)texSize.height() / 2.0);

      // 半透明
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
// プレビュー結果を描く GL版
//----------------------------------
void SceneViewer::drawGLPreview() {
  ViewSettings* settings = m_project->getViewSettings();
  if (!settings) return;

  int frame = settings->getFrame();

  // 素材の描画

  glPushMatrix();

  glEnable(GL_TEXTURE_2D);

  double modeVal = (settings->getImageMode() == ImageMode_Half) ? 0.5 : 1.0;

  QSize workAreaSize = m_project->getWorkAreaSize();
  QPointF texCornerPos((double)workAreaSize.width() / 2.0,
                       (double)workAreaSize.height() / 2.0);

  glTranslated(-texCornerPos.x(), -texCornerPos.y(), 0);

  int targetShapeTag = m_project->getOutputSettings()->getShapeTagId();

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

    if (!layerImage.size.isEmpty()) {
      double visibleVal =
          (m_project->getLayer(lay)->isVisibleInViewer() == 1) ? 0.5 : 1.0;
      double chan = modeVal * visibleVal;
      glColor4d(chan, chan, chan, chan);

      GLuint texName = (GLuint)layerImage.texture->textureId();
      glBindTexture(GL_TEXTURE_2D, texName);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      // シェイプが無い場合、「参照用」シェイプと判断して描画する
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

          // メッシュを表示
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
// ワークエリアのワクを描く
//----------------------------------
void SceneViewer::drawWorkArea() {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ワークエリアサイズが空ならreturn
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
// シェイプを描く
//----------------------------------
void SceneViewer::drawShapes() {
  auto drawOneShape = [&](const OneShape& shape, int frame) {
    GLdouble* vertexArray =
        shape.shapePairP->getVertexArray(frame, shape.fromTo, m_project);
    // シェイプが閉じている場合
    if (shape.shapePairP->isClosed()) {
      glBegin(GL_LINE_LOOP);
      GLdouble* v_p = vertexArray;
      for (int v = 0; v < shape.shapePairP->getVertexAmount(m_project); v++) {
        glVertex3d(v_p[0], v_p[1], v_p[2]);
        v_p += 3;
      }
      glEnd();
    }
    // シェイプが開いている場合
    else {
      glBegin(GL_LINE_STRIP);
      GLdouble* v_p = vertexArray;
      for (int v = 0; v < shape.shapePairP->getVertexAmount(m_project); v++) {
        glVertex3d(v_p[0], v_p[1], v_p[2]);
        v_p += 3;
      }
      glEnd();
    }
    // データを解放
    delete[] vertexArray;
  };

  PRINT_LOG("drawShapes start")
  QSize workAreaSize = m_project->getWorkAreaSize();
  // ワークエリアサイズが空ならreturn
  if (workAreaSize.isEmpty()) return;

  // 現在のフレームを得る
  int frame = m_project->getViewFrame();

  // ワークエリアの左下を原点、ワークエリアの縦横サイズを１に正規化した座標系に変換する
  glPushMatrix();
  glTranslated(-(double)workAreaSize.width() / 2.0,
               -(double)workAreaSize.height() / 2.0, 0.0);
  glScaled((double)workAreaSize.width(), (double)workAreaSize.height(), 1.0);

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
        glColor4d(0.7, 0.7, 0.7, 0.5);
        if (isCurrent) {
          if (isSelected)
            (fromTo == 0) ? glColor3d(1.0, 0.7, 0.0) : glColor3d(0.0, 0.7, 1.0);
          // ロックされていたら薄くする
          else if (shapePair->isLocked(fromTo) || layer->isLocked())
            (fromTo == 0) ? glColor4d(0.7, 0.2, 0.2, 0.7)
                          : glColor4d(0.2, 0.2, 0.7, 0.7);
          else
            (fromTo == 0) ? glColor3d(1.0, 0.0, 0.0) : glColor3d(0.0, 0.0, 1.0);
        }

        // 今のところReshapeToolのSnap対象のシェイプのみ
        if (tool && tool->setSpecialShapeColor(oneShape))
          glColor3d(1.0, 0.0, 1.0);

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

      // 今のところReshapeToolのSnap対象のシェイプのみ
      if (tool && tool->setSpecialShapeColor(oneShape))
        glColor3d(1.0, 0.0, 1.0);

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
      glColor3d(1.0, 0.0, 1.0);
      for (int f = key; f <= nextKey; f++) {
        drawOneShape(shape, f);
      }
    }
  }

  PRINT_LOG("  Draw Guides")
  // ガイドを描く
  if (m_hRuler->getGuideCount() > 0 || m_vRuler->getGuideCount() > 0) {
    glLineStipple(1, 0xAAAA);
    glEnable(GL_LINE_STIPPLE);
    glBegin(GL_LINES);
    // 描画範囲
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
  // ツールが取得できなかったらreturn
  // プロジェクトがカレントじゃなくてもreturn
  if (tool && m_project->isCurrent()) {
    // ツールにこのビューアのポインタを渡す
    tool->setViewer(this);
    // ツールの描画
    tool->draw();
  }

  glPopMatrix();
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
  GLdouble mat[16];
  QList<int> keepList;

  QList<int> pickRanges = {5, 10, 20};

  for (auto pickRange : pickRanges) {
    glSelectBuffer(512, selectBuffer);
    // アプリケーションをセレクションモードに指定する
    glRenderMode(GL_SELECT);

    glMatrixMode(GL_PROJECTION);
    glGetDoublev(GL_PROJECTION_MATRIX, mat);
    glPushMatrix();
    glLoadIdentity();
    // ピック領域の指定。この範囲に描画を制限する
#ifdef _WIN32
    gluPickMatrix(pos.x(), height() - pos.y(), pickRange, pickRange, viewport);
#endif
#ifdef MACOSX
    my_gluPickMatrix(pos.x(), height() - pos.y(), pickRange, pickRange,
                     viewport);
#endif

    glMultMatrixd(mat);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    // ネームスタックをクリアして空にする
    glInitNames();

    // 描画
    paintGL();
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    // int ret = 0;
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
