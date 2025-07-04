//---------------------------------------------------
// IwRenderInstance
// �e�t���[�����Ƃɍ����A�����_�����O�v�Z���s������
// IwRenderCommand::onPreview��������
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

// �O�p�`���\�[�g���� ���Ԃ��Ă���Ƃ��A�D��I��true�B
//  tri1��Depth�������i�������j�Ƃ���True
bool HEdepthLessThan(const HEFace* tri1, const HEFace* tri2) {
  // ���Ԃ�`�F�b�N
  bool tri1IsUra = tri1->size() < 0.0;
  bool tri2IsUra = tri2->size() < 0.0;
  if (tri1IsUra != tri2IsUra) return (tri1IsUra) ? true : false;

  // �����V�F�C�v���̕����O�p�`��D�悳����
  bool tri1IsConstDepth = tri1->hasConstantDepth();
  bool tri2IsConstDepth = tri2->hasConstantDepth();
  if (tri1IsConstDepth != tri2IsConstDepth) return tri2IsConstDepth;

  // �f�v�X�`�F�b�N
  return tri1->centroidDepth() < tri2->centroidDepth();
}

// double�l���������ǂ������`�F�b�N����
bool isDecimal(double val) { return val == std::floor(val); }

//---------------------------------------------------
// ���W����F��Ԃ��B�͈͊O�̏ꍇ�͓����F��Ԃ�
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
// uv���W�����ɁA�s�N�Z���l�����j�A��Ԃœ���B
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
// uv���W�����ɁA�ł��߂��s�N�Z���l�𓾂�B
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
  // �㉺�����̃T���v���i���E�̐ؕЁj
  if (mlssVal.r != 0 || mlssVal.g != 0) {
    float left  = getFloatFromUShort(mlssVal.r);
    float right = getFloatFromUShort(mlssVal.g);
    // ��s�N�Z���Ƃ̃~�b�N�X
    if (left > 0.5 || right > 0.5) {
      if (rv > lerp(left, right, ru)) sampleOffset += QPoint(0, 1);
    }
    // ���s�N�Z���Ƃ̃~�b�N�X
    else {
      if (rv < lerp(left, right, ru)) sampleOffset += QPoint(0, -1);
    }
  }
  // ���E�����̃T���v���i�㉺�̐ؕЁj
  if (mlssVal.b != 0 || mlssVal.m != 0) {
    float bottom = getFloatFromUShort(mlssVal.b);
    float top    = getFloatFromUShort(mlssVal.m);
    // �E�s�N�Z���Ƃ̃~�b�N�X
    if (bottom > 0.5 || top > 0.5) {
      if (ru > lerp(bottom, top, rv)) sampleOffset += QPoint(1, 0);
    }
    // ���s�N�Z���Ƃ̃~�b�N�X
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
// �����_�����O
//---------------------------------------------------
void IwRenderInstance::doRender() {
  if (isCanceled()) return;
  // ���ʂ����߂郉�X�^
  QSize workAreaSize = m_project->getWorkAreaSize();
  TRaster64P morphedRaster(workAreaSize.width(), workAreaSize.height());
  morphedRaster->fill(TPixel64::Transparent);
  if (isCanceled()) return;

  int targetShapeTag = m_outputSettings->getShapeTagId();
  // ���T���v��
  ResampleMode resampleMode = m_project->getRenderSettings()->getResampleMode();

  // ������A�e���C���ɂ���
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    if (isCanceled()) return;
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;
    // ���C���̉摜��������΃X�L�b�v
    if (layer->getImageFileName(m_frame).isEmpty()) continue;
    // ���C���ɃV�F�C�v��������΃X�L�b�v
    if (layer->getShapePairCount() == 0) continue;
    // ���C���������_�����O��\���Ȃ�X�L�b�v
    if (!layer->isVisibleInRender()) continue;
    // �V�F�C�v�������������Ă���
    QList<ShapePair*> tmpShapes;
    for (int s = layer->getShapePairCount() - 1; s >= 0; s--) {
      if (isCanceled()) return;
      ShapePair* shape = layer->getShapePair(s);
      if (!shape) continue;
      // �V�F�C�v�����X�g�ɒǉ�
      tmpShapes.append(shape);
      // �q�V�F�C�v�̂Ƃ��A���X�g�ɒǉ����Ď��̃V�F�C�v��
      if (!shape->isParent()) continue;
      // �e�V�F�C�v�̂Ƃ��A���[�v�������s��
      else {
        std::cout << shape->getName().toStdString() << std::endl;
        // �e�V�F�C�v���^�[�Q�b�g�ɂȂ��Ă��Ȃ��ꍇ�A�S�Ă̎q�V�F�C�v�������_�����O���Ȃ�
        if (shape->isRenderTarget(targetShapeTag)) {
          // ���C���̉摜�����[�v����
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
        // ���X�g�����Z�b�g
        tmpShapes.clear();
      }
    }
  }
  if (isCanceled()) return;

  // �t�@�C���ɏ����o��
  saveImage(morphedRaster);

  // ������invalid�ȃL���b�V���͂����g���Ă��Ȃ��V�F�C�v�̂͂�
  IwTriangleCache::instance()->removeInvalid(m_frame);
}

//---------------------------------------------------
// �����_�����O
//---------------------------------------------------
void IwRenderInstance::doPreview() {
  // ���ʂ����߂郉�X�^
  QSize workAreaSize = m_project->getWorkAreaSize();

  int targetShapeTag = m_outputSettings->getShapeTagId();

  // ������A�e���C���ɂ���
  for (int lay = m_project->getLayerCount() - 1; lay >= 0; lay--) {
    IwLayer* layer = m_project->getLayer(lay);
    if (!layer) continue;
    // ���C���̉摜��������΃X�L�b�v
    if (layer->getImageFileName(m_frame).isEmpty()) continue;
    // ���C���ɃV�F�C�v��������΃X�L�b�v
    if (layer->getShapePairCount() == 0) continue;
    // ���C���������_�����O��\���Ȃ�X�L�b�v
    if (!layer->isVisibleInRender()) continue;

    // �V�F�C�v�������������Ă���
    QList<ShapePair*> tmpShapes;
    for (int s = layer->getShapePairCount() - 1; s >= 0; s--) {
      ShapePair* shape = layer->getShapePair(s);
      if (!shape) continue;
      // �V�F�C�v�����X�g�ɒǉ�
      tmpShapes.append(shape);
      // �q�V�F�C�v�̂Ƃ��A���X�g�ɒǉ����Ď��̃V�F�C�v��
      if (!shape->isParent()) continue;
      // �e�V�F�C�v�̂Ƃ��A���[�v�������s��
      else {
        // �e�V�F�C�v���^�[�Q�b�g�ɂȂ��Ă��Ȃ��ꍇ�A�S�Ă̎q�V�F�C�v�������_�����O���Ȃ�
        if (shape->isRenderTarget(targetShapeTag)) {
          QPoint dummyOrigin;
          // ���C���̉摜�����[�v����
          warpLayer(layer, tmpShapes, true, dummyOrigin);
        }
        // ���X�g�����Z�b�g
        tmpShapes.clear();
      }
    }
  }
  // ������invalid�ȃL���b�V���͂����g���Ă��Ȃ��V�F�C�v�̂͂�
  IwTriangleCache::instance()->removeInvalid(m_frame);
}
//---------------------------------------------------
// �� ���ʂ��L���b�V���Ɋi�[
//---------------------------------------------------
void IwRenderInstance::storeImageToCache(TRaster64P morphedRaster) {
  TRasterImageP img(morphedRaster);
  // �v�Z���I�������A�L���b�V���ɕۑ�
  // �L���b�V���̃G�C���A�X�𓾂�
  QString cacheAlias =
      QString("PRJ%1_PREVIEW_FRAME%2").arg(m_project->getId()).arg(m_frame);

  IwImageCache::instance()->add(cacheAlias, img);
}

//---------------------------------------------------
// ���C�������[�v
//---------------------------------------------------
TRaster64P IwRenderInstance::warpLayer(IwLayer* layer,
                                       QList<ShapePair*>& shapes,
                                       bool isPreview, QPoint& origin) {
  // �摜���擾
  TRasterP ras = getLayerRaster(layer);
  if (!ras) return TRaster64P();

  // �v���r���[�ŁA���łɗL���ȃL���b�V��������ꍇ�͌v�Z���Ȃ�
  if (isPreview && IwTriangleCache::instance()->isValid(m_frame, shapes.last()))
    return TRaster64P();

  QList<CorrVector> corrVectors;
  // QPolygonF �𐶐����A�`��̓��O����ɗp����
  QVector<QPointF> parentShapeVerticesFrom, parentShapeVerticesTo;
  getCorrVectors(layer, shapes, corrVectors, parentShapeVerticesFrom,
                 parentShapeVerticesTo);

  if (isCanceled()) return TRaster64P();

  HEModel model;
  QPolygonF parentShapePolygon(parentShapeVerticesFrom);
  // fromShape�����ɁA���b�V����؂�:
  HEcreateTriangleMesh(corrVectors, model, parentShapePolygon, ras->getSize());

  // �O�p�`���\�[�g����
  std::sort(model.faces.begin(), model.faces.end(), HEdepthLessThan);

  if (isCanceled()) return TRaster64P();

  // �O�p�`�̈ʒu���L���b�V������
  HEcacheTriangles(model, shapes.last(), ras->getSize(), parentShapePolygon);

  if (isPreview) return TRaster64P();

  // �����AsrcRas��32bit�Ȃ�A������64bit��
  TRaster64P inRas;
  if (ras->getPixelSize() == 4) {
    inRas = TRaster64P(ras->getWrap(), ras->getLy());
    TRop::convert(inRas, ras);
  } else
    inRas = ras;

  // demultiply����
  if (checkIsPremultiplied(inRas)) TRop::depremultiply(inRas);

  // �����ŁAMatte�Ɏw�肵�����C��������ꍇ�A�}�b�g�摜���쐬����
  // shapes.last()�����̃O���[�v�̐e�V�F�C�v
  TRasterGR8P matteRas = createMatteRas(shapes.last());

  // �����ŁAMorphological
  // Supersampling���s���ꍇ�A�T���v���p�̋��E���}�b�v�摜�𐶐�����
  TRaster64P mlssRefRas = createMLSSRefRas(inRas);

  // �䂪�񂾌`���`�悷��
  TRaster64P outRas = HEmapTrianglesToRaster_Multi(
      model, inRas, matteRas, mlssRefRas, shapes.last(), origin,
      QPolygonF(parentShapeVerticesTo));

  return outRas;
}

//===================================================

//---------------------------------------------------
// �f�ރ��X�^�𓾂�
//---------------------------------------------------
TRasterP IwRenderInstance::getLayerRaster(IwLayer* layer, bool convertTo16bpc) {
  TImageP img = layer->getImage(m_frame);
  if (!img.getPointer()) return 0;

  TRasterP ras;
  TRasterImageP ri = (TRasterImageP)img;
  TToonzImageP ti  = (TToonzImageP)img;
  // �`��������Ȃ���� 0 ��Ԃ�
  if (!ri && !ti)
    return 0;
  else if (ri) {
    TRasterP tmpRas = ri->getRaster();
    // pixelSize = 4 �Ȃ� 8bpc�A8 �Ȃ� 16bpc
    // 8bit�Ȃ�16bit�ɕϊ�
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
// ���[�v���CorrVec�̒����ɍ��킹�A
// ���܂�Z���x�N�g���͂܂Ƃ߂Ȃ���i�[���Ă���
//---------------------------------------------------

void IwRenderInstance::getCorrVectors(IwLayer* /*layer*/,
                                      QList<ShapePair*>& shapes,
                                      QList<CorrVector>& corrVectors,
                                      QVector<QPointF>& parentShapeVerticesFrom,
                                      QVector<QPointF>& parentShapeVerticesTo) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // 1�s�N�Z��������̃V�F�C�v���W�n�̒l�𓾂�
  QPointF onePix(1.0 / (double)workAreaSize.width(),
                 1.0 / (double)workAreaSize.height());

  // ���t���[���̃V�F�C�v���Ԃ��A�Ή��_�x�N�g���̏W�܂�����
  // �e�V�F�C�v�ɂ���
  for (int sp = 0; sp < shapes.size(); sp++) {
    int depth            = sp;
    ShapePair* shapePair = shapes.at(sp);

    // �Ή��_�̕����_�̃x�W�G���W�l�����߂�
    QList<double> discreteCorrValuesFrom =
        shapePair->getDiscreteCorrValues(onePix, m_frame, 0);
    QList<double> discreteCorrValuesTo =
        shapePair->getDiscreteCorrValues(onePix, m_frame, 1);
    // �Ή��_�̕����_�̃E�F�C�g�����߂�
    QList<double> discreteCorrWeightsFrom =
        shapePair->getDiscreteCorrWeights(m_frame, 0);
    QList<double> discreteCorrWeightsTo =
        shapePair->getDiscreteCorrWeights(m_frame, 1);

    assert(discreteCorrValuesFrom.size() == discreteCorrWeightsFrom.size());

    QPointF firstPosFrom, lastPosFrom, prevPosFrom, firstPosTo, lastPosTo,
        prevPosTo;

    double firstWeightFrom, lastWeightFrom, prevWeightFrom, firstWeightTo,
        lastWeightTo, prevWeightTo;

    // �e�x�W�G���W���i�[
    for (int d = 0; d < discreteCorrValuesFrom.size(); d++) {
      QPointF tmpPosFrom = shapePair->getBezierPosFromValue(
          m_frame, 0, discreteCorrValuesFrom.at(d));
      QPointF tmpPosTo = shapePair->getBezierPosFromValue(
          m_frame, 1, discreteCorrValuesTo.at(d));

      // �����Ńs�N�Z�����W�ɂ���
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

        // �����ŁA���[�v��iTo�̕��j��CorrVec�̒������A1�s�N�Z�����Z���ꍇ�A
        //  ����CorrVec�ƘA������悤�ɂ���
        // ����ɁA�R���g���[���|�C���g�ƈ�v����CorrPoint�͎c��
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
    // �����V�F�C�v�̏ꍇ�A�Ō�ƍŏ����q���x�N�g����ǉ�
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

B1) �_�Q���܂���\���傫�ȎO�p�`(super triangle)��ǉ�����
B2) ��������ɂ�����钸�_pi��}�`�ɒǉ�
  B2-2) pi���܂ގO�p�`ABC�𔭌���, ���̎O�p�`��AB pi, BC pi, CA pi
��3�̎O�p�`�ɕ����D ���̎�, ��AB, BC, CA���X�^�b�NS�ɐςށD B2-3)
�X�^�b�NS����ɂȂ�܂ňȉ����J��Ԃ�
    B2-3-1)�X�^�b�NS����ӂ�pop���C������AB�Ƃ���D
       ��AB���܂�2�̎O�p�`��ABC��ABD�Ƃ���
        if( AB����������ł��� ) �������Ȃ�
        else if( CD����������ł���l�p�`ABCD����)
          ��AB��clip�C��AD/DB/BC/CA���X�^�b�N�ɐς�
        else if( ABC�̊O�ډ~����D������ )
          ��AB��flip�C��AD/DB/BC/CA���X�^�b�N�ɐς�
        else �������Ȃ�
B3) ��������̕��A���s��
  B3-1)�e�������AB�ɂ��Ĉȉ����J��Ԃ�
  B3-2)���ݐ}�`��AB�ƌ�������S�ẴG�b�W���L���[K�ɑ}��
  B3-3)�L���[K����ɂȂ�܂Ŏ����J��Ԃ�
    B3-3-1)�L���[K�̐擪�v�f��pop����CD�Ƃ��DCD���܂�2�̎O�p�`��CDE, CDF�Ƃ���
      if( �l�p�`ECFD���� )
        �G�b�WCD��flip. �V���ȃG�b�WEF���X�^�b�NN��push.
      else
        �G�b�WCD���L���[K�Ɍ�납��push.
  B3-3) B-2-3�Ɠ��l�̏������L���[N�ɑ΂��čs��
    (�V���ȃG�b�W�̃h���l�[���̊m�F���s��)
B4) ��������ɂ������Ȃ����_pi�̒ǉ�
  B4-1) pi���܂ގO�p�`ABC�𔭌���, ���̎O�p�`��AB pi, BC pi, CA pi
��3�̎O�p�`�ɕ����D ��AB, BC, CA���X�^�b�NS�ɐςށD B4-2)
�X�^�b�NS�ɂ���B2-3-1�Ɠ��l�̏������J��Ԃ� B5) super
triangle���_�Ƃ���Ɋւ��S�ẴG�b�W����菜��

*/

void IwRenderInstance::HEcreateTriangleMesh(
    QList<CorrVector>& corrVectors,  // ���́B�ό`�O��̑Ή��_�x�N�g���̏W�܂�
    HEModel& model, const QPolygonF& parentShapePolygon,
    const TDimension& srcDim) {
  // HEModel model;
  //   B1) �_�Q���܂���\���傫�ȎO�p�`(super triangle)��ǉ�����
  QSize workAreaSize = m_project->getWorkAreaSize();
  QSizeF superRectSize((qreal)std::max(workAreaSize.width() * 3, srcDim.lx),
                       (qreal)std::max(workAreaSize.height() * 3, srcDim.ly));
  QPointF superRectTopLeft(
      -(superRectSize.width() - workAreaSize.width()) * 0.5,
      -(superRectSize.height() - workAreaSize.height()) * 0.5);
  QRectF superRect(superRectTopLeft, superRectSize);

  // �� IwaWarper�̌v�Z�Ɏg�����W��Ԃ͍������_�Ȃ̂ŁAQt�Ƃ͏㉺���]���Ă���B
  //  �@topLeft�������AbottomRight���E��ƂȂ�
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
  // �΂ߐ��̐��������
  superTri1->edgeFromVertex(superTR)->setConstrained(false);
  // ���_����͎c��
  superTR->constrained = true;
  superBL->constrained = true;

  //    B2) ��������ɂ�����钸�_pi��}�`�ɒǉ�
  int i = 0;
  for (CorrVector& corrVec : corrVectors) {
    // �ʂ̃X�g���[�N�̒[�_���m����v���Ă���ꍇ�A�_���g���܂킷
    HEVertex* start = model.findVertex(corrVec.from_p[0], corrVec.to_p[0]);
    // ��߂̓_�A�܂��͑O��CorrVector�̏I�[�_�Ɉ�v���Ȃ�
    // �i���ʂ̃X�g���[�N�̃X�^�[�g�_�́j�ꍇ�A�V�K�ɓ_��ǉ�

    if (!start) {
      start = new HEVertex(corrVec.from_p[0], corrVec.to_p[0],
                           (double)corrVec.stackOrder, corrVec.from_weight[0],
                           corrVec.to_weight[0]);
      // ���f���ɓ_��ǉ�
      //     B2 - 2) pi���܂ގO�p�`ABC�𔭌���, ���̎O�p�`��AB pi, BC pi, CA pi
      //     ��3�̎O�p�`�ɕ����D ���̎�, ��AB, BC, CA���X�^�b�NS�ɐςށD B2 -
      //     3) �X�^�b�NS����ɂȂ�܂ňȉ����J��Ԃ� B2 - 3 -
      //     1)�X�^�b�NS����ӂ�pop���C������AB�Ƃ���D
      //     ��AB���܂�2�̎O�p�`��ABC��ABD�Ƃ���
      //     if (AB����������ł���) �������Ȃ�
      //     else if (CD����������ł���l�p�`ABCD����)
      //       ��AB��clip�C��AD / DB / BC / CA���X�^�b�N�ɐς�
      //     else if (ABC�̊O�ډ~����D������)
      //       ��AB��flip�C��AD / DB / BC / CA���X�^�b�N�ɐς�
      //     else �������Ȃ�
      model.addVertex(start, nullptr);
      // model.print();
    }

    // ��ڂ̓_��ǉ�����
    HEVertex* end = model.findVertex(corrVec.from_p[1], corrVec.to_p[1]);
    if (!end) {
      end = new HEVertex(corrVec.from_p[1], corrVec.to_p[1],
                         (double)corrVec.stackOrder, corrVec.from_weight[1],
                         corrVec.to_weight[1]);
      model.addVertex(end, start);
    } else  // ���łɒǉ��ς݂̓_�̏ꍇ�i���Ȑ��̎n�_�ɖ߂��Ă����Ƃ��Ȃǁj�́A����̂ݕt������
      model.setConstraint(start, end, true);
    i++;
  }

  // �e�V�F�C�v�`��Ɋ܂܂�Ă��邩�ǂ���
  // ����CorrVec�̑}����A�ĕ����O�ɂP�x�����`�F�b�N����
  model.checkFaceVisibility(parentShapePolygon);
  // ���[�v
  for (int g = 0; g < m_precision; g++) {
    // �e�O�p�`�̏d�S��ǉ� �����͎O�p�`�𕪊����邩���f����ʐς�臒l
    //        B4) ��������ɂ������Ȃ����_pi�̒ǉ�
    //        B4 - 1) pi���܂ގO�p�`ABC�𔭌���, ���̎O�p�`��AB pi, BC pi, CA pi
    //        ��3�̎O�p�`�ɕ����D ��AB, BC, CA���X�^�b�NS�ɐςށD B4 - 2)
    //        �X�^�b�NS�ɂ���B2 - 3 - 1�Ɠ��l�̏������J��Ԃ�
    model.insertCentroids(m_faceSizeThreshold);
    // �Ȃ��܂���
    model.smooth(m_smoothingThreshold);
  }

  //        B5) super triangle���_�Ƃ���Ɋւ��S�ẴG�b�W����菜��
  // model.deleteSuperTriangles();
}

//---------------------------------------------------

void MapTrianglesToRaster_Worker::run() {
  // �e�O�p�`�ɂ��āi��O�̎O�p�`���珇�Ɂj
  for (int h = m_from; h < m_to; h++) {
    HEFace* face = m_model->faces[h];

    if (m_parentInstance->isCanceled()) return;

    if (!face->isVisible) continue;
    // �O�p�`�p�b�`�̒��_���i�[���AY���W�͈̔͂��擾
    double to_ymin = 10000;
    double to_ymax = -10000;
    double to_xmin = 10000;
    double to_xmax = -10000;
    QPointF sp[3], wp[3];
    QPointF spMin, spMax;  // src�̃o�E���f�B���O�{�b�N�X
    Halfedge* he = face->halfedge;
    for (int p = 0; p < 3; p++) {
      HEVertex* v = he->vertex;
      // �����ŃT���v���_�̂��߂̃I�t�Z�b�g��������
      sp[p] = v->from_pos.toPointF() + m_sampleOffset;
      wp[p] = v->to_pos.toPointF() - m_outputOffset;
      // �͈͂��X�V
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

    /* �����ŁAm_outRas�͏o�̓T�C�Y�� m_subAmount �{����Ă��邱�Ƃɒ��ӁI�I */
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
    // outRas�̃T�C�Y�ŃN�����v
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

    // �e�X�L�������C���ɂ��ă��[�v
    for (int y = yminIndex; y <= ymaxIndex; y++) {
      // �}�b�g����
      TPixelGR8* mattePix;
      if (hasMatte) mattePix = m_matteRas->pixels(y);

      // Y�����̕����ɂ��ă��[�v
      for (int suby = 0; suby < m_subAmount; suby++) {
        // ���݂̕���Y���W
        double tmpY = (double)y + (0.5 + (double)suby) / (double)m_subAmount;

        // LR�͈̔͂����߂�
        bool pointIsUpper[3];
        for (int p = 0; p < 3; p++) {
          pointIsUpper[p] = (wp[p].y() > tmpY) ? true : false;
        }
        // �S�Ĉ���Ɋ���Ă���ꍇ�͎��̃X�L�������C���� (�[�����̏ꍇ)
        if (pointIsUpper[0] == pointIsUpper[1] &&
            pointIsUpper[1] == pointIsUpper[2])
          continue;
        // X���W�͈̔�
        double xmin = 10000;
        double xmax = -10000;
        QPointF uvMin, uvMax;
        // �e�ӂ𒲂ׂ�
        for (int p = 0; p < 3; p++) {
          int next_p = (p + 1 == 3) ? 0 : p + 1;
          // ���݂̕ӂ��X�L�������C�����܂����ł���ꍇ
          if (pointIsUpper[p] != pointIsUpper[next_p]) {
            QPointF vec  = wp[next_p] - wp[p];
            double ratio = (tmpY - wp[p].y()) / vec.y();
            // vec.y()�͔�O�̂͂�
            double tmp_x = wp[p].x() + ratio * vec.x();
            // UV���W�����߂�
            QPointF tmp_uv = sp[p] * (1.0 - ratio) + sp[next_p] * ratio;
            // �͈͂��X�V
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

        // �P�T�u�s�N�Z���i�񂾂Ƃ���UV�̕ω��̓x���������߂�
        /// �P�s�N�Z���i�񂾂Ƃ���UV�̕ω��̓x���������߂�
        QPointF uvAdvancePerOneSubPix =
            (uvMax - uvMin) / (xmax - xmin) / m_subAmount;

        int xminIndex = tfloor(xmin);
        int xmaxIndex = tceil(xmax);
        // outRas�̃T�C�Y�ŃN�����v
        if (xminIndex * m_subAmount >= m_outRas->getLx() ||
            xmaxIndex * m_subAmount < 0)
          continue;
        xminIndex = std::max(0, xminIndex);
        xmaxIndex = std::min(m_outRas->getLx() / m_subAmount - 1, xmaxIndex);

        // �X�^�[�g�n�_:��ԍ��̃s�N�Z����UV�l�����߂�
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

        // �X�L�������C�������ɂ��ǂ�
        for (int x = xminIndex; x <= xmaxIndex; x++) {
          double tmpXPos = (double)x;
          // double tmpXPos = (double)x - 0.5f;
          // X�����̕����ɂ��ă��[�v
          for (int subx = 0; subx < m_subAmount; subx++,
                   tmpXPos += 1.0 / (double)m_subAmount,
                   uv += uvAdvancePerOneSubPix, subIndex++, outpix++,
                   alpha_p++) {
            if (uv.x() < 0 || uv.x() >= m_srcRas->getLx() || uv.y() < 0 ||
                uv.y() >= m_srcRas->getLy())
              continue;

            // �}�X�N�𔲂�
            if (hasMatte && *tmpMattePix == 0) continue;

            // �����A���ɃA���t�@�l���P�Ȃ�A�`���Ȃ�
            if (outpix->m != TPixel64::maxChannelValue && tmpXPos >= xmin &&
                tmpXPos < xmax && m_subPointOccupation[subIndex] == false) {
              // TPixel64 pix = getPixelVal(srcRas, uv.toPoint());
              // uv���W�����ɁA�s�N�Z���l�����j�A��Ԃœ���B
              TPixel64 pix = (m_resampleMode == AreaAverage)
                                 ? getInterpolatedPixelVal(m_srcRas, uv)
                             : (m_resampleMode == NearestNeighbor)
                                 ? getNearestPixelVal(m_srcRas, uv)
                                 : getMlssPixelVal(m_srcRas, m_mlssRefRas,
                                                   uv);  // MLSS case

              // �V�F�C�v�̌`�Ŕ����ꍇ�́A�A���t�@�͕K��Max�ɂ���
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
      } else {  // �A���`�����̏ꍇ
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
// �@�䂪�񂾌`���`�悷��_�}���`�X���b�h��
// �@�O�p�`�̃��X�^���C�Y���s��
//---------------------------------------------------
TRaster64P IwRenderInstance::HEmapTrianglesToRaster_Multi(
    HEModel& model, TRaster64P srcRas, TRasterGR8P matteRas,
    TRaster64P mlssRefRas, ShapePair* shape, QPoint& origin,
    const QPolygonF& parentShapePolygon) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // �v�Z�͈�
  QRectF shapeBBox = shape->getBBox(
      m_frame, 1);  // TO �̃o�E���f�B���O�{�b�N�X�iIwaWarper���W�n�j
  shapeBBox          = QRectF(shapeBBox.left() * (double)workAreaSize.width(),
                              shapeBBox.top() * (double)workAreaSize.height(),
                              shapeBBox.width() * (double)workAreaSize.width(),
                              shapeBBox.height() * (double)workAreaSize.height());
  QRect boundingRect = shapeBBox.toRect()
                           .marginsAdded(QMargins(5, 5, 5, 5))
                           .intersected(QRect(QPoint(), workAreaSize));

  // ��ʊO��To�`�󂪂͂ݏo���Ă���Ƃ�
  if (boundingRect.isEmpty()) return TRaster64P();

  // �v�Z�͈͂̌��_
  origin = boundingRect.topLeft();

  // �T���v���_�̂��߂̃I�t�Z�b�g
  QPointF sampleOffset(
      0.5f * (double)(srcRas->getLx() - workAreaSize.width()),
      0.5f * (double)(srcRas->getLy() - workAreaSize.height()));
  // �o�͈ʒu�̃I�t�Z�b�g
  QPointF outputOffset(origin);

  // �A���t�@���A�f�ނŔ������A�V�F�C�v�̌`�ɔ�����
  AlphaMode alphaMode = m_project->getRenderSettings()->getAlphaMode();

  // ���T���v��
  ResampleMode resampleMode = m_project->getRenderSettings()->getResampleMode();

  // �T�u�s�N�Z��������
  int subAmount  = (resampleMode == AreaAverage) ? 4 : 1;
  int subAmount2 = subAmount * subAmount;

  int maxThreadCount = QThreadPool::globalInstance()->maxThreadCount() -
                       QThreadPool::globalInstance()->activeThreadCount();

  // �T�u�|�C���g�����ɕ`��ς݂��ǂ���
  int size = boundingRect.width() * boundingRect.height() * subAmount2;

  // �A���t�@�`�����l���ɗp����
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

      // �T�u�|�C���g�����ɕ`��ς݂��ǂ���
      bool* subPointOccupation = new bool[size];
      std::fill_n(subPointOccupation, size, false);
      subPointOccupationList.push_back(subPointOccupation);

      // ���ʂ����߂郉�X�^����������
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

  // ���ʂ�Ԃ�
  return retRas;
}

//---------------------------------------------------
// �}�b�g�摜���쐬����
//---------------------------------------------------
TRasterGR8P IwRenderInstance::createMatteRas(ShapePair* shape) {
  ShapePair::MatteInfo matteInfo = shape->matteInfo();
  if (matteInfo.layerName.isEmpty()) return TRasterGR8P();
  if (matteInfo.colors.isEmpty()) return TRasterGR8P();

  IwLayer* matteLayer = m_project->getLayerByName(matteInfo.layerName);
  if (!matteLayer) return TRasterGR8P();

  TRaster32P ras = (TRaster32P)getLayerRaster(matteLayer, false);
  if (!ras) return TRasterGR8P();  // �Ƃ肠���� 8bpc�̃}�b�g�摜�ɂ̂ݑΉ�

  // �v�Z�͈�
  QSize workAreaSize = m_project->getWorkAreaSize();
  QRectF shapeBBox   = shape->getBBox(
      m_frame, 1);  // TO �̃o�E���f�B���O�{�b�N�X�iIwaWarper���W�n�j
  shapeBBox = QRectF(shapeBBox.left() * (double)workAreaSize.width(),
                     shapeBBox.top() * (double)workAreaSize.height(),
                     shapeBBox.width() * (double)workAreaSize.width(),
                     shapeBBox.height() * (double)workAreaSize.height());

  QRect boundingRect = shapeBBox.toRect()
                           .marginsAdded(QMargins(5, 5, 5, 5))
                           .intersected(QRect(QPoint(), workAreaSize));

  // ��ʊO��To�`�󂪂͂ݏo���Ă���Ƃ�
  if (boundingRect.isEmpty()) return TRasterGR8P();

  // �T���v���_�̂��߂̃I�t�Z�b�g
  QPointF sampleOffset(0.5f * (double)(ras->getLx() - workAreaSize.width()) +
                           boundingRect.left(),
                       0.5f * (double)(ras->getLy() - workAreaSize.height()) +
                           boundingRect.top());

  TRasterGR8P matteRas =
      TRasterGR8P(boundingRect.width(), boundingRect.height());
  matteRas->clear();

  // ���Ƀ`�F�b�N�ς݂̃s�N�Z���l
  QList<TPixel32> inPixList, outPixList;

  // matteRas�̃X�L�������C�����Ƃ�
  for (int y = 0; y < matteRas->getLy(); y++) {
    TPixelGR8* mattePix = matteRas->pixels(y);

    int orgY = y + (int)std::round(sampleOffset.y());
    // matteLayer�̊O���̏ꍇ�Amatte��0
    if (orgY < 0 || orgY >= ras->getLy()) {
      for (int x = 0; x < matteRas->getLx(); x++, mattePix++) *mattePix = 0;
      continue;  // ���̍s��
    }

    TPixel32* orgPix = ras->pixels(orgY);
    int orgX         = (int)std::round(sampleOffset.x());

    // matteLayer�����ɂ͂ݏo�Ă��镔���Amatte��0
    int x = 0;
    for (; orgX < 0; orgX++, mattePix++, x++) {
      *mattePix = 0;
    }

    orgPix += orgX;

    for (; x < matteRas->getLx(); x++, mattePix++, orgX++) {
      // matteLayer���E�ɂ͂ݏo�Ă��镔���Amatte��0
      if (orgX >= ras->getLx()) {
        *mattePix = 0;
      } else {
        if (inPixList.contains(*orgPix))
          *mattePix = 1;
        else if (inPixList.contains(*orgPix))
          *mattePix = 0;
        else {  // ���菈�����s��
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

  // �}�X�N�𑾂点�鏈��
  int dilate = m_project->getRenderSettings()->getMatteDilate();
  for (int d = 1; d <= dilate; d++) {
    for (int y = 0; y < matteRas->getLy(); y++) {
      TPixelGR8* mattePix_up   = matteRas->pixels((y == 0) ? 0 : y - 1);
      TPixelGR8* mattePix      = matteRas->pixels(y);
      TPixelGR8* mattePix_down = matteRas->pixels(
          (y == matteRas->getLy() - 1) ? matteRas->getLy() - 1 : y + 1);

      for (int x = 0; x < matteRas->getLx();
           x++, mattePix++, mattePix_up++, mattePix_down++) {
        // matte �̒l��0�̂��̂�������������
        if (*mattePix != 0) continue;

        // �ߖT��d����������Ad+1������
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
// �t�@�C���ɏ����o��
//---------------------------------------------------

void IwRenderInstance::saveImage(TRaster64P ras) {
  TRasterImageP img(ras);

  // �t�@�C���p�X�𓾂�
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

// �e�t���[���A�e�V�F�C�v�̂����܂薈��
// ���_���W��UV���W���L���b�V������
void IwRenderInstance::HEcacheTriangles(
    HEModel& model, ShapePair* shape, const TDimension& srcDim,
    const QPolygonF& /*parentShapePolygon*/) {
  QSize workAreaSize = m_project->getWorkAreaSize();
  // �T���v���_�̂��߂̃I�t�Z�b�g
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
