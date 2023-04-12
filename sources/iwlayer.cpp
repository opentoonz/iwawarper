//---------------------------------------
// IwLayer
//---------------------------------------

#include "iwlayer.h"
#include "iwimagecache.h"
#include "iocommand.h"
#include "shapepair.h"

#include "iwapp.h"
#include "iwlayerhandle.h"
#include "iwprojecthandle.h"
#include "iwselectionhandle.h"

#include "iwselection.h"
#include "iwtimelineselection.h"
#include "iwtimelinekeyselection.h"
#include "iwtrianglecache.h"
#include "keydragtool.h"

#include <QVector>
#include <QPainter>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QMenu>

#include <iostream>

#include "iwproject.h"
#include "projectutils.h"

//---------------------------------------------------
IwLayer::IwLayer() : IwLayer(QString(""), Full, false, true, false, 0, 0) {}

//---------------------------------------------------
IwLayer::IwLayer(const QString& name, LayerVisibility visibleInViewer,
                 bool isExpanded, bool isVisibleInRender, bool isLocked,
                 int brightness, int contrast)
    : m_name(name)
    , m_isVisibleInViewer(visibleInViewer)
    , m_isExpanded(isExpanded)
    , m_isVisibleInRender(isVisibleInRender)
    , m_isLocked(isLocked)
    , m_brightness(brightness)
    , m_contrast(contrast) {
  IwProject* prj = IwApp::instance()->getCurrentProject()->getProject();
  if (!prj) return;

  if (m_name.isEmpty()) {
    int number = 1;
    while (1) {
      QString tmpName = QString("Layer%1").arg(QString::number(number));
      bool found      = false;
      for (int lay = 0; lay < prj->getLayerCount(); lay++) {
        if (tmpName == prj->getLayer(lay)->getName()) {
          found = true;
          break;
        }
      }

      if (!found) {
        setName(tmpName);
        break;
      } else
        number++;
    }
  }
  m_shapepairs.clear();
}

//---------------------------------------------------

IwLayer* IwLayer::clone() {
  QString cloneName = m_name + tr("_clone");
  IwLayer* ret =
      new IwLayer(cloneName, m_isVisibleInViewer, m_isExpanded,
                  m_isVisibleInRender, m_isLocked, m_brightness, m_contrast);

  // �t�H���_�̃��X�g
  for (auto folderDir : m_paths) {
    ret->getFolderPathIndex(folderDir.path());
  }
  // �t�H���_�C���f�b�N�X�A�t�@�C�������i�[����Ă��郊�X�g
  for (auto imagePath : m_images) {
    ret->insertPath(ret->getFrameLength(), imagePath);
  }

  // �V�F�C�v
  for (int s = 0; s < getShapePairCount(); s++) {
    ShapePair* shapePair = getShapePair(s);
    if (!shapePair) continue;
    ret->addShapePair(shapePair->clone());
  }

  return ret;
}

//---------------------------------------------------
// Returns file path at the frame.
// frame number starts from 0. Returns empty QString if ther is no image at the
// frame �t���[���ԍ�����i�[����Ă���t�@�C���p�X��Ԃ�
//(�t���[���͂O�X�^�[�g�B�����ꍇ�͋�String��Ԃ�)
QString IwLayer::getImageFilePath(int frame) {
  // �t�@�C�������X�g���͂ݏo���Ă������String��Ԃ�
  if (frame >= m_images.size() || frame < 0) return QString();

  QPair<int, QString> pathPair = m_images.at(frame);

  // �t�H���_�����X�g���t�H���_�C���f�b�N�X���͂ݏo���Ă������String��Ԃ�
  if (pathPair.first >= m_paths.size()) return QString();

  // �t�H���_���{�t�@�C������Ԃ��B
  return m_paths.at(pathPair.first).filePath(pathPair.second);
}

//---------------------------------------------------
// Returns file path at the frame, without the parent folder path.
// frame number starts from 0. Returns empty QString if ther is no image at the
// frame �t���[���ԍ�����i�[����Ă���t�@�C������Ԃ� Parent�t�H���_�����B
//(�t���[���͂O�X�^�[�g�B�����ꍇ�͋�String��Ԃ�)
QString IwLayer::getImageFileName(int frame) {
  // �t�@�C�������X�g���͂ݏo���Ă������String��Ԃ�
  if (frame < 0 || frame >= m_images.size()) return QString();

  // �t�@�C������Ԃ��B
  return m_images.at(frame).second;
}

//---------------------------------------------------
// Returns the parent folder path at the frame.
// frame number starts from 0. Returns empty QString if ther is no image at the
// frame �t���[���ԍ�����i�[����Ă���parent�t�H���_����Ԃ�
//(�t���[���͂O�X�^�[�g�B�����ꍇ�͋�String��Ԃ�)
QString IwLayer::getParentFolderName(int frame) {
  // �t�@�C�������X�g���͂ݏo���Ă������String��Ԃ�
  if (frame >= m_images.size()) return QString();

  QPair<int, QString> pathPair = m_images.at(frame);

  // �t�H���_�����X�g���t�H���_�C���f�b�N�X���͂ݏo���Ă������String��Ԃ�
  if (pathPair.first >= m_paths.size()) return QString();

  // �t�H���_����Ԃ��B
  return m_paths.at(pathPair.first).path();
}

