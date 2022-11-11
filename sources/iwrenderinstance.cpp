//---------------------------------------------------
// IwRenderInstance
// 各フレームごとに作られる、レンダリング計算を行う実体
// IwRenderCommand::onPreviewから作られる
//---------------------------------------------------

#include "iwrenderinstance.h"

#include "iwapp.h"
#include "iwproject.h"
#include "iwprojecthandle.h"
#include "outputsettings.h"

#include "iwlayer.h"
#include "shapepair.h"

#include "iwimagecache.h"
#include "iwtrianglecache.h"
#include "renderprogresspopup.h"

#include "ttoonzimage.h"
#include "trasterimage.h"
#include "tropcm.h"

#include "tpalette.h"

#include <iostream>
#include <math.h>

#include "tlevel_io.h"
#include "timageinfo.h"

#include "tnzimage.h"
#include "tiio.h"

#include <QPolygonF>
#include <QStack>
#include <QThreadPool>
#include <QPainter>
//-----------------------------------------------------------------------------

namespace {

// 三角形をソートする 裏返っているとき、優先的にtrue。
//  tri1のDepthが遠い（小さい）ときにTrue
bool HEdepthLessThan(const HEFace* tri1, const HEFace* tri2) {
  // 裏返りチェック
  bool tri1IsUra = tri1->size() < 0.0;
  bool tri2IsUra = tri2->size() < 0.0;
  if (tri1IsUra != tri2IsUra) return (tri1IsUra) ? true : false;

  // 同じシェイプ内の分割三角形を優先させる
  bool tri1IsConstDepth = tri1->hasConstantDepth();
  bool tri2IsConstDepth = tri2->hasConstantDepth();
  if (tri1IsConstDepth != tri2IsConstDepth) return tri2IsConstDepth;

  // デプスチェック
  return tri1->centroidDepth() < tri2->centroidDepth();
}

// double値が整数かどうかをチェックする
bool isDecimal(double val) { return val == std::floor(val); }

//---------------------------------------------------
// 座標から色を返す。範囲外の場合は透明色を返す
//---------------------------------------------------
inline TPixel64 getPixelVal(TRaster64P ras, QPoint& index) {
  if (index.x() < 0 || index.x() >= ras->getLx() || index.y() < 0 ||
      index.y() >= ras->getLy())
    return TPixel64::Transparent;

  TPixel64* pix = ras->pixels(index.y());
  pix += index.x();

  return *pix;
}
//---------------------------------------------------
// uv座標を元に、ピクセル値をリニア補間で得る。
//---------------------------------------------------
TPixel64 getInterpolatedPixelVal(TRaster64P srcRas, QPointF& uv) {
  auto lerp = [&](TPixel64& pix1, TPixel64& pix2, double ratio) {
    return TPixel64(pix1.r * (1.0 - ratio) + pix2.r * ratio,
                    pix1.g * (1.0 - ratio) + pix2.g * ratio,
                    pix1.b * (1.0 - ratio) + pix2.b * ratio,
                    pix1.m * (1.0 - ratio) + pix2.m * ratio);
  };

  QPoint uvIndex(tfloor(uv.x()), tfloor(uv.y()));
  QPointF uvRatio = uv - QPointF(uvIndex);

  TPixel64 basePix[2][2];
  basePix[0][0] = getPixelVal(srcRas, uvIndex);
  basePix[0][1] = getPixelVal(srcRas, QPoint(uvIndex.x() + 1, uvIndex.y()));
  basePix[1][0] = getPixelVal(srcRas, QPoint(uvIndex.x(), uvIndex.y() + 1));
  basePix[1][1] = getPixelVal(srcRas, QPoint(uvIndex.x() + 1, uvIndex.y() + 1));

  return lerp(lerp(basePix[0][0], basePix[0][1], uvRatio.x()),
              lerp(basePix[1][0], basePix[1][1], uvRatio.x()), uvRatio.y());
}

bool checkIsPremultiplied(TRaster64P ras) {
  for (int y = 0; y < ras->getLy(); y++) {
    TPixel64* pix = ras->pixels(y);
    for (int x = 0; x < ras->getLx(); x++, pix++) {
      if (pix->r > pix->m || pix->g > pix->m || pix->b > pix->m) return false;
    }
  }
  return true;
}

unsigned int taskId = 0;
}  // namespace

//---------------------------------------------------
IwRenderInstance::IwRenderInstance(int frame, IwProject* project,
                                   bool isPreview, RenderProgressPopup* popup)
    : m_frame(frame)
    , m_project(project)
    , m_isPreview(isPreview)
    , m_popup(popup)
    , m_warpStyle(WARP_FIXED)
    , m_smoothingThreshold(3.0)
    , m_taskId(taskId++) {
  RenderSettings* settings = m_project->getRenderSettings();
  m_precision              = settings->getWarpPrecision();
  m_faceSizeThreshold      = settings->getFaceSizeThreshold();
  m_antialias              = settings->getAntialias();
}

//---------------------------------------------------

void IwRenderInstance::run() {
  emit renderStarted(m_frame, m_taskId);
  if (m_isPreview)
    doPreview();
  else
    doRender();
  emit renderFinished(m_frame, m_taskId);
}

