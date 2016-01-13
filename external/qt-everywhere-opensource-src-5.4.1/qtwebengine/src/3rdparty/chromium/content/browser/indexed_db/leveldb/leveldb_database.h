// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_DATABASE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {
class Comparator;
class DB;
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

class CONTENT_EXPORT LevelDBDatabase {
 public:
  class ComparatorAdapter : public leveldb::Comparator {
   public:
    explicit ComparatorAdapter(const LevelDBComparator* comparator);

    virtual int Compare(const leveldb::Slice& a,
                        const leveldb::Slice& b) const OVERRIDE;

    virtual const char* Name() const OVERRIDE;

    virtual void FindShortestSeparator(std::string* start,
                                       const leveldb::Slice& limit) const
        OVERRIDE;
    virtual void FindShortSuccessor(std::string* key) const OVERRIDE;

   private:
    const LevelDBComparator* comparator_;
  };

  static leveldb::Status Open(const base::FilePath& file_name,
                              const LevelDBComparator* comparator,
                              scoped_ptr<LevelDBDatabase>* db,
                              bool* is_disk_full = 0);
  static scoped_ptr<LevelDBDatabase> OpenInMemory(
      const LevelDBComparator* comparator);
  static leveldb::Status Destroy(const base::FilePath& file_name);
  static scoped_ptr<LevelDBLock> LockForTesting(
      const base::FilePath& file_name);
  virtual ~LevelDBDatabase();

  leveldb::Status Put(const base::StringPiece& key, std::string* value);
  leveldb::Status Remove(const base::StringPiece& key);
  virtual leveldb::Status Get(const base::StringPiece& key,
                              std::string* value,
                              bool* found,
                              const LevelDBSnapshot* = 0);
  leveldb::Status Write(const LevelDBWriteBatch& write_batch);
  scoped_ptr<LevelDBIterator> CreateIterator(const LevelDBSnapshot* = 0);
  const LevelDBComparator* Comparator() const;
  void Compact(const base::StringPiece& start, const base::StringPiece& stop);
  void CompactAll();

 protected:
  LevelDBDatabase();

 private:
  friend class LevelDBSnapshot;

  scoped_ptr<leveldb::Env> env_;
  scoped_ptr<leveldb::Comparator> comparator_adapter_;
  scoped_ptr<leveldb::DB> db_;
  const LevelDBComparator* comparator_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_DATABASE_H_
