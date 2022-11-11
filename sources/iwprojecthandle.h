//---------------------------------------------------
// IwProjectHandle
// プロジェクト情報のカレントを管理する。
//---------------------------------------------------
#ifndef IWPROJECTHANDLE_H
#define IWPROJECTHANDLE_H

#include <QObject>
#include <QMap>

class IwProject;
class IwRenderInstance;  // 仮に

class IwProjectHandle : public QObject {
  Q_OBJECT

  IwProject* m_project;
  bool m_dirtyFlag;

  QMap<int, unsigned int> m_runningInstances;

public:
  IwProjectHandle();
  ~IwProjectHandle();

  IwProject* getProject() const;
  void setProject(IwProject* project);

  // 現在表示中のフレームが変わったら呼ぶ
  void notifyViewFrameChanged() { emit viewFrameChanged(); }
  // projectが切り替わったら呼ぶ
  void notifyProjectSwitched() { emit projectSwitched(); }
  // ViewSettingsが変わったら呼ぶ
  void notifyViewSettingsChanged() { emit viewSettingsChanged(); }

  void notifyShapeTagSettingsChanged() {
    emit shapeTagSettingsChanged();
    setDirty();
  }
  // projectの内容が変わったら呼ぶ
  void notifyProjectChanged(bool emitShapeTreeChanged = false) {
    emit projectChanged();
    if (emitShapeTreeChanged) emit shapeTreeChanged();
    setDirty();
  }
  // グループ関係がいじられたとき呼ぶ
  void notifyGroupChanged() { emit groupChanged(); }
  // GroupEditorとCurveEditorのやりとりのため。
  // 現在選択中のGroupのインデックスが変わったらemitする
  void notifyCurrentGroupIndexChanged(int index) {
    emit currentGroupIndexChanged(index);
  }

  //---レンダリング関係
  void notifyPreviewCacheInvalidated() { emit previewCacheInvalidated(); }

  void setDirty(bool dirty = true);
  bool isDirty() { return m_dirtyFlag; }

  void suspendRender(int frame);
  bool isRunningPreview(int frame);

  // シェイプ関係
  void notifyShapeTreeChanged() { emit shapeTreeChanged(); }

protected slots:
  // ロールが編集されたとき、現在のプロジェクトがあれば、
  // multiFrame情報を更新する IwAppでコネクトしている
  void onSequenceChanged();
  void onPreviewRenderStarted(int frame, unsigned int taskId);
  void onPreviewRenderFinished(int frame, unsigned int taskId);

signals:
  // カレントフレームが変わったとき
  void viewFrameChanged();
  // projectが切り替わった場合
  void projectSwitched();
  // それ以外、何でもViewSettingsが変わったら呼ばれる
  void viewSettingsChanged();
  // projectの内容が変わった場合(ワークエリアのサイズが変わったときとか)
  void projectChanged();

  void shapeTreeChanged();

  // グループ関係がいじられたとき
  void groupChanged();

  // GroupEditorとCurveEditorのやりとりのため。
  // 現在選択中のGroupのインデックスが変わったらemitする
  void currentGroupIndexChanged(int);

  //---レンダリング関係
  // プレビュー完成
  // void previewRenderCompleted();
  void previewRenderStarted(int frame);
  void previewRenderFinished(int frame);
  // キャッシュがInvalidateされた
  void previewCacheInvalidated();

  void dirtyFlagChanged();

  // シェイプタグ設定の更新
  void shapeTagSettingsChanged();
};

#endif