//---------------------------------------------------
// レンダリング
//---------------------------------------------------
void IwRenderInstance::doRender() {
  if (isCanceled()) return;
  // 結果を収めるラスタ
  QSize workAreaSize = m_project->getWorkAreaSize();
  TRaster64P morphedRaster(workAreaSize.width(), workAreaSize.height());
  morphedRaster->fill(TPixel64::Transparent);
  if (isCanceled()) return;

  int targetShapeTag = m_project->getOutputSettings()->getShapeTagId();

  // 下から、各レイヤについて
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    if (isCanceled()) return;
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;
    // レイヤの画像が無ければスキップ
    if (layer->getImageFileName(m_frame).isEmpty()) continue;
    // レイヤにシェイプが無ければスキップ
    if (layer->getShapePairCount() == 0) continue;
    // レイヤがレンダリング非表示ならスキップ
    if (!layer->isVisibleInRender()) continue;
    // シェイプを下から回収していく
    QList<ShapePair*> tmpShapes;
    for (int s = layer->getShapePairCount() - 1; s >= 0; s--) {
      if (isCanceled()) return;
      ShapePair* shape = layer->getShapePair(s);
      if (!shape) continue;
      // シェイプをリストに追加
      tmpShapes.append(shape);
      // 子シェイプのとき、リストに追加して次のシェイプへ
      if (!shape->isParent()) continue;
      // 親シェイプのとき、ワープ処理を行う
      else {
        std::cout << shape->getName().toStdString() << std::endl;
        // 親シェイプがターゲットになっていない場合、全ての子シェイプもレンダリングしない
        if (shape->isRenderTarget(targetShapeTag)) {
          // レイヤの画像をワープする
          QPoint origin;
          TRaster64P warpedLayerRas =
              warpLayer(layer, tmpShapes, false, origin);
          if (isCanceled()) return;
          if (warpedLayerRas)
            TRop::over(morphedRaster, warpedLayerRas,
                       TPoint(origin.x(), origin.y()));
        }
        // リストをリセット
        tmpShapes.clear();
      }
    }
  }
  if (isCanceled()) return;

  // ファイルに書き出す
  saveImage(morphedRaster);

  // ここでinvalidなキャッシュはもう使っていないシェイプのはず
  IwTriangleCache::instance()->removeInvalid(m_frame);
}

//---------------------------------------------------
// レンダリング
//---------------------------------------------------
void IwRenderInstance::doPreview() {
  // 結果を収めるラスタ
  QSize workAreaSize = m_project->getWorkAreaSize();

  int targetShapeTag = m_project->getOutputSettings()->getShapeTagId();

  // 下から、各レイヤについて
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;
    // レイヤの画像が無ければスキップ
    if (layer->getImageFileName(m_frame).isEmpty()) continue;
    // レイヤにシェイプが無ければスキップ
    if (layer->getShapePairCount() == 0) continue;
    // レイヤがレンダリング非表示ならスキップ
    if (!layer->isVisibleInRender()) continue;

    // シェイプを下から回収していく
    QList<ShapePair*> tmpShapes;
    for (int s = layer->getShapePairCount() - 1; s >= 0; s--) {
      ShapePair* shape = layer->getShapePair(s);
      if (!shape) continue;
      // シェイプをリストに追加
      tmpShapes.append(shape);
      // 子シェイプのとき、リストに追加して次のシェイプへ
      if (!shape->isParent()) continue;
      // 親シェイプのとき、ワープ処理を行う
      else {
        // 親シェイプがターゲットになっていない場合、全ての子シェイプもレンダリングしない
        if (shape->isRenderTarget(targetShapeTag)) {
          // レイヤの画像をワープする
          warpLayer(layer, tmpShapes, true);
        }
        // リストをリセット
        tmpShapes.clear();
      }
    }
  }
  // ここでinvalidなキャッシュはもう使っていないシェイプのはず
  IwTriangleCache::instance()->removeInvalid(m_frame);
}
//---------------------------------------------------
// ● 結果をキャッシュに格納
//---------------------------------------------------
void IwRenderInstance::storeImageToCache(TRaster64P morphedRaster) {
  TRasterImageP img(morphedRaster);
  // 計算が終わったら、キャッシュに保存
  // キャッシュのエイリアスを得る
  QString cacheAlias =
      QString("PRJ%1_PREVIEW_FRAME%2").arg(m_project->getId()).arg(m_frame);

  IwImageCache::instance()->add(cacheAlias, img);
}

