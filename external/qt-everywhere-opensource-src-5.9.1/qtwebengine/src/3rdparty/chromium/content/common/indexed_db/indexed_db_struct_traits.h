// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_STRUCT_TRAITS_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_STRUCT_TRAITS_H_

#include "content/common/indexed_db/indexed_db.mojom.h"
#include "content/common/indexed_db/indexed_db_metadata.h"

namespace mojo {

template <>
struct StructTraits<indexed_db::mojom::IndexKeysDataView,
                    content::IndexedDBIndexKeys> {
  static int64_t index_id(const content::IndexedDBIndexKeys& index_keys) {
    return index_keys.first;
  }
  static const std::vector<content::IndexedDBKey>& index_keys(
      const content::IndexedDBIndexKeys& index_keys) {
    return index_keys.second;
  }
  static bool Read(indexed_db::mojom::IndexKeysDataView data,
                   content::IndexedDBIndexKeys* out);
};

template <>
struct StructTraits<indexed_db::mojom::IndexMetadataDataView,
                    content::IndexedDBIndexMetadata> {
  static int64_t id(const content::IndexedDBIndexMetadata& metadata) {
    return metadata.id;
  }
  static base::string16 name(const content::IndexedDBIndexMetadata& metadata) {
    return metadata.name;
  }
  static const content::IndexedDBKeyPath& key_path(
      const content::IndexedDBIndexMetadata& metadata) {
    return metadata.key_path;
  }
  static bool unique(const content::IndexedDBIndexMetadata& metadata) {
    return metadata.unique;
  }
  static bool multi_entry(const content::IndexedDBIndexMetadata& metadata) {
    return metadata.multi_entry;
  }
  static bool Read(indexed_db::mojom::IndexMetadataDataView data,
                   content::IndexedDBIndexMetadata* out);
};

template <>
struct StructTraits<indexed_db::mojom::ObjectStoreMetadataDataView,
                    content::IndexedDBObjectStoreMetadata> {
  static int64_t id(const content::IndexedDBObjectStoreMetadata& metadata) {
    return metadata.id;
  }
  static base::string16 name(
      const content::IndexedDBObjectStoreMetadata& metadata) {
    return metadata.name;
  }
  static const content::IndexedDBKeyPath& key_path(
      const content::IndexedDBObjectStoreMetadata& metadata) {
    return metadata.key_path;
  }
  static bool auto_increment(
      const content::IndexedDBObjectStoreMetadata& metadata) {
    return metadata.auto_increment;
  }
  static int64_t max_index_id(
      const content::IndexedDBObjectStoreMetadata& metadata) {
    return metadata.max_index_id;
  }
  static MapValuesArrayView<int64_t, content::IndexedDBIndexMetadata> indexes(
      const content::IndexedDBObjectStoreMetadata& metadata) {
    return MapValuesToArray(metadata.indexes);
  }
  static bool Read(indexed_db::mojom::ObjectStoreMetadataDataView data,
                   content::IndexedDBObjectStoreMetadata* out);
};

template <>
struct StructTraits<indexed_db::mojom::DatabaseMetadataDataView,
                    content::IndexedDBDatabaseMetadata> {
  static int64_t id(const content::IndexedDBDatabaseMetadata& metadata) {
    return metadata.id;
  }
  static base::string16 name(
      const content::IndexedDBDatabaseMetadata& metadata) {
    return metadata.name;
  }
  static int64_t version(const content::IndexedDBDatabaseMetadata& metadata) {
    return metadata.version;
  }
  static int64_t max_object_store_id(
      const content::IndexedDBDatabaseMetadata& metadata) {
    return metadata.max_object_store_id;
  }
  static MapValuesArrayView<int64_t, content::IndexedDBObjectStoreMetadata>
  object_stores(const content::IndexedDBDatabaseMetadata& metadata) {
    return MapValuesToArray(metadata.object_stores);
  }
  static bool Read(indexed_db::mojom::DatabaseMetadataDataView data,
                   content::IndexedDBDatabaseMetadata* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_STRUCT_TRAITS_H_
