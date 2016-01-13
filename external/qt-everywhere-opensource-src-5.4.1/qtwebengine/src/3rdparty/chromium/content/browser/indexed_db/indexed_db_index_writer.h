// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INDEX_WRITER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INDEX_WRITER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/common/indexed_db/indexed_db_key_path.h"

namespace content {

class IndexedDBTransaction;
struct IndexedDBObjectStoreMetadata;

class IndexWriter {
 public:
  explicit IndexWriter(const IndexedDBIndexMetadata& index_metadata);

  IndexWriter(const IndexedDBIndexMetadata& index_metadata,
              const IndexedDBDatabase::IndexKeys& index_keys);

  bool VerifyIndexKeys(IndexedDBBackingStore* store,
                       IndexedDBBackingStore::Transaction* transaction,
                       int64 database_id,
                       int64 object_store_id,
                       int64 index_id,
                       bool* can_add_keys,
                       const IndexedDBKey& primary_key,
                       base::string16* error_message) const WARN_UNUSED_RESULT;

  void WriteIndexKeys(const IndexedDBBackingStore::RecordIdentifier& record,
                      IndexedDBBackingStore* store,
                      IndexedDBBackingStore::Transaction* transaction,
                      int64 database_id,
                      int64 object_store_id) const;

  ~IndexWriter();

 private:
  bool AddingKeyAllowed(IndexedDBBackingStore* store,
                        IndexedDBBackingStore::Transaction* transaction,
                        int64 database_id,
                        int64 object_store_id,
                        int64 index_id,
                        const IndexedDBKey& index_key,
                        const IndexedDBKey& primary_key,
                        bool* allowed) const WARN_UNUSED_RESULT;

  const IndexedDBIndexMetadata index_metadata_;
  IndexedDBDatabase::IndexKeys index_keys_;

  DISALLOW_COPY_AND_ASSIGN(IndexWriter);
};

bool MakeIndexWriters(
    IndexedDBTransaction* transaction,
    IndexedDBBackingStore* store,
    int64 database_id,
    const IndexedDBObjectStoreMetadata& metadata,
    const IndexedDBKey& primary_key,
    bool key_was_generated,
    const std::vector<IndexedDBDatabase::IndexKeys>& index_keys,
    ScopedVector<IndexWriter>* index_writers,
    base::string16* error_message,
    bool* completed) WARN_UNUSED_RESULT;

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_INDEX_WRITER_H_
