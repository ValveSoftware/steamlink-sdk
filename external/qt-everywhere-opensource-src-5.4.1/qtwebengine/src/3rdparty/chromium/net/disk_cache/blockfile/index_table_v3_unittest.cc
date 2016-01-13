// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/disk_cache/blockfile/addr.h"
#include "net/disk_cache/blockfile/disk_format_v3.h"
#include "net/disk_cache/blockfile/index_table_v3.h"
#include "testing/gtest/include/gtest/gtest.h"

using disk_cache::EntryCell;
using disk_cache::IndexCell;
using disk_cache::IndexTable;
using disk_cache::IndexTableInitData;

namespace {

int GetChecksum(const IndexCell& source) {
  // Only the cell pointer is relevant.
  disk_cache::Addr addr;
  IndexCell* cell = const_cast<IndexCell*>(&source);
  EntryCell entry = EntryCell::GetEntryCellForTest(0, 0, addr, cell, false);

  IndexCell result;
  entry.SerializaForTest(&result);
  return result.last_part >> 6;
}

class MockIndexBackend : public disk_cache::IndexTableBackend {
 public:
  MockIndexBackend() : grow_called_(false), buffer_len_(-1) {}
  virtual ~MockIndexBackend() {}

  bool grow_called() const { return grow_called_; }
  int buffer_len() const { return buffer_len_; }

  virtual void GrowIndex() OVERRIDE { grow_called_ = true; }
  virtual void SaveIndex(net::IOBuffer* buffer, int buffer_len) OVERRIDE {
    buffer_len_ = buffer_len;
  }
  virtual void DeleteCell(EntryCell cell) OVERRIDE {}
  virtual void FixCell(EntryCell cell) OVERRIDE {}

 private:
  bool grow_called_;
  int buffer_len_;
};

class TestCacheTables {
 public:
  // |num_entries| is the capacity of the main table. The extra table is half
  // the size of the main table.
  explicit TestCacheTables(int num_entries);
  ~TestCacheTables() {}

  void GetInitData(IndexTableInitData* result);
  void CopyFrom(const TestCacheTables& other);
  base::Time start_time() const { return start_time_; }

 private:
  scoped_ptr<uint64[]> main_bitmap_;
  scoped_ptr<disk_cache::IndexBucket[]> main_table_;
  scoped_ptr<disk_cache::IndexBucket[]> extra_table_;
  base::Time start_time_;
  int num_bitmap_bytes_;

