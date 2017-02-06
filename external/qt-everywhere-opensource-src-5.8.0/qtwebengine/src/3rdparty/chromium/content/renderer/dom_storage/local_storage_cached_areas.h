// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_
#define CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "url/origin.h"

namespace content {
class LocalStorageCachedArea;

namespace mojom {
class StoragePartitionService;
}

// Keeps a map of all the LocalStorageCachedArea objects in a renderer. This is
// needed because we can have n LocalStorageArea objects for the same origin but
// we want just one LocalStorageCachedArea to service them (no point in having
// multiple caches of the same data in the same process).
class LocalStorageCachedAreas {
 public:
  explicit LocalStorageCachedAreas(
      mojom::StoragePartitionService* storage_partition_service);
  ~LocalStorageCachedAreas();

  // Returns, creating if necessary, a cached storage area for the given origin.
  scoped_refptr<LocalStorageCachedArea>
      GetCachedArea(const url::Origin& origin);

  // Called by LocalStorageCachedArea on destruction.
  void CacheAreaClosed(LocalStorageCachedArea* cached_area);

 private:
  mojom::StoragePartitionService* const storage_partition_service_;

  // Maps from an origin to its LocalStorageCachedArea object. The object owns
  // itself.
  std::map<url::Origin, LocalStorageCachedArea*> cached_areas_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageCachedAreas);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREAS_H_
