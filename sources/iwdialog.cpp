//---------------------------------------------------
// IwDialog
// �l�ݒ�f�[�^�ɃW�I���g���f�[�^��
// �ǂݏ�������悤�ɂ���
//---------------------------------------------------

#include "iwdialog.h"

#include <QVariant>
#include <iostream>
#include <QGuiApplication>
#include <QScreen>

//---------------------------------------------------
// �R���X�g���N�^�ŁA�ʒu/�T�C�Y�����[�h
//---------------------------------------------------
IwDialog::IwDialog(QWidget* parent, SettingsId dialogName, bool isResizable)
    : QDialog(parent), m_dialogName(dialogName), m_isResizable(isResizable) {
  // �ʒu�����擾
  bool ok;
  int leftEdge = PersonalSettingsManager::instance()
                     ->getValue(PS_G_DialogPositions, m_dialogName, PS_LeftEdge)
                     .toInt(&ok);
  // �_�C�A���O��񂪓����Ă��Ȃ��ꍇ��return
  if (!ok) {
    m_geom = QRect();
    return;
  }

  int topEdge = PersonalSettingsManager::instance()
                    ->getValue(PS_G_DialogPositions, m_dialogName, PS_TopEdge)
                    .toInt();

  // ���T�C�Y�Ȃ�A�T�C�Y���擾
  if (m_isResizable) {
    int width = PersonalSettingsManager::instance()
                    ->getValue(PS_G_DialogPositions, m_dialogName, PS_Width)
                    .toInt();
    int height = PersonalSettingsManager::instance()
                     ->getValue(PS_G_DialogPositions, m_dialogName, PS_Height)
                     .toInt();

    m_geom = QRect(leftEdge, topEdge, width, height);
  }
  // ���T�C�Y�s�Ȃ�ړ�����
  else {
    m_geom = QRect(leftEdge, topEdge, 1, 1);
  }

  // �X�N���[����ʊO�̏ꍇ�́A�v���C�}���X�N���[���̒��S�Ɏ����Ă���
  QScreen* screen = QGuiApplication::screenAt(mapToGlobal(m_geom.center()));
  if (!screen) {
    m_geom.moveCenter(mapFromGlobal(
        QGuiApplication::primaryScreen()->virtualGeometry().center()));
  }
}

//---------------------------------------------------
// �f�X�g���N�^�ŁA�ʒu/�T�C�Y��ۑ�
//---------------------------------------------------
IwDialog::~IwDialog() {
  // ���[
  PersonalSettingsManager::instance()->setValue(PS_G_DialogPositions,
                                                m_dialogName, PS_LeftEdge,
                                                QVariant(geometry().x()));
  // ��[
  PersonalSettingsManager::instance()->setValue(
      PS_G_DialogPositions, m_dialogName, PS_TopEdge, QVariant(geometry().y()));

  // ���T�C�Y�Ȃ�A�T�C�Y���ۑ�
  if (m_isResizable) {
    // ����
    PersonalSettingsManager::instance()->setValue(PS_G_DialogPositions,
                                                  m_dialogName, PS_Width,
                                                  QVariant(geometry().width()));
    // ����
    PersonalSettingsManager::instance()->setValue(
        PS_G_DialogPositions, m_dialogName, PS_Height,
        QVariant(geometry().height()));
  }
}

//---------------------------------------------------
// �J���Ƃ��A�ʒu/�T�C�Y���W�I���g���ɓK�p
//---------------------------------------------------
void IwDialog::showEvent(QShowEvent*) {
  if (m_geom.isEmpty()) return;

  // ���T�C�Y�Ȃ�A�T�C�Y���擾
  if (m_isResizable) {
    setGeometry(m_geom);
  }
  // ���T�C�Y�s�Ȃ�ړ�����
  else {
    move(m_geom.left(), m_geom.top());
  }
}

//---------------------------------------------------
// ����Ƃ��A�W�I���g�����L�[�v
//---------------------------------------------------
void IwDialog::hideEvent(QHideEvent*) { m_geom = geometry(); }