  DISALLOW_COPY_AND_ASSIGN(TestCacheTables);
};

TestCacheTables::TestCacheTables(int num_entries) {
  DCHECK_GE(num_entries, 1024);
  DCHECK_EQ(num_entries, num_entries / 1024 * 1024);
  main_table_.reset(new disk_cache::IndexBucket[num_entries]);
  extra_table_.reset(new disk_cache::IndexBucket[num_entries / 2]);
  memset(main_table_.get(), 0, num_entries * sizeof(*main_table_.get()));
  memset(extra_table_.get(), 0, num_entries / 2 * sizeof(*extra_table_.get()));

  // We allow IndexBitmap smaller than a page because the code should not really
  // depend on that.
  num_bitmap_bytes_ = (num_entries + num_entries / 2) / 8;
  size_t required_size = sizeof(disk_cache::IndexHeaderV3) + num_bitmap_bytes_;
  main_bitmap_.reset(new uint64[required_size / sizeof(uint64)]);
  memset(main_bitmap_.get(), 0, required_size);

  disk_cache::IndexHeaderV3* header =
      reinterpret_cast<disk_cache::IndexHeaderV3*>(main_bitmap_.get());

  header->magic = disk_cache::kIndexMagicV3;
  header->version = disk_cache::kVersion3;
  header->table_len = num_entries + num_entries / 2;
  header->max_bucket = num_entries / 4 - 1;

  start_time_ = base::Time::Now();
  header->create_time = start_time_.ToInternalValue();
  header->base_time =
      (start_time_ - base::TimeDelta::FromDays(20)).ToInternalValue();

  if (num_entries < 64 * 1024)
    header->flags = disk_cache::SMALL_CACHE;
}

void TestCacheTables::GetInitData(IndexTableInitData* result) {
  result->index_bitmap =
      reinterpret_cast<disk_cache::IndexBitmap*>(main_bitmap_.get());

  result->main_table = main_table_.get();
  result->extra_table = extra_table_.get();

  result->backup_header.reset(new disk_cache::IndexHeaderV3);
  memcpy(result->backup_header.get(), result->index_bitmap,
         sizeof(result->index_bitmap->header));

  result->backup_bitmap.reset(new uint32[num_bitmap_bytes_ / sizeof(uint32)]);
  memcpy(result->backup_bitmap.get(), result->index_bitmap->bitmap,
         num_bitmap_bytes_);
}

void TestCacheTables::CopyFrom(const TestCacheTables& other) {
  disk_cache::IndexBitmap* this_bitmap =
    reinterpret_cast<disk_cache::IndexBitmap*>(main_bitmap_.get());
  disk_cache::IndexBitmap* other_bitmap =
    reinterpret_cast<disk_cache::IndexBitmap*>(other.main_bitmap_.get());

  DCHECK_GE(this_bitmap->header.table_len, other_bitmap->header.table_len);
  DCHECK_GE(num_bitmap_bytes_, other.num_bitmap_bytes_);

  memcpy(this_bitmap->bitmap, other_bitmap->bitmap, other.num_bitmap_bytes_);

  int main_table_buckets = (other_bitmap->header.table_len * 2 / 3) / 4;
  int extra_table_buckets = (other_bitmap->header.table_len * 1 / 3) / 4;
  memcpy(main_table_.get(), other.main_table_.get(),
         main_table_buckets * sizeof(disk_cache::IndexBucket));
  memcpy(extra_table_.get(), other.extra_table_.get(),
         extra_table_buckets * sizeof(disk_cache::IndexBucket));

  this_bitmap->header.num_entries = other_bitmap->header.num_entries;
  this_bitmap->header.used_cells = other_bitmap->header.used_cells;
  this_bitmap->header.max_bucket = other_bitmap->header.max_bucket;
  this_bitmap->header.create_time = other_bitmap->header.create_time;
  this_bitmap->header.base_time = other_bitmap->header.base_time;
  this_bitmap->header.flags = other_bitmap->header.flags;
  start_time_ = other.start_time_;
}

}  // namespace

TEST(DiskCacheIndexTable, EntryCell) {
  uint32 hash = 0x55aa6699;
  disk_cache::Addr addr(disk_cache::BLOCK_ENTRIES, 1, 5, 0x4531);
  bool small_table = true;
  int cell_num = 88;
  int reuse = 6;
  int timestamp = 123456;
  disk_cache::EntryState state = disk_cache::ENTRY_MODIFIED;
  disk_cache::EntryGroup group = disk_cache::ENTRY_HIGH_USE;

  for (int i = 0; i < 4; i++) {
    SCOPED_TRACE(i);
    EntryCell entry = EntryCell::GetEntryCellForTest(cell_num, hash, addr, NULL,
                                                     small_table);
    EXPECT_EQ(disk_cache::ENTRY_NO_USE, entry.GetGroup());
    EXPECT_EQ(disk_cache::ENTRY_NEW, entry.GetState());

    entry.SetGroup(group);
    entry.SetState(state);
    entry.SetReuse(reuse);
    entry.SetTimestamp(timestamp);

    EXPECT_TRUE(entry.IsValid());
    EXPECT_EQ(hash, entry.hash());
    EXPECT_EQ(cell_num, entry.cell_num());
    EXPECT_EQ(addr.value(), entry.GetAddress().value());

    EXPECT_EQ(group, entry.GetGroup());
    EXPECT_EQ(state, entry.GetState());
    EXPECT_EQ(reuse, entry.GetReuse());
    EXPECT_EQ(timestamp, entry.GetTimestamp());

    // Store the data and read it again.
    IndexCell cell;
    entry.SerializaForTest(&cell);

    EntryCell entry2 = EntryCell::GetEntryCellForTest(cell_num, hash, addr,
                                                      &cell, small_table);

    EXPECT_EQ(addr.value(), entry2.GetAddress().value());

    EXPECT_EQ(group, entry2.GetGroup());
    EXPECT_EQ(state, entry2.GetState());
    EXPECT_EQ(reuse, entry2.GetReuse());
    EXPECT_EQ(timestamp, entry2.GetTimestamp());

    small_table = !small_table;
    if (i == 1) {
      hash = ~hash;
      cell_num *= 5;
      state = disk_cache::ENTRY_USED;
      group = disk_cache::ENTRY_EVICTED;
      addr = disk_cache::Addr(disk_cache::BLOCK_EVICTED, 1, 6, 0x18a5);
      reuse = 15;  // 4 bits
      timestamp = 0xfffff;  // 20 bits.
    }
  }
}

