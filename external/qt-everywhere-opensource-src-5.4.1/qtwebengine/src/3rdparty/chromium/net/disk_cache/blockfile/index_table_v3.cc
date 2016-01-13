// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/index_table_v3.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bits.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"

using base::Time;
using base::TimeDelta;
using disk_cache::CellInfo;
using disk_cache::CellList;
using disk_cache::IndexCell;
using disk_cache::IndexIterator;

namespace {

// The following constants describe the bitfields of an IndexCell so they are
// implicitly synchronized with the descrption of IndexCell on file_format_v3.h.
const uint64 kCellLocationMask = (1 << 22) - 1;
const uint64 kCellIdMask = (1 << 18) - 1;
const uint64 kCellTimestampMask = (1 << 20) - 1;
const uint64 kCellReuseMask = (1 << 4) - 1;
const uint8 kCellStateMask = (1 << 3) - 1;
const uint8 kCellGroupMask = (1 << 3) - 1;
const uint8 kCellSumMask = (1 << 2) - 1;

const uint64 kCellSmallTableLocationMask = (1 << 16) - 1;
const uint64 kCellSmallTableIdMask = (1 << 24) - 1;

const int kCellIdOffset = 22;
const int kCellTimestampOffset = 40;
const int kCellReuseOffset = 60;
const int kCellGroupOffset = 3;
const int kCellSumOffset = 6;

const int kCellSmallTableIdOffset = 16;

// The number of bits that a hash has to be shifted to grab the part that
// defines the cell id.
const int kHashShift = 14;
const int kSmallTableHashShift = 8;

// Unfortunately we have to break the abstaction a little here: the file number
// where entries are stored is outside of the control of this code, and it is
// usually part of the stored address. However, for small tables we only store
// 16 bits of the address so the file number is never stored on a cell. We have
// to infere the file number from the type of entry (normal vs evicted), and
// the knowledge that given that the table will not keep more than 64k entries,
// a single file of each type is enough.
const int kEntriesFile = disk_cache::BLOCK_ENTRIES - 1;
const int kEvictedEntriesFile = disk_cache::BLOCK_EVICTED - 1;
const int kMaxLocation = 1 << 22;
const int kMinFileNumber = 1 << 16;

uint32 GetCellLocation(const IndexCell& cell) {
  return cell.first_part & kCellLocationMask;
}

uint32 GetCellSmallTableLocation(const IndexCell& cell) {
  return cell.first_part & kCellSmallTableLocationMask;
}

uint32 GetCellId(const IndexCell& cell) {
  return (cell.first_part >> kCellIdOffset) & kCellIdMask;
}

uint32 GetCellSmallTableId(const IndexCell& cell) {
  return (cell.first_part >> kCellSmallTableIdOffset) &
         kCellSmallTableIdMask;
}

int GetCellTimestamp(const IndexCell& cell) {
  return (cell.first_part >> kCellTimestampOffset) & kCellTimestampMask;
}

int GetCellReuse(const IndexCell& cell) {
  return (cell.first_part >> kCellReuseOffset) & kCellReuseMask;
}

int GetCellState(const IndexCell& cell) {
  return cell.last_part & kCellStateMask;
}

int GetCellGroup(const IndexCell& cell) {
  return (cell.last_part >> kCellGroupOffset) & kCellGroupMask;
}

int GetCellSum(const IndexCell& cell) {
  return (cell.last_part >> kCellSumOffset) & kCellSumMask;
}

void SetCellLocation(IndexCell* cell, uint32 address) {
  DCHECK_LE(address, static_cast<uint32>(kCellLocationMask));
  cell->first_part &= ~kCellLocationMask;
  cell->first_part |= address;
}

void SetCellSmallTableLocation(IndexCell* cell, uint32 address) {
  DCHECK_LE(address, static_cast<uint32>(kCellSmallTableLocationMask));
  cell->first_part &= ~kCellSmallTableLocationMask;
  cell->first_part |= address;
}

void SetCellId(IndexCell* cell, uint32 hash) {
  DCHECK_LE(hash, static_cast<uint32>(kCellIdMask));
  cell->first_part &= ~(kCellIdMask << kCellIdOffset);
  cell->first_part |= static_cast<int64>(hash) << kCellIdOffset;
}

void SetCellSmallTableId(IndexCell* cell, uint32 hash) {
  DCHECK_LE(hash, static_cast<uint32>(kCellSmallTableIdMask));
  cell->first_part &= ~(kCellSmallTableIdMask << kCellSmallTableIdOffset);
  cell->first_part |= static_cast<int64>(hash) << kCellSmallTableIdOffset;
}

void SetCellTimestamp(IndexCell* cell, int timestamp) {
  DCHECK_LT(timestamp, 1 << 20);
  DCHECK_GE(timestamp, 0);
  cell->first_part &= ~(kCellTimestampMask << kCellTimestampOffset);
  cell->first_part |= static_cast<int64>(timestamp) << kCellTimestampOffset;
}

void SetCellReuse(IndexCell* cell, int count) {
  DCHECK_LT(count, 16);
  DCHECK_GE(count, 0);
  cell->first_part &= ~(kCellReuseMask << kCellReuseOffset);
  cell->first_part |= static_cast<int64>(count) << kCellReuseOffset;
}

void SetCellState(IndexCell* cell, disk_cache::EntryState state) {
  cell->last_part &= ~kCellStateMask;
  cell->last_part |= state;
}

void SetCellGroup(IndexCell* cell, disk_cache::EntryGroup group) {
  cell->last_part &= ~(kCellGroupMask << kCellGroupOffset);
  cell->last_part |= group << kCellGroupOffset;
}

void SetCellSum(IndexCell* cell, int sum) {
  DCHECK_LT(sum, 4);
  DCHECK_GE(sum, 0);
  cell->last_part &= ~(kCellSumMask << kCellSumOffset);
  cell->last_part |= sum << kCellSumOffset;
}

// This is a very particular way to calculate the sum, so it will not match if
// compared a gainst a pure 2 bit, modulo 2 sum.
int CalculateCellSum(const IndexCell& cell) {
  uint32* words = bit_cast<uint32*>(&cell);
  uint8* bytes = bit_cast<uint8*>(&cell);
  uint32 result = words[0] + words[1];
  result += result >> 16;
  result += (result >> 8) + (bytes[8] & 0x3f);
  result += result >> 4;
  result += result >> 2;
  return result & 3;
}

bool SanityCheck(const IndexCell& cell) {
  if (GetCellSum(cell) != CalculateCellSum(cell))
    return false;

  if (GetCellState(cell) > disk_cache::ENTRY_USED ||
      GetCellGroup(cell) == disk_cache::ENTRY_RESERVED ||
      GetCellGroup(cell) > disk_cache::ENTRY_EVICTED) {
    return false;
  }

  return true;
}

int FileNumberFromLocation(int location) {
  return location / kMinFileNumber;
}

int StartBlockFromLocation(int location) {
  return location % kMinFileNumber;
}

bool IsValidAddress(disk_cache::Addr address) {
  if (!address.is_initialized() ||
      (address.file_type() != disk_cache::BLOCK_EVICTED &&
       address.file_type() != disk_cache::BLOCK_ENTRIES)) {
    return false;
  }

  return address.FileNumber() < FileNumberFromLocation(kMaxLocation);
}

bool IsNormalState(const IndexCell& cell) {
  disk_cache::EntryState state =
      static_cast<disk_cache::EntryState>(GetCellState(cell));
  DCHECK_NE(state, disk_cache::ENTRY_FREE);
  return state != disk_cache::ENTRY_DELETED &&
         state != disk_cache::ENTRY_FIXING;
}

inline int GetNextBucket(int min_bucket_num, int max_bucket_num,
                         disk_cache::IndexBucket* table,
                         disk_cache::IndexBucket** bucket) {
  if (!(*bucket)->next)
    return 0;

  int bucket_num = (*bucket)->next / disk_cache::kCellsPerBucket;
  if (bucket_num < min_bucket_num || bucket_num > max_bucket_num) {
    // The next bucket must fall within the extra table. Note that this is not
    // an uncommon path as growing the table may not cleanup the link from the
    // main table to the extra table, and that cleanup is performed here when
    // accessing that bucket for the first time. This behavior has to change if
    // the tables are ever shrinked.
    (*bucket)->next = 0;
    return 0;
  }
  *bucket = &table[bucket_num - min_bucket_num];
  return bucket_num;
}

// Updates the |iterator| with the current |cell|. This cell may cause all
// previous cells to be deleted (when a new target timestamp is found), the cell
// may be added to the list (if it matches the target timestamp), or may it be
// ignored.
void UpdateIterator(const disk_cache::EntryCell& cell,
                    int limit_time,
                    IndexIterator* iterator) {
  int time = cell.GetTimestamp();
  // Look for not interesting times.
  if (iterator->forward && time <= limit_time)
    return;
  if (!iterator->forward && time >= limit_time)
    return;

  if ((iterator->forward && time < iterator->timestamp) ||
      (!iterator->forward && time > iterator->timestamp)) {
    // This timestamp is better than the one we had.
    iterator->timestamp = time;
    iterator->cells.clear();
  }
  if (time == iterator->timestamp) {
    CellInfo cell_info = { cell.hash(), cell.GetAddress() };
    iterator->cells.push_back(cell_info);
  }
}

void InitIterator(IndexIterator* iterator) {
  iterator->cells.clear();
  iterator->timestamp = iterator->forward ? kint32max : 0;
}

}  // namespace

