// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

namespace content {

class LevelDBTransaction;
class LevelDBDatabase;

enum FailClass {
  FAIL_CLASS_NOTHING,
  FAIL_CLASS_LEVELDB_ITERATOR,
  FAIL_CLASS_LEVELDB_TRANSACTION,
};

enum FailMethod {
  FAIL_METHOD_NOTHING,
  FAIL_METHOD_COMMIT,
  FAIL_METHOD_COMMIT_DISK_FULL,
  FAIL_METHOD_GET,
  FAIL_METHOD_SEEK,
};

class MockBrowserTestIndexedDBClassFactory : public IndexedDBClassFactory {
 public:
  MockBrowserTestIndexedDBClassFactory();
  ~MockBrowserTestIndexedDBClassFactory() override;

  IndexedDBDatabase* CreateIndexedDBDatabase(
      const base::string16& name,
      IndexedDBBackingStore* backing_store,
      IndexedDBFactory* factory,
      const IndexedDBDatabase::Identifier& unique_identifier) override;
  IndexedDBTransaction* CreateIndexedDBTransaction(
      int64_t id,
      scoped_refptr<IndexedDBDatabaseCallbacks> callbacks,
      const std::set<int64_t>& scope,
      blink::WebIDBTransactionMode mode,
      IndexedDBDatabase* db,
      IndexedDBBackingStore::Transaction* backing_store_transaction) override;
  LevelDBTransaction* CreateLevelDBTransaction(LevelDBDatabase* db) override;
  LevelDBIteratorImpl* CreateIteratorImpl(
      std::unique_ptr<leveldb::Iterator> iterator) override;

  void FailOperation(FailClass failure_class,
                     FailMethod failure_method,
                     int fail_on_instance_num,
                     int fail_on_call_num);
  void Reset();

 private:
  FailClass failure_class_;
  FailMethod failure_method_;
  std::map<FailClass, int> instance_count_;
  std::map<FailClass, int> fail_on_instance_num_;
  std::map<FailClass, int> fail_on_call_num_;
  bool only_trace_calls_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_
