// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_BLOB_CACHE_IN_MEMORY_BLOB_CACHE_H_
#define BLIMP_COMMON_BLOB_CACHE_IN_MEMORY_BLOB_CACHE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "blimp/common/blimp_common_export.h"
#include "blimp/common/blob_cache/blob_cache.h"

namespace blimp {

// InMemoryBlobCache provides an in-memory implementation of BlobCache that
// never evicts items.
class BLIMP_COMMON_EXPORT InMemoryBlobCache : public BlobCache {
 public:
  InMemoryBlobCache();
  ~InMemoryBlobCache() override;

  // BlobCache implementation.
  bool Contains(const BlobId& id) const override;
  void Put(const BlobId& id, BlobDataPtr data) override;
  BlobDataPtr Get(const BlobId& id) const override;

 private:
  std::map<const BlobId, BlobDataPtr> cache_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryBlobCache);
};

}  // namespace blimp

#endif  // BLIMP_COMMON_BLOB_CACHE_IN_MEMORY_BLOB_CACHE_H_