namespace disk_cache {

EntryCell::~EntryCell() {
}

bool EntryCell::IsValid() const {
  return GetCellLocation(cell_) != 0;
}

// This code has to map the cell address (up to 22 bits) to a general cache Addr
// (up to 24 bits of general addressing). It also set the implied file_number
// in the case of small tables. See also the comment by the definition of
// kEntriesFile.
Addr EntryCell::GetAddress() const {
  uint32 location = GetLocation();
  int file_number = FileNumberFromLocation(location);
  if (small_table_) {
    DCHECK_EQ(0, file_number);
    file_number = (GetGroup() == ENTRY_EVICTED) ? kEvictedEntriesFile :
                                                  kEntriesFile;
  }
  DCHECK_NE(0, file_number);
  FileType file_type = (GetGroup() == ENTRY_EVICTED) ? BLOCK_EVICTED :
                                                       BLOCK_ENTRIES;
  return Addr(file_type, 1, file_number, StartBlockFromLocation(location));
}

EntryState EntryCell::GetState() const {
  return static_cast<EntryState>(GetCellState(cell_));
}

EntryGroup EntryCell::GetGroup() const {
  return static_cast<EntryGroup>(GetCellGroup(cell_));
}

int EntryCell::GetReuse() const {
  return GetCellReuse(cell_);
}

int EntryCell::GetTimestamp() const {
  return GetCellTimestamp(cell_);
}

void EntryCell::SetState(EntryState state) {
  SetCellState(&cell_, state);
}

void EntryCell::SetGroup(EntryGroup group) {
  SetCellGroup(&cell_, group);
}

void EntryCell::SetReuse(int count) {
  SetCellReuse(&cell_, count);
}

void EntryCell::SetTimestamp(int timestamp) {
  SetCellTimestamp(&cell_, timestamp);
}

// Static.
EntryCell EntryCell::GetEntryCellForTest(int32 cell_num,
                                         uint32 hash,
                                         Addr address,
                                         IndexCell* cell,
                                         bool small_table) {
  if (cell) {
    EntryCell entry_cell(cell_num, hash, *cell, small_table);
    return entry_cell;
  }

  return EntryCell(cell_num, hash, address, small_table);
}

void EntryCell::SerializaForTest(IndexCell* destination) {
  FixSum();
  Serialize(destination);
}

EntryCell::EntryCell() : cell_num_(0), hash_(0), small_table_(false) {
  cell_.Clear();
}

EntryCell::EntryCell(int32 cell_num,
                     uint32 hash,
                     Addr address,
                     bool small_table)
    : cell_num_(cell_num),
      hash_(hash),
      small_table_(small_table) {
  DCHECK(IsValidAddress(address) || !address.value());

  cell_.Clear();
  SetCellState(&cell_, ENTRY_NEW);
  SetCellGroup(&cell_, ENTRY_NO_USE);
  if (small_table) {
    DCHECK(address.FileNumber() == kEntriesFile ||
           address.FileNumber() == kEvictedEntriesFile);
    SetCellSmallTableLocation(&cell_, address.start_block());
    SetCellSmallTableId(&cell_, hash >> kSmallTableHashShift);
  } else {
    uint32 location = address.FileNumber() << 16 | address.start_block();
    SetCellLocation(&cell_, location);
    SetCellId(&cell_, hash >> kHashShift);
  }
}

EntryCell::EntryCell(int32 cell_num,
                     uint32 hash,
                     const IndexCell& cell,
                     bool small_table)
    : cell_num_(cell_num),
      hash_(hash),
      cell_(cell),
      small_table_(small_table) {
}

void EntryCell::FixSum() {
  SetCellSum(&cell_, CalculateCellSum(cell_));
}

uint32 EntryCell::GetLocation() const {
  if (small_table_)
    return GetCellSmallTableLocation(cell_);

  return GetCellLocation(cell_);
}

uint32 EntryCell::RecomputeHash() {
  if (small_table_) {
    hash_ &= (1 << kSmallTableHashShift) - 1;
    hash_ |= GetCellSmallTableId(cell_) << kSmallTableHashShift;
    return hash_;
  }

  hash_ &= (1 << kHashShift) - 1;
  hash_ |= GetCellId(cell_) << kHashShift;
  return hash_;
}

void EntryCell::Serialize(IndexCell* destination) const {
  *destination = cell_;
}

EntrySet::EntrySet() : evicted_count(0), current(0) {
}

EntrySet::~EntrySet() {
}

IndexIterator::IndexIterator() {
}

IndexIterator::~IndexIterator() {
}

IndexTableInitData::IndexTableInitData() {
}

IndexTableInitData::~IndexTableInitData() {
}

// -----------------------------------------------------------------------

IndexTable::IndexTable(IndexTableBackend* backend)
    : backend_(backend),
      header_(NULL),
      main_table_(NULL),
      extra_table_(NULL),
      modified_(false),
      small_table_(false) {
}

IndexTable::~IndexTable() {
}

// For a general description of the index tables see:
// http://www.chromium.org/developers/design-documents/network-stack/disk-cache/disk-cache-v3#TOC-Index
//
// The index is split between two tables: the main_table_ and the extra_table_.
// The main table can grow only by doubling its number of cells, while the
// extra table can grow slowly, because it only contain cells that overflow
// from the main table. In order to locate a given cell, part of the hash is
// used directly as an index into the main table; once that bucket is located,
// all cells with that partial hash (i.e., belonging to that bucket) are
// inspected, and if present, the next bucket (located on the extra table) is
// then located. For more information on bucket chaining see:
// http://www.chromium.org/developers/design-documents/network-stack/disk-cache/disk-cache-v3#TOC-Buckets
//
// There are two cases when increasing the size:
//  - Doubling the size of the main table
//  - Adding more entries to the extra table
//
// For example, consider a 64k main table with 8k cells on the extra table (for
// a total of 72k cells). Init can be called to add another 8k cells at the end
// (grow to 80k cells). When the size of the extra table approaches 64k, Init
// can be called to double the main table (to 128k) and go back to a small extra
// table.
void IndexTable::Init(IndexTableInitData* params) {
  bool growing = header_ != NULL;
  scoped_ptr<IndexBucket[]> old_extra_table;
  header_ = &params->index_bitmap->header;

  if (params->main_table) {
    if (main_table_) {
      // This is doubling the size of main table.
      DCHECK_EQ(base::bits::Log2Floor(header_->table_len),
                base::bits::Log2Floor(backup_header_->table_len) + 1);
      int extra_size = (header()->max_bucket - mask_) * kCellsPerBucket;
      DCHECK_GE(extra_size, 0);

      // Doubling the size implies deleting the extra table and moving as many
      // cells as we can to the main table, so we first copy the old one. This
      // is not required when just growing the extra table because we don't
      // move any cell in that case.
      old_extra_table.reset(new IndexBucket[extra_size]);
      memcpy(old_extra_table.get(), extra_table_,
             extra_size * sizeof(IndexBucket));
      memset(params->extra_table, 0, extra_size * sizeof(IndexBucket));
    }
    main_table_ = params->main_table;
  }
  DCHECK(main_table_);
  extra_table_ = params->extra_table;

  // extra_bits_ is really measured against table-size specific values.
  const int kMaxAbsoluteExtraBits = 12;  // From smallest to largest table.
  const int kMaxExtraBitsSmallTable = 6;  // From smallest to 64K table.

  extra_bits_ = base::bits::Log2Floor(header_->table_len) -
                base::bits::Log2Floor(kBaseTableLen);
  DCHECK_GE(extra_bits_, 0);
  DCHECK_LT(extra_bits_, kMaxAbsoluteExtraBits);

  // Note that following the previous code the constants could be derived as
  // kMaxAbsoluteExtraBits = base::bits::Log2Floor(max table len) -
  //                         base::bits::Log2Floor(kBaseTableLen);
  //                       = 22 - base::bits::Log2Floor(1024) = 22 - 10;
  // kMaxExtraBitsSmallTable = base::bits::Log2Floor(max 16 bit table) - 10.

  mask_ = ((kBaseTableLen / kCellsPerBucket) << extra_bits_) - 1;
  small_table_ = extra_bits_ < kMaxExtraBitsSmallTable;
  if (!small_table_)
    extra_bits_ -= kMaxExtraBitsSmallTable;

  // table_len keeps the max number of cells stored by the index. We need a
  // bitmap with 1 bit per cell, and that bitmap has num_words 32-bit words.
  int num_words = (header_->table_len + 31) / 32;

  if (old_extra_table) {
    // All the cells from the extra table are moving to the new tables so before
    // creating the bitmaps, clear the part of the bitmap referring to the extra
    // table.
    int old_main_table_bit_words = ((mask_ >> 1) + 1) * kCellsPerBucket / 32;
    DCHECK_GT(num_words, old_main_table_bit_words);
    memset(params->index_bitmap->bitmap + old_main_table_bit_words, 0,
           (num_words - old_main_table_bit_words) * sizeof(int32));

    DCHECK(growing);
    int old_num_words = (backup_header_.get()->table_len + 31) / 32;
    DCHECK_GT(old_num_words, old_main_table_bit_words);
    memset(backup_bitmap_storage_.get() + old_main_table_bit_words, 0,
           (old_num_words - old_main_table_bit_words) * sizeof(int32));
  }
  bitmap_.reset(new Bitmap(params->index_bitmap->bitmap, header_->table_len,
                           num_words));

  if (growing) {
    int old_num_words = (backup_header_.get()->table_len + 31) / 32;
    DCHECK_GE(num_words, old_num_words);
    scoped_ptr<uint32[]> storage(new uint32[num_words]);
    memcpy(storage.get(), backup_bitmap_storage_.get(),
           old_num_words * sizeof(int32));
    memset(storage.get() + old_num_words, 0,
           (num_words - old_num_words) * sizeof(int32));

    backup_bitmap_storage_.swap(storage);
    backup_header_->table_len = header_->table_len;
  } else {
    backup_bitmap_storage_.reset(params->backup_bitmap.release());
    backup_header_.reset(params->backup_header.release());
  }

  num_words = (backup_header_->table_len + 31) / 32;
  backup_bitmap_.reset(new Bitmap(backup_bitmap_storage_.get(),
                                  backup_header_->table_len, num_words));
  if (old_extra_table)
    MoveCells(old_extra_table.get());

  if (small_table_)
    DCHECK(header_->flags & SMALL_CACHE);

  // All tables and backups are needed for operation.
  DCHECK(main_table_);
  DCHECK(extra_table_);
  DCHECK(bitmap_.get());
}

void IndexTable::Shutdown() {
  header_ = NULL;
  main_table_ = NULL;
  extra_table_ = NULL;
  bitmap_.reset();
  backup_bitmap_.reset();
  backup_header_.reset();
  backup_bitmap_storage_.reset();
  modified_ = false;
}

// The general method for locating cells is to:
// 1. Get the first bucket. This usually means directly indexing the table (as
//     this method does), or iterating through all possible buckets.
// 2. Iterate through all the cells in that first bucket.
// 3. If there is a linked bucket, locate it directly in the extra table.
// 4. Go back to 2, as needed.
//
// One consequence of this pattern is that we never start looking at buckets in
// the extra table, unless we are following a link from the main table.
EntrySet IndexTable::LookupEntries(uint32 hash) {
  EntrySet entries;
  int bucket_num = static_cast<int>(hash & mask_);
  IndexBucket* bucket = &main_table_[bucket_num];
  do {
    for (int i = 0; i < kCellsPerBucket; i++) {
      IndexCell* current_cell = &bucket->cells[i];
      if (!GetLocation(*current_cell))
        continue;
      if (!SanityCheck(*current_cell)) {
        NOTREACHED();
        int cell_num = bucket_num * kCellsPerBucket + i;
        current_cell->Clear();
        bitmap_->Set(cell_num, false);
        backup_bitmap_->Set(cell_num, false);
        modified_ = true;
        continue;
      }
      int cell_num = bucket_num * kCellsPerBucket + i;
      if (MisplacedHash(*current_cell, hash)) {
        HandleMisplacedCell(current_cell, cell_num, hash & mask_);
      } else if (IsHashMatch(*current_cell, hash)) {
        EntryCell entry_cell(cell_num, hash, *current_cell, small_table_);
        CheckState(entry_cell);
        if (entry_cell.GetState() != ENTRY_DELETED) {
          entries.cells.push_back(entry_cell);
          if (entry_cell.GetGroup() == ENTRY_EVICTED)
            entries.evicted_count++;
        }
      }
    }
    bucket_num = GetNextBucket(mask_ + 1, header()->max_bucket, extra_table_,
                               &bucket);
  } while (bucket_num);
  return entries;
}

EntryCell IndexTable::CreateEntryCell(uint32 hash, Addr address) {
  DCHECK(IsValidAddress(address));
  DCHECK(address.FileNumber() || address.start_block());

  int bucket_num = static_cast<int>(hash & mask_);
  int cell_num = 0;
  IndexBucket* bucket = &main_table_[bucket_num];
  IndexCell* current_cell = NULL;
  bool found = false;
  do {
    for (int i = 0; i < kCellsPerBucket && !found; i++) {
      current_cell = &bucket->cells[i];
      if (!GetLocation(*current_cell)) {
        cell_num = bucket_num * kCellsPerBucket + i;
        found = true;
      }
    }
    if (found)
      break;
    bucket_num = GetNextBucket(mask_ + 1, header()->max_bucket, extra_table_,
                               &bucket);
  } while (bucket_num);

  if (!found) {
    bucket_num = NewExtraBucket();
    if (bucket_num) {
      cell_num = bucket_num * kCellsPerBucket;
      bucket->next = cell_num;
      bucket = &extra_table_[bucket_num - (mask_ + 1)];
      bucket->hash = hash & mask_;
      found = true;
    } else {
      // address 0 is a reserved value, and the caller interprets it as invalid.
      address.set_value(0);
    }
  }

  EntryCell entry_cell(cell_num, hash, address, small_table_);
  if (address.file_type() == BLOCK_EVICTED)
    entry_cell.SetGroup(ENTRY_EVICTED);
  else
    entry_cell.SetGroup(ENTRY_NO_USE);
  Save(&entry_cell);

  if (found) {
    bitmap_->Set(cell_num, true);
    backup_bitmap_->Set(cell_num, true);
    header()->used_cells++;
    modified_ = true;
  }

  return entry_cell;
}

EntryCell IndexTable::FindEntryCell(uint32 hash, Addr address) {
  return FindEntryCellImpl(hash, address, false);
}

int IndexTable::CalculateTimestamp(Time time) {
  TimeDelta delta = time - Time::FromInternalValue(header_->base_time);
  return std::max(delta.InMinutes(), 0);
}

base::Time IndexTable::TimeFromTimestamp(int timestamp) {
  return Time::FromInternalValue(header_->base_time) +
         TimeDelta::FromMinutes(timestamp);
}

void IndexTable::SetSate(uint32 hash, Addr address, EntryState state) {
  EntryCell cell = FindEntryCellImpl(hash, address, state == ENTRY_FREE);
  if (!cell.IsValid()) {
    NOTREACHED();
    return;
  }

  EntryState old_state = cell.GetState();
  switch (state) {
    case ENTRY_FREE:
      DCHECK_EQ(old_state, ENTRY_DELETED);
      break;
    case ENTRY_NEW:
      DCHECK_EQ(old_state, ENTRY_FREE);
      break;
    case ENTRY_OPEN:
      DCHECK_EQ(old_state, ENTRY_USED);
      break;
    case ENTRY_MODIFIED:
      DCHECK_EQ(old_state, ENTRY_OPEN);
      break;
    case ENTRY_DELETED:
      DCHECK(old_state == ENTRY_NEW || old_state == ENTRY_OPEN ||
             old_state == ENTRY_MODIFIED);
      break;
    case ENTRY_USED:
      DCHECK(old_state == ENTRY_NEW || old_state == ENTRY_OPEN ||
             old_state == ENTRY_MODIFIED);
      break;
    case ENTRY_FIXING:
      break;
  };

  modified_ = true;
  if (state == ENTRY_DELETED) {
    bitmap_->Set(cell.cell_num(), false);
    backup_bitmap_->Set(cell.cell_num(), false);
  } else if (state == ENTRY_FREE) {
    cell.Clear();
    Write(cell);
    header()->used_cells--;
    return;
  }
  cell.SetState(state);

  Save(&cell);
}

void IndexTable::UpdateTime(uint32 hash, Addr address, base::Time current) {
  EntryCell cell = FindEntryCell(hash, address);
  if (!cell.IsValid())
    return;

  int minutes = CalculateTimestamp(current);

  // Keep about 3 months of headroom.
  const int kMaxTimestamp = (1 << 20) - 60 * 24 * 90;
  if (minutes > kMaxTimestamp) {
    // TODO(rvargas):
    // Update header->old_time and trigger a timer
    // Rebaseline timestamps and don't update sums
    // Start a timer (about 2 backups)
    // fix all ckecksums and trigger another timer
    // update header->old_time because rebaseline is done.
    minutes = std::min(minutes, (1 << 20) - 1);
  }

  cell.SetTimestamp(minutes);
  Save(&cell);
}

void IndexTable::Save(EntryCell* cell) {
  cell->FixSum();
  Write(*cell);
}

void IndexTable::GetOldest(IndexIterator* no_use,
                           IndexIterator* low_use,
                           IndexIterator* high_use) {
  no_use->forward = true;
  low_use->forward = true;
  high_use->forward = true;
  InitIterator(no_use);
  InitIterator(low_use);
  InitIterator(high_use);

  WalkTables(-1, no_use, low_use, high_use);
}

bool IndexTable::GetNextCells(IndexIterator* iterator) {
  int current_time = iterator->timestamp;
  InitIterator(iterator);

  WalkTables(current_time, iterator, iterator, iterator);
  return !iterator->cells.empty();
}

void IndexTable::OnBackupTimer() {
  if (!modified_)
    return;

  int num_words = (header_->table_len + 31) / 32;
  int num_bytes = num_words * 4 + static_cast<int>(sizeof(*header_));
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(num_bytes));
  memcpy(buffer->data(), header_, sizeof(*header_));
  memcpy(buffer->data() + sizeof(*header_), backup_bitmap_storage_.get(),
         num_words * 4);
  backend_->SaveIndex(buffer, num_bytes);
  modified_ = false;
}

