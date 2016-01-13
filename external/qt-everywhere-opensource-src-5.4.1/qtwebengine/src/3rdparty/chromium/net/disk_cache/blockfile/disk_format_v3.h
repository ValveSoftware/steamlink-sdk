// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The cache is stored on disk as a collection of block-files, plus an index
// plus a collection of external files.
//
// Any data blob bigger than kMaxBlockSize (disk_cache/addr.h) will be stored in
// a separate file named f_xxx where x is a hexadecimal number. Shorter data
// will be stored as a series of blocks on a block-file. In any case, CacheAddr
// represents the address of the data inside the cache.
//
// The index is actually a collection of four files that store a hash table with
// allocation bitmaps and backup data. Hash collisions are handled directly by
// the table, which from some point of view behaves like a 4-way associative
// cache with overflow buckets (so not really open addressing).
//
// Basically the hash table is a collection of buckets. The first part of the
// table has a fixed number of buckets and it is directly addressed by the hash,
// while the second part of the table (stored on a second file) has a variable
// number of buckets. Each bucket stores up to four cells (each cell represents
// a possibl entry). The index bitmap tracks the state of individual cells.
//
// The last element of the cache is the block-file. A block file is a file
// designed to store blocks of data of a given size. For more details see
// disk_cache/disk_format_base.h
//
// A new cache is initialized with a set of block files (named data_0 through
// data_6), each one dedicated to store blocks of a given size or function. The
// number at the end of the file name is the block file number (in decimal).
//
// There are three "special" types of blocks: normal entries, evicted entries
// and control data for external files.
//
// The files that store internal information for the cache (blocks and index)
// are memory mapped. They have a location that is signaled every time the
// internal structures are modified, so it is possible to detect (most of the
// time) when the process dies in the middle of an update. There are dedicated
// backup files for cache bitmaps, used to detect entries out of date.
//
// Although cache files are to be consumed on the same machine that creates
// them, if files are to be moved accross machines, little endian storage is
// assumed.

#ifndef NET_DISK_CACHE_BLOCKFILE_DISK_FORMAT_V3_H_
#define NET_DISK_CACHE_BLOCKFILE_DISK_FORMAT_V3_H_

#include "base/basictypes.h"
#include "net/disk_cache/blockfile/disk_format_base.h"

namespace disk_cache {

const int kBaseTableLen = 0x400;
const uint32 kIndexMagicV3 = 0xC103CAC3;
const uint32 kVersion3 = 0x30000;  // Version 3.0.

// Flags for a given cache.
enum CacheFlags {
  SMALL_CACHE = 1 << 0,       // See IndexCell.
  CACHE_EVICTION_2 = 1 << 1,  // Keep multiple lists for eviction.
  CACHE_EVICTED = 1 << 2      // Already evicted at least one entry.
};

// Header for the master index file.
struct IndexHeaderV3 {
  uint32      magic;
  uint32      version;
  int32       num_entries;   // Number of entries currently stored.
  int32       num_bytes;     // Total size of the stored data.
  int32       last_file;     // Last external file created.
  int32       reserved1;
  CacheAddr   stats;         // Storage for usage data.
  int32       table_len;     // Actual size of the table.
  int32       crash;         // Signals a previous crash.
  int32       experiment;    // Id of an ongoing test.
  int32       max_bytes;     // Total maximum size of the stored data.
  uint32      flags;
  int32       used_cells;
  int32       max_bucket;
  uint64      create_time;   // Creation time for this set of files.
  uint64      base_time;     // Current base for timestamps.
  uint64      old_time;      // Previous time used for timestamps.
  int32       max_block_file;
  int32       num_no_use_entries;
  int32       num_low_use_entries;
  int32       num_high_use_entries;
  int32       reserved;
  int32       num_evicted_entries;
  int32       pad[6];
};

const int kBaseBitmapBytes = 3968;
// The IndexBitmap is directly saved to a file named index. The file grows in
// page increments (4096 bytes), but all bits don't have to be in use at any
// given time. The required file size can be computed from header.table_len.
struct IndexBitmap {
  IndexHeaderV3   header;
  uint32          bitmap[kBaseBitmapBytes / 4];  // First page of the bitmap.
};
COMPILE_ASSERT(sizeof(IndexBitmap) == 4096, bad_IndexHeader);

// Possible states for a given entry.
enum EntryState {
  ENTRY_FREE = 0,   // Available slot.
  ENTRY_NEW,        // The entry is being created.
  ENTRY_OPEN,       // The entry is being accessed.
  ENTRY_MODIFIED,   // The entry is being modified.
  ENTRY_DELETED,    // The entry is being deleted.
  ENTRY_FIXING,     // Inconsistent state. The entry is being verified.
  ENTRY_USED        // The slot is in use (entry is present).
};
COMPILE_ASSERT(ENTRY_USED <= 7, state_uses_3_bits);

enum EntryGroup {
  ENTRY_NO_USE = 0,   // The entry has not been reused.
  ENTRY_LOW_USE,      // The entry has low reuse.
  ENTRY_HIGH_USE,     // The entry has high reuse.
  ENTRY_RESERVED,     // Reserved for future use.
  ENTRY_EVICTED       // The entry was deleted.
};
COMPILE_ASSERT(ENTRY_USED <= 7, group_uses_3_bits);

#pragma pack(push, 1)
struct IndexCell {
  void Clear() { memset(this, 0, sizeof(*this)); }

