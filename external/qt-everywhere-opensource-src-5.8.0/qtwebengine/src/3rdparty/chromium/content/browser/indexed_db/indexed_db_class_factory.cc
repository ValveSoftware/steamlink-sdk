// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_class_factory.h"

#include <utility>

#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/leveldb/leveldb_iterator_impl.h"
#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"

namespace content {

static IndexedDBClassFactory::GetterCallback* s_factory_getter;
static ::base::LazyInstance<IndexedDBClassFactory>::Leaky s_factory =
    LAZY_INSTANCE_INITIALIZER;

void IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(GetterCallback* cb) {
  s_factory_getter = cb;
}

IndexedDBClassFactory* IndexedDBClassFactory::Get() {
  if (s_factory_getter)
    return (*s_factory_getter)();
  else
    return s_factory.Pointer();
}

IndexedDBDatabase* IndexedDBClassFactory::CreateIndexedDBDatabase(
    const base::string16& name,
    IndexedDBBackingStore* backing_store,
    IndexedDBFactory* factory,
    const IndexedDBDatabase::Identifier& unique_identifier) {
  return new IndexedDBDatabase(name, backing_store, factory, unique_identifier);
}

IndexedDBTransaction* IndexedDBClassFactory::CreateIndexedDBTransaction(
    int64_t id,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks,
    const std::set<int64_t>& scope,
    blink::WebIDBTransactionMode mode,
    IndexedDBDatabase* db,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  return new IndexedDBTransaction(id, callbacks, scope, mode, db,
                                  backing_store_transaction);
}

LevelDBTransaction* IndexedDBClassFactory::CreateLevelDBTransaction(
    LevelDBDatabase* db) {
  return new LevelDBTransaction(db);
}

content::LevelDBIteratorImpl* IndexedDBClassFactory::CreateIteratorImpl(
    std::unique_ptr<leveldb::Iterator> iterator) {
  return new LevelDBIteratorImpl(std::move(iterator));
}

}  // namespace content
