//---------------------------------------------------
// IwRenderInstance
// �e�t���[�����Ƃɍ����A�����_�����O�v�Z���s������
// IwRenderCommand::onPreview��������
//---------------------------------------------------
#ifndef IWRENDERINSTANCE_H
#define IWRENDERINSTANCE_H

#include <QList>
#include <QMap>
#include <QString>
#include <QPointF>
#include <QVector3D>
#include <array>
#include <QRunnable>
#include <QImage>

#include "trasterimage.h"
#include "halfedge.h"
#include "rendersettings.h"
class IwProject;
class IwLayer;
class ShapePair;
class RenderProgressPopup;
class IwRenderInstance;
class OutputSettings;

enum CorrVecId { CORRVEC_SRC = 0, CORRVEC_DST, CORRVEC_MIDDLE };
enum WarpStyle { WARP_FIXED = 0, WARP_SLIDING };

// �Ή��_�x�N�g��
struct CorrVector {
  QPointF from_p[2];
  QPointF to_p[2];
  int stackOrder;  // ���C���C���f�b�N�X �� �d�ˏ�
  bool isEdge;     // �֊s�Ȃ�true, �V�F�C�v�R���Ȃ�true
  double from_weight[2];
  double to_weight[2];
  // int indices[2];  // samplePoints�̃C���f�b�N�X
};

class MapTrianglesToRaster_Worker : public QRunnable {
  int m_from, m_to;  // face�̃C���f�b�N�X
  HEModel* m_model;
  IwRenderInstance* m_parentInstance;
  QPointF m_sampleOffset, m_outputOffset;
  TRaster64P m_outRas;
  TRaster64P m_srcRas;
  TRasterGR8P m_matteRas;
  TRaster64P m_mlssRefRas;
  bool* m_subPointOccupation;
  int m_subAmount;
  AlphaMode m_alphaMode;
  ResampleMode m_resampleMode;
  const QImage m_shapeAlphaImg;

  void run() override;

public:
  MapTrianglesToRaster_Worker(int from, int to, HEModel* model,
                              IwRenderInstance* parentInstance,
                              QPointF sampleOffset, QPointF outputOffset,
                              TRaster64P outRas, TRaster64P srcRas,
                              TRasterGR8P matteRas, TRaster64P mlssRefRas,
                              bool* subPointOccupation, int subAmount,
                              AlphaMode alphaMode, ResampleMode resampleMode,
                              QImage shapeAlphaImg)
      : m_from(from)
      , m_to(to)
      , m_model(model)
      , m_parentInstance(parentInstance)
      , m_sampleOffset(sampleOffset)
      , m_outputOffset(outputOffset)
      , m_outRas(outRas)
      , m_srcRas(srcRas)
      , m_matteRas(matteRas)
      , m_mlssRefRas(mlssRefRas)
      , m_subPointOccupation(subPointOccupation)
      , m_subAmount(subAmount)
      , m_alphaMode(alphaMode)
      , m_resampleMode(resampleMode)
      , m_shapeAlphaImg(shapeAlphaImg) {}
};

class CombineResults_Worker : public QRunnable {
  int m_from, m_to;
  TRaster64P m_outRas;
  QList<bool*>& m_subPointOccupationList;
  QList<TRaster64P>& m_outRasList;
  void run() override;

public:
  CombineResults_Worker(int from, int to, TRaster64P outRas,
                        QList<bool*>& subPointOccupationList,
                        QList<TRaster64P>& outRasList)
      : m_from(from)
      , m_to(to)
      , m_outRas(outRas)
      , m_subPointOccupationList(subPointOccupationList)
      , m_outRasList(outRasList) {}
};

class ResampleResults_Worker : public QRunnable {
  int m_from, m_to;
  TRaster64P m_outRas;
  TRaster64P m_retRas;
  int m_subAmount;
  bool m_antialias;
  void run() override;

public:
  ResampleResults_Worker(int from, int to, TRaster64P outRas, TRaster64P retRas,
                         int subAmount, bool antialias)
      : m_from(from)
      , m_to(to)
      , m_outRas(outRas)
      , m_retRas(retRas)
      , m_subAmount(subAmount)
      , m_antialias(antialias) {}
};

class IwRenderInstance : public QObject, public QRunnable {
  Q_OBJECT

  int m_frame;
  int m_outputFrame;
  IwProject* m_project;
  OutputSettings* m_outputSettings;
  bool m_isPreview;
  RenderProgressPopup* m_popup;
  int m_precision;
  double m_faceSizeThreshold;
  double m_smoothingThreshold;
  WarpStyle m_warpStyle;

  unsigned int m_taskId;

  bool m_antialias;

  void run() override;

  // ���ʂ��L���b�V���Ɋi�[
  void storeImageToCache(TRaster64P morphedRaster);

  // �V�F�C�v�̐e�q�O���[�v���ƂɌv�Z����
  TRaster64P warpLayer(IwLayer* layer, QList<ShapePair*>& shapes,
                       bool isPreview, QPoint& origin);

  //---------------

  // �f�ރ��X�^�𓾂�
  TRasterP getLayerRaster(IwLayer* layer, bool convertTo16bpc = true);

  // ���[�v���CorrVec�̒����ɍ��킹�A
  // ���܂�Z���x�N�g���͂܂Ƃ߂Ȃ���i�[���Ă���
  void getCorrVectors(IwLayer* layer, QList<ShapePair*>& shapes,
                      QList<CorrVector>& corrVectors,
                      QVector<QPointF>& parentShapeVerticesFrom,
                      QVector<QPointF>& parentShapeVerticesTo);

  // fromShape�����ɁA���b�V����؂� Halfedge�g����
  void HEcreateTriangleMesh(QList<CorrVector>& corrVectors, HEModel& model,
                            const QPolygonF& parentShapePolygon,
                            const TDimension& srcDim);

  // �䂪�񂾌`���`�悷��_�}���`�X���b�h��
  TRaster64P HEmapTrianglesToRaster_Multi(HEModel& model, TRaster64P srcRas,
                                          TRasterGR8P matteRas,
                                          TRaster64P mlssRefRas,
                                          ShapePair* shape, QPoint& origin,
                                          const QPolygonF& parentShapePolygon);

  // �O�p�`�̈ʒu���L���b�V������
  void HEcacheTriangles(HEModel& model, ShapePair* shape,
                        const TDimension& srcDim,
                        const QPolygonF& parentShapePolygon);

  // �}�b�g�摜���쐬����
  TRasterGR8P createMatteRas(ShapePair* shape);

  // Morphological
  // Supersampling���s���ꍇ�A�T���v���p�̋��E���}�b�v�摜�𐶐�����
  // RGBA�e�`�����l���ɂ��ꂼ�ꉺ�L�̒l��half-float�œ����
  // R:�������^���E���̍��̐ؕ� G:�������^���E���̉E�̐ؕ�
  // B:�������^���E���̉��̐ؕ� A:�������^���E���̏�̐ؕ�
  TRaster64P createMLSSRefRas(TRaster64P inRas);

  //--------------------------------
  // �t�@�C���ɏ����o��
  void saveImage(TRaster64P ras);
  //--------------------------------

public:
  IwRenderInstance(int frame, int outputFrame, IwProject* project,
                   OutputSettings* settings, bool isPreview,
                   RenderProgressPopup* popup = nullptr);
  void doPreview();
  void doRender();

  bool isCanceled();

  bool antialiasEnabled() const { return m_antialias; }

signals:
  void renderStarted(int frame, unsigned int taskId);
  void renderFinished(int frame, unsigned int taskId);
};

#endif
