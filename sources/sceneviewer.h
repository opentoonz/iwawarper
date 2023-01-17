//----------------------------------
// SceneViewer
// 1つのプロジェクトについて1つのエリア
//----------------------------------

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

#include "viewsettings.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTransform>
#include <QMouseEvent>

class IwProject;
class QMenu;
class Ruler;

class SceneViewer : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT

  IwProject *m_project = nullptr;
  QTransform m_affine;

  //--- 中ボタンドラッグによるパン用データ
  // マウス位置
  QPoint m_mousePos;
  // パン中かどうか
  bool m_isPanning;
  // 押されているボタン
  Qt::MouseButton m_mouseButton;

  Ruler *m_hRuler, *m_vRuler;

  // マウスカーソルを修飾キーに同期させる
  static Qt::KeyboardModifiers m_modifiers;

  // マウス位置からIwaWarperの座標系に変換する
  QPointF convertMousePosToIWCoord(const QPointF &mousePos);

  // もしプレビュー結果の画像がキャッシュされていたら、
  // それを使ってテクスチャを作る。テクスチャインデックスを返す
  unsigned int setPreviewTexture();

  // カレントレイヤ切り替えコマンドを登録する
  void addContextMenus(QMenu &menu);
  void updateRulers();

public:
  SceneViewer(QWidget *parent);

  ~SceneViewer();

  QSize sizeHint() const final override { return QSize(630, 440); }

  // クリック位置の描画アイテムのインデックスを返す
  int pick(const QPoint &);
  // クリック位置の描画アイテム全てのインデックスのリストを返す
  QList<int> pickAll(const QPoint &);

  // シェイプ座標上で、表示上１ピクセルの幅がどれくらいになるかを返す
  QPointF getOnePixelLength();
  const QTransform &getAffine() { return m_affine; }

  // 登録テクスチャの消去
  void deleteTextures();

  void setProject(IwProject *prj);

  void setRulers(Ruler *hRuler, Ruler *vRuler) {
    m_hRuler = hRuler;
    m_vRuler = vRuler;
  }

protected:
  void initializeGL() final override;
  void resizeGL(int width, int height) final override;
  void paintGL() final override;

  // ロードされた画像を表示する
  void drawImage();
  // プレビュー結果を描く GL版
  void drawGLPreview();

  // ワークエリアのワクを描く
  void drawWorkArea();
  // シェイプを描く
  void drawShapes();

  // 中ドラッグでパンする
  void mouseMoveEvent(QMouseEvent *e) final override;
  // 中クリックでパンモードにする
  void mousePressEvent(QMouseEvent *e) final override;
  // パンモードをクリアする
  void mouseReleaseEvent(QMouseEvent *e) final override;
  // パンモードをクリアする
  void leaveEvent(QEvent *e) final override;

  // 修飾キーをクリアする
  void enterEvent(QEvent *) final override;

  // 修飾キーを押すとカーソルを変える
  void keyPressEvent(QKeyEvent *e) final override;
  void keyReleaseEvent(QKeyEvent *e) final override;

  void wheelEvent(QWheelEvent *) final override;

  void renderText(double x, double y, const QString &str,
                  const QFont &font = QFont());

protected slots:
  // テクスチャインデックスを得る。無ければロードして登録
  ViewSettings::LAYERIMAGE getLayerImage(QString &path, int brightness,
                                         int contrast);

  // ViewSettingsが変わったら
  // 自分のプロジェクトがカレントなら、update
  void onViewSettingsChanged();

  // レイヤの内容が変わったら、現在表示しているフレームの内容が変わった時のみテクスチャを更新
  /// シーケンスの内容が変わったら、現在表示しているフレームの内容が変わった時のみテクスチャを更新
  void onLayerChanged();

  // プレビューが完成したとき、現在表示中のフレームかつ
  // プレビュー／ハーフモードだった場合は表示を更新する
  void onPreviewRenderFinished(int frame);

public slots:
  // サイズ調整
  void zoomIn();
  void zoomOut();
  void zoomReset();

  // カレントレイヤを変える
  void onChangeCurrentLayer();
};

#endif