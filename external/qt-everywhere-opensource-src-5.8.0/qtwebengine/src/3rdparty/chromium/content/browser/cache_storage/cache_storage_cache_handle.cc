// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_cache_handle.h"

namespace content {

CacheStorageCacheHandle::~CacheStorageCacheHandle() {
  if (cache_storage_ && cache_storage_cache_)
    cache_storage_->DropCacheHandleRef(cache_storage_cache_.get());
}

std::unique_ptr<CacheStorageCacheHandle> CacheStorageCacheHandle::Clone() {
  return std::unique_ptr<CacheStorageCacheHandle>(
      new CacheStorageCacheHandle(cache_storage_cache_, cache_storage_));
}

CacheStorageCacheHandle::CacheStorageCacheHandle(
    base::WeakPtr<CacheStorageCache> cache_storage_cache,
    base::WeakPtr<CacheStorage> cache_storage)
    : cache_storage_cache_(cache_storage_cache), cache_storage_(cache_storage) {
  DCHECK(cache_storage);
  DCHECK(cache_storage_cache_);
  cache_storage_->AddCacheHandleRef(cache_storage_cache_.get());
}

}  // namespace content
