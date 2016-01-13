// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_STATS_H_
#define NET_DISK_CACHE_BLOCKFILE_STATS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "net/disk_cache/blockfile/addr.h"

namespace base {
class HistogramSamples;
}  // namespace base

namespace disk_cache {

typedef std::vector<std::pair<std::string, std::string> > StatsItems;

// This class stores cache-specific usage information, for tunning purposes.
class Stats {
 public:
  static const int kDataSizesLength = 28;
  enum Counters {
    MIN_COUNTER = 0,
    OPEN_MISS = MIN_COUNTER,
    OPEN_HIT,
    CREATE_MISS,
    CREATE_HIT,
    RESURRECT_HIT,
    CREATE_ERROR,
    TRIM_ENTRY,
    DOOM_ENTRY,
    DOOM_CACHE,
    INVALID_ENTRY,
    OPEN_ENTRIES,  // Average number of open entries.
    MAX_ENTRIES,  // Maximum number of open entries.
    TIMER,
    READ_DATA,
    WRITE_DATA,
    OPEN_RANKINGS,  // An entry has to be read just to modify rankings.
    GET_RANKINGS,  // We got the ranking info without reading the whole entry.
    FATAL_ERROR,
    LAST_REPORT,  // Time of the last time we sent a report.
    LAST_REPORT_TIMER,  // Timer count of the last time we sent a report.
    DOOM_RECENT,  // The cache was partially cleared.
    UNUSED,  // Was: ga.js was evicted from the cache.
    MAX_COUNTER
  };

  Stats();
  ~Stats();

  // Initializes this object with |data| from disk.
  bool Init(void* data, int num_bytes, Addr address);

  // Generates a size distribution histogram.
  void InitSizeHistogram();

  // Returns the number of bytes needed to store the stats on disk.
  int StorageSize();

  // Tracks changes to the stoage space used by an entry.
  void ModifyStorageStats(int32 old_size, int32 new_size);

  // Tracks general events.
  void OnEvent(Counters an_event);
  void SetCounter(Counters counter, int64 value);
  int64 GetCounter(Counters counter) const;

  void GetItems(StatsItems* items);
  int GetHitRatio() const;
  int GetResurrectRatio() const;
  void ResetRatios();

  // Returns the lower bound of the space used by entries bigger than 512 KB.
  int GetLargeEntriesSize();

  // Writes the stats into |data|, to be stored at the given cache address.
  // Returns the number of bytes copied.
  int SerializeStats(void* data, int num_bytes, Addr* address);

 private:
  // Supports generation of SizeStats histogram data.
  int GetBucketRange(size_t i) const;
  int GetStatsBucket(int32 size);
  int GetRatio(Counters hit, Counters miss) const;

  Addr storage_addr_;
  int data_sizes_[kDataSizesLength];
  int64 counters_[MAX_COUNTER];

  DISALLOW_COPY_AND_ASSIGN(Stats);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_STATS_H_
