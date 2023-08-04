#ifndef SHAPEPAIR_H
#define SHAPEPAIR_H

//---------------------------------------
//  ShapePair
//  From-To�̌`����y�A�Ŏ���
//---------------------------------------

#include "keycontainer.h"

#include "keycontainer.h"
#include "colorsettings.h"

#include <QRectF>
#include <array>
#include <QMutex>
#include <QObject>

class IwProject;
class QPainter;
class QXmlStreamWriter;
class QXmlStreamReader;
class QMouseEvent;

class ShapePair : public QObject {
  Q_OBJECT

  QString m_name;
  // Correnspondence(�Ή��_)�Ԃ����������邩
  int m_precision;
  // �V�F�C�v�����Ă��邩�ǂ���
  bool m_isClosed;
  // �x�W�G�̃|�C���g��
  int m_bezierPointAmount;
  // Correspondence(�Ή��_)�̐�
  int m_corrPointAmount;
  // �`��f�[�^ - from, to �̂Q��
  KeyContainer<BezierPointList> m_formKeys[2];
  // Correspondence(�Ή��_)�f�[�^ - from, to �̂Q��
  KeyContainer<CorrPointList> m_corrKeys[2];

  // �ȉ��^�C�����C���p
  // �\�����J���Ă��邩�ǂ��� = �I������Ă��邩�ǂ���
  std::array<bool, 2> m_isExpanded;
  // ���b�N����Ă��邩�ǂ���
  std::array<bool, 2> m_isLocked;
  // �s���i�\���Œ�j����Ă��邩�ǂ���
  std::array<bool, 2> m_isPinned;

  // �^�O�̈ꗗ
  std::array<QList<int>, 2> m_shapeTag;

  // �V�F�C�v���e���ǂ���
  // �����_�����O�́A�e�V�F�C�v�ƁA���̉��Ɂi��������΁j
  // �������Ă���q�V�F�C�v���ꂩ���܂�ɂ��Čv�Z�����B
  // �����l�́AClosed�̏ꍇ��true�AOpened�̏ꍇ��false
  bool m_isParent;

  // �V�F�C�v���\������Ă��邩�ǂ����B��\���̃V�F�C�v�͑I���ł��Ȃ��B
  // �i���b�N����Ă���̂Ɠ����j�����_�����O�ɂ͗p������H
  bool m_isVisible;

  // �V�F�C�v�̃L�[�Ԃ̕�Ԃ̓x�����B
  // Smoothness = 0 (�����l)�F���j�A���
  // Smoothness = 1 : 3���X�v���C�����
  double m_animationSmoothness;

  QMutex mutex_;

public:
  ShapePair(int frame, bool isClosed, BezierPointList bPointList,
            CorrPointList cPointList, double dstShapeOffset = 0.0);

  //(�t�@�C���̃��[�h���̂ݎg��)��̃V�F�C�v�̃R���X�g���N�^
  ShapePair();

  // �R�s�[�R���X�g���N�^
  ShapePair(ShapePair* src);

  ShapePair* clone();

  // ����t���[���ł̃x�W�G�`���Ԃ�
  BezierPointList getBezierPointList(int frame, int fromTo);
  // ����t���[���ł̑Ή��_��Ԃ�
  CorrPointList getCorrPointList(int frame, int fromTo);

  double getBezierInterpolation(int frame, int fromTo);
  double getCorrInterpolation(int frame, int fromTo);
  void setBezierInterpolation(int frame, int fromTo, double interp);
  void setCorrInterpolation(int frame, int fromTo, double interp);

  // �w�肵���t���[���̃x�W�G���_����Ԃ�
  //  isFromShape = true �Ȃ�FROM, false�Ȃ�TO �� �V�F�C�v�̒��_�𓾂�
  double* getVertexArray(int frame, int fromTo, IwProject* project);

  // ���Ă��邩�ǂ�����Ԃ�
  bool isClosed() { return m_isClosed; }
  // �V�F�C�v�����B
  void setIsClosed(bool close) { m_isClosed = close; }

  // �\���p�̒��_�������߂�B�������Ă��邩�󂢂Ă��邩�ňقȂ�
  int getVertexAmount(IwProject* project);

  // �V�F�C�v�̃o�E���f�B���O�{�b�N�X��Ԃ�
  //  fromTo = 0 �Ȃ�FROM, 1�Ȃ�TO �� �o�E���f�B���O�{�b�N�X�𓾂�
  QRectF getBBox(int frame, int fromTo);

  // �x�W�G�̃|�C���g����Ԃ�
  int getBezierPointAmount() { return m_bezierPointAmount; }
  // Correspondence(�Ή��_)�̐���Ԃ�
  int getCorrPointAmount() { return m_corrPointAmount; }