//---------------------------------------------------
// レイヤをワープ
//---------------------------------------------------
TRaster64P IwRenderInstance::warpLayer(IwLayer* layer,
                                       QList<ShapePair*>& shapes,
                                       bool isPreview, QPoint& origin) {
  // 画像を取得
  TRasterP ras = getLayerRaster(layer);
  if (!ras) return TRaster64P();

  // プレビューで、すでに有効なキャッシュがある場合は計算しない
  if (isPreview && IwTriangleCache::instance()->isValid(m_frame, shapes.last()))
    return TRaster64P();

  QList<CorrVector> corrVectors;
  // QPolygonF を生成し、形状の内外判定に用いる
  QVector<QPointF> parentShapeVerticesFrom, parentShapeVerticesTo;
  getCorrVectors(layer, shapes, corrVectors, parentShapeVerticesFrom,
                 parentShapeVerticesTo);

  if (isCanceled()) return TRaster64P();

  HEModel model;
  QPolygonF parentShapePolygon(parentShapeVerticesFrom);
  // fromShapeを元に、メッシュを切る:
  HEcreateTriangleMesh(corrVectors, model, parentShapePolygon, ras->getSize());

  // 三角形をソートする
  std::sort(model.faces.begin(), model.faces.end(), HEdepthLessThan);

  if (isCanceled()) return TRaster64P();

  // 三角形の位置をキャッシュする
  HEcacheTriangles(model, shapes.last(), ras->getSize(), parentShapePolygon);

  if (isPreview) return TRaster64P();

  // もし、srcRasが32bitなら、ここで64bit化
  TRaster64P inRas;
  if (ras->getPixelSize() == 4) {
    inRas = TRaster64P(ras->getWrap(), ras->getLy());
    TRop::convert(inRas, ras);
  } else
    inRas = ras;

  // demultiplyする
  if (checkIsPremultiplied(inRas)) TRop::depremultiply(inRas);

  // ゆがんだ形状を描画する
  TRaster64P outRas = HEmapTrianglesToRaster_Multi(
      model, inRas, shapes.last(), origin, QPolygonF(parentShapeVerticesTo));

  return outRas;
}

//===================================================

//---------------------------------------------------
// 素材ラスタを得る
//---------------------------------------------------
TRasterP IwRenderInstance::getLayerRaster(IwLayer* layer) {
  TImageP img = layer->getImage(m_frame);
  if (!img.getPointer()) return 0;

  TRaster64P ras;
  TRasterImageP ri = (TRasterImageP)img;
  TToonzImageP ti  = (TToonzImageP)img;
  // 形式が合わなければ 0 を返す
  if (!ri && !ti)
    return 0;
  else if (ri) {
    TRasterP tmpRas = ri->getRaster();
    // pixelSize = 4 なら 8bpc、8 なら 16bpc
    // 8bitなら16bitに変換
    if (tmpRas->getPixelSize() == 4) {
      ras = TRaster64P(tmpRas->getWrap(), tmpRas->getLy());
      TRop::convert(ras, tmpRas);
    } else
      ras = tmpRas;
  } else if (ti) {
    TRasterCM32P rascm;
    rascm           = ti->getRaster();
    TRasterP tmpRas = TRaster32P(rascm->getWrap(), rascm->getLy());
    TRop::convert(tmpRas, rascm, ti->getPalette());
    ras = TRaster64P(tmpRas->getWrap(), tmpRas->getLy());
    TRop::convert(ras, tmpRas);
  }
  return ras;
}

//---------------------------------------------------
// ワープ先のCorrVecの長さに合わせ、
// あまり短いベクトルはまとめながら格納していく
//---------------------------------------------------

void IwRenderInstance::getCorrVectors(IwLayer* layer, QList<ShapePair*>& shapes,
                                      QList<CorrVector>& corrVectors,
                                      QVector<QPointF>& parentShapeVerticesFrom,
                                      QVector<QPointF>& parentShapeVerticesTo) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // 1ピクセルあたりのシェイプ座標系の値を得る
  QPointF onePix(1.0 / (double)workAreaSize.width(),
                 1.0 / (double)workAreaSize.height());

  // 現フレームのシェイプを補間し、対応点ベクトルの集まりを作る
  // 各シェイプについて
  for (int sp = 0; sp < shapes.size(); sp++) {
    int depth            = sp;
    ShapePair* shapePair = shapes.at(sp);

    // 対応点の分割点のベジエ座標値を求める
    QList<double> discreteCorrValuesFrom =
        shapePair->getDiscreteCorrValues(onePix, m_frame, 0);
    QList<double> discreteCorrValuesTo =
        shapePair->getDiscreteCorrValues(onePix, m_frame, 1);

    QPointF firstPosFrom, lastPosFrom, prevPosFrom, firstPosTo, lastPosTo,
        prevPosTo;
    // 各ベジエ座標を格納
    for (int d = 0; d < discreteCorrValuesFrom.size(); d++) {
      QPointF tmpPosFrom = shapePair->getBezierPosFromValue(
          m_frame, 0, discreteCorrValuesFrom.at(d));
      QPointF tmpPosTo = shapePair->getBezierPosFromValue(
          m_frame, 1, discreteCorrValuesTo.at(d));

      // ここでピクセル座標にする
      tmpPosFrom = QPointF(tmpPosFrom.x() * (double)workAreaSize.width(),
                           tmpPosFrom.y() * (double)workAreaSize.height());
      tmpPosTo   = QPointF(tmpPosTo.x() * (double)workAreaSize.width(),
                           tmpPosTo.y() * (double)workAreaSize.height());

      if (d == 0) {
        firstPosFrom = tmpPosFrom;
        firstPosTo   = tmpPosTo;
      } else {
        if (d == discreteCorrValuesFrom.size() - 1) {
          lastPosFrom = tmpPosFrom;
          lastPosTo   = tmpPosTo;
        }

        // ここで、ワープ先（Toの方）のCorrVecの長さが、５ピクセルより短い場合、
        //  次のCorrVecと連結するようにする
        // さらに、コントロールポイントと一致するCorrPointは残す
        QPointF vec = tmpPosTo - prevPosTo;
        if (vec.x() * vec.x() + vec.y() * vec.y() < 9.0 &&
            !isDecimal(discreteCorrValuesFrom.at(d)) &&
            !isDecimal(discreteCorrValuesTo.at(d)))
          continue;

        CorrVector corrVec = {
            {prevPosFrom, tmpPosFrom}, {prevPosTo, tmpPosTo}, depth, false};

        corrVectors.append(corrVec);
      }
      prevPosFrom = tmpPosFrom;
      prevPosTo   = tmpPosTo;

      if (shapePair->isParent()) {
        parentShapeVerticesFrom.append(tmpPosFrom);
        parentShapeVerticesTo.append(tmpPosTo);
      }
    }
    // 閉じたシェイプの場合、最後と最初を繋ぐベクトルを追加
    if (shapePair->isClosed()) {
      CorrVector corrVec = {
          {lastPosFrom, firstPosFrom}, {lastPosTo, firstPosTo}, depth, false};
      corrVectors.append(corrVec);
    }
  }
}

