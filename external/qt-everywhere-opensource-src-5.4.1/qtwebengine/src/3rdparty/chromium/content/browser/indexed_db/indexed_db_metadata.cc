// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_metadata.h"

namespace content {

IndexedDBObjectStoreMetadata::IndexedDBObjectStoreMetadata(
    const base::string16& name,
    int64 id,
    const IndexedDBKeyPath& key_path,
    bool auto_increment,
    int64 max_index_id)
    : name(name),
      id(id),
      key_path(key_path),
      auto_increment(auto_increment),
      max_index_id(max_index_id) {}

IndexedDBObjectStoreMetadata::IndexedDBObjectStoreMetadata() {}
IndexedDBObjectStoreMetadata::~IndexedDBObjectStoreMetadata() {}

IndexedDBDatabaseMetadata::IndexedDBDatabaseMetadata()
    : int_version(NO_INT_VERSION) {}
IndexedDBDatabaseMetadata::IndexedDBDatabaseMetadata(
    const base::string16& name,
    int64 id,
    const base::string16& version,
    int64 int_version,
    int64 max_object_store_id)
    : name(name),
      id(id),
      version(version),
      int_version(int_version),
      max_object_store_id(max_object_store_id) {}

IndexedDBDatabaseMetadata::~IndexedDBDatabaseMetadata() {}

}  // namespace content
