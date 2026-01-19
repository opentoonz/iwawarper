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

  // フォルダのリスト
  for (auto folderDir : m_paths) {
    ret->getFolderPathIndex(folderDir.path());
  }
  // フォルダインデックス、ファイル名が格納されているリスト
  for (auto imagePath : m_images) {
    ret->insertPath(ret->getFrameLength(), imagePath);
  }

  // シェイプ
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
// frame フレーム番号から格納されているファイルパスを返す
//(フレームは０スタート。無い場合は空Stringを返す)
QString IwLayer::getImageFilePath(int frame) {
  // ファイル名リストをはみ出していたら空Stringを返す
  if (frame >= m_images.size() || frame < 0) return QString();

  QPair<int, QString> pathPair = m_images.at(frame);

  // フォルダ名リストをフォルダインデックスがはみ出していたら空Stringを返す
  if (pathPair.first >= m_paths.size()) return QString();

  // フォルダ名＋ファイル名を返す。
  return m_paths.at(pathPair.first).filePath(pathPair.second);
}

//---------------------------------------------------
// Returns file path at the frame, without the parent folder path.
// frame number starts from 0. Returns empty QString if ther is no image at the
// frame フレーム番号から格納されているファイル名を返す Parentフォルダ無し。
//(フレームは０スタート。無い場合は空Stringを返す)
QString IwLayer::getImageFileName(int frame) {
  // ファイル名リストをはみ出していたら空Stringを返す
  if (frame < 0 || frame >= m_images.size()) return QString();

  // ファイル名を返す。
  return m_images.at(frame).second;
}

//---------------------------------------------------
// Returns the parent folder path at the frame.
// frame number starts from 0. Returns empty QString if ther is no image at the
// frame フレーム番号から格納されているparentフォルダ名を返す
//(フレームは０スタート。無い場合は空Stringを返す)
QString IwLayer::getParentFolderName(int frame) {
  // ファイル名リストをはみ出していたら空Stringを返す
  if (frame >= m_images.size()) return QString();

  QPair<int, QString> pathPair = m_images.at(frame);

  // フォルダ名リストをフォルダインデックスがはみ出していたら空Stringを返す
  if (pathPair.first >= m_paths.size()) return QString();

  // フォルダ名を返す。
  return m_paths.at(pathPair.first).path();
}

