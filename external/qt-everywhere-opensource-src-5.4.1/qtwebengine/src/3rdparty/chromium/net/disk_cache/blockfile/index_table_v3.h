// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_INDEX_TABLE_V3_H_
#define NET_DISK_CACHE_BLOCKFILE_INDEX_TABLE_V3_H_

// The IndexTable class is in charge of handling all the details about the main
// index table of the cache. It provides methods to locate entries in the cache,
// create new entries and modify existing entries. It hides the fact that the
// table is backed up across multiple physical files, and that the files can
// grow and be remapped while the cache is in use. However, note that this class
// doesn't do any direct management of the backing files, and it operates only
// with the tables in memory.
//
// When the current index needs to grow, the backend is notified so that files
// are extended and remapped as needed. After that, the IndexTable should be
// re-initialized with the new structures. Note that the IndexTable instance is
// still functional while the backend performs file IO.

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/disk_cache/blockfile/addr.h"
#include "net/disk_cache/blockfile/bitmap.h"
#include "net/disk_cache/blockfile/disk_format_v3.h"

namespace net {
class IOBuffer;
}

namespace disk_cache {

class BackendImplV3;
struct InitResult;

// An EntryCell represents a single entity stored by the index table. Users are
// expected to handle and store EntryCells on their own to track operations that
// they are performing with a given entity, as opposed to deal with pointers to
// individual positions on the table, given that the whole table can be moved to
// another place, and that would invalidate any pointers to individual cells in
// the table.
// However, note that it is also possible for an entity to be moved from one
// position to another, so an EntryCell may be invalid by the time a long
// operation completes. In that case, the caller should consult the table again
// using FindEntryCell().
class NET_EXPORT_PRIVATE EntryCell {
 public:
  ~EntryCell();

  bool IsValid() const;

  int32 cell_num() const { return cell_num_; }
  uint32 hash() const { return hash_; }

  Addr GetAddress() const;
  EntryState GetState() const;
  EntryGroup GetGroup() const;
  int GetReuse() const;
  int GetTimestamp() const;

  void SetState(EntryState state);
  void SetGroup(EntryGroup group);
  void SetReuse(int count);
  void SetTimestamp(int timestamp);

  static EntryCell GetEntryCellForTest(int32 cell_num,
                                       uint32 hash,
                                       Addr address,
                                       IndexCell* cell,
                                       bool small_table);
  void SerializaForTest(IndexCell* destination);

 private:
  friend class IndexTable;
  friend class CacheDumperHelper;

  EntryCell();
  EntryCell(int32 cell_num, uint32 hash, Addr address, bool small_table);
  EntryCell(int32 cell_num,
            uint32 hash,
            const IndexCell& cell,
            bool small_table);

  void Clear() { cell_.Clear(); }
  void FixSum();

  // Returns the raw value stored on the index table.
  uint32 GetLocation() const;

  // Recalculates hash_ assuming that only the low order bits are valid and the
  // rest come from cell_.
  uint32 RecomputeHash();

  void Serialize(IndexCell* destination) const;

  int32 cell_num_;
  uint32 hash_;
  IndexCell cell_;
  bool small_table_;
};

// Keeps a collection of EntryCells in order to be processed.
struct NET_EXPORT_PRIVATE EntrySet {
  EntrySet();
  ~EntrySet();

  int evicted_count;  // The numebr of evicted entries in this set.
  size_t current;     // The number of the cell that is being processed.
  std::vector<EntryCell> cells;
};

// A given entity referenced by the index table is uniquely identified by the
// combination of hash and address.
struct CellInfo { uint32 hash; Addr address; };
typedef std::vector<CellInfo> CellList;

// An index iterator is used to get a group of cells that share the same
// timestamp. When this structure is passed to GetNextCells(), the caller sets
// the initial timestamp and direction; whet it is used with GetOldest, the
// initial values are ignored.
struct NET_EXPORT_PRIVATE IndexIterator {
  IndexIterator();
  ~IndexIterator();

  CellList cells;
  int timestamp;  // The current low resolution timestamp for |cells|.
  bool forward;   // The direction of the iteration, in time.
};

// Methods that the backend has to implement to support the table. Note that the
// backend is expected to own all IndexTable instances, so it is expected to
// outlive the table.
class NET_EXPORT_PRIVATE IndexTableBackend {
 public:
  virtual ~IndexTableBackend() {}

  // The index has to grow.
  virtual void GrowIndex() = 0;

  // Save the index to the backup file.
  virtual void SaveIndex(net::IOBuffer* buffer, int buffer_len) = 0;

  // Deletes or fixes an invalid cell from the backend.
  virtual void DeleteCell(EntryCell cell) = 0;
  virtual void FixCell(EntryCell cell) = 0;
};

// The data required to initialize an index. Note that not all fields have to
// be provided when growing the tables.
struct NET_EXPORT_PRIVATE IndexTableInitData {
  IndexTableInitData();
  ~IndexTableInitData();

