//----------------------------------
// SceneViewer
// 1つのプロジェクトについて1つのエリア
//----------------------------------

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

#include "viewsettings.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTransform>
#include <QMouseEvent>
#include <QStack>
#include <QMatrix4x4>

class IwProject;
class QMenu;
class Ruler;

class QOpenGLShaderProgram;
class QOpenGLBuffer;
class QOpenGLVertexArrayObject;

class SceneViewer : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT

  IwProject *m_project = nullptr;
  QTransform m_affine;

  //--- 中ボタンドラッグによるパン用データ
  // マウス位置
  QPoint m_mousePos;
  // パン中かどうか
  bool m_isPanning;
  // 押されているボタン
  Qt::MouseButton m_mouseButton;

  Ruler *m_hRuler, *m_vRuler;

  // マウスカーソルを修飾キーに同期させる
  static Qt::KeyboardModifiers m_modifiers;

  // マウス位置からIwaWarperの座標系に変換する
  QPointF convertMousePosToIWCoord(const QPointF &mousePos);

  // もしプレビュー結果の画像がキャッシュされていたら、
  // それを使ってテクスチャを作る。テクスチャインデックスを返す
  unsigned int setPreviewTexture();

  // カレントレイヤ切り替えコマンドを登録する
  void addContextMenus(QMenu &menu);
  void updateRulers();

  // opengl
  QOpenGLShaderProgram *m_program_texture  = nullptr;
  QOpenGLShaderProgram *m_program_meshLine = nullptr;
  QOpenGLShaderProgram *m_program_line     = nullptr;
  QOpenGLShaderProgram *m_program_fill     = nullptr;
  QOpenGLBuffer *m_vbo                     = nullptr;
  QOpenGLBuffer *m_ibo                     = nullptr;  // index buffer
  QOpenGLVertexArrayObject *m_vao          = nullptr;

  QOpenGLBuffer *m_line_vbo            = nullptr;
  QOpenGLVertexArrayObject *m_line_vao = nullptr;

  GLint u_tex_matrix         = 0;
  GLint u_tex_texture        = 0;
  GLint u_tex_alpha          = 0;
  GLint u_tex_matte_sw       = 0;
  GLint u_tex_matteImgSize   = 0;
  GLint u_tex_workAreaSize   = 0;
  GLint u_tex_matteTexture   = 0;
  GLint u_tex_matteColors    = 0;
  GLint u_tex_matteTolerance = 0;
  GLint u_tex_matteDilate    = 0;

  GLint u_meshLine_matrix = 0;
  GLint u_meshLine_color  = 0;

  GLint u_line_matrix         = 0;
  GLint u_line_color          = 0;
  GLint u_line_viewportSize   = 0;
  GLint u_line_stippleFactor  = 0;
  GLint u_line_stipplePattern = 0;

  GLint u_fill_matrix = 0;
  GLint u_fill_color  = 0;

  QMatrix4x4 m_viewProjMatrix;
  QStack<QMatrix4x4> m_modelMatrix;

  QPainter *m_p = nullptr;

public:
  SceneViewer(QWidget *parent);

  ~SceneViewer();

  QSize sizeHint() const final override { return QSize(630, 440); }

  // クリック位置の描画アイテムのインデックスを返す
  int pick(const QPoint &);
  // クリック位置の描画アイテム全てのインデックスのリストを返す
  QList<int> pickAll(const QPoint &);

  // シェイプ座標上で、表示上１ピクセルの幅がどれくらいになるかを返す
  QPointF getOnePixelLength();
  const QTransform &getAffine() { return m_affine; }

  // 登録テクスチャの消去
  void deleteTextures();

  void setProject(IwProject *prj);

  void setRulers(Ruler *hRuler, Ruler *vRuler) {
    m_hRuler = hRuler;
    m_vRuler = vRuler;
  }

  void renderText(double x, double y, const QString &str, const QColor color,
                  const QFont &font = QFont());

  void setLineStipple(GLint factor, GLushort pattern);

  // utility functions for IwTool::draw
  void pushMatrix();
  void popMatrix();
  void translate(GLdouble, GLdouble, GLdouble);
  void scale(GLdouble, GLdouble, GLdouble);
  void setColor(const QColor &);
  QMatrix4x4 modelMatrix() { return m_modelMatrix.top(); }
  void doDrawLine(GLenum mode, QVector3D *verts, int vertCount);
  void doDrawFill(GLenum mode, QVector3D *verts, int vertCount,
                  QColor fillColor);
  void releaseBufferObjects();
  void bindBufferObjects();

  void doShapeRender();

protected:
  void initializeGL() final override;
  void resizeGL(int width, int height) final override;
  void paintGL() final override;

  // ロードされた画像を表示する
  void drawImage();
  // プレビュー結果を描く GL版
  void drawGLPreview();

  // ワークエリアのワクを描く
  void drawWorkArea();
  // シェイプを描く
  void drawShapes();

  // 中ドラッグでパンする
  void mouseMoveEvent(QMouseEvent *e) final override;
  // 中クリックでパンモードにする
  void mousePressEvent(QMouseEvent *e) final override;
  // パンモードをクリアする
  void mouseReleaseEvent(QMouseEvent *e) final override;
  // パンモードをクリアする
  void leaveEvent(QEvent *e) final override;

  // 修飾キーをクリアする
  void enterEvent(QEvent *) final override;

  // 修飾キーを押すとカーソルを変える
  void keyPressEvent(QKeyEvent *e) final override;
  void keyReleaseEvent(QKeyEvent *e) final override;

  void wheelEvent(QWheelEvent *) final override;

protected slots:
  // テクスチャインデックスを得る。無ければロードして登録
  ViewSettings::LAYERIMAGE getLayerImage(QString &path, int brightness,
                                         int contrast);

  // ViewSettingsが変わったら
  // 自分のプロジェクトがカレントなら、update
  void onViewSettingsChanged();

  // レイヤの内容が変わったら、現在表示しているフレームの内容が変わった時のみテクスチャを更新
  /// シーケンスの内容が変わったら、現在表示しているフレームの内容が変わった時のみテクスチャを更新
  void onLayerChanged();

  // プレビューが完成したとき、現在表示中のフレームかつ
  // プレビュー／ハーフモードだった場合は表示を更新する
  void onPreviewRenderFinished(int frame);

public slots:
  // サイズ調整
  void zoomIn();
  void zoomOut();
  void zoomReset();

  // カレントレイヤを変える
  void onChangeCurrentLayer();

  void cleanup() {}
};

#endif