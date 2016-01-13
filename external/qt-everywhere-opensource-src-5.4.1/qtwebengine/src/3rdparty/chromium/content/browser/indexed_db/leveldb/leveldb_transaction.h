// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_TRANSACTION_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_TRANSACTION_H_

#include <map>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/browser/indexed_db/leveldb/leveldb_database.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator.h"

namespace content {

class LevelDBWriteBatch;

class CONTENT_EXPORT LevelDBTransaction
    : public base::RefCounted<LevelDBTransaction> {
 public:

  void Put(const base::StringPiece& key, std::string* value);
  void Remove(const base::StringPiece& key);
  virtual leveldb::Status Get(const base::StringPiece& key,
                              std::string* value,
                              bool* found);
  virtual leveldb::Status Commit();
  void Rollback();

  scoped_ptr<LevelDBIterator> CreateIterator();

 protected:
  virtual ~LevelDBTransaction();
  explicit LevelDBTransaction(LevelDBDatabase* db);
  friend class IndexedDBClassFactory;

 private:
  friend class base::RefCounted<LevelDBTransaction>;
  FRIEND_TEST_ALL_PREFIXES(LevelDBDatabaseTest, Transaction);
  FRIEND_TEST_ALL_PREFIXES(LevelDBDatabaseTest, TransactionCommitTest);
  FRIEND_TEST_ALL_PREFIXES(LevelDBDatabaseTest, TransactionIterator);

  struct Record {
    Record();
    ~Record();
    std::string key;
    std::string value;
    bool deleted;
  };

  class Comparator {
   public:
    explicit Comparator(const LevelDBComparator* comparator)
        : comparator_(comparator) {}
    bool operator()(const base::StringPiece& a,
                    const base::StringPiece& b) const {
      return comparator_->Compare(a, b) < 0;
    }

   private:
    const LevelDBComparator* comparator_;
  };

  typedef std::map<base::StringPiece, Record*, Comparator> DataType;

  class DataIterator : public LevelDBIterator {
   public:
    static scoped_ptr<DataIterator> Create(LevelDBTransaction* transaction);
    virtual ~DataIterator();

    virtual bool IsValid() const OVERRIDE;
    virtual leveldb::Status SeekToLast() OVERRIDE;
    virtual leveldb::Status Seek(const base::StringPiece& slice) OVERRIDE;
    virtual leveldb::Status Next() OVERRIDE;
    virtual leveldb::Status Prev() OVERRIDE;
    virtual base::StringPiece Key() const OVERRIDE;
    virtual base::StringPiece Value() const OVERRIDE;
    bool IsDeleted() const;

   private:
    explicit DataIterator(LevelDBTransaction* transaction);
    DataType* data_;
    DataType::iterator iterator_;

    DISALLOW_COPY_AND_ASSIGN(DataIterator);
  };

  class TransactionIterator : public LevelDBIterator {
   public:
    virtual ~TransactionIterator();
    static scoped_ptr<TransactionIterator> Create(
        scoped_refptr<LevelDBTransaction> transaction);

    virtual bool IsValid() const OVERRIDE;
    virtual leveldb::Status SeekToLast() OVERRIDE;
    virtual leveldb::Status Seek(const base::StringPiece& target) OVERRIDE;
    virtual leveldb::Status Next() OVERRIDE;
    virtual leveldb::Status Prev() OVERRIDE;
    virtual base::StringPiece Key() const OVERRIDE;
    virtual base::StringPiece Value() const OVERRIDE;
    void DataChanged();

   private:
    explicit TransactionIterator(scoped_refptr<LevelDBTransaction> transaction);
    void HandleConflictsAndDeletes();
    void SetCurrentIteratorToSmallestKey();
    void SetCurrentIteratorToLargestKey();
    void RefreshDataIterator() const;
    bool DataIteratorIsLower() const;
    bool DataIteratorIsHigher() const;

    scoped_refptr<LevelDBTransaction> transaction_;
    const LevelDBComparator* comparator_;
    mutable scoped_ptr<DataIterator> data_iterator_;
    scoped_ptr<LevelDBIterator> db_iterator_;
    LevelDBIterator* current_;

    enum Direction {
      FORWARD,
      REVERSE
    };
    Direction direction_;
    mutable bool data_changed_;

    DISALLOW_COPY_AND_ASSIGN(TransactionIterator);
  };

  void Set(const base::StringPiece& key, std::string* value, bool deleted);
  void Clear();
  void RegisterIterator(TransactionIterator* iterator);
  void UnregisterIterator(TransactionIterator* iterator);
  void NotifyIterators();

  LevelDBDatabase* db_;
  const LevelDBSnapshot snapshot_;
  const LevelDBComparator* comparator_;
  Comparator data_comparator_;
  DataType data_;
  bool finished_;
  std::set<TransactionIterator*> iterators_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBTransaction);
};

// Reads go straight to the database, ignoring any writes cached in
// write_batch_, and writes are write-through, without consolidation.
class LevelDBDirectTransaction {
 public:
  static scoped_ptr<LevelDBDirectTransaction> Create(LevelDBDatabase* db);

  ~LevelDBDirectTransaction();
  void Put(const base::StringPiece& key, const std::string* value);
  leveldb::Status Get(const base::StringPiece& key,
                      std::string* value,
                      bool* found);
  void Remove(const base::StringPiece& key);
  leveldb::Status Commit();

 private:
  explicit LevelDBDirectTransaction(LevelDBDatabase* db);

  LevelDBDatabase* db_;
  scoped_ptr<LevelDBWriteBatch> write_batch_;
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBDirectTransaction);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_TRANSACTION_H_
