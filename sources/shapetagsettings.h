#pragma once

#ifndef SHAPETAGSETTINGS_H
#define SHAPETAGSETTINGS_H

#include <QMap>
#include <QList>
#include <QString>
#include <QColor>
#include <QIcon>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

struct ShapeTagInfo {
  QString name;
  QColor color;
  int iconShapeId;
  QIcon icon;
};

class ShapeTagSettings {
  // ID vs タグ情報のマップ
  QMap<int, ShapeTagInfo> m_shapeTags;
  // 表示順のリスト
  QList<int> m_tagIdList;

public:
  ShapeTagSettings();
  void addTag(const int id, const QString& name, const QColor& color,
              const int iconShapeId);
  void setShapeTagInfo(const int id, ShapeTagInfo info);
  void removeTag(const int id);
  void insertTag(const int id, ShapeTagInfo info, int listPos);

  int tagCount() { return m_tagIdList.count(); }
  ShapeTagInfo getTagFromId(int id) const { return m_shapeTags.value(id); }
  ShapeTagInfo getTagAt(int listPos) const {
    return getTagFromId(m_tagIdList.at(listPos));
  }
  int getIdAt(int listPos) const { return m_tagIdList.at(listPos); }
  int getListPosOf(int id) const { return m_tagIdList.indexOf(id); }

  void saveData(QXmlStreamWriter& writer);
  void loadData(QXmlStreamReader& reader);

  static QIcon getIconShape(int iconShapeId);
  static QList<int> iconShapeIds();
  static void initTagIcons();
};

#endif
