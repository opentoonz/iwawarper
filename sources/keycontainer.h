//-----------------------------------------------------------------------------
// KeyContainer �N���X
//  �L�[�t���[�������f�[�^�������e���v���[�g�N���X
//	�]�V�F�C�v�������� FormKey
//	�]Correspondence�i�Ή��_�j�������� CorrKey
//-----------------------------------------------------------------------------

#ifndef KEYCONTAINER_H
#define KEYCONTAINER_H

#include <QMap>
#include <QList>
#include <QPointF>

class QXmlStreamWriter;
class QXmlStreamReader;

// �x�W�G�Ȑ��̊e�_�̍��W
typedef struct BezierPoint {
  // �R���g���[���|�C���g���W
  QPointF pos;
  // �n���h�����P���W
  QPointF firstHandle;
  // �n���h�����Q���W
  QPointF secondHandle;
} BezierPoint;

// �Ή��_
typedef struct CorrPoint {
  // �Ή��_�̃x�W�G�ʒu
  // �l�� 0�`m_bezierPointCount(Closed)
  //      0�`m_bezierPointCount-1(Open)
  double value;
  // �Ή��_�̃E�F�C�g�i�c�݂������񂹂�j
  double weight;

  bool operator==(const CorrPoint& other) const {
    return (value == other.value && weight == other.weight);
  }
} CorrPoint;

// �V�F�C�v�`��̃f�[�^�\���i�f�[�^�����R���g���[���|�C���g���j
typedef QList<BezierPoint> BezierPointList;
// �Ή��_���̃f�[�^�\���i�f�[�^����Corr�|�C���g���j
typedef QList<CorrPoint> CorrPointList;

template <class T>
class KeyContainer {
  QMap<int, T> m_data;

  // �L�[�t���[���Ԃ̊�������w��B�ϓ����聁0.5�̏ꍇ�͓o�^���Ȃ�
  // key�̓Z�O�����g�̑O���̃L�[�t���[���B��ɏ��m_data��key�Ɋ܂܂��K�v�B
  QMap<int, double> m_interpolation;

public:
  KeyContainer() {}

  // �L�[�t���[������ǉ�����B�ȑO�Ƀf�[�^������ꍇ�A�������Ēu��������
  void setKeyData(int frame, T data);
  // �w�肳�ꂽ�t���[�����L�[�t���[�����ǂ�����Ԃ�
  bool isKey(int frame);
  // frame ����Łiframe���܂܂Ȃ��j��ԋ߂��L�[�t���[����Ԃ��B
  // ������� -1��Ԃ�
  int nextKey(int frame);
  // frame��Key�Ȃ�frame��Ԃ��A�����łȂ����frame���O�ň�ԋ߂��L�[�t���[����Ԃ��B
  // ������� -1��Ԃ�
  int belongingKey(int frame);

  // �w�肳�ꂽ�t���[���̒l��Ԃ��B(���ꉻ����)
  // �L�[�t���[���������ꍇ�͏����l�B
  // �L�[�t���[���̏ꍇ�͂��̒l�B
  // �L�[�t���[���ł͂Ȃ��ꍇ�͕�Ԃ��ĕԂ��B
  T getData(int frame, int maxValue = 0, double smoothness = 0.);
  // T getData(int frame);

  // �L�[�t���[���̐���Ԃ�
  int getKeyCount() { return m_data.size(); }
  // �w�肵���t���[���̃L�[�t���[����������
  void removeKeyData(int frame);

  // �f�[�^�𓾂�
  QMap<int, T> getData() { return m_data; }
  QMap<int, double>& getInterpolation() { return m_interpolation; }
  // �f�[�^���Z�b�g����
  void setData(QMap<int, T> data) { m_data = data; }
  void setInterpolation(QMap<int, double> interpolation) {
    m_interpolation = interpolation;
  }

  // �Z�[�u/���[�h(���ꉻ����)
  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);
};

// ���ꉻ
template <>
BezierPointList KeyContainer<BezierPointList>::getData(int frame, int maxValue,
                                                       double smoothness);
template <>
CorrPointList KeyContainer<CorrPointList>::getData(int frame, int maxValue,
                                                   double smoothness);

#endif