// -----------------------------------------------------------------------

EntryCell IndexTable::FindEntryCellImpl(uint32 hash, Addr address,
                                        bool allow_deleted) {
  int bucket_num = static_cast<int>(hash & mask_);
  IndexBucket* bucket = &main_table_[bucket_num];
  do {
    for (int i = 0; i < kCellsPerBucket; i++) {
      IndexCell* current_cell = &bucket->cells[i];
      if (!GetLocation(*current_cell))
        continue;
      DCHECK(SanityCheck(*current_cell));
      if (IsHashMatch(*current_cell, hash)) {
        // We have a match.
        int cell_num = bucket_num * kCellsPerBucket + i;
        EntryCell entry_cell(cell_num, hash, *current_cell, small_table_);
        if (entry_cell.GetAddress() != address)
          continue;

        if (!allow_deleted && entry_cell.GetState() == ENTRY_DELETED)
          continue;

        return entry_cell;
      }
    }
    bucket_num = GetNextBucket(mask_ + 1, header()->max_bucket, extra_table_,
                               &bucket);
  } while (bucket_num);
  return EntryCell();
}

void IndexTable::CheckState(const EntryCell& cell) {
  int current_state = cell.GetState();
  if (current_state != ENTRY_FIXING) {
    bool present = ((current_state & 3) != 0);  // Look at the last two bits.
    if (present != bitmap_->Get(cell.cell_num()) ||
        present != backup_bitmap_->Get(cell.cell_num())) {
      // There's a mismatch.
      if (current_state == ENTRY_DELETED) {
        // We were in the process of deleting this entry. Finish now.
        backend_->DeleteCell(cell);
      } else {
        current_state = ENTRY_FIXING;
        EntryCell bad_cell(cell);
        bad_cell.SetState(ENTRY_FIXING);
        Save(&bad_cell);
      }
    }
  }

  if (current_state == ENTRY_FIXING)
    backend_->FixCell(cell);
}