//---------------------------------------------------
/*

B1) 点群を包含する十分大きな三角形(super triangle)を追加する
B2) 線分制約にかかわる頂点piを図形に追加
  B2-2) piを含む三角形ABCを発見し, この三角形をAB pi, BC pi, CA pi
の3個の三角形に分割． この時, 辺AB, BC, CAをスタックSに積む． B2-3)
スタックSが空になるまで以下を繰り返す
    B2-3-1)スタックSから辺をpopし，これを辺ABとする．
       辺ABを含む2個の三角形をABCとABDとする
        if( ABが線分制約である ) 何もしない
        else if( CDが線分制約であり四角形ABCDが凸)
          辺ABをclip，辺AD/DB/BC/CAをスタックに積む
        else if( ABCの外接円内にDが入る )
          辺ABをflip，辺AD/DB/BC/CAをスタックに積む
        else 何もしない
B3) 制約線部の復帰を行う
  B3-1)各制約線分ABについて以下を繰り返す
  B3-2)現在図形とABと交差する全てのエッジをキューKに挿入
  B3-3)キューKが空になるまで次を繰り返す
    B3-3-1)キューKの先頭要素をpopし辺CDとす．CDを含む2個の三角形をCDE, CDFとする
      if( 四角形ECFDが凸 )
        エッジCDをflip. 新たなエッジEFをスタックNにpush.
      else
        エッジCDをキューKに後ろからpush.
  B3-3) B-2-3と同様の処理をキューNに対して行う
    (新たなエッジのドロネー性の確認を行う)
B4) 線分制約にかかわらない頂点piの追加
  B4-1) piを含む三角形ABCを発見し, この三角形をAB pi, BC pi, CA pi
の3個の三角形に分割． 辺AB, BC, CAをスタックSに積む． B4-2)
スタックSについてB2-3-1と同様の処理を繰り返す B5) super
triangle頂点とそれに関わる全てのエッジを取り除く

*/