//---------------------------------------------------
// get the folder path index, register if the path is not yet registered
// �f�B���N�g���p�X����́B
// �܂����X�g�ɖ����ꍇ�͒ǉ����ăC���f�b�N�X��Ԃ��B
int IwLayer::getFolderPathIndex(const QString& folderPath) {
  // �t�H���_�̃��X�g����Ȃ�ǉ����邾��
  if (m_paths.isEmpty()) {
    m_paths.append(QDir(folderPath));
    return 0;
  }

  for (int index = 0; index < m_paths.size(); index++) {
    QDir dir = m_paths.at(index);
    if (dir == QDir(folderPath)) return index;
  }

  m_paths.append(QDir(folderPath));
  return m_paths.size() - 1;
}
//---------------------------------------------------
// insert image path. (may need to emit a signal)
// �摜�p�X�̑}�� �� �V�O�i���͊O�Ŋe���G�~�b�g���邱��
//---------------------------------------------------
void IwLayer::insertPath(int index, QPair<int, QString> image) {
  m_images.insert(index, image);
}
//---------------------------------------------------
// delete image path. (may need to emit a signal)
// �摜�p�X����������B �� �V�O�i���͊O�Ŋe���G�~�b�g���邱��
//---------------------------------------------------
void IwLayer::deletePaths(int startIndex, int amount, bool skipPathCheck) {
  if (m_paths.isEmpty() || m_images.isEmpty()) return;

  // m_images��startIndex����amount�̖���������
  for (int i = 0; i < amount; i++) {
    // �i�O�̂��߁j��������ʒu��m_images�𒴂���������烋�[�v�𔲂���
    if (m_images.size() <= startIndex) break;
    // ����
    m_images.removeAt(startIndex);
  }

  // m_images����Ȃ�Am_paths���S��������return
  if (m_images.isEmpty()) {
    m_paths.clear();
    return;
  }

  if (skipPathCheck) return;

  // �����ŁA���Ɏg���Ă��Ȃ��t�@���_������ꍇ�́A��������
  // �g���Ă��Ȃ��t�H���_�C���f�b�N�X�̃��X�g�����
  QVector<int> unusedFolderIndeces;
  // m_paths�̃A�C�e���̐�����
  for (int p = 0; p < m_paths.size(); p++) {
    bool found = false;
    // m_images��T�����āA���̔ԍ�����������continue
    for (int i = 0; i < m_images.size(); i++) {
      if (m_images.at(i).first == p) {
        found = true;
        break;
      }
    }
    // ������΁A�g���Ă��Ȃ��t�H���_�C���f�b�N�X��ǉ�����
    if (!found) unusedFolderIndeces.append(p);
  }
  // �g���Ă��Ȃ��t�H���_�C���f�b�N�X����Ȃ�return
  if (unusedFolderIndeces.isEmpty()) return;

  // �V�����C���f�b�N�X�p��QMap�e�[�u�������
  QList<int> modifyPathIndexTable;

  // m_paths���������Ȃ���V�����C���f�b�N�X�̃e�[�u�������
  int modValue = 0;
  int oldSize  = m_paths.size();
  // m_paths�̊e�A�C�e���ɂ���
  for (int p = 0; p < oldSize; p++) {
    // p��unusedFolderIndeces�ɓ����Ă�����
    if (unusedFolderIndeces.contains(p)) {
      // m_paths����A�A�C�e��-modValue�Ԗڂ̃A�C�e��������
      m_paths.removeAt(unusedFolderIndeces.at(p) - modValue);
      // modifyPathIndexTable�ɂ͂Ƃ肠����0������
      modifyPathIndexTable.append(0);
      // modValue���C���N�������g
      modValue++;
    }
    // p��unusedFolderIndeces�ɓ����Ă��Ȃ�������
    //(�܂��g���Ă���t�H���_�C���f�b�N�X)
    else {
      // modifyPathIndexTable�ɁAp-modValue������
      modifyPathIndexTable.append(p - modValue);
    }
  }

  // m_images�̊e�t���[���ɂ���
  for (int i = 0; i < m_images.size(); i++) {
    // �V�����C���f�b�N�X���e�[�u���ɍ��킹�ĐU��Ȃ���
    m_images[i].first = modifyPathIndexTable.at(m_images.at(i).first);
  }
}

//---------------------------------------------------
// replace image path. (may need to emit a signal)
// �摜�p�X�̍����ւ� �� �V�O�i���͊O�Ŋe���G�~�b�g���邱��
//---------------------------------------------------
void IwLayer::replacePath(int index, QPair<int, QString> image) {
  if (index < 0 || index >= m_images.size()) return;
  m_images.replace(index, image);

  // replacePath�̌��ʌ㔼�t���[���ɋ�R�}���ł����Ƃ��Adelete����
  if (index == m_images.size() - 1 && image.second.isEmpty()) {
    int amount = 0;
    int i;
    for (i = m_images.size() - 1; i >= 0; i--) {
      if (!m_images.at(i).second.isEmpty()) break;
      amount++;
    }
    deletePaths(i + 1, amount);
  }
}

//---------------------------------------------------
// �L���b�V��������΂����Ԃ��B
// ������Ή摜�����[�h���ăL���b�V���Ɋi�[����
//---------------------------------------------------
TImageP IwLayer::getImage(int index) {
  // �p�X���擾
  QString path = getImageFilePath(index);
  // �p�X����Ȃ��Img��Ԃ�
  if (path.isEmpty()) return TImageP();

  // �L���b�V������Ă�����A�����Ԃ�
  if (IwImageCache::instance()->isCached(path))
    return IwImageCache::instance()->get(path);

  TImageP img = IoCmd::loadImage(path);

  // �摜�����[�h�ł�����A�L���b�V���ɓ����
  if (img.getPointer()) {
    std::cout << "load success" << std::endl;
    IwImageCache::instance()->add(path, img);
  }
  return img;
}

//---------------------------------------------------

int IwLayer::getShapePairCount(bool skipDeletedShape) {
  if (!skipDeletedShape) return m_shapepairs.count();

  int count = 0;
  for (const auto shapepair : m_shapepairs) {
    if (shapepair) count++;
  }
  return count;
}

