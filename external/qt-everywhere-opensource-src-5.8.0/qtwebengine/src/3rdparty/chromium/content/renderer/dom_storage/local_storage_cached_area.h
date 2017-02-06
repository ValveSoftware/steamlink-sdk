// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREA_H_
#define CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREA_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/nullable_string16.h"
#include "content/common/leveldb_wrapper.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class DOMStorageMap;
class LocalStorageArea;
class LocalStorageCachedAreas;

namespace mojom {
class StoragePartitionService;
}

// An in-process implementation of LocalStorage using a LevelDB Mojo service.
// Maintains a complete cache of the origin's Map of key/value pairs for fast
// access. The cache is primed on first access and changes are written to the
// backend through the level db interface pointer. Mutations originating in
// other processes are applied to the cache via mojom::LevelDBObserver
// callbacks.
// There is one LocalStorageCachedArea for potentially many LocalStorageArea
// objects.
class LocalStorageCachedArea : public mojom::LevelDBObserver,
                               public base::RefCounted<LocalStorageCachedArea> {
 public:
  LocalStorageCachedArea(
      const url::Origin& origin,
      mojom::StoragePartitionService* storage_partition_service,
      LocalStorageCachedAreas* cached_areas);

  // These correspond to blink::WebStorageArea.
  unsigned GetLength();
  base::NullableString16 GetKey(unsigned index);
  base::NullableString16 GetItem(const base::string16& key);
  bool SetItem(const base::string16& key,
               const base::string16& value,
               const GURL& page_url,
               const std::string& storage_area_id);
  void RemoveItem(const base::string16& key,
                  const GURL& page_url,
                  const std::string& storage_area_id);
  void Clear(const GURL& page_url, const std::string& storage_area_id);

  // Allow this object to keep track of the LocalStorageAreas corresponding to
  // it, which is needed for mutation event notifications.
  void AreaCreated(LocalStorageArea* area);
  void AreaDestroyed(LocalStorageArea* area);

  const url::Origin& origin() { return origin_; }

 private:
  friend class base::RefCounted<LocalStorageCachedArea>;
  ~LocalStorageCachedArea() override;

  // LevelDBObserver:
  void KeyAdded(mojo::Array<uint8_t> key,
                mojo::Array<uint8_t> value,
                const mojo::String& source) override;
  void KeyChanged(mojo::Array<uint8_t> key,
                  mojo::Array<uint8_t> new_value,
                  mojo::Array<uint8_t> old_value,
                  const mojo::String& source) override;
  void KeyDeleted(mojo::Array<uint8_t> key,
                  mojo::Array<uint8_t> old_value,
                  const mojo::String& source) override;
  void AllDeleted(const mojo::String& source) override;
  void GetAllComplete(const mojo::String& source) override;

  // Common helper for KeyAdded() and KeyChanged()
  void KeyAddedOrChanged(mojo::Array<uint8_t> key,
                         mojo::Array<uint8_t> new_value,
                         const base::NullableString16& old_value,
                         const mojo::String& source);

  // Synchronously fetches the origin's local storage data if it hasn't been
  // fetched already.
  void EnsureLoaded();

  void OnSetItemComplete(const base::string16& key, bool success);
  void OnRemoveItemComplete(const base::string16& key, bool success);
  void OnClearComplete(bool success);

  // Resets the object back to its newly constructed state.
  void Reset();

  url::Origin origin_;
  scoped_refptr<DOMStorageMap> map_;
  std::map<base::string16, int> ignore_key_mutations_;
  bool ignore_all_mutations_ = false;
  std::string get_all_request_id_;
  mojom::LevelDBWrapperPtr leveldb_;
  mojo::Binding<mojom::LevelDBObserver> binding_;
  LocalStorageCachedAreas* cached_areas_;
  std::map<std::string, LocalStorageArea*> areas_;
  base::WeakPtrFactory<LocalStorageCachedArea> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageCachedArea);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_LOCAL_STORAGE_CACHED_AREA_H_