void IndexTable::Write(const EntryCell& cell) {
  IndexBucket* bucket = NULL;
  int bucket_num = cell.cell_num() / kCellsPerBucket;
  if (bucket_num < static_cast<int32>(mask_ + 1)) {
    bucket = &main_table_[bucket_num];
  } else {
    DCHECK_LE(bucket_num, header()->max_bucket);
    bucket = &extra_table_[bucket_num - (mask_ + 1)];
  }

  int cell_number = cell.cell_num() % kCellsPerBucket;
  if (GetLocation(bucket->cells[cell_number]) && cell.GetLocation()) {
    DCHECK_EQ(cell.GetLocation(),
              GetLocation(bucket->cells[cell_number]));
  }
  cell.Serialize(&bucket->cells[cell_number]);
}

int IndexTable::NewExtraBucket() {
  int safe_window = (header()->table_len < kNumExtraBlocks * 2) ?
                    kNumExtraBlocks / 4 : kNumExtraBlocks;
  if (header()->table_len - header()->max_bucket * kCellsPerBucket <
      safe_window) {
    backend_->GrowIndex();
  }

  if (header()->max_bucket * kCellsPerBucket ==
      header()->table_len - kCellsPerBucket) {
    return 0;
  }

  header()->max_bucket++;
  return header()->max_bucket;
}