void IwRenderInstance::HEcreateTriangleMesh(
    QList<CorrVector>& corrVectors,  // 入力。変形前後の対応点ベクトルの集まり
    HEModel& model, const QPolygonF& parentShapePolygon,
    const TDimension& srcDim) {
  // HEModel model;
  //   B1) 点群を包含する十分大きな三角形(super triangle)を追加する
  QSize workAreaSize = m_project->getWorkAreaSize();
  QSizeF superRectSize((qreal)std::max(workAreaSize.width() * 3, srcDim.lx),
                       (qreal)std::max(workAreaSize.height() * 3, srcDim.ly));
  QPointF superRectTopLeft(
      -(superRectSize.width() - workAreaSize.width()) * 0.5,
      -(superRectSize.height() - workAreaSize.height()) * 0.5);
  QRectF superRect(superRectTopLeft, superRectSize);

  // ※ IwaWarperの計算に使う座標空間は左下原点なので、Qtとは上下反転している。
  //  　topLeftが左下、bottomRightが右上となる
  HEVertex* superBL = new HEVertex(superRect.topLeft(), -100);
  HEVertex* superBR = new HEVertex(superRect.topRight(), -100);
  HEVertex* superTR = new HEVertex(superRect.bottomRight(), -100);
  HEVertex* superTL = new HEVertex(superRect.bottomLeft(), -100);
  model.vertices.append(superBL);
  model.vertices.append(superBR);
  model.vertices.append(superTR);
  model.vertices.append(superTL);
  model.superTriangleVertices.append(model.vertices);
  HEFace* superTri1 = model.addFace(superBL, superBR, superTR);
  superTri1->setConstrained(true);
  HEFace* superTri2 = model.addFace(superTR, superTL, superBL);
  superTri2->setConstrained(true);
  // 斜め線の制約を解除
  superTri1->edgeFromVertex(superTR)->setConstrained(false);
  // 頂点制約は残す
  superTR->constrained = true;
  superBL->constrained = true;

  //    B2) 線分制約にかかわる頂点piを図形に追加
  HEVertex* preAddedVert = nullptr;
  int i                  = 0;
  for (CorrVector& corrVec : corrVectors) {
    // 別のストロークの端点同士が一致している場合、点を使いまわす
    HEVertex* start = model.findVertex(corrVec.from_p[0], corrVec.to_p[0]);
    // 一つめの点、または前のCorrVectorの終端点に一致しない
    // （＝別のストロークのスタート点の）場合、新規に点を追加

    if (!start) {
      start = new HEVertex(corrVec.from_p[0], corrVec.to_p[0],
                           (double)corrVec.stackOrder);
      // モデルに点を追加
      //     B2 - 2) piを含む三角形ABCを発見し, この三角形をAB pi, BC pi, CA pi
      //     の3個の三角形に分割． この時, 辺AB, BC, CAをスタックSに積む． B2 -
      //     3) スタックSが空になるまで以下を繰り返す B2 - 3 -
      //     1)スタックSから辺をpopし，これを辺ABとする．
      //     辺ABを含む2個の三角形をABCとABDとする
      //     if (ABが線分制約である) 何もしない
      //     else if (CDが線分制約であり四角形ABCDが凸)
      //       辺ABをclip，辺AD / DB / BC / CAをスタックに積む
      //     else if (ABCの外接円内にDが入る)
      //       辺ABをflip，辺AD / DB / BC / CAをスタックに積む
      //     else 何もしない
      model.addVertex(start, nullptr);
      // model.print();
    }

    // 二つ目の点を追加する
    HEVertex* end = model.findVertex(corrVec.from_p[1], corrVec.to_p[1]);
    if (!end) {
      end = new HEVertex(corrVec.from_p[1], corrVec.to_p[1],
                         (double)corrVec.stackOrder);
      model.addVertex(end, start);
    } else  // すでに追加済みの点の場合（平曲線の始点に戻ってきたときなど）は、制約のみ付加する
      model.setConstraint(start, end, true);
    i++;
  }

  // 親シェイプ形状に含まれているかどうか
  // 初期CorrVecの挿入後、再分割前に１度だけチェックする
  model.checkFaceVisibility(parentShapePolygon);
  // ループ
  for (int g = 0; g < m_precision; g++) {
    // 各三角形の重心を追加 引数は三角形を分割するか判断する面積の閾値
    //        B4) 線分制約にかかわらない頂点piの追加
    //        B4 - 1) piを含む三角形ABCを発見し, この三角形をAB pi, BC pi, CA pi
    //        の3個の三角形に分割． 辺AB, BC, CAをスタックSに積む． B4 - 2)
    //        スタックSについてB2 - 3 - 1と同様の処理を繰り返す
    model.insertCentroids(m_faceSizeThreshold);
    // なじませる
    model.smooth(m_smoothingThreshold);
  }

  //        B5) super triangle頂点とそれに関わる全てのエッジを取り除く
  // model.deleteSuperTriangles();
}

//---------------------------------------------------

