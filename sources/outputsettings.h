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

private:
  // 保存形式
  QString m_saver;
  // 保存フォルダ
  QString m_directory;
  // 拡張子
  QString m_extension;
  // 保存パスのフォーマット
  QString m_format;

  // Initial frame number レンダリング1フレーム目につくフレーム番号
  int m_initialFrameNumber;
  // Increment フレーム番号をいくつ進めるか
  int m_increment;
  // Number of digits フレーム番号の桁数
  int m_numberOfDigits;
  // Use project name [base]タグを使うかどうか。→クリック時のみ、m_formatを更新
  bool m_useSource;
  // Add frame number [num]タグを使うかどうか。→クリック時のみ、m_formatを更新
  bool m_addNumber;
  // Add Extension [ext]タグを使うかどうか。→クリック時のみ、m_formatを更新
  bool m_replaceExt;

  // 保存フレーム範囲
  SaveRange m_saveRange;

  // ファイル形式毎のプロパティ
  QMap<QString, MyPropertyGroup *> m_formatProperties;

  // FROMまたはTOの親シェイプに、特定のタグのついたものだけレンダリングする
  // -1の場合はタグを無視する。子シェイプのタグは無視する。
  int m_shapeTagId;

  QString m_shapeImageFileName;
  int m_shapeImageSizeId;

public:
  OutputSettings();

  QString getSaver() { return m_saver; }
  void setSaver(QString saver) { m_saver = saver; }

  QString getDirectory() { return m_directory; }
  void setDirectory(QString directory) { m_directory = directory; }

  QString getExtension() { return m_extension; }
  void setExtension(QString extension) { m_extension = extension; }

  QString getFormat() { return m_format; }
  void setFormat(QString format) { m_format = format; }

  int getInitialFrameNumber() { return m_initialFrameNumber; }
  void setInitiaFrameNumber(int ifn) { m_initialFrameNumber = ifn; }

  int getIncrement() { return m_increment; }
  void setIncrement(int incr) { m_increment = incr; }

  int getNumberOfDigits() { return m_numberOfDigits; }
  void setNumberOfDigits(int nod) { m_numberOfDigits = nod; }

  bool isUseSource() { return m_useSource; }
  void setIsUseSource(bool use) { m_useSource = use; }

  bool isAddNumber() { return m_addNumber; }
  void setIsAddNumber(bool add) { m_addNumber = add; }

  bool isReplaceExt() { return m_replaceExt; }
  void setIsReplaceExt(bool replace) { m_replaceExt = replace; }

  SaveRange getSaveRange() { return m_saveRange; }
  void setSaveRange(SaveRange saveRange) { m_saveRange = saveRange; }

  int getShapeTagId() { return m_shapeTagId; }
  void setShapeTagId(int tag) { m_shapeTagId = tag; }

  QString getShapeImageFileName() { return m_shapeImageFileName; }
  void setShapeImageFileName(QString name) { m_shapeImageFileName = name; }
  int getShapeImageSizeId() { return m_shapeImageSizeId; }
  void setShapeImageSizeId(int id) { m_shapeImageSizeId = id; }

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

  // 保存/ロード
  void saveData(QXmlStreamWriter &writer);
  void loadData(QXmlStreamReader &reader);
};

#endif