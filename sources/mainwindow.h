#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <iostream>
#include <QMainWindow>
#include "menubarcommand.h"

class QMenuBar;
class QActionGroup;
class QIcon;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QMenu;

class ToolBoxDock;
class SceneViewer;
class MainStatusBar;

class TimeLineWindow;
class ToolOptionPanel;
class LayerOptionPanel;
class ShapeTreeWindow;

class MainWindow : public QMainWindow {
  Q_OBJECT

  // �����̃c�[���{�b�N�X
  ToolBoxDock *m_toolBox;
  // ���S�̃��[�N�G���A
  SceneViewer *m_viewer;
  // �X�e�[�^�X�o�[�ƃt���[���X���C�_
  MainStatusBar *m_mainStatusBar;
  // �^�C�����C��
  TimeLineWindow *m_timeLineWindow;
  // �c�[���I�v�V����
  ToolOptionPanel *m_toolOptionPanel;
  // ���C���[�I�v�V����
  LayerOptionPanel *m_layerOptionPanel;
  // �V�F�C�v�c���[
  ShapeTreeWindow *m_shapeTreeWindow;
  // �c�[���o�[�R�}���h�̃O���[�v
  QActionGroup *m_toolsActionGroup;

  // ���j���[�o�[�̍쐬
  QWidget *createMenuBarWidget();
  // �\�����[�h
  QComboBox *m_viewModeCombo;
  // �\���`�F�b�N�{�b�N�X
  QCheckBox *m_fromShow, *m_toShow, *m_meshShow, *m_matteApply;
  // ���[�N�G���A�̃T�C�Y
  QLineEdit *m_workAreaWidthEdit, *m_workAreaHeightEdit;

  // �A�N�V�����̓o�^
  void defineActions();
  // �A�N�V�����̓o�^�i�ЂƂ��Ăԁj
  QAction *createAction(const char *id, const QString &name,
                        const QString &defaultShortcut,
                        CommandType type = MenuFileCommandType);

  // �g�O���A�A�C�R���t���̃A�N�V���������
  QAction *createToggleWithIcon(const char *id, const QString &iconName,
                                const QString &name,
                                const QString &defaultShortcut,
                                CommandType type = MenuFileCommandType);

  // �c�[���R�}���h�̓o�^�i�ЂƂ��Ăԁj
  QAction *createToolAction(const char *id, const char *iconName,
                            const QString &name,
                            const QString &defaultShortcut);

  QAction *createMenuAction(const char *id, const QString &name);

  // �A�C�R����Ԃ�
  QIcon createIcon(const QString &iconName);

  void addActionToMenu(QMenu *menu, CommandId id);
  QMenu *setupLanguageMenu();

public:
  MainWindow(QWidget *parent = 0, Qt::WindowFlags flags = Qt::Widget);
  ~MainWindow() {}

  void onExit();
  void onNewProject();

  // �v���W�F�N�g�̕ۑ�/���[�h
  void onOpenProject();
  void onImportProject();
  void onSaveProject();
  void onSaveAsProject();
  void onSaveProjectWithDateTime();
  void onInsertImages();

  // Undo, Redo
  void onUndo();
  void onRedo();

  // �X�e�[�^�X�o�[�Ƀ��b�Z�[�W��\������
  void showMessageOnStatusBar(const QString &message, int timeout = 0);

  // �t���[���ړ�
  void onNextFrame();
  void onPreviousFrame();

  void onClearMeshCache();

  SceneViewer *getViewer() { return m_viewer; }

protected:
  void closeEvent(QCloseEvent *) override;

protected slots:
  // �v���r���[�̌v�Z���I�������Preview�\�����[�h�ɂ��ĕ\�����X�V
  void onPreviewRenderCompleted(int frame);
  // �R���{�{�b�N�X���؂�ւ������\�����X�V
  void onViewModeComboActivated(int);

  void onProjectSwitched();
  void onProjectChanged();

  // ���b�N����
  void onFromShowClicked(bool);
  void onToShowClicked(bool);
  // ���b�V���̕\��
  void onMeshShowClicked(bool);
  // �}�b�g��K�p�\��
  void onMatteApplyClicked(bool);

  // ���[�N�G���A�T�C�Y�̒l���ҏW���ꂽ�Ƃ��A�v���W�F�N�g�̓��e���X�V����
  void onWorkAreaSizeEdited();
  void onAboutTriggered();
  void onLanguageChanged(QAction *);
public slots:
  // �^�C�g���o�[�̍X�V
  void updateTitle();
};

class RecentFiles : public QObject {
  Q_OBJECT
  QList<QString> m_recentFiles;

  RecentFiles();

public:
  static RecentFiles *instance();
  ~RecentFiles();

  void addPath(QString path);
  QString getPath(int index) const;
  void movePath(int fromIndex, int toIndex);
  void clearRecentFilesList();
  void loadRecentFiles();
  void saveRecentFiles();

protected:
  void refreshRecentFilesMenu();
  QList<QString> getFilesNameList();

protected slots:
  void onMenuTriggered(QAction *);
};
#endif