void MapTrianglesToRaster_Worker::run() {
  // 各三角形について（手前の三角形から順に）
  for (int h = m_from; h < m_to; h++) {
    HEFace* face = m_model->faces[h];

    if (m_parentInstance->isCanceled()) return;

    if (!face->isVisible) continue;
    // 三角形パッチの頂点を格納しつつ、Y座標の範囲を取得
    double ymin = 10000;
    double ymax = -10000;
    double xmin = 10000;
    double xmax = -10000;
    QPointF sp[3], wp[3];
    QPointF spMin, spMax;  // srcのバウンディングボックス
    Halfedge* he = face->halfedge;
    for (int p = 0; p < 3; p++) {
      HEVertex* v = he->vertex;
      // ここでサンプル点のためのオフセットを加える
      sp[p] = v->from_pos.toPointF() + m_sampleOffset;
      wp[p] = v->to_pos.toPointF() - m_outputOffset;
      // 範囲を更新
      if (wp[p].y() < ymin) ymin = wp[p].y();
      if (wp[p].y() > ymax) ymax = wp[p].y();
      if (wp[p].x() < xmin) xmin = wp[p].x();
      if (wp[p].x() > xmax) xmax = wp[p].x();
      spMin.setX(std::min(spMin.x(), sp[p].x()));
      spMin.setY(std::min(spMin.y(), sp[p].y()));
      spMax.setX(std::max(spMax.x(), sp[p].x()));
      spMax.setX(std::max(spMax.x(), sp[p].x()));

      he = he->next;
    }

    /* ここで、m_outRasは出力サイズの m_subAmount 倍されていることに注意！！ */
    // std::cout << "src ras size = (" << m_srcRas->getLx() << ", " <<
    // m_srcRas->getLy() << ")" << std::endl; std::cout << "src min = (" <<
    // spMin.x() << ", " << spMin.y() << ")" << std::endl; std::cout << "src max
    // = (" << spMax.x() << ", " << spMax.y() << ")" << std::endl << std::endl;
    // std::cout << "out ras size = (" << m_outRas->getLx() << ", " <<
    // m_outRas->getLy() << ")" << std::endl; std::cout << "wrp min = (" << xmin
    // << ", " << ymin << ")" << std::endl; std::cout << "wrp max = (" << xmax
    // << ", " << ymax << ")" << std::endl << std::endl;

    int yminIndex = tfloor(ymin);
    int ymaxIndex = tceil(ymax);
    // outRasのサイズでクランプ
    if (yminIndex * m_subAmount >= m_outRas->getLy() ||
        ymaxIndex * m_subAmount < 0)
      continue;
    if (tfloor(xmin) * m_subAmount >= m_outRas->getLx() ||
        tceil(xmax) * m_subAmount < 0)
      continue;
    if (spMax.x() < 0 || spMin.x() >= m_srcRas->getLx() || spMax.y() < 0 ||
        spMin.y() >= m_srcRas->getLy())
      continue;

    yminIndex = std::max(0, yminIndex);
    ymaxIndex = std::min(m_outRas->getLy() / m_subAmount - 1, ymaxIndex);

    // 各スキャンラインについてループ
    for (int y = yminIndex; y <= ymaxIndex; y++) {
      // Y方向の分割についてループ
      for (int suby = 0; suby < m_subAmount; suby++) {
        // 現在の分割Y座標
        double tmpY = (double)y + (0.5 + (double)suby) / (double)m_subAmount;

        // LRの範囲を求める
        bool pointIsUpper[3];
        for (int p = 0; p < 3; p++) {
          pointIsUpper[p] = (wp[p].y() > tmpY) ? true : false;
        }
        // 全て一方に寄っている場合は次のスキャンラインへ (端っこの場合)
        if (pointIsUpper[0] == pointIsUpper[1] &&
            pointIsUpper[1] == pointIsUpper[2])
          continue;
        // X座標の範囲
        double xmin = 10000;
        double xmax = -10000;
        QPointF uvMin, uvMax;
        // 各辺を調べる
        for (int p = 0; p < 3; p++) {
          int next_p = (p + 1 == 3) ? 0 : p + 1;
          // 現在の辺がスキャンラインをまたいでいる場合
          if (pointIsUpper[p] != pointIsUpper[next_p]) {
            QPointF vec  = wp[next_p] - wp[p];
            double ratio = (tmpY - wp[p].y()) / vec.y();
            // vec.y()は非０のはず
            double tmp_x = wp[p].x() + ratio * vec.x();
            // UV座標も求める
            QPointF tmp_uv = sp[p] * (1.0 - ratio) + sp[next_p] * ratio;
            // 範囲を更新
            if (tmp_x < xmin) {
              xmin  = tmp_x;
              uvMin = tmp_uv;
            }
            if (tmp_x > xmax) {
              xmax  = tmp_x;
              uvMax = tmp_uv;
            }
          }
        }

        // １サブピクセル進んだときのUVの変化の度合いを求める
        /// １ピクセル進んだときのUVの変化の度合いを求める
        QPointF uvAdvancePerOneSubPix =
            (uvMax - uvMin) / (xmax - xmin) / m_subAmount;

        int xminIndex = tfloor(xmin);
        int xmaxIndex = tceil(xmax);
        // outRasのサイズでクランプ
        if (xminIndex * m_subAmount >= m_outRas->getLx() ||
            xmaxIndex * m_subAmount < 0)
          continue;
        xminIndex = std::max(0, xminIndex);
        xmaxIndex = std::min(m_outRas->getLx() / m_subAmount - 1, xmaxIndex);

        // スタート地点:一番左のピクセルのUV値を求める
        double startXPos = (double)xminIndex + 0.5 / (double)m_subAmount;
        // double startXPos = (double)xminIndex - 0.5f + 0.5f/(double)subAmount;
        QPointF uv = uvMin - (xmin - (double)startXPos) * uvAdvancePerOneSubPix;

        int row = y * m_subAmount + suby;

        TPixel64* outpix = m_outRas->pixels(row);
        outpix += xminIndex * m_subAmount;

        const uchar* alpha_p = m_shapeAlphaImg.constScanLine(row);
        // const QRgb* alpha_p = (QRgb*)m_shapeAlphaImg.constScanLine(row);
        alpha_p += xminIndex * m_subAmount;

        int subIndex = row * m_outRas->getLx() + xminIndex * m_subAmount;

        // スキャンラインを横にたどる
        for (int x = xminIndex; x <= xmaxIndex; x++) {
          double tmpXPos = (double)x;
          // double tmpXPos = (double)x - 0.5f;
          // X方向の分割についてループ
          for (int subx = 0; subx < m_subAmount; subx++,
                   tmpXPos += 1.0 / (double)m_subAmount,
                   uv += uvAdvancePerOneSubPix, subIndex++, outpix++,
                   alpha_p++) {
            if (uv.x() < 0 || uv.x() >= m_srcRas->getLx() || uv.y() < 0 ||
                uv.y() >= m_srcRas->getLy())
              continue;
            // もし、既にアルファ値が１なら、描かない
            if (outpix->m != TPixel64::maxChannelValue && tmpXPos >= xmin &&
                tmpXPos < xmax && m_subPointOccupation[subIndex] == false) {
              // TPixel64 pix = getPixelVal(srcRas, uv.toPoint());
              // uv座標を元に、ピクセル値をリニア補間で得る。
              TPixel64 pix = getInterpolatedPixelVal(m_srcRas, uv);

              // シェイプの形で抜く場合は、アルファは必ずMaxにする
              if (m_alphaMode == ShapeAlpha) pix.m = TPixel64::maxChannelValue;

              if (m_parentInstance->antialiasEnabled())
                pix.m = (unsigned short)((int)pix.m * (int)(*alpha_p) /
                                         (int)UCHAR_MAX);

              *outpix = pix;

              m_subPointOccupation[subIndex] = true;
            }
          }
        }
      }
    }
  }
}
//---------------------------------------------------

