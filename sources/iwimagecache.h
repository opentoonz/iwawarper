//---------------------------------------------------
// IwImageCache
// DVのキャッシュ機能を使って画像キャッシュをするインタフェース
//---------------------------------------------------
#ifndef IWIMAGECACHE_H
#define IWIMAGECACHE_H

#include <QString>
#include "timagecache.h"

class IwImageCache  // singleton
{
  IwImageCache();

public:
  static IwImageCache *instance();

  // 格納：パスをIdにしてTImagePを格納
  void add(const QString &id, const TImageP &img);
  // 取り出し：パスをIdにしてTImagePを返す
  TImageP get(const QString &id) const;
  // キャッシュされているかどうか
  bool isCached(const QString &id) const;
  // キャッシュを消去
  void remove(const QString &id);
  // 全消去
  void clear();
};

#endif