//---------------------------------------------------
// �V�F�C�v��Ԃ�
//---------------------------------------------------
ShapePair* IwLayer::getShapePair(int shapePairIndex) {
  if (shapePairIndex >= m_shapepairs.size()) return 0;
  return m_shapepairs.at(shapePairIndex);
}

//---------------------------------------------------
// �V�F�C�v�̒ǉ��Bindex���w�肵�Ă��Ȃ���ΐV���ɒǉ�
//---------------------------------------------------
void IwLayer::addShapePair(ShapePair* shapePair, int index) {
  // m_shapes�ɂ��ł�index�̃A�C�e�����L��ꍇ�A�㏑������
  if (index >= 0 && index < m_shapepairs.size()) {
    ShapePair* shapeToBeReplaced = m_shapepairs.at(index);
    if (shapeToBeReplaced) {
      ShapePair* oldParentShape = getParentShape(shapeToBeReplaced);
      if (oldParentShape)
        IwTriangleCache::instance()->invalidateShapeCache(oldParentShape);
    }

    m_shapepairs.replace(index, shapePair);
  } else
    m_shapepairs.append(shapePair);

  ShapePair* newParentShape = getParentShape(shapePair);
  if (newParentShape)
    IwTriangleCache::instance()->invalidateShapeCache(newParentShape);
}

//---------------------------------------------------
// �V�F�C�v�̍폜
//---------------------------------------------------
void IwLayer::deleteShapePair(int shapePairIndex, bool doRemove) {
  assert(0 <= shapePairIndex && shapePairIndex < m_shapepairs.count());

  // Triangle Cache�̍폜
  ShapePair* shapeToBeRemoved = m_shapepairs.at(shapePairIndex);
  ShapePair* childShape       = nullptr;
  if (shapeToBeRemoved) {
    // �֘A����Shape��invalidate
    IwTriangleCache::instance()->invalidateShapeCache(
        getParentShape(shapeToBeRemoved));
    // parent�V�F�C�v���폜�����ꍇ�A�ЂƂ�̃V�F�C�v���q�������󂯂�
    // �q�V�F�C�v�����邩�ǂ����̊m�F
    if (shapeToBeRemoved->isParent())
      for (int index = shapePairIndex + 1; index < m_shapepairs.count();
           index++) {
        ShapePair* shape = m_shapepairs.at(index);
        if (!shape) continue;
        if (shape->isParent()) break;
        childShape = shape;
        break;
      }
  }

  if (doRemove)
    // �{���ɒ��g������
    m_shapepairs.removeAt(shapePairIndex);
  else
    // index�̒��g��0�ɒu��������
    m_shapepairs.replace(shapePairIndex, 0);

  // Triangle Cache�̍폜 ���̂Q
  if (childShape) {
    IwTriangleCache::instance()->invalidateShapeCache(
        getParentShape(childShape));
  }
}

//---------------------------------------------------
// �V�F�C�v�̍폜
//---------------------------------------------------
void IwLayer::deleteShapePair(ShapePair* shapePair, bool doRemove) {
  if (!m_shapepairs.contains(shapePair)) return;
  int shapePairIndex = m_shapepairs.indexOf(shapePair);

  deleteShapePair(shapePairIndex, doRemove);
}

//---------------------------------------------------
// �V�F�C�v���X�g�̊ہX�����ւ�.�@��舵������
//---------------------------------------------------
void IwLayer::replaceShapePairs(QList<ShapePair*> shapePairs) {
  m_shapepairs = shapePairs;

  IwTriangleCache::instance()->invalidateAllCache();
}

//---------------------------------------------------
// �W�J���Ă��邩�ǂ����𓥂܂��Ē��̃V�F�C�v�̓W�J�����擾���ĕ\���s����Ԃ�
//---------------------------------------------------

int IwLayer::getRowCount() {
  if (!m_isExpanded) return 1;

  int rowCount = 1;
  for (int sp = 0; sp < m_shapepairs.size(); sp++) {
    ShapePair* pair = m_shapepairs.at(sp);
    if (pair && pair->isAtLeastOneShapeExpanded())
      rowCount += pair->getRowCount();
  }

  return rowCount;
}

//---------------------------------------------------
// �`��(�w�b�_����)
//---------------------------------------------------

