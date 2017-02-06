// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_metadata.h"

namespace content {

IndexedDBIndexMetadata::IndexedDBIndexMetadata() = default;

IndexedDBIndexMetadata::IndexedDBIndexMetadata(const base::string16& name,
                                               int64_t id,
                                               const IndexedDBKeyPath& key_path,
                                               bool unique,
                                               bool multi_entry)
    : name(name),
      id(id),
      key_path(key_path),
      unique(unique),
      multi_entry(multi_entry) {}

IndexedDBIndexMetadata::IndexedDBIndexMetadata(
    const IndexedDBIndexMetadata& other) = default;

IndexedDBIndexMetadata::~IndexedDBIndexMetadata() = default;

IndexedDBIndexMetadata& IndexedDBIndexMetadata::operator=(
    const IndexedDBIndexMetadata& other) = default;

IndexedDBObjectStoreMetadata::IndexedDBObjectStoreMetadata(
    const base::string16& name,
    int64_t id,
    const IndexedDBKeyPath& key_path,
    bool auto_increment,
    int64_t max_index_id)
    : name(name),
      id(id),
      key_path(key_path),
      auto_increment(auto_increment),
      max_index_id(max_index_id) {}

IndexedDBObjectStoreMetadata::IndexedDBObjectStoreMetadata() = default;

IndexedDBObjectStoreMetadata::IndexedDBObjectStoreMetadata(
    const IndexedDBObjectStoreMetadata& other) = default;

IndexedDBObjectStoreMetadata::~IndexedDBObjectStoreMetadata() = default;

IndexedDBObjectStoreMetadata& IndexedDBObjectStoreMetadata::operator=(
    const IndexedDBObjectStoreMetadata& other) = default;

IndexedDBDatabaseMetadata::IndexedDBDatabaseMetadata() : version(NO_VERSION) {}

IndexedDBDatabaseMetadata::IndexedDBDatabaseMetadata(
    const base::string16& name,
    int64_t id,
    int64_t version,
    int64_t max_object_store_id)
    : name(name),
      id(id),
      version(version),
      max_object_store_id(max_object_store_id) {}

IndexedDBDatabaseMetadata::IndexedDBDatabaseMetadata(
    const IndexedDBDatabaseMetadata& other) = default;

IndexedDBDatabaseMetadata::~IndexedDBDatabaseMetadata() = default;

IndexedDBDatabaseMetadata& IndexedDBDatabaseMetadata::operator=(
    IndexedDBDatabaseMetadata& other) = default;

}  // namespace content
