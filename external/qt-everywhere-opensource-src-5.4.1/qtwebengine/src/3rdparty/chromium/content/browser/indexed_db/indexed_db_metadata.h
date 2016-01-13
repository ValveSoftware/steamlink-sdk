// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_METADATA_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_METADATA_H_

#include <map>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "content/common/indexed_db/indexed_db_key_path.h"

namespace content {

struct IndexedDBIndexMetadata {
  IndexedDBIndexMetadata() {}
  IndexedDBIndexMetadata(const base::string16& name,
                         int64 id,
                         const IndexedDBKeyPath& key_path,
                         bool unique,
                         bool multi_entry)
      : name(name),
        id(id),
        key_path(key_path),
        unique(unique),
        multi_entry(multi_entry) {}
  base::string16 name;
  int64 id;
  IndexedDBKeyPath key_path;
  bool unique;
  bool multi_entry;

  static const int64 kInvalidId = -1;
};

struct CONTENT_EXPORT IndexedDBObjectStoreMetadata {
  IndexedDBObjectStoreMetadata();
  IndexedDBObjectStoreMetadata(const base::string16& name,
                               int64 id,
                               const IndexedDBKeyPath& key_path,
                               bool auto_increment,
                               int64 max_index_id);
  ~IndexedDBObjectStoreMetadata();
  base::string16 name;
  int64 id;
  IndexedDBKeyPath key_path;
  bool auto_increment;
  int64 max_index_id;

  static const int64 kInvalidId = -1;

  typedef std::map<int64, IndexedDBIndexMetadata> IndexMap;
  IndexMap indexes;
};

struct CONTENT_EXPORT IndexedDBDatabaseMetadata {
  // TODO(jsbell): These can probably be collapsed into 0.
  enum {
    NO_INT_VERSION = -1,
    DEFAULT_INT_VERSION = 0
  };

  typedef std::map<int64, IndexedDBObjectStoreMetadata> ObjectStoreMap;

  IndexedDBDatabaseMetadata();
  IndexedDBDatabaseMetadata(const base::string16& name,
                            int64 id,
                            const base::string16& version,
                            int64 int_version,
                            int64 max_object_store_id);
  ~IndexedDBDatabaseMetadata();

  base::string16 name;
  int64 id;
  base::string16 version;
  int64 int_version;
  int64 max_object_store_id;

  ObjectStoreMap object_stores;
};
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_METADATA_H_