void CombineResults_Worker::run() {
  int i = m_from * m_outRas->getLx();
  for (int y = m_from; y < m_to; y++) {
    TPixel64* pix = m_outRas->pixels(y);
    for (int x = 0; x < m_outRas->getLx(); x++, pix++, i++) {
      for (int t = 0; t < m_outRasList.size(); t++) {
        if (m_subPointOccupationList[t][i]) {
          *pix = m_outRasList[t]->pixels(y)[x];
          break;
        }
      }
    }
  }
}

//---------------------------------------------------

void ResampleResults_Worker::run() {
  int subAmount2    = m_subAmount * m_subAmount;
  TPixel64** outpix = new TPixel64*[m_subAmount];
  for (int y = m_from; y < m_to; y++) {
    TPixel64* retpix = m_retRas->pixels(y);
    for (int s = 0; s < m_subAmount; s++)
      outpix[s] = m_outRas->pixels(y * m_subAmount + s);

    for (int x = 0; x < m_retRas->getLx(); x++, retpix++) {
      unsigned long long rr = 0, gg = 0, bb = 0, mm = 0;
      for (int sy = 0; sy < m_subAmount; sy++) {
        for (int sx = 0; sx < m_subAmount; sx++, outpix[sy]++) {
          if (outpix[sy]->m) {  // add premultiplied color
            rr += (unsigned long long)outpix[sy]->r *
                  (unsigned long long)outpix[sy]->m /
                  (unsigned long long)USHRT_MAX;
            gg += (unsigned long long)outpix[sy]->g *
                  (unsigned long long)outpix[sy]->m /
                  (unsigned long long)USHRT_MAX;
            bb += (unsigned long long)outpix[sy]->b *
                  (unsigned long long)outpix[sy]->m /
                  (unsigned long long)USHRT_MAX;
            mm += (unsigned long long)outpix[sy]->m;
          }
        }
      }
      if (m_antialias) {
        retpix->r = unsigned short(rr / subAmount2);
        retpix->g = unsigned short(gg / subAmount2);
        retpix->b = unsigned short(bb / subAmount2);
        retpix->m = unsigned short(mm / subAmount2);
      } else {  // アンチ無しの場合
        if (mm) {
          retpix->r = unsigned short(rr * (unsigned long long)USHRT_MAX / mm);
          retpix->g = unsigned short(gg * (unsigned long long)USHRT_MAX / mm);
          retpix->b = unsigned short(bb * (unsigned long long)USHRT_MAX / mm);
          retpix->m = USHRT_MAX;
        } else {
          retpix->r = 0;
          retpix->g = 0;
          retpix->b = 0;
          retpix->m = 0;
        }
      }
    }
  }
  delete[] outpix;
}
//---------------------------------------------------
// 　ゆがんだ形状を描画する_マルチスレッド版
// 　三角形のラスタライズを行う
//---------------------------------------------------
TRaster64P IwRenderInstance::HEmapTrianglesToRaster_Multi(
    HEModel& model, TRaster64P srcRas, ShapePair* shape, QPoint& origin,
    const QPolygonF& parentShapePolygon) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // 計算範囲
  QRectF shapeBBox =
      shape->getBBox(m_frame, 1);  // TO のバウンディングボックス（IwaWarper座標系）
  shapeBBox          = QRectF(shapeBBox.left() * (double)workAreaSize.width(),
                              shapeBBox.top() * (double)workAreaSize.height(),
                              shapeBBox.width() * (double)workAreaSize.width(),
                              shapeBBox.height() * (double)workAreaSize.height());
  QRect boundingRect = shapeBBox.toRect()
                           .marginsAdded(QMargins(5, 5, 5, 5))
                           .intersected(QRect(QPoint(), workAreaSize));

  // 画面外にTo形状がはみ出しているとき
  if (boundingRect.isEmpty()) return TRaster64P();

  // 計算範囲の原点
  origin = boundingRect.topLeft();

  // サンプル点のためのオフセット
  QPointF sampleOffset(
      0.5f * (double)(srcRas->getLx() - workAreaSize.width()),
      0.5f * (double)(srcRas->getLy() - workAreaSize.height()));
  // 出力位置のオフセット
  QPointF outputOffset(origin);

  // アルファを、素材で抜くか、シェイプの形に抜くか
  AlphaMode alphaMode = m_project->getRenderSettings()->getAlphaMode();

  // サブピクセル分割数
  int subAmount  = 4;
  int subAmount2 = subAmount * subAmount;

  int maxThreadCount = QThreadPool::globalInstance()->maxThreadCount() -
                       QThreadPool::globalInstance()->activeThreadCount();

  // サブポイントが既に描画済みかどうか
  int size = boundingRect.width() * boundingRect.height() * subAmount2;

  // アルファチャンネルに用いる
  QImage shapeAlphaImg(boundingRect.width() * subAmount,
                       boundingRect.height() * subAmount,
                       QImage::Format_Grayscale8);
  {
    QTransform trans;
    trans.scale((qreal)subAmount, (qreal)subAmount);
    trans.translate(-outputOffset.x(), -outputOffset.y());
    QPolygonF scaledShape = trans.map(parentShapePolygon);
    shapeAlphaImg.fill(Qt::black);
    QPainter p(&shapeAlphaImg);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawPolygon(scaledShape, Qt::WindingFill);
  }

  TRaster64P outRas = TRaster64P(boundingRect.width() * subAmount,
                                 boundingRect.height() * subAmount);
  outRas->clear();
  {
    QList<bool*> subPointOccupationList;
    QList<TRaster64P> outRasList;
    int tmpStart = 0;
    for (int t = 0; t < 1; t++) {
      int tmpEnd = model.faces.size();

      // サブポイントが既に描画済みかどうか
      bool* subPointOccupation = new bool[size];
      std::fill_n(subPointOccupation, size, false);
      subPointOccupationList.push_back(subPointOccupation);

      // 結果を収めるラスタを準備する
      TRaster64P outRasT = TRaster64P(boundingRect.width() * subAmount,
                                      boundingRect.height() * subAmount);
      outRasT->clear();
      outRasList.push_back(outRasT);

      MapTrianglesToRaster_Worker* task = new MapTrianglesToRaster_Worker(
          tmpStart, tmpEnd, &model, this, sampleOffset, outputOffset, outRasT,
          srcRas, subPointOccupation, subAmount, alphaMode, shapeAlphaImg);

      QThreadPool::globalInstance()->start(task);

      tmpStart = tmpEnd;
    }
    QThreadPool::globalInstance()->waitForDone();

    tmpStart = 0;
    for (int t = 0; t < maxThreadCount; t++) {
      int tmpEnd = (int)std::round((double)(outRas->getLy() * (t + 1)) /
                                   (double)maxThreadCount);
      if (tmpStart == tmpEnd) continue;

      CombineResults_Worker* task = new CombineResults_Worker(
          tmpStart, tmpEnd, outRas, subPointOccupationList, outRasList);

      QThreadPool::globalInstance()->start(task);

      tmpStart = tmpEnd;
    }
    QThreadPool::globalInstance()->waitForDone();

    for (bool* p : subPointOccupationList) delete[] p;
  }

  TRaster64P retRas = TRaster64P(boundingRect.width(), boundingRect.height());
  retRas->clear();

  int tmpStart = 0;
  for (int t = 0; t < maxThreadCount; t++) {
    int tmpEnd = (int)std::round((double)(retRas->getLy() * (t + 1)) /
                                 (double)maxThreadCount);
    if (tmpStart == tmpEnd) continue;

    ResampleResults_Worker* task = new ResampleResults_Worker(
        tmpStart, tmpEnd, outRas, retRas, subAmount, m_antialias);

    QThreadPool::globalInstance()->start(task);

    tmpStart = tmpEnd;
  }
  QThreadPool::globalInstance()->waitForDone();

  // 結果を返す
  return retRas;
}
//---------------------------------------------------
// ファイルに書き出す
//---------------------------------------------------

