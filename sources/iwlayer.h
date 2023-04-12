#ifndef IWLAYER_H
#define IWLAYER_H
//---------------------------------------
// IwLayer
//---------------------------------------

#include "timage.h"

#include <QString>
#include <QList>
#include <QPair>
#include <QDir>
#include <QObject>

// class IwShape;
class ShapePair;

class QPainter;

struct OneShape;
class IwTimeLineSelection;
class QXmlStreamWriter;
class QXmlStreamReader;

class QMenu;
class QMouseEvent;

class IwLayer : public QObject {
  Q_OBJECT
public:
  enum LayerVisibility { Invisible = 0, HalfVisible, Full };

private:
  QString m_name;

  // Viewer��Ō����邩�ǂ����̃X�C�b�`
  // 0:�s�� 1:���� 2: 100��������
  LayerVisibility m_isVisibleInViewer;

  // Rendering����邩�ǂ���
  bool m_isVisibleInRender;

  // �^�C�����C����œW�J���Ă��邩�ǂ����̃X�C�b�`
  bool m_isExpanded;

  // ���b�N����Ă��邩
  bool m_isLocked;

  // �t�H���_�̃��X�g
  QList<QDir> m_paths;
  // �t���[�������̃��X�g
  // �t�H���_�C���f�b�N�X�A�t�@�C�������i�[����Ă���B
  QList<QPair<int, QString>> m_images;

  // �V�F�C�v
  QList<ShapePair*> m_shapepairs;

  int m_brightness, m_contrast;

public:
  IwLayer();
  IwLayer(const QString& name, LayerVisibility visibleInViewer, bool isExpanded,
          bool isVisibleInRender, bool isLocked, int brightness, int contrast);
  // IwLayer(const IwLayer&) {};
  IwLayer* clone();

  // �t���[���ԍ�����i�[����Ă���t�@�C���p�X��Ԃ�
  //(�t���[���͂O�X�^�[�g�B�����ꍇ�͋�String��Ԃ�)
  QString getImageFilePath(int frame);

  // �t���[���ԍ�����i�[����Ă���t�@�C������Ԃ�
  //  Parent�t�H���_�����B
  //(�t���[���͂O�X�^�[�g�B�����ꍇ�͋�String��Ԃ�)
  QString getImageFileName(int frame);

  // �t���[���ԍ�����i�[����Ă���parent�t�H���_����Ԃ�
  //(�t���[���͂O�X�^�[�g�B�����ꍇ�͋�String��Ԃ�)
  QString getParentFolderName(int frame);

  // �f�B���N�g���p�X����́B
  // �܂����X�g�ɖ����ꍇ�͒ǉ����ăC���f�b�N�X��Ԃ��B
  int getFolderPathIndex(const QString& folderPath);

  QString getName() { return m_name; }
  void setName(QString& name) { m_name = name; }

  // �t���[������Ԃ�
  int getFrameLength() { return m_images.size(); }

  // �摜�p�X�̑}�� �� �V�O�i���͊O�Ŋe���G�~�b�g���邱��
  void insertPath(int index, QPair<int, QString> image);
  // �摜�p�X����������B �� �V�O�i���͊O�Ŋe���G�~�b�g���邱��
  void deletePaths(int startIndex, int amount, bool skipPathCheck = false);
  // �摜�p�X�̍����ւ� �� �V�O�i���͊O�Ŋe���G�~�b�g���邱��
  void replacePath(int index, QPair<int, QString> image);

  // �L���b�V��������΂����Ԃ��B
  // ������Ή摜�����[�h���ăL���b�V���Ɋi�[����
  TImageP getImage(int index);

  // �V�F�C�v
  int getShapePairCount(bool skipDeletedShape = false);
  ShapePair* getShapePair(int shapePairIndex);
  // �V�F�C�v�̒ǉ��Bindex���w�肵�Ă��Ȃ���ΐV���ɒǉ�
  void addShapePair(ShapePair* shapePair, int index = -1);
  void deleteShapePair(int shapePairIndex, bool doRemove = false);
  void deleteShapePair(ShapePair* shapePair, bool doRemove = false);

  // �V�F�C�v���X�g�̊ہX�����ւ�
  void replaceShapePairs(QList<ShapePair*>);