void IwLayer::drawTimeLineHead(QPainter& p, int vpos, int width, int rowHeight,
                               int& currentRow, int mouseOverRow,
                               int mouseHPos) {
  bool isHighlightRow =
      (mouseOverRow == currentRow) && 0 < mouseHPos && mouseHPos <= width;

  enum MouseOnButton {
    None,
    Expand,
    Visible,
    Render,
    Lock
  } mouseOnButton = None;

  if (isHighlightRow) {
    mouseOnButton = (mouseHPos < 0)    ? None
                    : (mouseHPos < 19) ? Expand
                    : (mouseHPos < 39) ? Visible
                    : (mouseHPos < 59) ? Render
                    : (mouseHPos < 79) ? Lock
                                       : None;
  }

  bool isCurrent = (IwApp::instance()->getCurrentLayer()->getLayer() == this);

  int currentVpos = vpos;
  // �܂��͎������g
  QRect rowRect(0, vpos, width - 1, rowHeight);

  auto marginFor = [&](int height) { return (rowHeight - height) / 2; };

  p.setPen(Qt::black);
  if (m_isLocked) {
    p.setBrush(QColor(80, 80, 80));
    p.drawRect(rowRect);
  } else if (isCurrent) {
    p.setBrush(QColor(83, 133, 166));
    p.drawRect(rowRect);
  } else {
    p.setBrush(QColor(83, 99, 119));
    p.drawRect(rowRect);
    // p.setBrush(QBrush(rowGrad));
    if (isHighlightRow && mouseOnButton == None) {
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(255, 255, 220, 50));
      p.drawRect(rowRect);
    }
  }

  bool containsVisibleShape = false;
  for (int sp = 0; sp < m_shapepairs.size(); sp++) {
    ShapePair* shapePair = m_shapepairs.at(sp);
    if (shapePair && shapePair->isAtLeastOneShapeExpanded()) {
      containsVisibleShape = true;
      break;
    }
  }

  if (containsVisibleShape) {
    // Expand�{�^����Ƀ}�E�X���������炻�����n�C���C�g
    if (mouseOnButton == Expand) {
      p.setBrush(QColor(255, 255, 220, 100));
      p.setPen(Qt::NoPen);
      p.drawRect(2, vpos + marginFor(15), 15, 15);
    }
    p.drawPixmap(5, vpos + marginFor(9), 9, 9,
                 (m_isExpanded) ? QPixmap(":Resources/tl_expand_on.svg")
                                : QPixmap(":Resources/tl_expand_off.svg"));
  }

  // �\���^��\���{�^��
  p.drawPixmap(22, vpos + marginFor(15), 15, 15,
               (m_isVisibleInViewer == Invisible)
                   ? QPixmap(":Resources/visible_off.svg")
                   : ((m_isVisibleInViewer == HalfVisible)
                          ? QPixmap(":Resources/visible_half.svg")
                          : QPixmap(":Resources/visible_on.svg")));
  if (m_isLocked) {
    p.fillRect(QRect(19, vpos + marginFor(19), 19, 19),
               QColor(80, 80, 80, 128));
  } else if (mouseOnButton == Visible) {
    p.fillRect(QRect(19, vpos + marginFor(19), 19, 19),
               QColor(255, 255, 220, 100));
  }

  p.drawPixmap(42, vpos + marginFor(15), 15, 15,
               (m_isVisibleInRender) ? QPixmap(":Resources/render_on.svg")
                                     : QPixmap(":Resources/render_off.svg"));
  if (m_isLocked) {
    p.fillRect(QRect(39, vpos + marginFor(19), 19, 19),
               QColor(80, 80, 80, 128));
  } else if (mouseOnButton == Render) {
    p.fillRect(QRect(39, vpos + marginFor(19), 19, 19),
               QColor(255, 255, 220, 100));
  }

  p.drawPixmap(60, vpos + marginFor(18), 18, 18,
               (m_isLocked) ? QPixmap(":Resources/lock_on.svg")
                            : QPixmap(":Resources/lock.svg"));
  if (mouseOnButton == Lock) {
    p.setBrush(QColor(255, 255, 220, 100));
    p.setPen(Qt::NoPen);
    p.drawRect(59, vpos + marginFor(19), 19, 19);
  }

  p.setBrush(Qt::NoBrush);
  if (isCurrent)
    p.setPen(Qt::yellow);
  else if (m_isLocked)
    p.setPen(Qt::lightGray);
  else
    p.setPen(Qt::white);
  p.drawText(rowRect.adjusted(80, 0, 0, 0), Qt::AlignVCenter, m_name);

  currentRow++;

  if (m_isExpanded) {
    currentVpos += rowHeight;
    ShapePair* parentShape = nullptr;
    // �V�F�C�v
    for (int sp = 0; sp < m_shapepairs.size(); sp++) {
      ShapePair* shapePair = m_shapepairs.at(sp);
      if (shapePair && shapePair->isAtLeastOneShapeExpanded()) {
        if (shapePair->isParent()) parentShape = shapePair;
        ShapePair* nextShape = nullptr;
        for (int next_sp = sp + 1; next_sp < m_shapepairs.size(); next_sp++) {
          if (m_shapepairs.at(next_sp) &&
              m_shapepairs.at(next_sp)->isAtLeastOneShapeExpanded()) {
            nextShape = m_shapepairs.at(next_sp);
            break;
          }
        }
        // currentVpos�͊֐�drawTimeLineHead���ŕ`���Ȃ���C���N�������g
        shapePair->drawTimeLineHead(p, currentVpos, width, rowHeight,
                                    currentRow, mouseOverRow, mouseHPos,
                                    parentShape, nextShape, m_isLocked);
      }
    }
  }

  if (isCurrent) {
    p.setPen(Qt::lightGray);
    p.setBrush(Qt::NoBrush);
    p.drawRect(rowRect);
  }
}
//---------------------------------------------------
// �`��(�{��)
//---------------------------------------------------