void IndexTable::WalkTables(int limit_time,
                            IndexIterator* no_use,
                            IndexIterator* low_use,
                            IndexIterator* high_use) {
  header_->num_no_use_entries = 0;
  header_->num_low_use_entries = 0;
  header_->num_high_use_entries = 0;
  header_->num_evicted_entries = 0;

  for (int i = 0; i < static_cast<int32>(mask_ + 1); i++) {
    int bucket_num = i;
    IndexBucket* bucket = &main_table_[i];
    do {
      UpdateFromBucket(bucket, i, limit_time, no_use, low_use, high_use);

      bucket_num = GetNextBucket(mask_ + 1, header()->max_bucket, extra_table_,
                                 &bucket);
    } while (bucket_num);
  }
  header_->num_entries = header_->num_no_use_entries +
                         header_->num_low_use_entries +
                         header_->num_high_use_entries +
                         header_->num_evicted_entries;
  modified_ = true;
}

void IndexTable::UpdateFromBucket(IndexBucket* bucket, int bucket_hash,
                                  int limit_time,
                                  IndexIterator* no_use,
                                  IndexIterator* low_use,
                                  IndexIterator* high_use) {
  for (int i = 0; i < kCellsPerBucket; i++) {
    IndexCell& current_cell = bucket->cells[i];
    if (!GetLocation(current_cell))
      continue;
    DCHECK(SanityCheck(current_cell));
    if (!IsNormalState(current_cell))
      continue;

    EntryCell entry_cell(0, GetFullHash(current_cell, bucket_hash),
                         current_cell, small_table_);
    switch (GetCellGroup(current_cell)) {
      case ENTRY_NO_USE:
        UpdateIterator(entry_cell, limit_time, no_use);
        header_->num_no_use_entries++;
        break;
      case ENTRY_LOW_USE:
        UpdateIterator(entry_cell, limit_time, low_use);
        header_->num_low_use_entries++;
        break;
      case ENTRY_HIGH_USE:
        UpdateIterator(entry_cell, limit_time, high_use);
        header_->num_high_use_entries++;
        break;
      case ENTRY_EVICTED:
        header_->num_evicted_entries++;
        break;
      default:
        NOTREACHED();
    }
  }
}