  // �����̃t���[�����A�`��̃L�[�t���[�����ǂ�����Ԃ�
  bool isFormKey(int frame, int fromTo) {
    return m_formKeys[fromTo].isKey(frame);
  }
  // �����̃t���[�����A�Ή��_�̃L�[�t���[�����ǂ�����Ԃ�
  bool isCorrKey(int frame, int fromTo) {
    return m_corrKeys[fromTo].isKey(frame);
  }

  // frame��Key�Ȃ�frame��Ԃ��A�����łȂ����frame���O�ň�ԋ߂��L�[�t���[����Ԃ��B
  // ������� -1��Ԃ�
  int belongingFormKey(int frame, int fromTo) {
    return m_formKeys[fromTo].belongingKey(frame);
  }
  int belongingCorrKey(int frame, int fromTo) {
    return m_corrKeys[fromTo].belongingKey(frame);
  }

  // �`��L�[�t���[���̃Z�b�g
  void setFormKey(int frame, int fromTo, BezierPointList bPointList);
  // �`��L�[�t���[������������
  void removeFormKey(int frame, int fromTo, bool enableRemoveLastKey = false);

  // �Ή��_�L�[�t���[���̃Z�b�g
  void setCorrKey(int frame, int fromTo, CorrPointList cPointList);
  // �Ή��_�L�[�t���[������������
  void removeCorrKey(int frame, int fromTo, bool enableRemoveLastKey = false);

  // �w��ʒu�ɃR���g���[���|�C���g��ǉ�����@�R���g���[���|�C���g�̃C���f�b�N�X��Ԃ�
  int addControlPoint(const QPointF& pos, int frame, int fromTo);
  // �w��ʒu�ɑΉ��_��ǉ����� ���܂���������true��Ԃ�
  bool addCorrPoint(const QPointF& pos, int frame, int fromTo);

  // �w��C���f�b�N�X�̑Ή��_���������� ���܂���������true��Ԃ�
  bool deleteCorrPoint(int index);

  // �}�E�X�ʒu�ɍŋߖT�̃x�W�G���W�𓾂�
  double getNearestBezierPos(const QPointF& pos, int frame, int fromTo);
  double getNearestBezierPos(const QPointF& pos, int frame, int fromTo,
                             double& dist);
  // �}�E�X�ʒu�ɍŋߖT�̃x�W�G���W�𓾂�(�͈͂���)
  double getNearestBezierPos(const QPointF& pos, int frame, int fromTo,
                             const double rangeBefore, const double rangeAfter,
                             double& dist);

  // ���[�N�G���A�̃��T�C�Y�ɍ��킹�t���ŃX�P�[�����ăV�F�C�v�̃T�C�Y���ێ�����
  void counterResize(QSizeF workAreaScale);

  // �`��f�[�^��Ԃ�
  QMap<int, BezierPointList> getFormData(int fromTo);
  QMap<int, double> getFormInterpolation(int fromTo);
  // �`��f�[�^���Z�b�g
  void setFormData(QMap<int, BezierPointList> data, int fromTo);
  // �Ή��_�f�[�^��Ԃ�
  QMap<int, CorrPointList> getCorrData(int fromTo);
  QMap<int, double> getCorrInterpolation(int fromTo);
  // �Ή��_�f�[�^���Z�b�g
  void setCorrData(QMap<int, CorrPointList> data, int fromTo);

  // �Ή��_�̍��W���X�g�𓾂�
  QList<QPointF> getCorrPointPositions(int frame, int fromTo);
  // �Ή��_�̃E�F�C�g�̃��X�g�𓾂�
  QList<double> getCorrPointWeights(int frame, int fromTo);

  // ���݂̃t���[����Correnspondence(�Ή��_)�Ԃ𕪊������_�̕����l�𓾂�
  QList<double> getDiscreteCorrValues(const QPointF& onePix, int frame,
                                      int fromTo);

  // ���݂̃t���[����Correnspondence(�Ή��_)�Ԃ𕪊������_�̃E�F�C�g�l�𓾂�
  QList<double> getDiscreteCorrWeights(int frame, int fromTo);

  // �x�W�G�l������W�l���v�Z����
  QPointF getBezierPosFromValue(int frame, int fromTo, double bezierValue);

  // �w��x�W�G�l�ł̂P�x�W�G�l������̍��W�l�̕ω��ʂ�Ԃ�
  double getBezierSpeedAt(int frame, int fromTo, double bezierValue,
                          double distance);

  // �w�肵���x�W�G�l�Ԃ̃s�N�Z��������Ԃ�
  double getBezierLengthFromValueRange(const QPointF& onePix, int frame,
                                       int fromTo, double startVal,
                                       double endVal);

  // Correnspondence(�Ή��_)�Ԃ����������邩�A��I/O
  int getEdgeDensity() { return m_precision; }
  void setEdgeDensity(int prec);

  // �V�F�C�v�̃L�[�Ԃ̕�Ԃ̓x�����A��I/O
  double getAnimationSmoothness() { return m_animationSmoothness; }
  void setAnimationSmoothness(double smoothness);

