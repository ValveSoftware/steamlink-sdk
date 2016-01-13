// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_MEMORY_MEM_RANKINGS_H_
#define NET_DISK_CACHE_MEMORY_MEM_RANKINGS_H_

#include "base/basictypes.h"

namespace disk_cache {

class MemEntryImpl;

// This class handles the ranking information for the memory-only cache.
class MemRankings {
 public:
  MemRankings() : head_(NULL), tail_(NULL) {}
  ~MemRankings();

  // Inserts a given entry at the head of the queue.
  void Insert(MemEntryImpl* node);

  // Removes a given entry from the LRU list.
  void Remove(MemEntryImpl* node);

  // Moves a given entry to the head.
  void UpdateRank(MemEntryImpl* node);

  // Iterates through the list.
  MemEntryImpl* GetNext(MemEntryImpl* node);
  MemEntryImpl* GetPrev(MemEntryImpl* node);

 private:
  MemEntryImpl* head_;
  MemEntryImpl* tail_;

  DISALLOW_COPY_AND_ASSIGN(MemRankings);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_MEMORY_MEM_RANKINGS_H_