void IwLayer::drawTimeLine(QPainter& p, int vpos, int width, int fromFrame,
                           int toFrame, int frameWidth, int rowHeight,
                           int& currentRow, int mouseOverRow,
                           double mouseOverFrameD,
                           IwTimeLineSelection* selection) {
  // ���̃��[�����J�����g�ŁA����SequenceSelection���J�����g���ǂ���
  bool isCurrent = (IwApp::instance()->getCurrentLayer()->getLayer() == this) &&
                   (IwSelection::getCurrent() == selection);
  bool isHighlightRow = (mouseOverRow == currentRow);

  int currentVpos = vpos;
  // 1�t���[�����̑f�ރo�[
  QRect frameRect(0, vpos, frameWidth, rowHeight);
  QRect sukimaRect(0, vpos, 3, rowHeight);

  QLinearGradient rowGrad(QPointF(0.0, 0.0), QPointF(0.0, (double)rowHeight));
  rowGrad.setColorAt(0.0, QColor(130, 223, 237, 255));
  rowGrad.setColorAt(1.0, QColor(103, 167, 196, 255));
  rowGrad.setSpread(QGradient::RepeatSpread);

  bool sameAsPrevious = false;
  int sameAsNextCount = 0;

  // �e�t���[���ŁA���g�̂�����̂����`��
  for (int f = fromFrame; f <= toFrame; f++) {
    QRect tmpRect = frameRect.translated(frameWidth * f, 0);

    if (f >= m_images.size()) {
      p.setPen(Qt::black);
      p.setBrush(Qt::NoBrush);
      p.drawRect(tmpRect);
      continue;
    }

    sameAsPrevious = sameAsNextCount > 0;
    sameAsNextCount--;
    // �����t�@�C�������A�����Ă��鐔���Čv�Z
    if (sameAsNextCount < 0) {
      sameAsNextCount = 0;
      while (1) {
        if (f + sameAsNextCount >= m_images.size()) break;
        if (getImageFileName(f) != getImageFileName(f + sameAsNextCount + 1))
          break;
        sameAsNextCount++;
      }
    }

    if (!sameAsPrevious) {
      bool isEmpty = m_images.at(f).second.isEmpty();

      if (!isEmpty) {
        // �x�[�X�F
        if (m_isLocked)
          p.setBrush(QColor(80, 80, 80));
        else
          p.setBrush(QColor(83, 99, 119));
        p.setPen(Qt::black);
        p.drawRect(
            tmpRect.adjusted(0, 0, tmpRect.width() * sameAsNextCount, 0));
      }

      for (int tmpF = f; tmpF <= f + sameAsNextCount; tmpF++) {
        QRect eachRect   = frameRect.translated(frameWidth * tmpF, 0);
        QRect eachSukima = sukimaRect.translated(frameWidth * tmpF, 0);
        if (isEmpty) {
          p.setPen(Qt::black);
          p.setBrush(QColor(255, 255, 255, 5));
          p.drawRect(eachRect);
        }
        if (m_isLocked) continue;
        if (isHighlightRow && (int)mouseOverFrameD == tmpF) {
          p.setBrush(QColor(255, 255, 220, 50));
          p.setPen(Qt::NoPen);
          p.drawRect(eachRect);
        }
        // �t���[�����I������Ă�����F�ς���
        if (isCurrent && selection->isFrameSelected(tmpF)) {
          p.setPen(Qt::NoPen);
          p.setBrush(QColor(100, 153, 176, 128));
          p.drawRect(eachRect.adjusted(3, 0, 0, 0));
        }
        // ���Ԃ��I������Ă�����F�ς���
        if (tmpF != f && isCurrent && selection->isBorderSelected(tmpF)) {
          p.setPen(Qt::NoPen);
          p.setBrush(QColor(100, 153, 176, 180));
          p.drawRect(eachSukima);
        }
      }

      // �t�@�C�����̕`��
      p.setPen(m_isLocked ? Qt::lightGray : Qt::white);
      if (sameAsNextCount > 0)
        p.drawText(
            tmpRect.adjusted(7, 0, tmpRect.width() * sameAsNextCount - 3, 0),
            Qt::AlignLeft | Qt::AlignVCenter, getImageFileName(f));
      else
        p.drawText(tmpRect.adjusted(3, 0, 0, 0), Qt::AlignCenter,
                   getImageFileName(f));
    }

    // ���ԃ{�b�N�X�̕`��
    QRect tmpSukima = sukimaRect.translated(frameWidth * f, 0);
    p.setPen(Qt::black);
    // ���Ԃ��I������Ă�����F�ς���
    if (isCurrent && selection->isBorderSelected(f) && !m_isLocked)
      p.setBrush(Qt::white);
    else
      p.setBrush(Qt::darkGray);
    if (sameAsPrevious)
      p.drawRect(tmpSukima.adjusted(0, rowHeight * 0.8, 0, 0));
    else
      p.drawRect(tmpSukima);
  }

  // �Ō�̌��ԕ�����`��
  p.setPen(Qt::black);
  // �t���[�����I������Ă�����F�ς���
  if (isCurrent && selection->isBorderSelected(getFrameLength()) && !m_isLocked)
    p.setBrush(Qt::white);
  else
    p.setBrush(Qt::darkGray);
  p.drawRect(sukimaRect.translated(frameWidth * getFrameLength(), 0));

  currentRow++;

  if (!m_isExpanded) return;

  currentVpos += rowHeight;
  // �V�F�C�v
  for (int sp = 0; sp < m_shapepairs.size(); sp++) {
    ShapePair* shapePair = m_shapepairs.at(sp);
    if (shapePair && shapePair->isAtLeastOneShapeExpanded()) {
      // currentVpos�͊֐�drawTimeLineHead���ŕ`���Ȃ���C���N�������g
      shapePair->drawTimeLine(p, currentVpos, width, fromFrame, toFrame,
                              frameWidth, rowHeight, currentRow, mouseOverRow,
                              mouseOverFrameD, m_isLocked, m_isVisibleInRender);
    }
  }
}

//---------------------------------------------------
// ���N���b�N���̏��� row�͂��̃��C�����ŏォ��O�Ŏn�܂�s���B
// �ĕ`�悪�K�v�Ȃ�true��Ԃ�
//---------------------------------------------------