// This code is only called from Init() so the internal state of this object is
// in flux (this method is performing the last steps of re-initialization). As
// such, random methods are not supposed to work at this point, so whatever this
// method calls should be relatively well controlled and it may require some
// degree of "stable state faking".
void IndexTable::MoveCells(IndexBucket* old_extra_table) {
  int max_hash = (mask_ + 1) / 2;
  int max_bucket = header()->max_bucket;
  header()->max_bucket = mask_;
  int used_cells = header()->used_cells;

  // Consider a large cache: a cell stores the upper 18 bits of the hash
  // (h >> 14). If the table is say 8 times the original size (growing from 4x),
  // the bit that we are interested in would be the 3rd bit of the stored value,
  // in other words 'multiplier' >> 1.
  uint32 new_bit = (1 << extra_bits_) >> 1;

  scoped_ptr<IndexBucket[]> old_main_table;
  IndexBucket* source_table = main_table_;
  bool upgrade_format = !extra_bits_;
  if (upgrade_format) {
    // This method should deal with migrating a small table to a big one. Given
    // that the first thing to do is read the old table, set small_table_ for
    // the size of the old table. Now, when moving a cell, the result cannot be
    // placed in the old table or we will end up reading it again and attempting
    // to move it, so we have to copy the whole table at once.
    DCHECK(!small_table_);
    small_table_ = true;
    old_main_table.reset(new IndexBucket[max_hash]);
    memcpy(old_main_table.get(), main_table_, max_hash * sizeof(IndexBucket));
    memset(main_table_, 0, max_hash * sizeof(IndexBucket));
    source_table = old_main_table.get();
  }

  for (int i = 0; i < max_hash; i++) {
    int bucket_num = i;
    IndexBucket* bucket = &source_table[i];
    do {
      for (int j = 0; j < kCellsPerBucket; j++) {
        IndexCell& current_cell = bucket->cells[j];
        if (!GetLocation(current_cell))
          continue;
        DCHECK(SanityCheck(current_cell));
        if (bucket_num == i) {
          if (upgrade_format || (GetHashValue(current_cell) & new_bit)) {
            // Move this cell to the upper half of the table.
            MoveSingleCell(&current_cell, bucket_num * kCellsPerBucket + j, i,
                           true);
          }
        } else {
          // All cells on extra buckets have to move.
          MoveSingleCell(&current_cell, bucket_num * kCellsPerBucket + j, i,
                         true);
        }
      }

      // There is no need to clear the old bucket->next value because if falls
      // within the main table so it will be fixed when attempting to follow
      // the link.
      bucket_num = GetNextBucket(max_hash, max_bucket, old_extra_table,
                                 &bucket);
    } while (bucket_num);
  }

  DCHECK_EQ(header()->used_cells, used_cells);

  if (upgrade_format) {
    small_table_ = false;
    header()->flags &= ~SMALL_CACHE;
  }
}

