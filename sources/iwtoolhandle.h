//-----------------------------------------------------------------------------
// IwToolHandle
// ツールのカレントを管理する
//-----------------------------------------------------------------------------
#ifndef IWTOOLHANDLE_H
#define IWTOOLHANDLE_H

#include <QObject>
#include <QString>

class IwTool;

class IwToolHandle : public QObject {
  Q_OBJECT

  IwTool* m_tool;
  QString m_toolName;

public:
  IwToolHandle();

  // カレントツールを取得
  IwTool* getTool() const;
  // カレントツールをセット
  void setTool(QString name);

signals:
  // ツールが切り替わったら呼ばれる
  void toolSwitched();
};

#endif