//---------------------------------------------------
// get the folder path index, register if the path is not yet registered
// ディレクトリパスを入力。
// まだリストに無い場合は追加してインデックスを返す。
int IwLayer::getFolderPathIndex(const QString& folderPath) {
  // フォルダのリストが空なら追加するだけ
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
// 画像パスの挿入 ※ シグナルは外で各自エミットすること
//---------------------------------------------------
void IwLayer::insertPath(int index, QPair<int, QString> image) {
  m_images.insert(index, image);
}
//---------------------------------------------------
// delete image path. (may need to emit a signal)
// 画像パスを消去する。 ※ シグナルは外で各自エミットすること
//---------------------------------------------------
void IwLayer::deletePaths(int startIndex, int amount, bool skipPathCheck) {
  if (m_paths.isEmpty() || m_images.isEmpty()) return;

  // m_imagesをstartIndexからamountの枚数分消去
  for (int i = 0; i < amount; i++) {
    // （念のため）消去する位置がm_imagesを超えちゃったらループを抜ける
    if (m_images.size() <= startIndex) break;
    // 消去
    m_images.removeAt(startIndex);
  }

  // m_imagesが空なら、m_pathsも全消去してreturn
  if (m_images.isEmpty()) {
    m_paths.clear();
    return;
  }

  if (skipPathCheck) return;

  // ここで、既に使われていないファルダがある場合は、整理する
  // 使われていないフォルダインデックスのリストを作る
  QVector<int> unusedFolderIndeces;
  // m_pathsのアイテムの数だけ
  for (int p = 0; p < m_paths.size(); p++) {
    bool found = false;
    // m_imagesを探索して、その番号を見つけたらcontinue
    for (int i = 0; i < m_images.size(); i++) {
      if (m_images.at(i).first == p) {
        found = true;
        break;
      }
    }
    // 無ければ、使われていないフォルダインデックスを追加する
    if (!found) unusedFolderIndeces.append(p);
  }
  // 使われていないフォルダインデックスが空ならreturn
  if (unusedFolderIndeces.isEmpty()) return;

  // 新しいインデックス用のQMapテーブルを作る
  QList<int> modifyPathIndexTable;

  // m_pathsを消去しながら新しいインデックスのテーブルを作る
  int modValue = 0;
  int oldSize  = m_paths.size();
  // m_pathsの各アイテムについて
  for (int p = 0; p < oldSize; p++) {
    // pがunusedFolderIndecesに入っていたら
    if (unusedFolderIndeces.contains(p)) {
      // m_pathsから、アイテム-modValue番目のアイテムを消す
      m_paths.removeAt(unusedFolderIndeces.at(p) - modValue);
      // modifyPathIndexTableにはとりあえず0を入れる
      modifyPathIndexTable.append(0);
      // modValueをインクリメント
      modValue++;
    }
    // pがunusedFolderIndecesに入っていなかったら
    //(まだ使っているフォルダインデックス)
    else {
      // modifyPathIndexTableに、p-modValueを入れる
      modifyPathIndexTable.append(p - modValue);
    }
  }

  // m_imagesの各フレームについて
  for (int i = 0; i < m_images.size(); i++) {
    // 新しいインデックスをテーブルに合わせて振りなおす
    m_images[i].first = modifyPathIndexTable.at(m_images.at(i).first);
  }
}

//---------------------------------------------------
// replace image path. (may need to emit a signal)
// 画像パスの差し替え ※ シグナルは外で各自エミットすること
//---------------------------------------------------
void IwLayer::replacePath(int index, QPair<int, QString> image) {
  if (index < 0 || index >= m_images.size()) return;
  m_images.replace(index, image);

  // replacePathの結果後半フレームに空コマができたとき、deleteする
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
// キャッシュがあればそれを返す。
// 無ければ画像をロードしてキャッシュに格納する
//---------------------------------------------------
TImageP IwLayer::getImage(int index) {
  // パスを取得
  QString path = getImageFilePath(index);
  // パスが空なら空Imgを返す
  if (path.isEmpty()) return TImageP();

  // キャッシュされていたら、それを返す
  if (IwImageCache::instance()->isCached(path))
    return IwImageCache::instance()->get(path);

  TImageP img = IoCmd::loadImage(path);

  // 画像がロードできたら、キャッシュに入れる
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
// シェイプを返す
//---------------------------------------------------
ShapePair* IwLayer::getShapePair(int shapePairIndex) {
  if (shapePairIndex >= m_shapepairs.size()) return 0;
  return m_shapepairs.at(shapePairIndex);
}

//---------------------------------------------------
// シェイプの追加。indexを指定していなければ新たに追加
//---------------------------------------------------
void IwLayer::addShapePair(ShapePair* shapePair, int index) {
  // m_shapesにすでにindexのアイテムが有る場合、上書きする
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
// シェイプの削除
//---------------------------------------------------
void IwLayer::deleteShapePair(int shapePairIndex, bool doRemove) {
  assert(0 <= shapePairIndex && shapePairIndex < m_shapepairs.count());

  // Triangle Cacheの削除
  ShapePair* shapeToBeRemoved = m_shapepairs.at(shapePairIndex);
  ShapePair* childShape       = nullptr;
  if (shapeToBeRemoved) {
    // 関連するShapeのinvalidate
    IwTriangleCache::instance()->invalidateShapeCache(
        getParentShape(shapeToBeRemoved));
    // parentシェイプを削除した場合、ひとつ上のシェイプが子を引き受ける
    // 子シェイプがいるかどうかの確認
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
    // 本当に中身を消す
    m_shapepairs.removeAt(shapePairIndex);
  else
    // indexの中身を0に置き換える
    m_shapepairs.replace(shapePairIndex, 0);

  // Triangle Cacheの削除 その２
  if (childShape) {
    IwTriangleCache::instance()->invalidateShapeCache(
        getParentShape(childShape));
  }
}

//---------------------------------------------------
// シェイプの削除
//---------------------------------------------------
void IwLayer::deleteShapePair(ShapePair* shapePair, bool doRemove) {
  if (!m_shapepairs.contains(shapePair)) return;
  int shapePairIndex = m_shapepairs.indexOf(shapePair);

  deleteShapePair(shapePairIndex, doRemove);
}

//---------------------------------------------------
// シェイプリストの丸々差し替え.　取り扱い注意
//---------------------------------------------------
void IwLayer::replaceShapePairs(QList<ShapePair*> shapePairs) {
  m_shapepairs = shapePairs;

  IwTriangleCache::instance()->invalidateAllCache();
}

//---------------------------------------------------
// 展開しているかどうかを踏まえて中のシェイプの展開情報を取得して表示行数を返す
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
// 描画(ヘッダ部分)
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
  // まずは自分自身
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
    // Expandボタン上にマウスがあったらそこをハイライト
    if (mouseOnButton == Expand) {
      p.setBrush(QColor(255, 255, 220, 100));
      p.setPen(Qt::NoPen);
      p.drawRect(2, vpos + marginFor(15), 15, 15);
    }
    p.drawPixmap(5, vpos + marginFor(9), 9, 9,
                 (m_isExpanded) ? QPixmap(":Resources/tl_expand_on.svg")
                                : QPixmap(":Resources/tl_expand_off.svg"));
  }

  // 表示／非表示ボタン
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
    // シェイプ
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
        // currentVposは関数drawTimeLineHead内で描きながらインクリメント
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
// 描画(本体)
//---------------------------------------------------

void IwLayer::drawTimeLine(QPainter& p, int vpos, int width, int fromFrame,
                           int toFrame, int frameWidth, int rowHeight,
                           int& currentRow, int mouseOverRow,
                           double mouseOverFrameD,
                           IwTimeLineSelection* selection) {
  // このロールがカレントで、かつSequenceSelectionがカレントかどうか
  bool isCurrent = (IwApp::instance()->getCurrentLayer()->getLayer() == this) &&
                   (IwSelection::getCurrent() == selection);
  bool isHighlightRow = (mouseOverRow == currentRow);

  int currentVpos = vpos;
  // 1フレーム分の素材バー
  QRect frameRect(0, vpos, frameWidth, rowHeight);
  QRect sukimaRect(0, vpos, 3, rowHeight);

  QLinearGradient rowGrad(QPointF(0.0, 0.0), QPointF(0.0, (double)rowHeight));
  rowGrad.setColorAt(0.0, QColor(130, 223, 237, 255));
  rowGrad.setColorAt(1.0, QColor(103, 167, 196, 255));
  rowGrad.setSpread(QGradient::RepeatSpread);

  bool sameAsPrevious = false;
  int sameAsNextCount = 0;

  // 各フレームで、中身のあるものだけ描画
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
    // 同じファイル名が連続している数を再計算
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
        // ベース色
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
        // フレームが選択されていたら色変える
        if (isCurrent && selection->isFrameSelected(tmpF)) {
          p.setPen(Qt::NoPen);
          p.setBrush(QColor(100, 153, 176, 128));
          p.drawRect(eachRect.adjusted(3, 0, 0, 0));
        }
        // 隙間が選択されていたら色変える
        if (tmpF != f && isCurrent && selection->isBorderSelected(tmpF)) {
          p.setPen(Qt::NoPen);
          p.setBrush(QColor(100, 153, 176, 180));
          p.drawRect(eachSukima);
        }
      }

      // ファイル名の描画
      p.setPen(m_isLocked ? Qt::lightGray : Qt::white);
      if (sameAsNextCount > 0)
        p.drawText(
            tmpRect.adjusted(7, 0, tmpRect.width() * sameAsNextCount - 3, 0),
            Qt::AlignLeft | Qt::AlignVCenter, getImageFileName(f));
      else
        p.drawText(tmpRect.adjusted(3, 0, 0, 0), Qt::AlignCenter,
                   getImageFileName(f));
    }

    // 隙間ボックスの描画
    QRect tmpSukima = sukimaRect.translated(frameWidth * f, 0);
    p.setPen(Qt::black);
    // 隙間が選択されていたら色変える
    if (isCurrent && selection->isBorderSelected(f) && !m_isLocked)
      p.setBrush(Qt::white);
    else
      p.setBrush(Qt::darkGray);
    if (sameAsPrevious)
      p.drawRect(tmpSukima.adjusted(0, rowHeight * 0.8, 0, 0));
    else
      p.drawRect(tmpSukima);
  }

  // 最後の隙間部分を描画
  p.setPen(Qt::black);
  // フレームが選択されていたら色変える
  if (isCurrent && selection->isBorderSelected(getFrameLength()) && !m_isLocked)
    p.setBrush(Qt::white);
  else
    p.setBrush(Qt::darkGray);
  p.drawRect(sukimaRect.translated(frameWidth * getFrameLength(), 0));

  currentRow++;

  if (!m_isExpanded) return;

  currentVpos += rowHeight;
  // シェイプ
  for (int sp = 0; sp < m_shapepairs.size(); sp++) {
    ShapePair* shapePair = m_shapepairs.at(sp);
    if (shapePair && shapePair->isAtLeastOneShapeExpanded()) {
      // currentVposは関数drawTimeLineHead内で描きながらインクリメント
      shapePair->drawTimeLine(p, this, currentVpos, width, fromFrame, toFrame,
                              frameWidth, rowHeight, currentRow, mouseOverRow,
                              mouseOverFrameD, m_isLocked, m_isVisibleInRender);
    }
  }
}

//---------------------------------------------------
// 左クリック時の処理 rowはこのレイヤ内で上から０で始まる行数。
// 再描画が必要ならtrueを返す
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
      // カレントのレイヤをこれにする
      bool ret = IwApp::instance()->getCurrentLayer()->setLayer(this);

      if (m_isLocked) return ret;

      // 他をクリックしたら、そのレイヤのタイムラインを全選択させる
      // IwTimeLineSelectionをカレントに
      IwTimeLineSelection::instance()->makeCurrent();
      ret = ret & IwTimeLineSelection::instance()->selectFrames(
                      0, getFrameLength() - 1, true);
      if (ret)
        IwApp::instance()->getCurrentSelection()->notifySelectionChanged();
      return ret;
    }
  }

  // カレントのレイヤをこれにする
  bool ret = IwApp::instance()->getCurrentLayer()->setLayer(this);

  // シェイプ部分のクリック操作無効化
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
// ダブルクリック時、名前部分だったらtrue。シェイプかどうかも判定
//---------------------------------------------------
bool IwLayer::onDoubleClick(int row, int mouseHPos, ShapePair** shapeP) {
  if (row >= getRowCount()) return false;
  // ロックされていたら無反応
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
// 左のアイテムリスト右クリック時の操作
//---------------------------------------------------
void IwLayer::onRightClick(int row, QMenu& menu) {
  if (row >= getRowCount()) return;

  if (row == 0)
    return;
  else {
    if (m_shapepairs.size() <= 1) return;

    int currentRow = 1;
    int aboveIndex = -1;  // ひとつ上の表示シェイプのインデックス
    for (int sp = 0; sp < m_shapepairs.size(); sp++) {
      ShapePair* shapePair = m_shapepairs.at(sp);
      if (!shapePair || !shapePair->isAtLeastOneShapeExpanded()) continue;
      if (row < currentRow + shapePair->getRowCount()) {
        menu.addSeparator();
        if (sp > 0) {
          QAction* upAct = new QAction(tr("Move Shape Upward"), this);
          upAct->setData(sp);  // シェイプインデックスをアクションに登録
          connect(upAct, SIGNAL(triggered()), this, SLOT(onMoveShapeUpward()));
          menu.addAction(upAct);

          if (aboveIndex >= 0) {
            QAction* aboveAct =
                new QAction(tr("Move Above the Upper Expanded Shape"), this);
            aboveAct->setData(QPoint(
                sp, aboveIndex));  // シェイプインデックスをアクションに登録
            connect(aboveAct, SIGNAL(triggered()), this,
                    SLOT(onMoveAboveUpperShape()));
            menu.addAction(aboveAct);
          }
        }
        if (sp < m_shapepairs.size() - 1) {
          QAction* downAct = new QAction(tr("Move Shape Downward"), this);
          downAct->setData(sp);  // シェイプインデックスをアクションに登録
          connect(downAct, SIGNAL(triggered()), this,
                  SLOT(onMoveShapeDownward()));
          menu.addAction(downAct);

          // 下を探す
          int belowIndex = -1;  // ひとつ下の表示シェイプのインデックス
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
                sp, belowIndex));  // シェイプインデックスをアクションに登録
            connect(belowAct, SIGNAL(triggered()), this,
                    SLOT(onMoveBelowLowerShape()));
            menu.addAction(belowAct);
          }
        }

        //---- シェイプの親子を変更する
        QString str =
            (shapePair->isParent()) ? tr("Set to Child") : tr("Set to Parent");
        QAction* parentChildAct = new QAction(str, this);
        parentChildAct->setData(sp);  // シェイプインデックスをアクションに登録
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
// タイムライン上左クリック時の処理
//---------------------------------------------------
bool IwLayer::onTimeLineClick(int row,  // rowは0から始まるクリックされた行数
                              double frameD, bool controlPressed,
                              bool shiftPressed, Qt::MouseButton button) {
  if (row >= getRowCount()) return false;
  // レイヤの行
  if (row <= 0) return false;  // レイヤの行の処理はTimeLineWindow内で済ませた

  if (m_isLocked) return false;  // レイヤがロックされていたら操作無効。
  // 以下、シェイプ又はキーフレームの行がクリックされた場合
  int currentRow = 1;
  for (int sp = 0; sp < m_shapepairs.size(); sp++) {
    ShapePair* shapePair = m_shapepairs.at(sp);
    if (!shapePair || !shapePair->isAtLeastOneShapeExpanded()) continue;

    // ここのシェイプがクリックされた
    if (row < currentRow + shapePair->getRowCount()) {
      // シェイプの中の何行目がクリックされたか
      int rowInShape = row - currentRow;

      return shapePair->onTimeLineClick(rowInShape, frameD, controlPressed,
                                        shiftPressed, button);
    }
    currentRow += shapePair->getRowCount();
  }
  return false;
}

//---------------------------------------------------
// マウスホバー時
//---------------------------------------------------

bool IwLayer::onMouseMove(int frameIndex, bool isBorder) {
  // とりあえずボーダー以外は何もしない
  if (!isBorder) return false;
  if (frameIndex <= 0) return false;
  if (m_isLocked) return false;
  // 画像シーケンスの右端だけ有効にする
  if (getImageFileName(frameIndex) == getImageFileName(frameIndex - 1))
    return false;

  return true;
}

//---------------------------------------------------
// シェイプの名前を返す
//---------------------------------------------------

int IwLayer::getNameFromShapePair(OneShape shape) {
  int index = m_shapepairs.indexOf(shape.shapePairP);

  // もし見つからなかったら0を返す
  if (index == -1) return 0;

  // 見つかったら、描画用のシェイプIDに変換
  //(インデックスに１足す→FromToフラグの1桁→下に４桁の０をつける)
  return (index + 1) * 100000 + shape.fromTo * 10000;
}
//---------------------------------------------------
// 名前からシェイプを返す
//---------------------------------------------------
OneShape IwLayer::getShapePairFromName(int name) {
  OneShape ret(0, 0);
  // シェイプが無ければ無い
  if (m_shapepairs.isEmpty()) return ret;
  // もし、10000より小さい場合はシェイプはつかめていない
  if (name < 10000) return ret;
  // インデックスを得る
  int index = (int)(name / 100000) - 1;
  if (index < m_shapepairs.count()) ret.shapePairP = m_shapepairs.at(index);
  // FromToを得る
  ret.fromTo = (int)((name % 100000) / 10000);

  // インデックスからシェイプを返す。無ければ０を返す
  return ret;
}

//---------------------------------------------------
// このレイヤに引数のシェイプが属していたらtrue
//---------------------------------------------------
bool IwLayer::contains(ShapePair* shape) {
  if (!shape) return false;

  // 見つかったらtrue
  return m_shapepairs.contains(shape);
}
//---------------------------------------------------
// シェイプからインデックスを返す 無ければ-1
//---------------------------------------------------
int IwLayer::getIndexFromShapePair(const ShapePair* shapePair) const {
  return m_shapepairs.indexOf((ShapePair*)shapePair);
}

//---------------------------------------------------
// Undo操作用。フレームのデータをそのまま返す
//---------------------------------------------------
QPair<int, QString> IwLayer::getPathData(int frame) {
  if (frame < 0 || frame >= m_images.size()) return QPair<int, QString>(0, "");
  return m_images.at(frame);
}

//---------------------------------------------------
// 自分がParentなら自分を返す。子なら親Shapeのポインタを返す
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
// セーブ/ロード
//---------------------------------------------------

void IwLayer::saveData(QXmlStreamWriter& writer) {
  // 名前
  writer.writeTextElement("name", m_name);
  // 表示非表示
  writer.writeTextElement("isVisibleInViewer",
                          QString::number(m_isVisibleInViewer));
  writer.writeTextElement("isVisibleInRender",
                          (m_isVisibleInRender) ? "1" : "0");
  // ロック
  writer.writeTextElement("locked", (m_isLocked) ? "True" : "False");

  // レイヤーオプション
  writer.writeStartElement("LayerOptions");
  writer.writeTextElement("brightness", QString::number(m_brightness));
  writer.writeTextElement("contrast", QString::number(m_contrast));
  writer.writeEndElement();

  // フォルダリスト
  writer.writeStartElement("Paths");
  for (int p = 0; p < m_paths.size(); p++) {
    writer.writeTextElement("path", m_paths.at(p).absolutePath());
  }
  writer.writeEndElement();

  // フレームのリスト
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

  // シェイプペア
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
    // 名前
    if (reader.name() == "name") {
      m_name = reader.readElementText();
    } else if (reader.name() == "isVisibleInViewer") {
      m_isVisibleInViewer = (LayerVisibility)(reader.readElementText().toInt());
    } else if (reader.name() == "isVisibleInRender") {
      m_isVisibleInRender = reader.readElementText().toInt() != 0;
    } else if (reader.name() == "locked")
      m_isLocked = (reader.readElementText() == "True") ? true : false;

    // レイヤーオプション
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

    // フォルダリスト
    else if (reader.name() == "Paths") {
      if (!m_paths.isEmpty()) m_paths.clear();

      while (reader.readNextStartElement()) {
        if (reader.name() == "path") {
          m_paths.append(QDir(reader.readElementText()));
        } else
          reader.skipCurrentElement();
      }
    }
    // フレームのリスト
    else if (reader.name() == "Images") {
      if (!m_images.isEmpty()) m_images.clear();

      while (reader.readNextStartElement()) {
        if (reader.name() == "image") {
          int pathIndex =
              reader.attributes().value("pathIndex").toString().toInt();
          QString fileName = reader.attributes().value("fileName").toString();
          int length = reader.attributes().value("length").toString().toInt();

          // データに入れる
          for (int f = 0; f < length; f++)
            m_images.append(QPair<int, QString>(pathIndex, fileName));
          reader.skipCurrentElement();
        } else
          reader.skipCurrentElement();
      }

    }
    // シェイプペア
    else if (reader.name() == "ShapePairs") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "ShapePair") {
          ShapePair* shapePair = new ShapePair();

          shapePair->loadData(reader);

          // プロジェクトに追加
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