// Goes over some significant values for a cell's sum.
TEST(DiskCacheIndexTable, EntryCellSum) {
  IndexCell source;
  source.Clear();
  EXPECT_EQ(0, GetChecksum(source));

  source.first_part++;
  EXPECT_EQ(1, GetChecksum(source));

  source.Clear();
  source.last_part = 0x80;
  EXPECT_EQ(0, GetChecksum(source));

  source.last_part = 0x55;
  EXPECT_EQ(3, GetChecksum(source));

  source.first_part = 0x555555;
  EXPECT_EQ(2, GetChecksum(source));

  source.last_part = 0;
  EXPECT_EQ(1, GetChecksum(source));

  source.first_part = GG_UINT64_C(0x8000000080000000);
  EXPECT_EQ(0, GetChecksum(source));

  source.first_part = GG_UINT64_C(0x4000000040000000);
  EXPECT_EQ(2, GetChecksum(source));

  source.first_part = GG_UINT64_C(0x200000020000000);
  EXPECT_EQ(1, GetChecksum(source));

  source.first_part = GG_UINT64_C(0x100000010010000);
  EXPECT_EQ(3, GetChecksum(source));

  source.first_part = 0x80008000;
  EXPECT_EQ(0, GetChecksum(source));

  source.first_part = GG_UINT64_C(0x800000008000);
  EXPECT_EQ(1, GetChecksum(source));

  source.first_part = 0x8080;
  EXPECT_EQ(0, GetChecksum(source));

  source.first_part = 0x800080;
  EXPECT_EQ(1, GetChecksum(source));

  source.first_part = 0x88;
  EXPECT_EQ(0, GetChecksum(source));

  source.first_part = 0x808;
  EXPECT_EQ(1, GetChecksum(source));

  source.first_part = 0xA;
  EXPECT_EQ(0, GetChecksum(source));

  source.first_part = 0x22;
  EXPECT_EQ(1, GetChecksum(source));
}

TEST(DiskCacheIndexTable, Basics) {
  TestCacheTables cache(1024);
  IndexTableInitData init_data;
  cache.GetInitData(&init_data);

  IndexTable index(NULL);
  index.Init(&init_data);

  // Write some entries.
  disk_cache::CellList entries;
  for (int i = 0; i < 250; i++) {
    SCOPED_TRACE(i);
    uint32 hash = i * i * 1111 + i * 11;
    disk_cache::Addr addr(disk_cache::BLOCK_ENTRIES, 1, 5, i * 13 + 1);
    EntryCell entry = index.CreateEntryCell(hash, addr);
    EXPECT_TRUE(entry.IsValid());

    disk_cache::CellInfo info = { hash, addr };
    entries.push_back(info);
  }

  // Read them back.
  for (size_t i = 0; i < entries.size(); i++) {
    SCOPED_TRACE(i);
    uint32 hash = entries[i].hash;
    disk_cache::Addr addr = entries[i].address;

    disk_cache::EntrySet found_entries = index.LookupEntries(hash);
    ASSERT_EQ(1u, found_entries.cells.size());
    EXPECT_TRUE(found_entries.cells[0].IsValid());
    EXPECT_EQ(hash, found_entries.cells[0].hash());
    EXPECT_EQ(addr.value(), found_entries.cells[0].GetAddress().value());

    EntryCell entry = index.FindEntryCell(hash, addr);
    EXPECT_TRUE(entry.IsValid());
    EXPECT_EQ(hash, entry.hash());
    EXPECT_EQ(addr.value(), entry.GetAddress().value());

    // Delete the first 100 entries.
    if (i < 100)
      index.SetSate(hash, addr, disk_cache::ENTRY_DELETED);
  }

  // See what we have now.
  for (size_t i = 0; i < entries.size(); i++) {
    SCOPED_TRACE(i);
    uint32 hash = entries[i].hash;
    disk_cache::Addr addr = entries[i].address;

    disk_cache::EntrySet found_entries = index.LookupEntries(hash);
    if (i < 100) {
      EXPECT_EQ(0u, found_entries.cells.size());
    } else {
      ASSERT_EQ(1u, found_entries.cells.size());
      EXPECT_TRUE(found_entries.cells[0].IsValid());
      EXPECT_EQ(hash, found_entries.cells[0].hash());
      EXPECT_EQ(addr.value(), found_entries.cells[0].GetAddress().value());
    }
  }
}

