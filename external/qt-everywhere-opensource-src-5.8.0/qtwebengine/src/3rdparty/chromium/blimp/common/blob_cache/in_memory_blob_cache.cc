// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/blob_cache/in_memory_blob_cache.h"

#include "base/logging.h"

namespace blimp {

InMemoryBlobCache::InMemoryBlobCache() {}

InMemoryBlobCache::~InMemoryBlobCache() {}

void InMemoryBlobCache::Put(const BlobId& id, BlobDataPtr data) {
  if (Contains(id)) {
    // In cases where the engine has miscalculated what is already available
    // on the client, Put() might be called unnecessarily, which should be
    // ignored.
    VLOG(2) << "Item with ID " << id << " already exists in cache.";
    return;
  }
  cache_.insert(std::make_pair(id, std::move(data)));
}

bool InMemoryBlobCache::Contains(const BlobId& id) const {
  return cache_.find(id) != cache_.end();
}

BlobDataPtr InMemoryBlobCache::Get(const BlobId& id) const {
  if (!Contains(id)) {
    return nullptr;
  }

  return cache_.find(id)->second;
}

}  // namespace blimp
