// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBMetadata.h"

#include "public/platform/modules/indexeddb/WebIDBMetadata.h"

namespace blink {

constexpr int64_t IDBIndexMetadata::InvalidId;

constexpr int64_t IDBObjectStoreMetadata::InvalidId;

IDBIndexMetadata::IDBIndexMetadata() = default;

IDBIndexMetadata::IDBIndexMetadata(const String& name,
                                   int64_t id,
                                   const IDBKeyPath& keyPath,
                                   bool unique,
                                   bool multiEntry)
    : name(name),
      id(id),
      keyPath(keyPath),
      unique(unique),
      multiEntry(multiEntry) {}

IDBObjectStoreMetadata::IDBObjectStoreMetadata() = default;

IDBObjectStoreMetadata::IDBObjectStoreMetadata(const String& name,
                                               int64_t id,
                                               const IDBKeyPath& keyPath,
                                               bool autoIncrement,
                                               int64_t maxIndexId)
    : name(name),
      id(id),
      keyPath(keyPath),
      autoIncrement(autoIncrement),
      maxIndexId(maxIndexId) {}

RefPtr<IDBObjectStoreMetadata> IDBObjectStoreMetadata::createCopy() const {
  RefPtr<IDBObjectStoreMetadata> copy = adoptRef(
      new IDBObjectStoreMetadata(name, id, keyPath, autoIncrement, maxIndexId));

  for (const auto& it : indexes) {
    IDBIndexMetadata* index = it.value.get();
    RefPtr<IDBIndexMetadata> indexCopy =
        adoptRef(new IDBIndexMetadata(index->name, index->id, index->keyPath,
                                      index->unique, index->multiEntry));
    copy->indexes.add(it.key, std::move(indexCopy));
  }
  return copy;
}

IDBDatabaseMetadata::IDBDatabaseMetadata()
    : version(IDBDatabaseMetadata::NoVersion) {}

IDBDatabaseMetadata::IDBDatabaseMetadata(const String& name,
                                         int64_t id,
                                         int64_t version,
                                         int64_t maxObjectStoreId)
    : name(name),
      id(id),
      version(version),
      maxObjectStoreId(maxObjectStoreId) {}

IDBDatabaseMetadata::IDBDatabaseMetadata(const WebIDBMetadata& webMetadata)
    : name(webMetadata.name),
      id(webMetadata.id),
      version(webMetadata.version),
      maxObjectStoreId(webMetadata.maxObjectStoreId) {
  for (size_t i = 0; i < webMetadata.objectStores.size(); ++i) {
    const WebIDBMetadata::ObjectStore& webObjectStore =
        webMetadata.objectStores[i];
    RefPtr<IDBObjectStoreMetadata> objectStore =
        adoptRef(new IDBObjectStoreMetadata(
            webObjectStore.name, webObjectStore.id,
            IDBKeyPath(webObjectStore.keyPath), webObjectStore.autoIncrement,
            webObjectStore.maxIndexId));

    for (size_t j = 0; j < webObjectStore.indexes.size(); ++j) {
      const WebIDBMetadata::Index& webIndex = webObjectStore.indexes[j];
      RefPtr<IDBIndexMetadata> index = adoptRef(new IDBIndexMetadata(
          webIndex.name, webIndex.id, IDBKeyPath(webIndex.keyPath),
          webIndex.unique, webIndex.multiEntry));
      objectStore->indexes.set(webIndex.id, std::move(index));
    }
    objectStores.set(webObjectStore.id, std::move(objectStore));
  }
}

void IDBDatabaseMetadata::copyFrom(const IDBDatabaseMetadata& metadata) {
  name = metadata.name;
  id = metadata.id;
  version = metadata.version;
  maxObjectStoreId = metadata.maxObjectStoreId;
}

}  // namespace blink
