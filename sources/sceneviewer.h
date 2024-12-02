//----------------------------------
// SceneViewer
// 1�̃v���W�F�N�g�ɂ���1�̃G���A
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

  //--- ���{�^���h���b�O�ɂ��p���p�f�[�^
  // �}�E�X�ʒu
  QPoint m_mousePos;
  // �p�������ǂ���
  bool m_isPanning;
  // ������Ă���{�^��
  Qt::MouseButton m_mouseButton;

  Ruler *m_hRuler, *m_vRuler;

  // �}�E�X�J�[�\�����C���L�[�ɓ���������
  static Qt::KeyboardModifiers m_modifiers;

  // �}�E�X�ʒu����IwaWarper�̍��W�n�ɕϊ�����
  QPointF convertMousePosToIWCoord(const QPointF &mousePos);

  // �����v���r���[���ʂ̉摜���L���b�V������Ă�����A
  // ������g���ăe�N�X�`�������B�e�N�X�`���C���f�b�N�X��Ԃ�
  unsigned int setPreviewTexture();

  // �J�����g���C���؂�ւ��R�}���h��o�^����
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

  // �N���b�N�ʒu�̕`��A�C�e���̃C���f�b�N�X��Ԃ�
  int pick(const QPoint &);
  // �N���b�N�ʒu�̕`��A�C�e���S�ẴC���f�b�N�X�̃��X�g��Ԃ�
  QList<int> pickAll(const QPoint &);

  // �V�F�C�v���W��ŁA�\����P�s�N�Z���̕����ǂꂭ�炢�ɂȂ邩��Ԃ�
  QPointF getOnePixelLength();
  const QTransform &getAffine() { return m_affine; }

  // �o�^�e�N�X�`���̏���
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

  // ���[�h���ꂽ�摜��\������
  void drawImage();
  // �v���r���[���ʂ�`�� GL��
  void drawGLPreview();

  // ���[�N�G���A�̃��N��`��
  void drawWorkArea();
  // �V�F�C�v��`��
  void drawShapes();

  // ���h���b�O�Ńp������
  void mouseMoveEvent(QMouseEvent *e) final override;
  // ���N���b�N�Ńp�����[�h�ɂ���
  void mousePressEvent(QMouseEvent *e) final override;
  // �p�����[�h���N���A����
  void mouseReleaseEvent(QMouseEvent *e) final override;
  // �p�����[�h���N���A����
  void leaveEvent(QEvent *e) final override;

  // �C���L�[���N���A����
  void enterEvent(QEvent *) final override;

  // �C���L�[�������ƃJ�[�\����ς���
  void keyPressEvent(QKeyEvent *e) final override;
  void keyReleaseEvent(QKeyEvent *e) final override;

  void wheelEvent(QWheelEvent *) final override;

protected slots:
  // �e�N�X�`���C���f�b�N�X�𓾂�B������΃��[�h���ēo�^
  ViewSettings::LAYERIMAGE getLayerImage(QString &path, int brightness,
                                         int contrast);

  // ViewSettings���ς������
  // �����̃v���W�F�N�g���J�����g�Ȃ�Aupdate
  void onViewSettingsChanged();

  // ���C���̓��e���ς������A���ݕ\�����Ă���t���[���̓��e���ς�������̂݃e�N�X�`�����X�V
  /// �V�[�P���X�̓��e���ς������A���ݕ\�����Ă���t���[���̓��e���ς�������̂݃e�N�X�`�����X�V
  void onLayerChanged();

  // �v���r���[�����������Ƃ��A���ݕ\�����̃t���[������
  // �v���r���[�^�n�[�t���[�h�������ꍇ�͕\�����X�V����
  void onPreviewRenderFinished(int frame);

public slots:
  // �T�C�Y����
  void zoomIn();
  void zoomOut();
  void zoomReset();

  // �J�����g���C����ς���
  void onChangeCurrentLayer();

  void cleanup() {}
};

#endif