  // Viewer��Ō����邩�ǂ����̃X�C�b�`��Ԃ�
  LayerVisibility isVisibleInViewer() { return m_isVisibleInViewer; }
  void setVisibilityInViewer(LayerVisibility visiblity) {
    m_isVisibleInViewer = visiblity;
  }
  bool isVisibleInRender() { return m_isVisibleInRender; }
  void setVisibleInRender(bool on) { m_isVisibleInRender = on; }

  //----------------------------------
  // �ȉ��A�^�C�����C���ł̕\���p
  //----------------------------------
  // �W�J���Ă��邩�ǂ����𓥂܂��Ē��̃V�F�C�v�̓W�J�����擾���ĕ\���s����Ԃ�
  int getRowCount();
  // �`��(�w�b�_����)
  void drawTimeLineHead(QPainter& p, int vpos, int width, int rowHeight,
                        int& currentRow, int mouseOverRow, int mouseHPos);
  // �`��(�{��)
  void drawTimeLine(QPainter& p, int vpos, int width, int fromFrame,
                    int toFrame, int frameWidth, int rowHeight, int& currentRow,
                    int mouseOverRow, double mouseOverFrameD,
                    IwTimeLineSelection* selection);
  // ���̃A�C�e�����X�g���N���b�N���̏���
  // row�͂��̃��C�����ŏォ��O�Ŏn�܂�s���B �ĕ`�悪�K�v�Ȃ�true��Ԃ�
  bool onLeftClick(int row, int mouseHPos, QMouseEvent* e);
  // �_�u���N���b�N���A���O������������true�B�V�F�C�v���ǂ���������
  bool onDoubleClick(int row, int mouseHPos, ShapePair** shapeP);
  // ���̃A�C�e�����X�g�E�N���b�N���̑���
  void onRightClick(int row, QMenu& menu);

  // �^�C�����C���㍶�N���b�N���̏���
  bool onTimeLineClick(int row, double frameD, bool controlPressed,
                       bool shiftPressed, Qt::MouseButton button);

  // �}�E�X�z�o�[���̏����B�h���b�O�\�ȃ{�[�_�[�̂Ƃ�true��Ԃ�
  bool onMouseMove(int frameIndex, bool isBorder);

  // �Z�[�u/���[�h
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);

  // ���O��Ԃ�
  int getNameFromShapePair(OneShape shape);
  // �V�F�C�v��Ԃ�
  OneShape getShapePairFromName(int name);
  // ���̃��C���Ɉ����̃V�F�C�v�y�A�������Ă�����true
  bool contains(ShapePair*);
  // �V�F�C�v����C���f�b�N�X��Ԃ� �������-1
  int getIndexFromShapePair(const ShapePair*) const;
  // �V�F�C�v�����ւ���
  void swapShapes(int index1, int index2) {
    if (index1 < 0 || index1 >= m_shapepairs.size() || index2 < 0 ||
        index2 >= m_shapepairs.size() || index1 == index2)
      return;
    m_shapepairs.swapItemsAt(index1, index2);
  }
  // �V�F�C�v���ړ�����
  void moveShape(int from, int to) {
    if (from < 0 || from >= m_shapepairs.size() || to < 0 ||
        to >= m_shapepairs.size() || from == to)
      return;
    m_shapepairs.move(from, to);
  }

  // Undo����p�B�t���[���̃f�[�^�����̂܂ܕԂ�
  QPair<int, QString> getPathData(int frame);

  // ������Parent�Ȃ玩����Ԃ��B�q�Ȃ�eShape�̃|�C���^��Ԃ�
  ShapePair* getParentShape(ShapePair* shape);

  int brightness() { return m_brightness; }
  void setBrightness(int brightness) { m_brightness = brightness; }
  int contrast() { return m_contrast; }
  void setContrast(int contrast) { m_contrast = contrast; }

  void setExpanded(bool lock) { m_isExpanded = lock; }
  bool isExpanded() { return m_isExpanded; }

  void setLocked(bool Lock) { m_isLocked = Lock; }
  bool isLocked() { return m_isLocked; }

protected slots:
  void onMoveShapeUpward();
  void onMoveShapeDownward();
  void onMoveAboveUpperShape();
  void onMoveBelowLowerShape();

  void onSwitchParentChild();
};

#endif
