//---------------------------------------------
// OutputSettings
// 現在のフレーム番号に対応するファイルの保存先パスを返す。
//---------------------------------------------

#ifndef OUTPUTSETTINGS_H
#define OUTPUTSETTINGS_H

//---------------------------------------------
// 対応保存形式のリスト
//---------------------------------------------
#define Saver_TIFF "TIFF"
#define Saver_SGI "SGI"
#define Saver_PNG "PNG"
#define Saver_TGA "TGA"

#include <QString>
#include <QStringList>
#include <QMap>

class TPropertyGroup;
class MyPropertyGroup;
class QXmlStreamWriter;
class QXmlStreamReader;

class OutputSettings {
public:
  struct SaveRange {
    int startFrame;
    int endFrame;
    int stepFrame;
  };

  enum RenderState { Off = 0, On, Done };

private:
  // 保存形式
  QString m_saver;
  // 保存フォルダ
  QString m_directory;
  // 保存パスのフォーマット
  QString m_format;

  // Initial frame number レンダリング1フレーム目につくフレーム番号
  int m_initialFrameNumber;
  // Increment フレーム番号をいくつ進めるか
  int m_increment;
  // Number of digits フレーム番号の桁数
  int m_numberOfDigits;

  // 保存フレーム範囲
  SaveRange m_saveRange;

  // ファイル形式毎のプロパティ
  QMap<QString, MyPropertyGroup *> m_formatProperties;

  // FROMまたはTOの親シェイプに、特定のタグのついたものだけレンダリングする
  // -1の場合はタグを無視する。子シェイプのタグは無視する。
  int m_shapeTagId;

  QString m_shapeImageFileName;
  int m_shapeImageSizeId;

  RenderState m_renderState;

  // 現在の出力設定でレンダリング対象となるシェイプが用いている
  // アルファマットレイヤーのレイヤー名一覧を一時的に保持する。レンダリング開始時に再取得する
  QStringList m_tmp_matteLayerNames;

public:
  OutputSettings();
  OutputSettings(const OutputSettings &);

  QString getSaver() { return m_saver; }
  void setSaver(QString saver) { m_saver = saver; }

  QString getDirectory() { return m_directory; }
  void setDirectory(QString directory) { m_directory = directory; }

  QString getFormat() { return m_format; }
  void setFormat(QString format) { m_format = format; }

  int getInitialFrameNumber() { return m_initialFrameNumber; }
  void setInitiaFrameNumber(int ifn) { m_initialFrameNumber = ifn; }

  int getIncrement() { return m_increment; }
  void setIncrement(int incr) { m_increment = incr; }

  int getNumberOfDigits() { return m_numberOfDigits; }
  void setNumberOfDigits(int nod) { m_numberOfDigits = nod; }

  SaveRange getSaveRange() { return m_saveRange; }
  void setSaveRange(SaveRange saveRange) { m_saveRange = saveRange; }

  int getShapeTagId() { return m_shapeTagId; }
  void setShapeTagId(int tag) { m_shapeTagId = tag; }

  QString getShapeImageFileName() { return m_shapeImageFileName; }
  void setShapeImageFileName(QString name) { m_shapeImageFileName = name; }
  int getShapeImageSizeId() { return m_shapeImageSizeId; }
  void setShapeImageSizeId(int id) { m_shapeImageSizeId = id; }

  RenderState renderState() { return m_renderState; }
  void setRenderState(RenderState state) { m_renderState = state; }

  // frameに対する保存パスを返す
  QString getPath(int frame, QString projectName,
                  QString formatStr = QString());

  // 保存形式に対する標準の拡張子を返す
  static QString getStandardExtension(QString saver);

  // m_initialFrameNumber, m_increment, m_numberOfDigits
  // から保存用の文字列を作る
  QString getNumberFormatString();
  // ↑の逆。文字列からパラメータを得る
  void setNumberFormat(QString str);

  // プロパティを得る
  TPropertyGroup *getFileFormatProperties(QString saver);

  void obtainMatteLayerNames();

  // 保存/ロード
  void saveData(QXmlStreamWriter &writer);
  void loadData(QXmlStreamReader &reader);
};

class RenderQueue {
  QList<OutputSettings *> m_outputs;
  int m_currentSettingsId;

public:
  RenderQueue();

  // 保存/ロード
  void saveData(QXmlStreamWriter &writer);
  void loadData(QXmlStreamReader &reader);
  void loadPrevVersionData(QXmlStreamReader &reader);

  // frameに対する保存パスを返す
  QString getPath(int frame, QString projectName, QString formatStr = QString(),
                  int queueId = -1);

  // Onになっているアイテムを返す
  QList<OutputSettings *> activeItems();
  QList<OutputSettings *> allItems() { return m_outputs; }
  // 現在のアイテムを返す
  OutputSettings *currentOutputSettings();
  OutputSettings *outputSettings(int index) {
    assert(index >= 0 && index < m_outputs.size());
    return m_outputs[index];
  }

  int currentSettingsId() { return m_currentSettingsId; }
  void setCurrentSettingsId(int id) { m_currentSettingsId = id; }
  void cloneCurrentItem();
  void removeCurrentItem();
};

#endif