void IwRenderInstance::saveImage(TRaster64P ras) {
  TRasterImageP img(ras);

  // ファイルパスを得る
  QString path = m_project->getOutputPath(m_frame);

  OutputSettings* settings = m_project->getOutputSettings();

  QString ext = OutputSettings::getStandardExtension(settings->getSaver());

  TPropertyGroup* prop =
      settings->getFileFormatProperties(settings->getSaver());

  TImageWriterP writer(TFilePath(path.toStdWString()));
  writer->setProperties(prop);
  writer->save(img);
}

//---------------------------------------------------

// 各フレーム、各シェイプのかたまり毎に
// 頂点座標とUV座標をキャッシュする
void IwRenderInstance::HEcacheTriangles(HEModel& model, ShapePair* shape,
                                        const TDimension& srcDim,
                                        const QPolygonF& parentShapePolygon) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // サンプル点のためのオフセット
  QPointF sampleOffset(0.5 * (double)(srcDim.lx - workAreaSize.width()),
                       0.5 * (double)(srcDim.ly - workAreaSize.height()));
  auto getUV = [&](const QPointF& samplePoint) {
    return QPointF((samplePoint.x() + sampleOffset.x()) / (double)srcDim.lx,
                   (samplePoint.y() + sampleOffset.y()) / (double)srcDim.ly);
  };

  int count  = model.faces.size() * 3;
  int* ids   = new int[count];
  int* ids_p = ids;
  for (HEFace* face : model.faces) {
    if (!face->isVisible) {
      count -= 3;
      continue;
    }

    Halfedge* edge = face->halfedge;
    for (int i = 0; i < 3; i++) {
      ids_p[i] = model.vertices.indexOf(edge->vertex);
      edge     = edge->next;
    }
    ids_p += 3;
  }

  int pointCount = model.vertices.size();
  Vertex* vert   = new Vertex[pointCount];

  Vertex* v_p = vert;
  for (HEVertex* heV : model.vertices) {
    v_p->pos[0] = heV->to_pos.x();
    v_p->pos[1] = heV->to_pos.y();
    v_p->pos[2] = heV->to_pos.z();
    QPointF uv  = getUV(heV->from_pos.toPointF());
    v_p->uv[0]  = uv.x();
    v_p->uv[1]  = uv.y();
    v_p++;
  }

  IwTriangleCache::instance()->addCache(m_frame, shape,
                                        {vert, ids, count, true});
}

//---------------------------------------------------

bool IwRenderInstance::isCanceled() {
  if (!m_popup)
    return false;
  else
    return m_popup->isCanceled();
}