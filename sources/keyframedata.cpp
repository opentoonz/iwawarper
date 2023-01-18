//----------------------------------------------------
// KeyFrameData
// KeyFrameEditor���̃R�s�[/�y�[�X�g�ň����f�[�^
//----------------------------------------------------

#include "keyframedata.h"
#include "iwapp.h"
#include "iwprojecthandle.h"
#include "iwproject.h"

#include "shapepair.h"
#include "iwtrianglecache.h"

KeyFrameData::KeyFrameData() {}

//----------------------------------------------------

KeyFrameData::KeyFrameData(const KeyFrameData* src) { m_data = src->m_data; }

//----------------------------------------------------
// �I��͈͂���f�[�^���i�[����
//----------------------------------------------------

void KeyFrameData::setKeyFrameData(OneShape& shape, QList<int>& selectedFrames,
                                   KeyFrameEditor::KEYMODE keymode) {
  // ���݂̑I��͈͂��� �R�s�[��_�i��ԍ��̃t���[���j�𓾂�
  int baseFrame = 10000;
  for (int f = 0; f < selectedFrames.size(); f++) {
    // �ŏ��l�̍X�V
    if (selectedFrames.at(f) < baseFrame) baseFrame = selectedFrames.at(f);
  }

  for (int f = 0; f < selectedFrames.size(); f++) {
    CellData cellData;
    int frame        = selectedFrames.at(f);
    cellData.pos     = QPoint(frame - baseFrame, 0);
    cellData.keymode = keymode;

    cellData.shapeWasClosed = shape.shapePairP->isClosed();
    cellData.bpCount        = shape.shapePairP->getBezierPointAmount();
    cellData.cpCount        = shape.shapePairP->getCorrPointAmount();

    // �`��L�[�t���[���ŁA�����[�h��Form/Both�̏ꍇ�A�L�[�����i�[
    if (shape.shapePairP->isFormKey(frame, shape.fromTo) &&
        (keymode == KeyFrameEditor::KeyMode_Form ||
         keymode == KeyFrameEditor::KeyMode_Both)) {
      cellData.bpList =
          shape.shapePairP->getBezierPointList(frame, shape.fromTo);
      cellData.b_interp =
          shape.shapePairP->getBezierInterpolation(frame, shape.fromTo);
    }

    // �Ή��_�L�[�t���[���ŁA�����[�h��Corr/Both�̏ꍇ�A�L�[�����i�[
    if (shape.shapePairP->isCorrKey(frame, shape.fromTo) &&
        (keymode == KeyFrameEditor::KeyMode_Corr ||
         keymode == KeyFrameEditor::KeyMode_Both)) {
      cellData.cpList = shape.shapePairP->getCorrPointList(frame, shape.fromTo);
      cellData.c_interp =
          shape.shapePairP->getCorrInterpolation(frame, shape.fromTo);
    }

    m_data.push_back(cellData);
  }
}

//----------------------------------------------------
// �w��Z���Ƀf�[�^���㏑���y�[�X�g����
//----------------------------------------------------