bool IwLayer::onLeftClick(int row, int mouseHPos, QMouseEvent* e) {
  if (row >= getRowCount()) return false;

  if (row == 0) {
    if (mouseHPos < 19) {
      // expand button is not visible when no shapes
      if (getShapePairCount(true) == 0) return false;
      m_isExpanded = !m_isExpanded;
      return true;
    } else if (mouseHPos < 79) {
      ProjectUtils::LayerPropertyType type =
          (mouseHPos < 39)   ? ProjectUtils::Visibility
          : (mouseHPos < 59) ? ProjectUtils::Render
                             : ProjectUtils::Lock;

      if (m_isLocked && type != ProjectUtils::Lock) return false;

      ProjectUtils::toggleLayerProperty(this, type);

      return true;
    } else {
      // �J�����g�̃��C��������ɂ���
      bool ret = IwApp::instance()->getCurrentLayer()->setLayer(this);

      if (m_isLocked) return ret;

      // �����N���b�N������A���̃��C���̃^�C�����C����S�I��������
      // IwTimeLineSelection���J�����g��
      IwTimeLineSelection::instance()->makeCurrent();
      ret = ret & IwTimeLineSelection::instance()->selectFrames(
                      0, getFrameLength() - 1, true);
      if (ret)
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return ret;
    }
  }

  // �J�����g�̃��C��������ɂ���
  bool ret = IwApp::instance()->getCurrentLayer()->setLayer(this);

  // �V�F�C�v�����̃N���b�N���얳����
  if (m_isLocked) return ret;

  int currentRow = 1;
  for (int sp = 0; sp < m_shapepairs.size(); sp++) {
    ShapePair* shapePair = m_shapepairs.at(sp);
    if (!shapePair || !shapePair->isAtLeastOneShapeExpanded()) continue;
    if (row < currentRow + shapePair->getRowCount()) {
      return ret | shapePair->onLeftClick(row - currentRow, mouseHPos, e);
    }
    currentRow += shapePair->getRowCount();
  }
  return ret;
}
//---------------------------------------------------
// �_�u���N���b�N���A���O������������true�B�V�F�C�v���ǂ���������
//---------------------------------------------------
bool IwLayer::onDoubleClick(int row, int mouseHPos, ShapePair** shapeP) {
  if (row >= getRowCount()) return false;
  // ���b�N����Ă����疳����
  if (m_isLocked) return false;

  if (row == 0) {
    if (mouseHPos >= 80)
      return true;
    else
      return false;
  } else {
    int currentRow = 1;
    for (int sp = 0; sp < m_shapepairs.size(); sp++) {
      ShapePair* shapePair = m_shapepairs.at(sp);
      if (!shapePair || !shapePair->isAtLeastOneShapeExpanded()) continue;
      if (row < currentRow + shapePair->getRowCount()) {
        if (row == currentRow) {
          (*shapeP) = shapePair;
          return true;
        } else
          return false;
      }
      currentRow += shapePair->getRowCount();
    }
    return false;
  }
}

//---------------------------------------------------
// ���̃A�C�e�����X�g�E�N���b�N���̑���
//---------------------------------------------------
void IwLayer::onRightClick(int row, QMenu& menu) {
  if (row >= getRowCount()) return;

  if (row == 0)
    return;
  else {
    if (m_shapepairs.size() <= 1) return;

    int currentRow = 1;
    int aboveIndex = -1;  // �ЂƂ�̕\���V�F�C�v�̃C���f�b�N�X
    for (int sp = 0; sp < m_shapepairs.size(); sp++) {
      ShapePair* shapePair = m_shapepairs.at(sp);
      if (!shapePair || !shapePair->isAtLeastOneShapeExpanded()) continue;
      if (row < currentRow + shapePair->getRowCount()) {
        menu.addSeparator();
        if (sp > 0) {
          QAction* upAct = new QAction(tr("Move Shape Upward"), this);
          upAct->setData(sp);  // �V�F�C�v�C���f�b�N�X���A�N�V�����ɓo�^
          connect(upAct, SIGNAL(triggered()), this, SLOT(onMoveShapeUpward()));
          menu.addAction(upAct);

          if (aboveIndex >= 0) {
            QAction* aboveAct =
                new QAction(tr("Move Above the Upper Expanded Shape"), this);
            aboveAct->setData(QPoint(
                sp, aboveIndex));  // �V�F�C�v�C���f�b�N�X���A�N�V�����ɓo�^
            connect(aboveAct, SIGNAL(triggered()), this,
                    SLOT(onMoveAboveUpperShape()));
            menu.addAction(aboveAct);
          }
        }
        if (sp < m_shapepairs.size() - 1) {
          QAction* downAct = new QAction(tr("Move Shape Downward"), this);
          downAct->setData(sp);  // �V�F�C�v�C���f�b�N�X���A�N�V�����ɓo�^
          connect(downAct, SIGNAL(triggered()), this,
                  SLOT(onMoveShapeDownward()));
          menu.addAction(downAct);

          // ����T��
          int belowIndex = -1;  // �ЂƂ��̕\���V�F�C�v�̃C���f�b�N�X
          for (int bsp = sp + 1; bsp < m_shapepairs.size(); bsp++) {
            ShapePair* belowShapePair = m_shapepairs.at(bsp);
            if (!belowShapePair || !belowShapePair->isAtLeastOneShapeExpanded())
              continue;
            belowIndex = bsp;
            break;
          }

          if (belowIndex >= 0) {
            QAction* belowAct =
                new QAction(tr("Move Below the Lower Expanded Shape"), this);
            belowAct->setData(QPoint(
                sp, belowIndex));  // �V�F�C�v�C���f�b�N�X���A�N�V�����ɓo�^
            connect(belowAct, SIGNAL(triggered()), this,
                    SLOT(onMoveBelowLowerShape()));
            menu.addAction(belowAct);
          }
        }

        //---- �V�F�C�v�̐e�q��ύX����
        QString str =
            (shapePair->isParent()) ? tr("Set to Child") : tr("Set to Parent");
        QAction* parentChildAct = new QAction(str, this);
        parentChildAct->setData(sp);  // �V�F�C�v�C���f�b�N�X���A�N�V�����ɓo�^
        connect(parentChildAct, SIGNAL(triggered()), this,
                SLOT(onSwitchParentChild()));
        menu.addAction(parentChildAct);
        //----

        return;
      }
      currentRow += shapePair->getRowCount();
      aboveIndex = sp;
    }
  }
}

