// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_EVICTION_V3_H_
#define NET_DISK_CACHE_BLOCKFILE_EVICTION_V3_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "net/disk_cache/blockfile/disk_format_v3.h"
#include "net/disk_cache/blockfile/index_table_v3.h"

namespace disk_cache {

class BackendImplV3;
class CacheRankingsBlock;
class EntryImplV3;

namespace Rankings {
typedef int List;
}

// This class implements the eviction algorithm for the cache and it is tightly
// integrated with BackendImpl.
class EvictionV3 {
 public:
  EvictionV3();
  ~EvictionV3();

  void Init(BackendImplV3* backend);
  void Stop();

  // Deletes entries from the cache until the current size is below the limit.
  // If empty is true, the whole cache will be trimmed, regardless of being in
  // use.
  void TrimCache(bool empty);

  // Notifications of interesting events for a given entry.
  void OnOpenEntry(EntryImplV3* entry);
  void OnCreateEntry(EntryImplV3* entry);

  // Testing interface.
  void SetTestMode();
  void TrimDeletedList(bool empty);

 private:
  void PostDelayedTrim();
  void DelayedTrim();
  bool ShouldTrim();
  bool ShouldTrimDeleted();
  bool EvictEntry(CacheRankingsBlock* node, bool empty, Rankings::List list);

  void TrimCacheV2(bool empty);
  void TrimDeleted(bool empty);

  bool NodeIsOldEnough(CacheRankingsBlock* node, int list);
  int SelectListByLength();
  void ReportListStats();

  BackendImplV3* backend_;
  IndexTable* index_;
  IndexHeaderV3* header_;
  int max_size_;
  int trim_delays_;
  bool lru_;
  bool first_trim_;
  bool trimming_;
  bool delay_trim_;
  bool init_;
  bool test_mode_;
  base::WeakPtrFactory<EvictionV3> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EvictionV3);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_EVICTION_V3_H_
