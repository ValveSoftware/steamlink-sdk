// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/addr.h"
#include "net/disk_cache/blockfile/block_bitmaps_v3.h"
#include "net/disk_cache/blockfile/block_files.h"
#include "net/disk_cache/blockfile/disk_format_base.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that we add and remove blocks correctly.
TEST(DiskCacheBlockBitmaps, V3AllocationMap) {
  disk_cache::BlockBitmaps block_bitmaps;
  disk_cache::BlockFilesBitmaps bitmaps;

  const int kNumHeaders = 10;
  disk_cache::BlockFileHeader headers[kNumHeaders];
  for (int i = 0; i < kNumHeaders; i++) {
    memset(&headers[i], 0, sizeof(headers[i]));
    headers[i].magic = disk_cache::kBlockMagic;
    headers[i].version = disk_cache::kBlockCurrentVersion;
    headers[i].this_file = static_cast<int16>(i);
    headers[i].empty[3] = 200;
    headers[i].max_entries = 800;
    bitmaps.push_back(disk_cache::BlockHeader(&headers[i]));
  }

  block_bitmaps.Init(bitmaps);

  // Create a bunch of entries.
  const int kSize = 100;
  disk_cache::Addr address[kSize];
  for (int i = 0; i < kSize; i++) {
    SCOPED_TRACE(i);
    int block_size = i % 4 + 1;
    ASSERT_TRUE(block_bitmaps.CreateBlock(disk_cache::BLOCK_1K, block_size,
                                          &address[i]));
    EXPECT_EQ(disk_cache::BLOCK_1K, address[i].file_type());
    EXPECT_EQ(block_size, address[i].num_blocks());
    int start = address[i].start_block();

    // Verify that the allocated entry doesn't cross a 4 block boundary.
    EXPECT_EQ(start / 4, (start + block_size - 1) / 4);
  }

  for (int i = 0; i < kSize; i++) {
    SCOPED_TRACE(i);
    EXPECT_TRUE(block_bitmaps.IsValid(address[i]));
  }

  // The first part of the allocation map should be completely filled. We used
  // 10 bits per each of four entries, so 250 bits total. All entries should go
  // to the third file.
  uint8* buffer = reinterpret_cast<uint8*>(&headers[2].allocation_map);
  for (int i = 0; i < 29; i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(0xff, buffer[i]);
  }

  for (int i = 0; i < kSize; i++) {
    SCOPED_TRACE(i);
    block_bitmaps.DeleteBlock(address[i]);
  }

  // The allocation map should be empty.
  for (int i =0; i < 50; i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(0, buffer[i]);
  }
}