// Tests handling of multiple entries with the same hash.
TEST(DiskCacheIndexTable, SameHash) {
  TestCacheTables cache(1024);
  IndexTableInitData init_data;
  cache.GetInitData(&init_data);

  IndexTable index(NULL);
  index.Init(&init_data);

  disk_cache::CellList entries;
  uint32 hash = 0x55aa55bb;
  for (int i = 0; i < 6; i++) {
    SCOPED_TRACE(i);
    disk_cache::Addr addr(disk_cache::BLOCK_ENTRIES, 1, 5, i * 13 + 1);
    EntryCell entry = index.CreateEntryCell(hash, addr);
    EXPECT_TRUE(entry.IsValid());

    disk_cache::CellInfo info = { hash, addr };
    entries.push_back(info);
  }

  disk_cache::EntrySet found_entries = index.LookupEntries(hash);
  EXPECT_EQ(0, found_entries.evicted_count);
  ASSERT_EQ(6u, found_entries.cells.size());

  for (size_t i = 0; i < found_entries.cells.size(); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(entries[i].address, found_entries.cells[i].GetAddress());
  }

  // Now verify handling of entries on different states.
  index.SetSate(hash, entries[0].address, disk_cache::ENTRY_DELETED);
  index.SetSate(hash, entries[1].address, disk_cache::ENTRY_DELETED);
  index.SetSate(hash, entries[2].address, disk_cache::ENTRY_USED);
  index.SetSate(hash, entries[3].address, disk_cache::ENTRY_USED);
  index.SetSate(hash, entries[4].address, disk_cache::ENTRY_USED);

  found_entries = index.LookupEntries(hash);
  EXPECT_EQ(0, found_entries.evicted_count);
  ASSERT_EQ(4u, found_entries.cells.size());

  index.SetSate(hash, entries[3].address, disk_cache::ENTRY_OPEN);
  index.SetSate(hash, entries[4].address, disk_cache::ENTRY_OPEN);

  found_entries = index.LookupEntries(hash);
  EXPECT_EQ(0, found_entries.evicted_count);
  ASSERT_EQ(4u, found_entries.cells.size());

  index.SetSate(hash, entries[4].address, disk_cache::ENTRY_MODIFIED);

  found_entries = index.LookupEntries(hash);
  EXPECT_EQ(0, found_entries.evicted_count);
  ASSERT_EQ(4u, found_entries.cells.size());

  index.SetSate(hash, entries[1].address, disk_cache::ENTRY_FREE);

  found_entries = index.LookupEntries(hash);
  EXPECT_EQ(0, found_entries.evicted_count);
  ASSERT_EQ(4u, found_entries.cells.size());

  // FindEntryCell should not see deleted entries.
  EntryCell entry = index.FindEntryCell(hash, entries[0].address);
  EXPECT_FALSE(entry.IsValid());

  // A free entry is gone.
  entry = index.FindEntryCell(hash, entries[1].address);
  EXPECT_FALSE(entry.IsValid());

  // Locate a used entry, and evict it. This is not really a correct operation
  // in that an existing cell doesn't transition to evicted; instead a new cell
  // for the evicted entry (on a different block file) should be created. Still,
  // at least evicted_count would be valid.
  entry = index.FindEntryCell(hash, entries[2].address);
  EXPECT_TRUE(entry.IsValid());
  entry.SetGroup(disk_cache::ENTRY_EVICTED);
  index.Save(&entry);

  found_entries = index.LookupEntries(hash);
  EXPECT_EQ(1, found_entries.evicted_count);
  ASSERT_EQ(4u, found_entries.cells.size());

  // Now use the proper way to get an evicted entry.
  disk_cache::Addr addr2(disk_cache::BLOCK_EVICTED, 1, 6, 6);  // Any address.
  entry = index.CreateEntryCell(hash, addr2);
  EXPECT_TRUE(entry.IsValid());
  EXPECT_EQ(disk_cache::ENTRY_EVICTED, entry.GetGroup());

  found_entries = index.LookupEntries(hash);
  EXPECT_EQ(2, found_entries.evicted_count);
  ASSERT_EQ(5u, found_entries.cells.size());
}