void KeyFrameData::getKeyFrameData(OneShape& shape, QList<int>& selectedFrames,
                                   KeyFrameEditor::KEYMODE keymode) const {
  // �Z���I��͈͂��������return
  if (selectedFrames.isEmpty()) return;

  // �v���W�F�N�g�������return
  IwProject* project = IwApp::instance()->getCurrentProject()->getProject();
  if (!project) return;

  // �`��L�[
  bool doFormPaste = (keymode == KeyFrameEditor::KeyMode_Form ||
                      keymode == KeyFrameEditor::KeyMode_Both);
  // �Ή��_�L�[
  bool doCorrPaste = (keymode == KeyFrameEditor::KeyMode_Corr ||
                      keymode == KeyFrameEditor::KeyMode_Both);

  // ���݂̑I��͈͂���A�y�[�X�g��_�i��ԍ��̃t���[���j�𓾂�
  int baseFrame = 10000;
  for (int f = 0; f < selectedFrames.size(); f++) {
    // �ŏ��l�̍X�V
    if (selectedFrames.at(f) < baseFrame) baseFrame = selectedFrames.at(f);
  }

  // �R�s�[���ꂽ�e�Z���ɂ��āA�ΏۃZ���Ƀy�[�X�g���\���ǂ����̔��f���s��
  QList<bool> pasteAvailability;
  for (int d = 0; d < m_data.size(); d++) {
    CellData cellData = m_data.at(d);

    // �y�[�X�g�Ώۃt���[�����W
    int pasteFrame = baseFrame + cellData.pos.x();

    // �����@ Open/Close��������炻�������y�[�X�g�ł��Ȃ��Breturn
    if (cellData.shapeWasClosed != shape.shapePairP->isClosed()) return;

    // ���W�����݂̃Z���\�����͂ݏo���Ă�����false�ɂ���continue
    if (pasteFrame < 0 || pasteFrame >= project->getProjectFrameLength()) {
      pasteAvailability.push_back(false);
      continue;
    }

    // ���[�h���ɔ���𕪂���
    //  �`��L�[�̑���
    //  �����A �R���g���[���|�C���g��������
    if (doFormPaste &&
        (cellData.keymode == KeyFrameEditor::KeyMode_Form ||
         cellData.keymode == KeyFrameEditor::KeyMode_Both) &&
        shape.shapePairP->getBezierPointAmount() != cellData.bpCount) {
      return;
    }

    // �Ή��_�L�[�̑���
    //  �����A Corr�_��������
    if (doCorrPaste &&
        (cellData.keymode == KeyFrameEditor::KeyMode_Corr ||
         cellData.keymode == KeyFrameEditor::KeyMode_Both) &&
        shape.shapePairP->getCorrPointAmount() != cellData.cpCount) {
      return;
    }

    pasteAvailability.push_back(true);
  }

  // �������ŁA�u�y�[�X�g��ɃL�[�t���[���𖳂����Ȃ��v�������l������
  // �y�[�X�g�̃��X�g��T��
  // �擪�̃L�[�t���[�������y�[�X�g�֎~�ɂ���������`�F�b�N
  //  �@ �I���t���[�����ɃL�[������
  //  �A Dst�͈͂ɂ��̃V�F�C�v�̑S�ẴL�[�������Ă���

  for (int d = 0; d < m_data.size();) {
    // �y�[�X�g�̃Z���𔭌�
    if (pasteAvailability.at(d)) {
      QList<int> dstFrames;
      QList<int> srcIndices;
      while (1) {
        if (d >= m_data.size()) break;
        if (!pasteAvailability.at(d))  // �t���[���͈͂̒[���z�������
          break;
        dstFrames.push_back(m_data.at(d).pos.x() + baseFrame);
        srcIndices.push_back(d);
        d++;
      }

      // ���`��L�[
      if (doFormPaste) {
        bool keyFound = false;
        // �@ �I���t���[�����Ɍ`��L�[������� ���̍s�͖��Ȃ�
        // ����Corr�̃`�F�b�N��
        for (int s = 0; s < srcIndices.size(); s++) {
          if (!m_data.at(s).bpList.isEmpty()) {
            keyFound = true;
            break;
          }
        }
        // ���̏�����
        if (!keyFound) {
          int keyAmount     = 0;
          int firstKeyFrame = -1;
          // �A Dst�͈͂ɂ��̃V�F�C�v�̑S�ẴL�[�������Ă���
          for (auto dstFrame : dstFrames) {
            // �L�[�����J�E���g
            if (shape.shapePairP->isFormKey(dstFrame, shape.fromTo)) {
              keyAmount++;
              // �ŏ��̃t���[����ێ�
              if (firstKeyFrame < 0) firstKeyFrame = dstFrame;
            }
          }
          // ��������v������A����̓C�J���IFormKey�̍ŏ��̃t���[�����y�[�X�g�s�ɂ��Ďc��
          if (shape.shapePairP->getFormKeyFrameAmount(shape.fromTo) ==
              keyAmount) {
            // QPoint(firstKeyFrame,
            // currentDstRow)�̃Z���̃R�s�[�\�t���O��false�ɂ���
            int frameToBeKept  = firstKeyFrame;
            int copiedFramePos = frameToBeKept - baseFrame;
            for (int i = 0; i < m_data.size(); i++) {
              if (m_data.at(i).pos.x() == copiedFramePos) {
                pasteAvailability.replace(i, false);
                break;
              }
            }
          }
        }
      }

      // ���Ή��_�L�[
      if (doCorrPaste) {
        bool keyFound = false;
        // �@ �I���t���[�����ɑΉ��_�L�[������� ���̍s�͖��Ȃ�
        // ����Corr�̃`�F�b�N��
        for (int s = 0; s < srcIndices.size(); s++) {
          if (!m_data.at(s).cpList.isEmpty()) {
            keyFound = true;
            break;
          }
        }
        // ���̏�����
        if (!keyFound) {
          int keyAmount     = 0;
          int firstKeyFrame = -1;
          // �A Dst�͈͂ɂ��̃V�F�C�v�̑S�ẴL�[�������Ă���
          for (auto dstFrame : dstFrames) {
            // �L�[�����J�E���g
            if (shape.shapePairP->isCorrKey(dstFrame, shape.fromTo)) {
              keyAmount++;
              // �ŏ��̃t���[����ێ�
              if (firstKeyFrame < 0) firstKeyFrame = dstFrame;
            }
          }
          // ��������v������A����̓C�J���IFormKey�̍ŏ��̃t���[�����y�[�X�g�s�ɂ��Ďc��
          if (shape.shapePairP->getCorrKeyFrameAmount(shape.fromTo) ==
              keyAmount) {
            // QPoint(firstKeyFrame,
            // currentDstRow)�̃Z���̃R�s�[�\�t���O��false�ɂ���
            int frameToBeKept  = firstKeyFrame;
            int copiedFramePos = frameToBeKept - baseFrame;
            for (int i = 0; i < m_data.size(); i++) {
              if (m_data.at(i).pos.x() == copiedFramePos) {
                pasteAvailability.replace(i, false);
                break;
              }
            }
          }
        }
        //-----
      }
    } else
      d++;
  }

  // �y�[�X�g����
  for (int p = 0; p < pasteAvailability.size(); p++) {
    if (!pasteAvailability.at(p)) continue;

    int dstFrame = m_data.at(p).pos.x() + baseFrame;

    if (doFormPaste) {
      if (!m_data.at(p).bpList.isEmpty())
        shape.shapePairP->setFormKey(
            dstFrame, shape.fromTo,
            m_data.at(p).bpList);  // ���ɃL�[������ꍇ��replace�����
      else
        shape.shapePairP->removeFormKey(
            dstFrame, shape.fromTo);  // �L�[����Ȃ��ꍇ�͖��������

      int start, end;
      shape.shapePairP->getFormKeyRange(start, end, dstFrame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, project->getParentShape(shape.shapePairP));
    }
    if (doCorrPaste) {
      if (!m_data.at(p).cpList.isEmpty())
        shape.shapePairP->setCorrKey(
            dstFrame, shape.fromTo,
            m_data.at(p).cpList);  // ���ɃL�[������ꍇ��replace�����
      else
        shape.shapePairP->removeCorrKey(
            dstFrame, shape.fromTo);  // �L�[����Ȃ��ꍇ�͖��������

      int start, end;
      shape.shapePairP->getCorrKeyRange(start, end, dstFrame, shape.fromTo);
      IwTriangleCache::instance()->invalidateCache(
          start, end, project->getParentShape(shape.shapePairP));
    }
  }
}