void IndexTable::MoveSingleCell(IndexCell* current_cell, int cell_num,
                                int main_table_index, bool growing) {
  uint32 hash = GetFullHash(*current_cell, main_table_index);
  EntryCell old_cell(cell_num, hash, *current_cell, small_table_);

  // This method may be called when moving entries from a small table to a
  // normal table. In that case, the caller (MoveCells) has to read the old
  // table, so it needs small_table_ set to true, but this method needs to
  // write to the new table so small_table_ has to be set to false, and the
  // value restored to true before returning.
  bool upgrade_format = !extra_bits_ && growing;
  if (upgrade_format)
    small_table_ = false;
  EntryCell new_cell = CreateEntryCell(hash, old_cell.GetAddress());

  if (!new_cell.IsValid()) {
    // We'll deal with this entry later.
    if (upgrade_format)
      small_table_ = true;
    return;
  }

  new_cell.SetState(old_cell.GetState());
  new_cell.SetGroup(old_cell.GetGroup());
  new_cell.SetReuse(old_cell.GetReuse());
  new_cell.SetTimestamp(old_cell.GetTimestamp());
  Save(&new_cell);
  modified_ = true;
  if (upgrade_format)
    small_table_ = true;

  if (old_cell.GetState() == ENTRY_DELETED) {
    bitmap_->Set(new_cell.cell_num(), false);
    backup_bitmap_->Set(new_cell.cell_num(), false);
  }

  if (!growing || cell_num / kCellsPerBucket == main_table_index) {
    // Only delete entries that live on the main table.
    if (!upgrade_format) {
      old_cell.Clear();
      Write(old_cell);
    }

    if (cell_num != new_cell.cell_num()) {
      bitmap_->Set(old_cell.cell_num(), false);
      backup_bitmap_->Set(old_cell.cell_num(), false);
    }
  }
  header()->used_cells--;
}