//---------------------------------------------------
// �^�C�����C���㍶�N���b�N���̏���
//---------------------------------------------------
bool IwLayer::onTimeLineClick(int row,  // row��0����n�܂�N���b�N���ꂽ�s��
                              double frameD, bool controlPressed,
                              bool shiftPressed, Qt::MouseButton button) {
  if (row >= getRowCount()) return false;
  // ���C���̍s
  if (row <= 0) return false;  // ���C���̍s�̏�����TimeLineWindow���ōς܂���

  if (m_isLocked) return false;  // ���C�������b�N����Ă����瑀�얳���B
  // �ȉ��A�V�F�C�v���̓L�[�t���[���̍s���N���b�N���ꂽ�ꍇ
  int currentRow = 1;
  for (int sp = 0; sp < m_shapepairs.size(); sp++) {
    ShapePair* shapePair = m_shapepairs.at(sp);
    if (!shapePair || !shapePair->isAtLeastOneShapeExpanded()) continue;

    // �����̃V�F�C�v���N���b�N���ꂽ
    if (row < currentRow + shapePair->getRowCount()) {
      // �V�F�C�v�̒��̉��s�ڂ��N���b�N���ꂽ��
      int rowInShape = row - currentRow;

      return shapePair->onTimeLineClick(rowInShape, frameD, controlPressed,
                                        shiftPressed, button);
    }
    currentRow += shapePair->getRowCount();
  }
  return false;
}

//---------------------------------------------------
// �}�E�X�z�o�[��
//---------------------------------------------------

bool IwLayer::onMouseMove(int frameIndex, bool isBorder) {
  // �Ƃ肠�����{�[�_�[�ȊO�͉������Ȃ�
  if (!isBorder) return false;
  if (frameIndex <= 0) return false;
  if (m_isLocked) return false;
  // �摜�V�[�P���X�̉E�[�����L���ɂ���
  if (getImageFileName(frameIndex) == getImageFileName(frameIndex - 1))
    return false;

  return true;
}

//---------------------------------------------------
// �V�F�C�v�̖��O��Ԃ�
//---------------------------------------------------

int IwLayer::getNameFromShapePair(OneShape shape) {
  int index = m_shapepairs.indexOf(shape.shapePairP);

  // ����������Ȃ�������0��Ԃ�
  if (index == -1) return 0;

  // ����������A�`��p�̃V�F�C�vID�ɕϊ�
  //(�C���f�b�N�X�ɂP������FromTo�t���O��1�������ɂS���̂O������)
  return (index + 1) * 100000 + shape.fromTo * 10000;
}
//---------------------------------------------------
// ���O����V�F�C�v��Ԃ�
//---------------------------------------------------
OneShape IwLayer::getShapePairFromName(int name) {
  OneShape ret(0, 0);
  // �V�F�C�v��������Ζ���
  if (m_shapepairs.isEmpty()) return ret;
  // �����A10000��菬�����ꍇ�̓V�F�C�v�͂��߂Ă��Ȃ�
  if (name < 10000) return ret;
  // �C���f�b�N�X�𓾂�
  int index = (int)(name / 100000) - 1;
  if (index < m_shapepairs.count()) ret.shapePairP = m_shapepairs.at(index);
  // FromTo�𓾂�
  ret.fromTo = (int)((name % 100000) / 10000);

  // �C���f�b�N�X����V�F�C�v��Ԃ��B������΂O��Ԃ�
  return ret;
}

//---------------------------------------------------
// ���̃��C���Ɉ����̃V�F�C�v�������Ă�����true
//---------------------------------------------------
bool IwLayer::contains(ShapePair* shape) {
  if (!shape) return false;

  // ����������true
  return m_shapepairs.contains(shape);
}
//---------------------------------------------------
// �V�F�C�v����C���f�b�N�X��Ԃ� �������-1
//---------------------------------------------------
int IwLayer::getIndexFromShapePair(const ShapePair* shapePair) const {
  return m_shapepairs.indexOf((ShapePair*)shapePair);
}

//---------------------------------------------------
// Undo����p�B�t���[���̃f�[�^�����̂܂ܕԂ�
//---------------------------------------------------
QPair<int, QString> IwLayer::getPathData(int frame) {
  if (frame < 0 || frame >= m_images.size()) return QPair<int, QString>(0, "");
  return m_images.at(frame);
}

//---------------------------------------------------
// ������Parent�Ȃ玩����Ԃ��B�q�Ȃ�eShape�̃|�C���^��Ԃ�
//---------------------------------------------------

ShapePair* IwLayer::getParentShape(ShapePair* shape) {
  if (!shape || !contains(shape)) return nullptr;
  if (shape->isParent()) return shape;
  for (int s = m_shapepairs.indexOf(shape) - 1; s >= 0; s--) {
    if (!m_shapepairs[s]) continue;
    if (m_shapepairs[s]->isParent()) return m_shapepairs[s];
  }
  return nullptr;
}

//---------------------------------------------------

void IwLayer::onMoveShapeUpward() {
  int shapeIndex = qobject_cast<QAction*>(sender())->data().toInt();
  if (shapeIndex <= 0 || shapeIndex >= m_shapepairs.size()) return;
  ProjectUtils::SwapShapes(this, shapeIndex - 1, shapeIndex);
}

void IwLayer::onMoveShapeDownward() {
  int shapeIndex = qobject_cast<QAction*>(sender())->data().toInt();
  if (shapeIndex < 0 || shapeIndex >= m_shapepairs.size() - 1) return;
  ProjectUtils::SwapShapes(this, shapeIndex, shapeIndex + 1);
}

void IwLayer::onMoveAboveUpperShape() {
  QPoint data    = qobject_cast<QAction*>(sender())->data().toPoint();
  int shapeIndex = data.x();
  if (shapeIndex <= 0 || shapeIndex >= m_shapepairs.size()) return;
  int dstIndex = data.y();
  if (dstIndex <= 0 || dstIndex >= m_shapepairs.size()) return;

  ProjectUtils::MoveShape(this, shapeIndex, dstIndex);
}

