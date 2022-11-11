//---------------------------------------------------
// IwImageCache
// OpenToonzのキャッシュ機能を使って画像キャッシュをするインタフェース
//---------------------------------------------------

#include "iwimagecache.h"

IwImageCache::IwImageCache() {}

IwImageCache *IwImageCache::instance() {
  static IwImageCache _instance;
  return &_instance;
}

//---------------------------------------------------
// 格納：パスをIdにしてTImagePを格納 すでにある場合は上書き
//---------------------------------------------------
void IwImageCache::add(const QString &id, const TImageP &img) {
  if (isCached(id)) remove(id);
  TImageCache::instance()->add(id, img);
}

//---------------------------------------------------
// 取り出し：パスをIdにしてTImagePを返す
//---------------------------------------------------
TImageP IwImageCache::get(const QString &id) const {
  return TImageCache::instance()->get(id, true);
}

//---------------------------------------------------
// キャッシュされているかどうか
//---------------------------------------------------
bool IwImageCache::isCached(const QString &id) const {
  return TImageCache::instance()->isCached(id.toStdString());
}

//---------------------------------------------------
// キャッシュを消去
//---------------------------------------------------
void IwImageCache::remove(const QString &id) {
  TImageCache::instance()->remove(id);
}

void IwImageCache::clear() { TImageCache::instance()->clear(); }