void IndexTable::HandleMisplacedCell(IndexCell* current_cell, int cell_num,
                                     int main_table_index) {
  NOTREACHED();  // No unit tests yet.

  // The cell may be misplaced, or a duplicate cell exists with this data.
  uint32 hash = GetFullHash(*current_cell, main_table_index);
  MoveSingleCell(current_cell, cell_num, main_table_index, false);

  // Now look for a duplicate cell.
  CheckBucketList(hash & mask_);
}

void IndexTable::CheckBucketList(int bucket_num) {
  typedef std::pair<int, EntryGroup> AddressAndGroup;
  std::set<AddressAndGroup> entries;
  IndexBucket* bucket = &main_table_[bucket_num];
  int bucket_hash = bucket_num;
  do {
    for (int i = 0; i < kCellsPerBucket; i++) {
      IndexCell* current_cell = &bucket->cells[i];
      if (!GetLocation(*current_cell))
        continue;
      if (!SanityCheck(*current_cell)) {
        NOTREACHED();
        current_cell->Clear();
        continue;
      }
      int cell_num = bucket_num * kCellsPerBucket + i;
      EntryCell cell(cell_num, GetFullHash(*current_cell, bucket_hash),
                     *current_cell, small_table_);
      if (!entries.insert(std::make_pair(cell.GetAddress().value(),
                                         cell.GetGroup())).second) {
        current_cell->Clear();
        continue;
      }
      CheckState(cell);
    }

    bucket_num = GetNextBucket(mask_ + 1, header()->max_bucket, extra_table_,
                              &bucket);
  } while (bucket_num);
}

uint32 IndexTable::GetLocation(const IndexCell& cell) {
  if (small_table_)
    return GetCellSmallTableLocation(cell);

  return GetCellLocation(cell);
}

uint32 IndexTable::GetHashValue(const IndexCell& cell) {
  if (small_table_)
    return GetCellSmallTableId(cell);

  return GetCellId(cell);
}

uint32 IndexTable::GetFullHash(const IndexCell& cell, uint32 lower_part) {
  // It is OK for the high order bits of lower_part to overlap with the stored
  // part of the hash.
  if (small_table_)
    return (GetCellSmallTableId(cell) << kSmallTableHashShift) | lower_part;

  return (GetCellId(cell) << kHashShift) | lower_part;
}

// All the bits stored in the cell should match the provided hash.
bool IndexTable::IsHashMatch(const IndexCell& cell, uint32 hash) {
  hash = small_table_ ? hash >> kSmallTableHashShift : hash >> kHashShift;
  return GetHashValue(cell) == hash;
}

bool IndexTable::MisplacedHash(const IndexCell& cell, uint32 hash) {
  if (!extra_bits_)
    return false;

  uint32 mask = (1 << extra_bits_) - 1;
  hash = small_table_ ? hash >> kSmallTableHashShift : hash >> kHashShift;
  return (GetHashValue(cell) & mask) != (hash & mask);
}

}  // namespace disk_cache
