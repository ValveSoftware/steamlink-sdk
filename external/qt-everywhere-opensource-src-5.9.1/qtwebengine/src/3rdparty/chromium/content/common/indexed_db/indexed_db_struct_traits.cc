// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_param_traits.h"
#include "content/common/indexed_db/indexed_db_struct_traits.h"
#include "mojo/common/common_custom_types_struct_traits.h"

namespace mojo {

// static
bool StructTraits<indexed_db::mojom::IndexKeysDataView,
                  content::IndexedDBIndexKeys>::
    Read(indexed_db::mojom::IndexKeysDataView data,
         content::IndexedDBIndexKeys* out) {
  out->first = data.index_id();
  return data.ReadIndexKeys(&out->second);
}

// static
bool StructTraits<indexed_db::mojom::IndexMetadataDataView,
                  content::IndexedDBIndexMetadata>::
    Read(indexed_db::mojom::IndexMetadataDataView data,
         content::IndexedDBIndexMetadata* out) {
  out->id = data.id();
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadKeyPath(&out->key_path))
    return false;
  out->unique = data.unique();
  out->multi_entry = data.multi_entry();
  return true;
}

// static
bool StructTraits<indexed_db::mojom::ObjectStoreMetadataDataView,
                  content::IndexedDBObjectStoreMetadata>::
    Read(indexed_db::mojom::ObjectStoreMetadataDataView data,
         content::IndexedDBObjectStoreMetadata* out) {
  out->id = data.id();
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadKeyPath(&out->key_path))
    return false;
  out->auto_increment = data.auto_increment();
  out->max_index_id = data.max_index_id();
  ArrayDataView<indexed_db::mojom::IndexMetadataDataView> indexes;
  data.GetIndexesDataView(&indexes);
  for (size_t i = 0; i < indexes.size(); ++i) {
    indexed_db::mojom::IndexMetadataDataView index;
    indexes.GetDataView(i, &index);
    DCHECK(!base::ContainsKey(out->indexes, index.id()));
    if (!StructTraits<
            indexed_db::mojom::IndexMetadataDataView,
            content::IndexedDBIndexMetadata>::Read(index,
                                                   &out->indexes[index.id()]))
      return false;
  }
  return true;
}

// static
bool StructTraits<indexed_db::mojom::DatabaseMetadataDataView,
                  content::IndexedDBDatabaseMetadata>::
    Read(indexed_db::mojom::DatabaseMetadataDataView data,
         content::IndexedDBDatabaseMetadata* out) {
  out->id = data.id();
  if (!data.ReadName(&out->name))
    return false;
  out->version = data.version();
  out->max_object_store_id = data.max_object_store_id();
  ArrayDataView<indexed_db::mojom::ObjectStoreMetadataDataView> object_stores;
  data.GetObjectStoresDataView(&object_stores);
  for (size_t i = 0; i < object_stores.size(); ++i) {
    indexed_db::mojom::ObjectStoreMetadataDataView object_store;
    object_stores.GetDataView(i, &object_store);
    DCHECK(!base::ContainsKey(out->object_stores, object_store.id()));
    if (!StructTraits<indexed_db::mojom::ObjectStoreMetadataDataView,
                      content::IndexedDBObjectStoreMetadata>::
            Read(object_store, &out->object_stores[object_store.id()]))
      return false;
  }
  return true;
}

}  // namespace mojo
