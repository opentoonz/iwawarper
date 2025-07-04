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

#include "half.h"

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
inline TPixel64 getPixelVal(TRaster64P ras, const QPoint& index) {
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
  auto lerp = [&](const TPixel64& pix1, const TPixel64& pix2, double ratio) {
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

//---------------------------------------------------
// uv座標を元に、最も近いピクセル値を得る。
//---------------------------------------------------
TPixel64 getNearestPixelVal(TRaster64P srcRas, QPointF& uv) {
  QPoint uvIndex(tround(uv.x() - 0.5), tround(uv.y() - 0.5));
  return getPixelVal(srcRas, uvIndex);
}

float getFloatFromUShort(unsigned short val) {
  FLOAT16 half;
  half.m_uiFormat = val;
  return FLOAT16::ToFloat32Fast(half);
}

inline float lerp(float v1, float v2, float ratio) {
  return v1 * (1.f - ratio) + v2 * ratio;
}

//---------------------------------------------------
// Morphological Supersampling
//---------------------------------------------------
TPixel64 getMlssPixelVal(TRaster64P srcRas, TRaster64P mlssRefRas,
                         QPointF& uv) {
  QPoint uvIndex(tround(uv.x() - 0.5), tround(uv.y() - 0.5));

  TPixel64 mlssVal = getPixelVal(mlssRefRas, uvIndex);

  if (mlssVal == TPixel64(0, 0, 0, 0)) return getPixelVal(srcRas, uvIndex);

  float ru = uv.x() - (float)uvIndex.x();
  float rv = uv.y() - (float)uvIndex.y();

  QPoint sampleOffset;
  // 上下方向のサンプル（左右の切片）
  if (mlssVal.r != 0 || mlssVal.g != 0) {
    float left  = getFloatFromUShort(mlssVal.r);
    float right = getFloatFromUShort(mlssVal.g);
    // 上ピクセルとのミックス
    if (left > 0.5 || right > 0.5) {
      if (rv > lerp(left, right, ru)) sampleOffset += QPoint(0, 1);
    }
    // 下ピクセルとのミックス
    else {
      if (rv < lerp(left, right, ru)) sampleOffset += QPoint(0, -1);
    }
  }
  // 左右方向のサンプル（上下の切片）
  if (mlssVal.b != 0 || mlssVal.m != 0) {
    float bottom = getFloatFromUShort(mlssVal.b);
    float top    = getFloatFromUShort(mlssVal.m);
    // 右ピクセルとのミックス
    if (bottom > 0.5 || top > 0.5) {
      if (ru > lerp(bottom, top, rv)) sampleOffset += QPoint(1, 0);
    }
    // 左ピクセルとのミックス
    else {
      if (ru < lerp(bottom, top, rv)) sampleOffset += QPoint(-1, 0);
    }
  }

  return getPixelVal(srcRas, uvIndex + sampleOffset);
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
IwRenderInstance::IwRenderInstance(int frame, int outputFrame,
                                   IwProject* project, OutputSettings* os,
                                   bool isPreview, RenderProgressPopup* popup)
    : m_frame(frame)
    , m_outputFrame(outputFrame)
    , m_project(project)
    , m_outputSettings(os)
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

  int targetShapeTag = m_outputSettings->getShapeTagId();
  // リサンプル
  ResampleMode resampleMode = m_project->getRenderSettings()->getResampleMode();

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
          if (warpedLayerRas) {
            if (resampleMode == AreaAverage)
              TRop::over(morphedRaster, warpedLayerRas,
                         TPoint(origin.x(), origin.y()));
            else
              TRop::over(morphedRaster, warpedLayerRas,
                         TTranslation(origin.x(), origin.y()),
                         TRop::ClosestPixel);
          }
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

  int targetShapeTag = m_outputSettings->getShapeTagId();

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
          QPoint dummyOrigin;
          // レイヤの画像をワープする
          warpLayer(layer, tmpShapes, true, dummyOrigin);
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

  // ここで、Matteに指定したレイヤがある場合、マット画像を作成する
  // shapes.last()がこのグループの親シェイプ
  TRasterGR8P matteRas = createMatteRas(shapes.last());

  // ここで、Morphological
  // Supersamplingを行う場合、サンプル用の境界線マップ画像を生成する
  TRaster64P mlssRefRas = createMLSSRefRas(inRas);

  // ゆがんだ形状を描画する
  TRaster64P outRas = HEmapTrianglesToRaster_Multi(
      model, inRas, matteRas, mlssRefRas, shapes.last(), origin,
      QPolygonF(parentShapeVerticesTo));

  return outRas;
}

//===================================================

//---------------------------------------------------
// 素材ラスタを得る
//---------------------------------------------------
TRasterP IwRenderInstance::getLayerRaster(IwLayer* layer, bool convertTo16bpc) {
  TImageP img = layer->getImage(m_frame);
  if (!img.getPointer()) return 0;

  TRasterP ras;
  TRasterImageP ri = (TRasterImageP)img;
  TToonzImageP ti  = (TToonzImageP)img;
  // 形式が合わなければ 0 を返す
  if (!ri && !ti)
    return 0;
  else if (ri) {
    TRasterP tmpRas = ri->getRaster();
    // pixelSize = 4 なら 8bpc、8 なら 16bpc
    // 8bitなら16bitに変換
    if (tmpRas->getPixelSize() == 4 && convertTo16bpc) {
      ras = TRaster64P(tmpRas->getWrap(), tmpRas->getLy());
      TRop::convert(ras, tmpRas);
    } else
      ras = tmpRas;
  } else if (ti) {
    TRasterCM32P rascm;
    rascm           = ti->getRaster();
    TRasterP tmpRas = TRaster32P(rascm->getWrap(), rascm->getLy());
    TRop::convert(tmpRas, rascm, ti->getPalette());
    if (convertTo16bpc) {
      ras = TRaster64P(tmpRas->getWrap(), tmpRas->getLy());
      TRop::convert(ras, tmpRas);
    } else
      ras = tmpRas;
  }
  return ras;
}

//---------------------------------------------------
// ワープ先のCorrVecの長さに合わせ、
// あまり短いベクトルはまとめながら格納していく
//---------------------------------------------------

void IwRenderInstance::getCorrVectors(IwLayer* /*layer*/,
                                      QList<ShapePair*>& shapes,
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
    // 対応点の分割点のウェイトを求める
    QList<double> discreteCorrWeightsFrom =
        shapePair->getDiscreteCorrWeights(m_frame, 0);
    QList<double> discreteCorrWeightsTo =
        shapePair->getDiscreteCorrWeights(m_frame, 1);

    assert(discreteCorrValuesFrom.size() == discreteCorrWeightsFrom.size());

    QPointF firstPosFrom, lastPosFrom, prevPosFrom, firstPosTo, lastPosTo,
        prevPosTo;

    double firstWeightFrom, lastWeightFrom, prevWeightFrom, firstWeightTo,
        lastWeightTo, prevWeightTo;

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

      double tmpWeightFrom = discreteCorrWeightsFrom.at(d);
      double tmpWeightTo   = discreteCorrWeightsTo.at(d);
      assert(tmpWeightTo > 0.5);
      if (d == 0) {
        firstPosFrom    = tmpPosFrom;
        firstPosTo      = tmpPosTo;
        firstWeightFrom = tmpWeightFrom;
        firstWeightTo   = tmpWeightTo;
      } else {
        if (d == discreteCorrValuesFrom.size() - 1) {
          lastPosFrom    = tmpPosFrom;
          lastPosTo      = tmpPosTo;
          lastWeightFrom = tmpWeightFrom;
          lastWeightTo   = tmpWeightTo;
        }

        // ここで、ワープ先（Toの方）のCorrVecの長さが、1ピクセルより短い場合、
        //  次のCorrVecと連結するようにする
        // さらに、コントロールポイントと一致するCorrPointは残す
        QPointF vec = tmpPosTo - prevPosTo;
        if (vec.x() * vec.x() + vec.y() * vec.y() < 1.0 &&
            !isDecimal(discreteCorrValuesFrom.at(d)) &&
            !isDecimal(discreteCorrValuesTo.at(d)))
          continue;

        CorrVector corrVec = {{prevPosFrom, tmpPosFrom},
                              {prevPosTo, tmpPosTo},
                              depth,
                              false,
                              {prevWeightFrom, tmpWeightFrom},
                              {prevWeightTo, tmpWeightTo}};

        corrVectors.append(corrVec);
      }
      prevPosFrom    = tmpPosFrom;
      prevPosTo      = tmpPosTo;
      prevWeightFrom = tmpWeightFrom;
      prevWeightTo   = tmpWeightTo;

      if (shapePair->isParent()) {
        parentShapeVerticesFrom.append(tmpPosFrom);
        parentShapeVerticesTo.append(tmpPosTo);
      }
    }
    // 閉じたシェイプの場合、最後と最初を繋ぐベクトルを追加
    if (shapePair->isClosed()) {
      CorrVector corrVec = {{lastPosFrom, firstPosFrom},
                            {lastPosTo, firstPosTo},
                            depth,
                            false,
                            {lastWeightFrom, firstWeightFrom},
                            {lastWeightTo, firstWeightTo}};
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
  int i = 0;
  for (CorrVector& corrVec : corrVectors) {
    // 別のストロークの端点同士が一致している場合、点を使いまわす
    HEVertex* start = model.findVertex(corrVec.from_p[0], corrVec.to_p[0]);
    // 一つめの点、または前のCorrVectorの終端点に一致しない
    // （＝別のストロークのスタート点の）場合、新規に点を追加

    if (!start) {
      start = new HEVertex(corrVec.from_p[0], corrVec.to_p[0],
                           (double)corrVec.stackOrder, corrVec.from_weight[0],
                           corrVec.to_weight[0]);
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
                         (double)corrVec.stackOrder, corrVec.from_weight[1],
                         corrVec.to_weight[1]);
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
    double to_ymin = 10000;
    double to_ymax = -10000;
    double to_xmin = 10000;
    double to_xmax = -10000;
    QPointF sp[3], wp[3];
    QPointF spMin, spMax;  // srcのバウンディングボックス
    Halfedge* he = face->halfedge;
    for (int p = 0; p < 3; p++) {
      HEVertex* v = he->vertex;
      // ここでサンプル点のためのオフセットを加える
      sp[p] = v->from_pos.toPointF() + m_sampleOffset;
      wp[p] = v->to_pos.toPointF() - m_outputOffset;
      // 範囲を更新
      if (wp[p].y() < to_ymin) to_ymin = wp[p].y();
      if (wp[p].y() > to_ymax) to_ymax = wp[p].y();
      if (wp[p].x() < to_xmin) to_xmin = wp[p].x();
      if (wp[p].x() > to_xmax) to_xmax = wp[p].x();
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

    int yminIndex = tfloor(to_ymin);
    int ymaxIndex = tceil(to_ymax);
    // outRasのサイズでクランプ
    if (yminIndex * m_subAmount >= m_outRas->getLy() ||
        ymaxIndex * m_subAmount < 0)
      continue;
    if (tfloor(to_xmin) * m_subAmount >= m_outRas->getLx() ||
        tceil(to_xmax) * m_subAmount < 0)
      continue;
    if (spMax.x() < 0 || spMin.x() >= m_srcRas->getLx() || spMax.y() < 0 ||
        spMin.y() >= m_srcRas->getLy())
      continue;

    yminIndex = std::max(0, yminIndex);
    ymaxIndex = std::min(m_outRas->getLy() / m_subAmount - 1, ymaxIndex);

    bool hasMatte = !!m_matteRas;

    // 各スキャンラインについてループ
    for (int y = yminIndex; y <= ymaxIndex; y++) {
      // マット処理
      TPixelGR8* mattePix;
      if (hasMatte) mattePix = m_matteRas->pixels(y);

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

        TPixelGR8* tmpMattePix;
        if (hasMatte) tmpMattePix = mattePix + xminIndex;

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

            // マスクを抜く
            if (hasMatte && *tmpMattePix == 0) continue;

            // もし、既にアルファ値が１なら、描かない
            if (outpix->m != TPixel64::maxChannelValue && tmpXPos >= xmin &&
                tmpXPos < xmax && m_subPointOccupation[subIndex] == false) {
              // TPixel64 pix = getPixelVal(srcRas, uv.toPoint());
              // uv座標を元に、ピクセル値をリニア補間で得る。
              TPixel64 pix = (m_resampleMode == AreaAverage)
                                 ? getInterpolatedPixelVal(m_srcRas, uv)
                             : (m_resampleMode == NearestNeighbor)
                                 ? getNearestPixelVal(m_srcRas, uv)
                                 : getMlssPixelVal(m_srcRas, m_mlssRefRas,
                                                   uv);  // MLSS case

              // シェイプの形で抜く場合は、アルファは必ずMaxにする
              if (m_alphaMode == ShapeAlpha) pix.m = TPixel64::maxChannelValue;

              if (m_parentInstance->antialiasEnabled())
                pix.m = (unsigned short)((int)pix.m * (int)(*alpha_p) /
                                         (int)UCHAR_MAX);

              *outpix = pix;

              m_subPointOccupation[subIndex] = true;
            }
          }

          if (hasMatte) tmpMattePix++;
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
        retpix->r = (unsigned short)(rr / subAmount2);
        retpix->g = (unsigned short)(gg / subAmount2);
        retpix->b = (unsigned short)(bb / subAmount2);
        retpix->m = (unsigned short)(mm / subAmount2);
      } else {  // アンチ無しの場合
        if (mm) {
          retpix->r = (unsigned short)(rr * (unsigned long long)USHRT_MAX / mm);
          retpix->g = (unsigned short)(gg * (unsigned long long)USHRT_MAX / mm);
          retpix->b = (unsigned short)(bb * (unsigned long long)USHRT_MAX / mm);
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
    HEModel& model, TRaster64P srcRas, TRasterGR8P matteRas,
    TRaster64P mlssRefRas, ShapePair* shape, QPoint& origin,
    const QPolygonF& parentShapePolygon) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // 計算範囲
  QRectF shapeBBox = shape->getBBox(
      m_frame, 1);  // TO のバウンディングボックス（IwaWarper座標系）
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

  // リサンプル
  ResampleMode resampleMode = m_project->getRenderSettings()->getResampleMode();

  // サブピクセル分割数
  int subAmount  = (resampleMode == AreaAverage) ? 4 : 1;
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
          srcRas, matteRas, mlssRefRas, subPointOccupation, subAmount,
          alphaMode, resampleMode, shapeAlphaImg);

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
// マット画像を作成する
//---------------------------------------------------
TRasterGR8P IwRenderInstance::createMatteRas(ShapePair* shape) {
  ShapePair::MatteInfo matteInfo = shape->matteInfo();
  if (matteInfo.layerName.isEmpty()) return TRasterGR8P();
  if (matteInfo.colors.isEmpty()) return TRasterGR8P();

  IwLayer* matteLayer = m_project->getLayerByName(matteInfo.layerName);
  if (!matteLayer) return TRasterGR8P();

  TRaster32P ras = (TRaster32P)getLayerRaster(matteLayer, false);
  if (!ras) return TRasterGR8P();  // とりあえず 8bpcのマット画像にのみ対応

  // 計算範囲
  QSize workAreaSize = m_project->getWorkAreaSize();
  QRectF shapeBBox   = shape->getBBox(
      m_frame, 1);  // TO のバウンディングボックス（IwaWarper座標系）
  shapeBBox = QRectF(shapeBBox.left() * (double)workAreaSize.width(),
                     shapeBBox.top() * (double)workAreaSize.height(),
                     shapeBBox.width() * (double)workAreaSize.width(),
                     shapeBBox.height() * (double)workAreaSize.height());

  QRect boundingRect = shapeBBox.toRect()
                           .marginsAdded(QMargins(5, 5, 5, 5))
                           .intersected(QRect(QPoint(), workAreaSize));

  // 画面外にTo形状がはみ出しているとき
  if (boundingRect.isEmpty()) return TRasterGR8P();

  // サンプル点のためのオフセット
  QPointF sampleOffset(0.5f * (double)(ras->getLx() - workAreaSize.width()) +
                           boundingRect.left(),
                       0.5f * (double)(ras->getLy() - workAreaSize.height()) +
                           boundingRect.top());

  TRasterGR8P matteRas =
      TRasterGR8P(boundingRect.width(), boundingRect.height());
  matteRas->clear();

  // 既にチェック済みのピクセル値
  QList<TPixel32> inPixList, outPixList;

  // matteRasのスキャンラインごとに
  for (int y = 0; y < matteRas->getLy(); y++) {
    TPixelGR8* mattePix = matteRas->pixels(y);

    int orgY = y + (int)std::round(sampleOffset.y());
    // matteLayerの外側の場合、matteは0
    if (orgY < 0 || orgY >= ras->getLy()) {
      for (int x = 0; x < matteRas->getLx(); x++, mattePix++) *mattePix = 0;
      continue;  // 次の行へ
    }

    TPixel32* orgPix = ras->pixels(orgY);
    int orgX         = (int)std::round(sampleOffset.x());

    // matteLayerが左にはみ出ている部分、matteは0
    int x = 0;
    for (; orgX < 0; orgX++, mattePix++, x++) {
      *mattePix = 0;
    }

    orgPix += orgX;

    for (; x < matteRas->getLx(); x++, mattePix++, orgX++) {
      // matteLayerが右にはみ出ている部分、matteは0
      if (orgX >= ras->getLx()) {
        *mattePix = 0;
      } else {
        if (inPixList.contains(*orgPix))
          *mattePix = 1;
        else if (inPixList.contains(*orgPix))
          *mattePix = 0;
        else {  // 判定処理を行う
          bool found = false;
          for (auto matteCol : matteInfo.colors) {
            if (std::abs(matteCol.red() - orgPix->r) <= matteInfo.tolerance &&
                std::abs(matteCol.green() - orgPix->g) <= matteInfo.tolerance &&
                std::abs(matteCol.blue() - orgPix->b) <= matteInfo.tolerance) {
              found = true;
              break;
            }
          }
          if (found) {
            *mattePix = 1;
            inPixList.append(*orgPix);
          } else {
            *mattePix = 0;
            outPixList.append(*orgPix);
          }
        }
        orgPix++;
      }
    }
  }

  // マスクを太らせる処理
  int dilate = m_project->getRenderSettings()->getMatteDilate();
  for (int d = 1; d <= dilate; d++) {
    for (int y = 0; y < matteRas->getLy(); y++) {
      TPixelGR8* mattePix_up   = matteRas->pixels((y == 0) ? 0 : y - 1);
      TPixelGR8* mattePix      = matteRas->pixels(y);
      TPixelGR8* mattePix_down = matteRas->pixels(
          (y == matteRas->getLy() - 1) ? matteRas->getLy() - 1 : y + 1);

      for (int x = 0; x < matteRas->getLx();
           x++, mattePix++, mattePix_up++, mattePix_down++) {
        // matte の値が0のものだけ処理をする
        if (*mattePix != 0) continue;

        // 近傍にdがあったら、d+1を入れる
        if (x > 0) {
          if (*(mattePix_up - 1) == d || *(mattePix - 1) == d ||
              *(mattePix_down - 1) == d) {
            *mattePix = d + 1;
            continue;
          }
        }
        if (*(mattePix_up) == d || *(mattePix_down) == d) {
          *mattePix = d + 1;
          continue;
        }
        if (x < matteRas->getLx() - 1) {
          if (*(mattePix_up + 1) == d || *(mattePix + 1) == d ||
              *(mattePix_down + 1) == d) {
            *mattePix = d + 1;
            continue;
          }
        }
      }
    }
  }

  return matteRas;
}

//---------------------------------------------------
// ファイルに書き出す
//---------------------------------------------------

void IwRenderInstance::saveImage(TRaster64P ras) {
  TRasterImageP img(ras);

  // ファイルパスを得る
  QString path = m_outputSettings->getPath(m_frame, m_outputFrame,
                                           m_project->getProjectName());

  QString ext =
      OutputSettings::getStandardExtension(m_outputSettings->getSaver());

  TPropertyGroup* prop =
      m_outputSettings->getFileFormatProperties(m_outputSettings->getSaver());

  TImageWriterP writer(TFilePath(path.toStdWString()));
  writer->setProperties(prop);
  writer->setBackgroundColor(TPixel32::White);
  writer->save(img);
}

//---------------------------------------------------

// 各フレーム、各シェイプのかたまり毎に
// 頂点座標とUV座標をキャッシュする
void IwRenderInstance::HEcacheTriangles(
    HEModel& model, ShapePair* shape, const TDimension& srcDim,
    const QPolygonF& /*parentShapePolygon*/) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // サンプル点のためのオフセット
  QPointF sampleOffset(0.5 * (double)(srcDim.lx - workAreaSize.width()),
                       0.5 * (double)(srcDim.ly - workAreaSize.height()));
  auto getUV = [&](const QVector2D& samplePoint) {
    return QVector2D((samplePoint.x() + sampleOffset.x()) / (double)srcDim.lx,
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

  int pointCount   = model.vertices.size();
  MeshVertex* vert = new MeshVertex[pointCount];

  MeshVertex* v_p = vert;
  for (HEVertex* heV : model.vertices) {
    v_p->setPosition(heV->to_pos);
    v_p->setUV(getUV(heV->from_pos.toVector2D()));
    v_p++;
  }

  IwTriangleCache::instance()->addCache(m_frame, shape,
                                        {vert, ids, pointCount, count, true});
}

//---------------------------------------------------

bool IwRenderInstance::isCanceled() {
  if (!m_popup)
    return false;
  else
    return m_popup->isCanceled();
}