TEST(DiskCacheIndexTable, Timestamps) {
  TestCacheTables cache(1024);
  IndexTableInitData init_data;
  cache.GetInitData(&init_data);

  IndexTable index(NULL);
  index.Init(&init_data);

  // The granularity should be 1 minute.
  int timestamp1 = index.CalculateTimestamp(cache.start_time());
  int timestamp2 = index.CalculateTimestamp(cache.start_time() +
                                            base::TimeDelta::FromSeconds(59));
  EXPECT_EQ(timestamp1, timestamp2);

  int timestamp3 = index.CalculateTimestamp(cache.start_time() +
                                            base::TimeDelta::FromSeconds(61));
  EXPECT_EQ(timestamp1 + 1, timestamp3);

  int timestamp4 = index.CalculateTimestamp(cache.start_time() +
                                            base::TimeDelta::FromSeconds(119));
  EXPECT_EQ(timestamp1 + 1, timestamp4);

  int timestamp5 = index.CalculateTimestamp(cache.start_time() +
                                            base::TimeDelta::FromSeconds(121));
  EXPECT_EQ(timestamp1 + 2, timestamp5);

  int timestamp6 = index.CalculateTimestamp(cache.start_time() -
                                            base::TimeDelta::FromSeconds(30));
  EXPECT_EQ(timestamp1 - 1, timestamp6);

  // The base should be 20 days in the past.
  int timestamp7 = index.CalculateTimestamp(cache.start_time() -
                                            base::TimeDelta::FromDays(20));
  int timestamp8 = index.CalculateTimestamp(cache.start_time() -
                                            base::TimeDelta::FromDays(35));
  EXPECT_EQ(timestamp7, timestamp8);
  EXPECT_EQ(0, timestamp8);

  int timestamp9 = index.CalculateTimestamp(cache.start_time() -
                                            base::TimeDelta::FromDays(19));
  EXPECT_NE(0, timestamp9);
}

