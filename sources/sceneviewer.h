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

class IwProject;
class QMenu;
class Ruler;

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

  void renderText(double x, double y, const QString &str,
                  const QFont &font = QFont());

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
};

#endif