void IwLayer::onMoveBelowLowerShape() {
  QPoint data    = qobject_cast<QAction*>(sender())->data().toPoint();
  int shapeIndex = data.x();
  if (shapeIndex <= 0 || shapeIndex >= m_shapepairs.size()) return;
  int dstIndex = data.y();
  if (dstIndex <= 0 || dstIndex >= m_shapepairs.size()) return;

  ProjectUtils::MoveShape(this, shapeIndex, dstIndex);
}
//---------------------------------------------------
void IwLayer::onSwitchParentChild() {
  int shapeIndex = qobject_cast<QAction*>(sender())->data().toInt();
  if (shapeIndex <= 0 || shapeIndex >= m_shapepairs.size()) return;
  ProjectUtils::switchParentChild(m_shapepairs.at(shapeIndex));
}

//---------------------------------------------------
//---------------------------------------------------
// �Z�[�u/���[�h
//---------------------------------------------------

void IwLayer::saveData(QXmlStreamWriter& writer) {
  // ���O
  writer.writeTextElement("name", m_name);
  // �\����\��
  writer.writeTextElement("isVisibleInViewer",
                          QString::number(m_isVisibleInViewer));
  writer.writeTextElement("isVisibleInRender",
                          (m_isVisibleInRender) ? "1" : "0");
  // ���b�N
  writer.writeTextElement("locked", (m_isLocked) ? "True" : "False");

  // ���C���[�I�v�V����
  writer.writeStartElement("LayerOptions");
  writer.writeTextElement("brightness", QString::number(m_brightness));
  writer.writeTextElement("contrast", QString::number(m_contrast));
  writer.writeEndElement();

  // �t�H���_���X�g
  writer.writeStartElement("Paths");
  for (int p = 0; p < m_paths.size(); p++) {
    writer.writeTextElement("path", m_paths.at(p).absolutePath());
  }
  writer.writeEndElement();

  // �t���[���̃��X�g
  writer.writeStartElement("Images");
  if (!m_images.isEmpty()) {
    int count           = 0;
    int tmpFolderIndex  = m_images.at(0).first;
    QString tmpFileName = m_images.at(0).second;
    for (int i = 0; i < m_images.size(); i++) {
      int newIndex    = m_images.at(i).first;
      QString newName = m_images.at(i).second;
      if (newIndex == tmpFolderIndex && newName == tmpFileName)
        count++;
      else {
        writer.writeEmptyElement("image");
        writer.writeAttribute("pathIndex", QString::number(tmpFolderIndex));
        writer.writeAttribute("fileName", tmpFileName);
        writer.writeAttribute("length", QString::number(count));
        tmpFolderIndex = newIndex;
        tmpFileName    = newName;
        count          = 1;
      }

      if (i == m_images.size() - 1) {
        writer.writeEmptyElement("image");
        writer.writeAttribute("pathIndex", QString::number(tmpFolderIndex));
        writer.writeAttribute("fileName", tmpFileName);
        writer.writeAttribute("length", QString::number(count));
      }
    }
  }
  writer.writeEndElement();

  // �V�F�C�v�y�A
  writer.writeComment("ShapePairs");
  writer.writeStartElement("ShapePairs");

  for (int sp = 0; sp < m_shapepairs.size(); sp++) {
    ShapePair* shapePair = m_shapepairs.at(sp);
    if (!shapePair) continue;

    writer.writeStartElement("ShapePair");

    shapePair->saveData(writer);

    writer.writeEndElement();
  }
  writer.writeEndElement();
}

//---------------------------------------------------

void IwLayer::loadData(QXmlStreamReader& reader) {
  while (reader.readNextStartElement()) {
    // ���O
    if (reader.name() == "name") {
      m_name = reader.readElementText();
    } else if (reader.name() == "isVisibleInViewer") {
      m_isVisibleInViewer = (LayerVisibility)(reader.readElementText().toInt());
    } else if (reader.name() == "isVisibleInRender") {
      m_isVisibleInRender = reader.readElementText().toInt() != 0;
    } else if (reader.name() == "locked")
      m_isLocked = (reader.readElementText() == "True") ? true : false;

    // ���C���[�I�v�V����
    else if (reader.name() == "LayerOptions") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "brightness")
          m_brightness = reader.readElementText().toInt();
        else if (reader.name() == "contrast")
          m_contrast = reader.readElementText().toInt();
        else
          reader.skipCurrentElement();
      }
    }

    // �t�H���_���X�g
    else if (reader.name() == "Paths") {
      if (!m_paths.isEmpty()) m_paths.clear();

      while (reader.readNextStartElement()) {
        if (reader.name() == "path") {
          m_paths.append(QDir(reader.readElementText()));
        } else
          reader.skipCurrentElement();
      }
    }
    // �t���[���̃��X�g
    else if (reader.name() == "Images") {
      if (!m_images.isEmpty()) m_images.clear();

      while (reader.readNextStartElement()) {
        if (reader.name() == "image") {
          int pathIndex =
              reader.attributes().value("pathIndex").toString().toInt();
          QString fileName = reader.attributes().value("fileName").toString();
          int length = reader.attributes().value("length").toString().toInt();

          // �f�[�^�ɓ����
          for (int f = 0; f < length; f++)
            m_images.append(QPair<int, QString>(pathIndex, fileName));
          reader.skipCurrentElement();
        } else
          reader.skipCurrentElement();
      }

    }
    // �V�F�C�v�y�A
    else if (reader.name() == "ShapePairs") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "ShapePair") {
          ShapePair* shapePair = new ShapePair();

          shapePair->loadData(reader);

          // �v���W�F�N�g�ɒǉ�
          addShapePair(shapePair);
        } else
          reader.skipCurrentElement();
        std::cout << "End ShapePairs - Line:" << reader.lineNumber()
                  << "  Name:" << reader.name().toString().toStdString()
                  << std::endl;
      }
    } else
      reader.skipCurrentElement();
  }
}