  // A cell is a 9 byte bit-field that stores 7 values:
  //   location : 22 bits
  //   id : 18 bits
  //   timestamp : 20 bits
  //   reuse : 4 bits
  //   state : 3 bits
  //   group : 3 bits
  //   sum : 2 bits
  // The id is derived from the full hash of the entry.
  //
  // The actual layout is as follows:
  //
  // first_part (low order 32 bits):
  //   0000 0000 0011 1111 1111 1111 1111 1111 : location
  //   1111 1111 1100 0000 0000 0000 0000 0000 : id
  //
  // first_part (high order 32 bits):
  //   0000 0000 0000 0000 0000 0000 1111 1111 : id
  //   0000 1111 1111 1111 1111 1111 0000 0000 : timestamp
  //   1111 0000 0000 0000 0000 0000 0000 0000 : reuse
  //
  // last_part:
  //   0000 0111 : state
  //   0011 1000 : group
  //   1100 0000 : sum
  //
  // The small-cache version of the format moves some bits from the location to
  // the id fileds, like so:
  //   location : 16 bits
  //   id : 24 bits
  //
  // first_part (low order 32 bits):
  //   0000 0000 0000 0000 1111 1111 1111 1111 : location
  //   1111 1111 1111 1111 0000 0000 0000 0000 : id
  //
  // The actual bit distribution between location and id is determined by the
  // table size (IndexHeaderV3.table_len). Tables smaller than 65536 entries
  // use the small-cache version; after that size, caches should have the
  // SMALL_CACHE flag cleared.
  //
  // To locate a given entry after recovering the location from the cell, the
  // file type and file number are appended (see disk_cache/addr.h). For a large
  // table only the file type is implied; for a small table, the file number
  // is also implied, and it should be the first file for that type of entry,
  // as determined by the EntryGroup (two files in total, one for active entries
  // and another one for evicted entries).
  //
  // For example, a small table may store something like 0x1234 as the location
  // field. That means it stores the entry number 0x1234. If that record belongs
  // to a deleted entry, the regular cache address may look something like
  //     BLOCK_EVICTED + 1 block + file number 6 + entry number 0x1234
  //     so Addr = 0xf0061234
  //
  // If that same Addr is stored on a large table, the location field would be
  // 0x61234

  uint64      first_part;
  uint8       last_part;
};
COMPILE_ASSERT(sizeof(IndexCell) == 9, bad_IndexCell);

const int kCellsPerBucket = 4;
struct IndexBucket {
  IndexCell   cells[kCellsPerBucket];
  int32       next;
  uint32      hash;  // The high order byte is reserved (should be zero).
};
COMPILE_ASSERT(sizeof(IndexBucket) == 44, bad_IndexBucket);
const int kBytesPerCell = 44 / kCellsPerBucket;

// The main cache index. Backed by a file named index_tb1.
// The extra table (index_tb2) has a similar format, but different size.
struct Index {
  // Default size. Actual size controlled by header.table_len.
  IndexBucket table[kBaseTableLen / kCellsPerBucket];
};
#pragma pack(pop)

// Flags that can be applied to an entry.
enum EntryFlags {
  PARENT_ENTRY = 1,         // This entry has children (sparse) entries.
  CHILD_ENTRY = 1 << 1      // Child entry that stores sparse data.
};

struct EntryRecord {
  uint32      hash;
  uint32      pad1;
  uint8       reuse_count;
  uint8       refetch_count;
  int8        state;              // Current EntryState.
  uint8       flags;              // Any combination of EntryFlags.
  int32       key_len;
  int32       data_size[4];       // We can store up to 4 data streams for each
  CacheAddr   data_addr[4];       // entry.
  uint32      data_hash[4];
  uint64      creation_time;
  uint64      last_modified_time;
  uint64      last_access_time;
  int32       pad[3];
  uint32      self_hash;
};
COMPILE_ASSERT(sizeof(EntryRecord) == 104, bad_EntryRecord);

struct ShortEntryRecord {
  uint32      hash;
  uint32      pad1;
  uint8       reuse_count;
  uint8       refetch_count;
  int8        state;              // Current EntryState.
  uint8       flags;
  int32       key_len;
  uint64      last_access_time;
  uint32      long_hash[5];
  uint32      self_hash;
};
COMPILE_ASSERT(sizeof(ShortEntryRecord) == 48, bad_ShortEntryRecord);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_DISK_FORMAT_V3_H_
