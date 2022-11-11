#include "shapetagsettings.h"

#include <QPixmap>
#include <QPainter>
#include <QMap>

namespace {
QMap<int, QPixmap> iconShapes;

QIcon getTagIcon(const QColor& color, const int& iconShapeId) {
  // TODO: 範囲外の場合
  QPixmap pm(iconShapes.value(iconShapeId));

  QPainter painter(&pm);
  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
  painter.fillRect(0, 0, 12, 12, color);

  return QIcon(pm);
}
}  // namespace

void ShapeTagSettings::initTagIcons() {
  iconShapes = {
      {0, QPixmap(":Resources/shapetagicons/icon0.svg")},
      {1, QPixmap(":Resources/shapetagicons/icon1.svg")},
      {2, QPixmap(":Resources/shapetagicons/icon2.svg")},
      {3, QPixmap(":Resources/shapetagicons/icon3.svg")},
      {4, QPixmap(":Resources/shapetagicons/icon4.svg")},
      {5, QPixmap(":Resources/shapetagicons/icon5.svg")},
      {101, QPixmap(":Resources/shapetagicons/icon_a.svg")},
      {102, QPixmap(":Resources/shapetagicons/icon_b.svg")},
      {103, QPixmap(":Resources/shapetagicons/icon_c.svg")},
      {104, QPixmap(":Resources/shapetagicons/icon_d.svg")},
      {105, QPixmap(":Resources/shapetagicons/icon_e.svg")},
      {106, QPixmap(":Resources/shapetagicons/icon_f.svg")},
      {107, QPixmap(":Resources/shapetagicons/icon_g.svg")},
      {108, QPixmap(":Resources/shapetagicons/icon_h.svg")},
  };
}

ShapeTagSettings::ShapeTagSettings() {
  // 初期タグをつくる
  addTag(1, "tag 1", QColor(Qt::cyan), 0);
  addTag(2, "tag 2", QColor(Qt::yellow), 1);
  addTag(3, "tag 3", QColor(Qt::magenta), 2);
}

void ShapeTagSettings::addTag(const int id, const QString& name,
                              const QColor& color, const int iconShapeId) {
  ShapeTagInfo info = {name, color, iconShapeId,
                       getTagIcon(color, iconShapeId)};

  // 　TODO: すでに登録済みのIDの場合
  m_shapeTags[id] = info;
  m_tagIdList.append(id);
}

void ShapeTagSettings::setShapeTagInfo(const int id, ShapeTagInfo info) {
  info.icon       = getTagIcon(info.color, info.iconShapeId);
  m_shapeTags[id] = info;
}

void ShapeTagSettings::removeTag(const int id) {
  // TODO: 範囲外の場合
  m_shapeTags.remove(id);
  m_tagIdList.removeAll(id);
}

void ShapeTagSettings::insertTag(const int id, ShapeTagInfo info, int listPos) {
  if (info.icon.isNull()) info.icon = getTagIcon(info.color, info.iconShapeId);

  m_shapeTags[id] = info;
  m_tagIdList.insert(listPos, id);
}

void ShapeTagSettings::saveData(QXmlStreamWriter& writer) {
  for (auto id : m_tagIdList) {
    writer.writeStartElement("ShapeTagInfo");
    ShapeTagInfo info = m_shapeTags.value(id);
    writer.writeTextElement("id", QString::number(id));
    writer.writeTextElement("name", info.name);
    writer.writeTextElement("color", info.color.name());
    writer.writeTextElement("iconShapeId", QString::number(info.iconShapeId));
    writer.writeEndElement();
  }
}

void ShapeTagSettings::loadData(QXmlStreamReader& reader) {
  m_shapeTags.clear();
  m_tagIdList.clear();

  while (reader.readNextStartElement()) {
    if (reader.name() == "ShapeTagInfo") {
      ShapeTagInfo info;
      int id;
      while (reader.readNextStartElement()) {
        if (reader.name() == "id")
          id = reader.readElementText().toInt();
        else if (reader.name() == "name")
          info.name = reader.readElementText();
        else if (reader.name() == "color")
          info.color.setNamedColor(reader.readElementText());
        else if (reader.name() == "iconShapeId")
          info.iconShapeId = reader.readElementText().toInt();
        else
          reader.skipCurrentElement();
      }
      info.icon = getTagIcon(info.color, info.iconShapeId);

      m_shapeTags[id] = info;
      m_tagIdList.append(id);
    } else
      reader.skipCurrentElement();
  }
}

QIcon ShapeTagSettings::getIconShape(int iconShapeId) {
  return getTagIcon(QColor(Qt::white), iconShapeId);
}

QList<int> ShapeTagSettings::iconShapeIds() { return iconShapes.keys(); }