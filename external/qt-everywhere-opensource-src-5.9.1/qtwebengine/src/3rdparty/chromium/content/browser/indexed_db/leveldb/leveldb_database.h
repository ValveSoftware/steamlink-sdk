// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_DATABASE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/memory_dump_provider.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {
class Comparator;
class DB;
class FilterPolicy;
class Env;
class Snapshot;
}

namespace content {

class LevelDBComparator;
class LevelDBDatabase;
class LevelDBIterator;
class LevelDBWriteBatch;

class LevelDBSnapshot {
 private:
  friend class LevelDBDatabase;
  friend class LevelDBTransaction;

  explicit LevelDBSnapshot(LevelDBDatabase* db);
  ~LevelDBSnapshot();

  leveldb::DB* db_;
  const leveldb::Snapshot* snapshot_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBSnapshot);
};

class CONTENT_EXPORT LevelDBLock {
 public:
  LevelDBLock() {}
  virtual ~LevelDBLock() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LevelDBLock);
};

class CONTENT_EXPORT LevelDBDatabase
    : public base::trace_event::MemoryDumpProvider {
 public:
  class ComparatorAdapter : public leveldb::Comparator {
   public:
    explicit ComparatorAdapter(const LevelDBComparator* comparator);

    int Compare(const leveldb::Slice& a,
                const leveldb::Slice& b) const override;

    const char* Name() const override;

    void FindShortestSeparator(std::string* start,
                               const leveldb::Slice& limit) const override;
    void FindShortSuccessor(std::string* key) const override;

   private:
    const LevelDBComparator* comparator_;
  };

  static leveldb::Status Open(const base::FilePath& file_name,
                              const LevelDBComparator* comparator,
                              std::unique_ptr<LevelDBDatabase>* db,
                              bool* is_disk_full = 0);
  static std::unique_ptr<LevelDBDatabase> OpenInMemory(
      const LevelDBComparator* comparator);
  static leveldb::Status Destroy(const base::FilePath& file_name);
  static std::unique_ptr<LevelDBLock> LockForTesting(
      const base::FilePath& file_name);
  ~LevelDBDatabase() override;

  leveldb::Status Put(const base::StringPiece& key, std::string* value);
  leveldb::Status Remove(const base::StringPiece& key);
  virtual leveldb::Status Get(const base::StringPiece& key,
                              std::string* value,
                              bool* found,
                              const LevelDBSnapshot* = 0);
  leveldb::Status Write(const LevelDBWriteBatch& write_batch);
  std::unique_ptr<LevelDBIterator> CreateIterator(const LevelDBSnapshot* = 0);
  const LevelDBComparator* Comparator() const;
  void Compact(const base::StringPiece& start, const base::StringPiece& stop);
  void CompactAll();

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 protected:
  LevelDBDatabase();

 private:
  friend class LevelDBSnapshot;

  void CloseDatabase();

  std::unique_ptr<leveldb::Env> env_;
  std::unique_ptr<leveldb::Comparator> comparator_adapter_;
  std::unique_ptr<leveldb::DB> db_;
  std::unique_ptr<const leveldb::FilterPolicy> filter_policy_;
  const LevelDBComparator* comparator_;
  std::string file_name_for_tracing;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_DATABASE_H_
