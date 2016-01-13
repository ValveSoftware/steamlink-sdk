// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_BLOCKFILE_BLOCK_BITMAPS_V3_H_
#define NET_DISK_CACHE_BLOCKFILE_BLOCK_BITMAPS_V3_H_

#include "base/files/file_path.h"
#include "net/base/net_export.h"
#include "net/disk_cache/blockfile/addr.h"
#include "net/disk_cache/blockfile/block_files.h"

namespace disk_cache {

class BackendImplV3;

// This class is the interface in the v3 disk cache to the set of files holding
// cached data that is small enough to not be efficiently stored in a dedicated
// file (i.e. < kMaxBlockSize). It is primarily used to allocate and free
// regions in those files used to store data.
class NET_EXPORT_PRIVATE BlockBitmaps {
 public:
  BlockBitmaps();
  ~BlockBitmaps();

  void Init(const BlockFilesBitmaps& bitmaps);

  // Creates a new entry on a block file. block_type indicates the size of block
  // to be used (as defined on cache_addr.h), block_count is the number of
  // blocks to allocate, and block_address is the address of the new entry.
  bool CreateBlock(FileType block_type, int block_count, Addr* block_address);

  // Removes an entry from the block files.
  void DeleteBlock(Addr address);

  // Releases the internal bitmaps. The cache is being purged.
  void Clear();

  // Sends UMA stats.
  void ReportStats();

  // Returns true if the blocks pointed by a given address are currently used.
  // This method is only intended for debugging.
  bool IsValid(Addr address);

 private:
  // Returns the header number that stores a given address.
  int GetHeaderNumber(Addr address);

  // Returns the appropriate header to use for a new block.
  int HeaderNumberForNewBlock(FileType block_type, int block_count);

  // Retrieves stats for the given file index.
  void GetFileStats(int index, int* used_count, int* load);

  BlockFilesBitmaps bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(BlockBitmaps);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_BLOCK_BITMAPS_V3_H_