// Tests GetOldest and GetNextCells.
TEST(DiskCacheIndexTable, Iterations) {
  TestCacheTables cache(1024);
  IndexTableInitData init_data;
  cache.GetInitData(&init_data);

  IndexTable index(NULL);
  index.Init(&init_data);

  base::Time time = cache.start_time();

  // Write some entries.
  disk_cache::CellList entries;
  for (int i = 0; i < 44; i++) {
    SCOPED_TRACE(i);
    uint32 hash = i;  // The entries will be ordered on the table.
    disk_cache::Addr addr(disk_cache::BLOCK_ENTRIES, 1, 5, i * 13 + 1);
    if (i < 10 || i == 40)
      addr = disk_cache::Addr(disk_cache::BLOCK_EVICTED, 1, 6, i * 13 + 1);

    EntryCell entry = index.CreateEntryCell(hash, addr);
    EXPECT_TRUE(entry.IsValid());

    disk_cache::CellInfo info = { hash, addr };
    entries.push_back(info);

    if (i < 10 || i == 40) {
      // Do nothing. These are ENTRY_EVICTED by default.
    } else if (i < 20 || i == 41) {
      entry.SetGroup(disk_cache::ENTRY_HIGH_USE);
      index.Save(&entry);
    } else if (i < 30 || i == 42) {
      entry.SetGroup(disk_cache::ENTRY_LOW_USE);
      index.Save(&entry);
    }

    // Entries [30,39] and 43 are marked as ENTRY_NO_USE (the default).

    if (!(i % 10))
      time += base::TimeDelta::FromMinutes(1);

    index.UpdateTime(hash, addr, time);
  }

  // Get the oldest entries of each group.
  disk_cache::IndexIterator no_use, low_use, high_use;
  index.GetOldest(&no_use, &low_use, &high_use);
  ASSERT_EQ(10u, no_use.cells.size());
  ASSERT_EQ(10u, low_use.cells.size());
  ASSERT_EQ(10u, high_use.cells.size());

  EXPECT_EQ(entries[10].hash, high_use.cells[0].hash);
  EXPECT_EQ(entries[19].hash, high_use.cells[9].hash);
  EXPECT_EQ(entries[20].hash, low_use.cells[0].hash);
  EXPECT_EQ(entries[29].hash, low_use.cells[9].hash);
  EXPECT_EQ(entries[30].hash, no_use.cells[0].hash);
  EXPECT_EQ(entries[39].hash, no_use.cells[9].hash);

  // Now start an iteration from the head (most recent entry).
  disk_cache::IndexIterator iterator;
  iterator.timestamp = index.CalculateTimestamp(time) + 1;
  iterator.forward = false;  // Back in time.

  ASSERT_TRUE(index.GetNextCells(&iterator));
  ASSERT_EQ(3u, iterator.cells.size());
  EXPECT_EQ(entries[41].hash, iterator.cells[0].hash);
  EXPECT_EQ(entries[42].hash, iterator.cells[1].hash);
  EXPECT_EQ(entries[43].hash, iterator.cells[2].hash);

  ASSERT_TRUE(index.GetNextCells(&iterator));
  ASSERT_EQ(10u, iterator.cells.size());
  EXPECT_EQ(entries[30].hash, iterator.cells[0].hash);
  EXPECT_EQ(entries[39].hash, iterator.cells[9].hash);

  ASSERT_TRUE(index.GetNextCells(&iterator));
  ASSERT_EQ(10u, iterator.cells.size());
  EXPECT_EQ(entries[20].hash, iterator.cells[0].hash);
  EXPECT_EQ(entries[29].hash, iterator.cells[9].hash);

  ASSERT_TRUE(index.GetNextCells(&iterator));
  ASSERT_EQ(10u, iterator.cells.size());
  EXPECT_EQ(entries[10].hash, iterator.cells[0].hash);
  EXPECT_EQ(entries[19].hash, iterator.cells[9].hash);

  ASSERT_FALSE(index.GetNextCells(&iterator));

  // Now start an iteration from the tail (oldest entry).
  iterator.timestamp = 0;
  iterator.forward = true;

  ASSERT_TRUE(index.GetNextCells(&iterator));
  ASSERT_EQ(10u, iterator.cells.size());
  EXPECT_EQ(entries[10].hash, iterator.cells[0].hash);
  EXPECT_EQ(entries[19].hash, iterator.cells[9].hash);

  ASSERT_TRUE(index.GetNextCells(&iterator));
  ASSERT_EQ(10u, iterator.cells.size());
  EXPECT_EQ(entries[20].hash, iterator.cells[0].hash);
  EXPECT_EQ(entries[29].hash, iterator.cells[9].hash);

  ASSERT_TRUE(index.GetNextCells(&iterator));
  ASSERT_EQ(10u, iterator.cells.size());
  EXPECT_EQ(entries[30].hash, iterator.cells[0].hash);
  EXPECT_EQ(entries[39].hash, iterator.cells[9].hash);

  ASSERT_TRUE(index.GetNextCells(&iterator));
  ASSERT_EQ(3u, iterator.cells.size());
  EXPECT_EQ(entries[41].hash, iterator.cells[0].hash);
  EXPECT_EQ(entries[42].hash, iterator.cells[1].hash);
  EXPECT_EQ(entries[43].hash, iterator.cells[2].hash);
}

// Tests doubling of the table.
TEST(DiskCacheIndexTable, Doubling) {
  IndexTable index(NULL);
  int size = 1024;
  scoped_ptr<TestCacheTables> cache(new TestCacheTables(size));
  int entry_id = 0;
  disk_cache::CellList entries;

  // Go from 1024 to 256k cells.
  for (int resizes = 0; resizes <= 8; resizes++) {
    scoped_ptr<TestCacheTables> old_cache(cache.Pass());
    cache.reset(new TestCacheTables(size));
    cache.get()->CopyFrom(*old_cache.get());

    IndexTableInitData init_data;
    cache.get()->GetInitData(&init_data);
    index.Init(&init_data);

    // Write some entries.
    for (int i = 0; i < 250; i++, entry_id++) {
      SCOPED_TRACE(entry_id);
      uint32 hash = entry_id * i * 321 + entry_id * 13;
      disk_cache::Addr addr(disk_cache::BLOCK_ENTRIES, 1, 5, entry_id * 17 + 1);
      EntryCell entry = index.CreateEntryCell(hash, addr);
      EXPECT_TRUE(entry.IsValid());

      disk_cache::CellInfo info = { hash, addr };
      entries.push_back(info);
    }
    size *= 2;
  }

  // Access all the entries.
  for (size_t i = 0; i < entries.size(); i++) {
    SCOPED_TRACE(i);
    disk_cache::EntrySet found_entries = index.LookupEntries(entries[i].hash);
    ASSERT_EQ(1u, found_entries.cells.size());
    EXPECT_TRUE(found_entries.cells[0].IsValid());
  }
}