  // �`��L�[�t���[������Ԃ�
  int getFormKeyFrameAmount(int fromTo);
  // �Ή��_�L�[�t���[������Ԃ�
  int getCorrKeyFrameAmount(int fromTo);

  // �\�[�g���ꂽ�`��L�[�t���[���̃��X�g��Ԃ�
  QList<int> getFormKeyFrameList(int fromTo);
  // �\�[�g���ꂽ�Ή��_�L�[�t���[���̃��X�g��Ԃ�
  QList<int> getCorrKeyFrameList(int fromTo);

  void getFormKeyRange(int& start, int& end, int frame, int fromTo);
  void getCorrKeyRange(int& start, int& end, int frame, int fromTo);
  //----------------------------------
  // �ȉ��A�^�C�����C���ł̕\���p
  //----------------------------------
  // �W�J���Ă��邩�ǂ����𓥂܂��ĕ\���s����Ԃ�
  int getRowCount();

  // �`��(�w�b�_����)
  void drawTimeLineHead(QPainter& p, int& vpos, int width, int rowHeight,
                        int& currentRow, int mouseOverRow, int mouseHPos,
                        ShapePair* parentShape,
                        ShapePair* nextShape,  // �� �e�q�֌W�̐�����������
                        bool layerIsLocked);
  // �`��(�{��)
  void drawTimeLine(QPainter& p, int& vpos, int width, int fromFrame,
                    int toFrame, int frameWidth, int rowHeight, int& currentRow,
                    int mouseOverRow, double mouseOverFrameD,
                    bool layerIsLocked, bool layerIsVisibleInRender);

  // �^�C�����C���N���b�N��
  bool onTimeLineClick(int rowInShape, double frameD, bool ctrlPressed,
                       bool shiftPressed, Qt::MouseButton button);

  QString getName() { return m_name; }
  void setName(QString name) { m_name = name; }

  // ���N���b�N���̏��� row�͂��̃��C�����ŏォ��O�Ŏn�܂�s���B
  // �ĕ`�悪�K�v�Ȃ�true��Ԃ�
  bool onLeftClick(int row, int mouseHPos, QMouseEvent* e);

  // From�V�F�C�v��To�V�F�C�v�ɃR�s�[
  void copyFromShapeToToShape(double offset = 0.0);

  bool isExpanded(int fromTo) { return m_isExpanded[fromTo]; }
  bool isAtLeastOneShapeExpanded();

  void setExpanded(int fromTo, bool expand) { m_isExpanded[fromTo] = expand; }
  bool isLocked(int fromTo) { return m_isLocked[fromTo]; }
  void setLocked(int fromTo, bool lock) { m_isLocked[fromTo] = lock; }

  bool isPinned(int fromTo) { return m_isPinned[fromTo]; }
  void setPinned(int fromTo, bool pin) { m_isPinned[fromTo] = pin; }

  bool isParent() { return m_isParent; }
  void setIsParent(bool parent) { m_isParent = parent; }

  // �V�F�C�v�^�O

  QList<int>& getShapeTags(int fromTo) { return m_shapeTag[fromTo]; }
  void addShapeTag(int fromTo, int shapeTagId) {
    if (m_shapeTag[fromTo].contains(shapeTagId)) return;
    m_shapeTag[fromTo].append(shapeTagId);
    std::sort(m_shapeTag[fromTo].begin(), m_shapeTag[fromTo].end());
  }
  void removeShapeTag(int fromTo, int shapeTagId) {
    m_shapeTag[fromTo].removeAll(shapeTagId);
  }
  bool containsShapeTag(int fromTo, int shapeTagId) {
    return m_shapeTag[fromTo].contains(shapeTagId);
  }
  int shapeTagCount(int fromTo) { return m_shapeTag[fromTo].count(); }
  bool isRenderTarget(int shapeTagId) {
    if (shapeTagId == -1) return true;
    return containsShapeTag(0, shapeTagId) || containsShapeTag(1, shapeTagId);
  }

  bool isVisible() { return m_isVisible; }
  void setVisible(bool on) { m_isVisible = on; }

  // �Z�[�u/���[�h
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);
};

struct OneShape {
  ShapePair* shapePairP;
  int fromTo;

  OneShape(ShapePair* shape = 0, int ft = 0) {
    shapePairP = shape;
    fromTo     = ft;
  }
  bool operator==(const OneShape& right) const {
    return shapePairP == right.shapePairP && fromTo == right.fromTo;
  }
  bool operator!=(const OneShape& right) const {
    return shapePairP != right.shapePairP || fromTo != right.fromTo;
  }

  BezierPointList getBezierPointList(int frame) {
    return shapePairP->getBezierPointList(frame, fromTo);
  }

  OneShape getPartner() { return OneShape(shapePairP, (fromTo + 1) % 2); }
};

#endif