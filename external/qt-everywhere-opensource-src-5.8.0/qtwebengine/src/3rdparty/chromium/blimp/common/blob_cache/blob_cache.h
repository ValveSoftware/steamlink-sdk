// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_BLOB_CACHE_BLOB_CACHE_H_
#define BLIMP_COMMON_BLOB_CACHE_BLOB_CACHE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "blimp/common/blimp_common_export.h"

namespace blimp {

using BlobId = std::string;
using BlobData = base::RefCountedData<std::string>;

// Immutable, ref-counted representation of blob payloads, suitable for sharing
// across threads.
using BlobDataPtr = scoped_refptr<const BlobData>;

// An interface for a cache of blobs.
class BLIMP_COMMON_EXPORT BlobCache {
 public:
  virtual ~BlobCache() {}

  // Returns true if there is a cache item |id|.
  virtual bool Contains(const BlobId& id) const = 0;

  // Stores |data| with the identifier |id| in the cache.
  // Command is ignored if there is already a cache item stored under |id|.
  virtual void Put(const BlobId& id, BlobDataPtr data) = 0;

  // Returns a pointer to the cache item |id|, or nullptr if no cache item
  // exists under that identifier.
  virtual BlobDataPtr Get(const BlobId& id) const = 0;
};

}  // namespace blimp

#endif  // BLIMP_COMMON_BLOB_CACHE_BLOB_CACHE_H_