  IndexBitmap* index_bitmap;
  IndexBucket* main_table;
  IndexBucket* extra_table;
  scoped_ptr<IndexHeaderV3> backup_header;
  scoped_ptr<uint32[]> backup_bitmap;
};

// See the description at the top of this file.
class NET_EXPORT_PRIVATE IndexTable {
 public:
  explicit IndexTable(IndexTableBackend* backend);
  ~IndexTable();

  // Initializes the object, or re-initializes it when the backing files grow.
  // Note that the only supported way to initialize this objeect is using
  // pointers that come from the files being directly mapped in memory. If that
  // is not the case, it must be emulated in a convincing way, for example
  // making sure that the tables for re-init look the same as the tables to be
  // replaced.
  void Init(IndexTableInitData* params);

  // Releases the resources acquired during Init().
  void Shutdown();

  // Locates a resouce on the index. Returns a list of all resources that match
  // the provided hash.
  EntrySet LookupEntries(uint32 hash);

  // Creates a new cell to store a new resource.
  EntryCell CreateEntryCell(uint32 hash, Addr address);

  // Locates a particular cell. This method allows a caller to perform slow
  // operations with some entries while the index evolves, by returning the
  // current state of a cell. If the desired cell cannot be located, the return
  // object will be invalid.
  EntryCell FindEntryCell(uint32 hash, Addr address);

  // Returns an IndexTable timestamp for a given absolute time. The actual
  // resolution of the timestamp should be considered an implementation detail,
  // but it certainly is lower than seconds. The important part is that a group
  // of cells will share the same timestamp (see IndexIterator).
  int CalculateTimestamp(base::Time time);

  // Returns the equivalent time for a cell timestamp.
  base::Time TimeFromTimestamp(int timestamp);

  // Updates a particular cell.
  void SetSate(uint32 hash, Addr address, EntryState state);
  void UpdateTime(uint32 hash, Addr address, base::Time current);

  // Saves the contents of |cell| to the table.
  void Save(EntryCell* cell);

  // Returns the oldest entries for each group of entries. The initial values
  // for the provided iterators are ignored. Entries are assigned to iterators
  // according to their EntryGroup.
  void GetOldest(IndexIterator* no_use,
                 IndexIterator* low_use,
                 IndexIterator* high_use);

  // Returns the next group of entries for the provided iterator. This method
  // does not return the cells matching the initial iterator's timestamp,
  // but rather cells after (or before, depending on the iterator's |forward|
  // member) that timestamp.
  bool GetNextCells(IndexIterator* iterator);

  // Called each time the index should save the backup information. The caller
  // can assume that anything that needs to be saved is saved when this method
  // is called, and that there is only one source of timming information, and
  // that source is controlled by the owner of this object.
  void OnBackupTimer();

  IndexHeaderV3* header() { return header_; }
  const IndexHeaderV3* header() const { return header_; }

 private:
  EntryCell FindEntryCellImpl(uint32 hash, Addr address, bool allow_deleted);
  void CheckState(const EntryCell& cell);
  void Write(const EntryCell& cell);
  int NewExtraBucket();
  void WalkTables(int limit_time,
                  IndexIterator* no_use,
                  IndexIterator* low_use,
                  IndexIterator* high_use);
  void UpdateFromBucket(IndexBucket* bucket, int bucket_hash,
                        int limit_time,
                        IndexIterator* no_use,
                        IndexIterator* low_use,
                        IndexIterator* high_use);
  void MoveCells(IndexBucket* old_extra_table);
  void MoveSingleCell(IndexCell* current_cell, int cell_num,
                      int main_table_index, bool growing);
  void HandleMisplacedCell(IndexCell* current_cell, int cell_num,
                           int main_table_index);
  void CheckBucketList(int bucket_id);

  uint32 GetLocation(const IndexCell& cell);
  uint32 GetHashValue(const IndexCell& cell);
  uint32 GetFullHash(const IndexCell& cell, uint32 lower_part);
  bool IsHashMatch(const IndexCell& cell, uint32 hash);
  bool MisplacedHash(const IndexCell& cell, uint32 hash);

  IndexTableBackend* backend_;
  IndexHeaderV3* header_;
  scoped_ptr<Bitmap> bitmap_;
  scoped_ptr<Bitmap> backup_bitmap_;
  scoped_ptr<uint32[]> backup_bitmap_storage_;
  scoped_ptr<IndexHeaderV3> backup_header_;
  IndexBucket* main_table_;
  IndexBucket* extra_table_;
  uint32 mask_;     // Binary mask to map a hash to the hash table.
  int extra_bits_;  // How many bits are in mask_ above the default value.
  bool modified_;
  bool small_table_;

  DISALLOW_COPY_AND_ASSIGN(IndexTable);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_INDEX_TABLE_V3_H_
