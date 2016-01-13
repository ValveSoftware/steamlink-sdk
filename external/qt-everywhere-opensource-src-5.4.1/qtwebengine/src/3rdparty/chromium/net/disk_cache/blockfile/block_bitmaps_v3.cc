// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/block_bitmaps_v3.h"

#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "net/disk_cache/blockfile/disk_format_base.h"
#include "net/disk_cache/blockfile/trace.h"

using base::TimeTicks;

namespace disk_cache {

BlockBitmaps::BlockBitmaps() {
}

BlockBitmaps::~BlockBitmaps() {
}

void BlockBitmaps::Init(const BlockFilesBitmaps& bitmaps) {
  bitmaps_ = bitmaps;
}

bool BlockBitmaps::CreateBlock(FileType block_type,
                               int block_count,
                               Addr* block_address) {
  DCHECK_NE(block_type, EXTERNAL);
  DCHECK_NE(block_type, RANKINGS);
  if (block_count < 1 || block_count > kMaxNumBlocks)
    return false;

  int header_num = HeaderNumberForNewBlock(block_type, block_count);
  if (header_num < 0)
    return false;

  int index;
  if (!bitmaps_[header_num].CreateMapBlock(block_count, &index))
    return false;

  if (!index && (block_type == BLOCK_ENTRIES || block_type == BLOCK_EVICTED) &&
      !bitmaps_[header_num].CreateMapBlock(block_count, &index)) {
    // index 0 for entries is a reserved value.
    return false;
  }

  Addr address(block_type, block_count, bitmaps_[header_num].FileId(), index);
  block_address->set_value(address.value());
  Trace("CreateBlock 0x%x", address.value());
  return true;
}

void BlockBitmaps::DeleteBlock(Addr address) {
  if (!address.is_initialized() || address.is_separate_file())
    return;

  int header_num = GetHeaderNumber(address);
  if (header_num < 0)
    return;

  Trace("DeleteBlock 0x%x", address.value());
  bitmaps_[header_num].DeleteMapBlock(address.start_block(),
                                      address.num_blocks());
}

void BlockBitmaps::Clear() {
  bitmaps_.clear();
}

void BlockBitmaps::ReportStats() {
  int used_blocks[kFirstAdditionalBlockFile];
  int load[kFirstAdditionalBlockFile];
  for (int i = 0; i < kFirstAdditionalBlockFile; i++) {
    GetFileStats(i, &used_blocks[i], &load[i]);
  }
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_0", used_blocks[0]);
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_1", used_blocks[1]);
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_2", used_blocks[2]);
  UMA_HISTOGRAM_COUNTS("DiskCache.Blocks_3", used_blocks[3]);

  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_0", load[0], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_1", load[1], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_2", load[2], 101);
  UMA_HISTOGRAM_ENUMERATION("DiskCache.BlockLoad_3", load[3], 101);
}

bool BlockBitmaps::IsValid(Addr address) {
#ifdef NDEBUG
  return true;
#else
  if (!address.is_initialized() || address.is_separate_file())
    return false;

  int header_num = GetHeaderNumber(address);
  if (header_num < 0)
    return false;

  bool rv = bitmaps_[header_num].UsedMapBlock(address.start_block(),
                                              address.num_blocks());
  DCHECK(rv);
  return rv;
#endif
}

int BlockBitmaps::GetHeaderNumber(Addr address) {
  DCHECK_GE(bitmaps_.size(), static_cast<size_t>(kFirstAdditionalBlockFileV3));
  DCHECK(address.is_block_file() || !address.is_initialized());
  if (!address.is_initialized())
    return -1;

  int file_index = address.FileNumber();
  if (static_cast<unsigned int>(file_index) >= bitmaps_.size())
    return -1;

  return file_index;
}

int BlockBitmaps::HeaderNumberForNewBlock(FileType block_type,
                                          int block_count) {
  DCHECK_GT(block_type, 0);
  int header_num = block_type - 1;
  bool found = true;

  TimeTicks start = TimeTicks::Now();
  while (bitmaps_[header_num].NeedToGrowBlockFile(block_count)) {
    header_num = bitmaps_[header_num].NextFileId();
    if (!header_num) {
      found = false;
      break;
    }
  }

  if (!found) {
    // Restart the search, looking for any file with space. We know that all
    // files of this type are low on free blocks, but we cannot grow any file
    // at this time.
    header_num = block_type - 1;
    do {
      if (bitmaps_[header_num].CanAllocate(block_count)) {
        found = true;  // Make sure file 0 is not mistaken with a failure.
        break;
      }
      header_num = bitmaps_[header_num].NextFileId();
    } while (header_num);

    if (!found)
      header_num = -1;
  }

  HISTOGRAM_TIMES("DiskCache.GetFileForNewBlock", TimeTicks::Now() - start);
  return header_num;
}

// We are interested in the total number of blocks used by this file type, and
// the max number of blocks that we can store (reported as the percentage of
// used blocks). In order to find out the number of used blocks, we have to
// substract the empty blocks from the total blocks for each file in the chain.
void BlockBitmaps::GetFileStats(int index, int* used_count, int* load) {
  int max_blocks = 0;
  *used_count = 0;
  *load = 0;
  do {
    int capacity = bitmaps_[index].Capacity();
    int used = capacity - bitmaps_[index].EmptyBlocks();
    DCHECK_GE(used, 0);

    max_blocks += capacity;
    *used_count += used;

    index = bitmaps_[index].NextFileId();
  } while (index);

  if (max_blocks)
    *load = *used_count * 100 / max_blocks;
}

}  // namespace disk_cache