// Tests bucket chaining when growing the index.
TEST(DiskCacheIndexTable, BucketChains) {
  IndexTable index(NULL);
  int size = 1024;
  scoped_ptr<TestCacheTables> cache(new TestCacheTables(size));
  disk_cache::CellList entries;

  IndexTableInitData init_data;
  cache.get()->GetInitData(&init_data);
  index.Init(&init_data);

  // Write some entries.
  for (int i = 0; i < 8; i++) {
    SCOPED_TRACE(i);
    uint32 hash = i * 256;
    disk_cache::Addr addr(disk_cache::BLOCK_ENTRIES, 1, 5, i * 7 + 1);
    EntryCell entry = index.CreateEntryCell(hash, addr);
    EXPECT_TRUE(entry.IsValid());

    disk_cache::CellInfo info = { hash, addr };
    entries.push_back(info);
  }

  // Double the size.
  scoped_ptr<TestCacheTables> old_cache(cache.Pass());
  cache.reset(new TestCacheTables(size * 2));
  cache.get()->CopyFrom(*old_cache.get());

  cache.get()->GetInitData(&init_data);
  index.Init(&init_data);

  // Write more entries, starting with the upper half of the table.
  for (int i = 9; i < 11; i++) {
    SCOPED_TRACE(i);
    uint32 hash = i * 256;
    disk_cache::Addr addr(disk_cache::BLOCK_ENTRIES, 1, 5, i * 7 + 1);
    EntryCell entry = index.CreateEntryCell(hash, addr);
    EXPECT_TRUE(entry.IsValid());

    disk_cache::CellInfo info = { hash, addr };
    entries.push_back(info);
  }

  // Access all the entries.
  for (size_t i = 0; i < entries.size(); i++) {
    SCOPED_TRACE(i);
    disk_cache::EntrySet found_entries = index.LookupEntries(entries[i].hash);
    ASSERT_EQ(1u, found_entries.cells.size());
    EXPECT_TRUE(found_entries.cells[0].IsValid());
  }
}

// Tests that GrowIndex is called.
TEST(DiskCacheIndexTable, GrowIndex) {
  TestCacheTables cache(1024);
  IndexTableInitData init_data;
  cache.GetInitData(&init_data);
  MockIndexBackend backend;

  IndexTable index(&backend);
  index.Init(&init_data);

  // Write some entries.
  for (int i = 0; i < 512; i++) {
    SCOPED_TRACE(i);
    uint32 hash = 0;
    disk_cache::Addr addr(disk_cache::BLOCK_ENTRIES, 1, 5, i + 1);
    EntryCell entry = index.CreateEntryCell(hash, addr);
    EXPECT_TRUE(entry.IsValid());
  }

  EXPECT_TRUE(backend.grow_called());
}

TEST(DiskCacheIndexTable, SaveIndex) {
  TestCacheTables cache(1024);
  IndexTableInitData init_data;
  cache.GetInitData(&init_data);
  MockIndexBackend backend;

  IndexTable index(&backend);
  index.Init(&init_data);

  uint32 hash = 0;
  disk_cache::Addr addr(disk_cache::BLOCK_ENTRIES, 1, 5, 6);
  EntryCell entry = index.CreateEntryCell(hash, addr);
  EXPECT_TRUE(entry.IsValid());

  index.OnBackupTimer();
  int expected = (1024 + 512) / 8 + sizeof(disk_cache::IndexHeaderV3);
  EXPECT_EQ(expected, backend.buffer_len());
}
