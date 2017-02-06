// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_cached_area.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "content/common/dom_storage/dom_storage_map.h"
#include "content/common/storage_partition_service.mojom.h"
#include "content/renderer/dom_storage/local_storage_area.h"
#include "content/renderer/dom_storage/local_storage_cached_areas.h"
#include "mojo/common/common_type_converters.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebStorageEventDispatcher.h"
#include "url/gurl.h"

namespace content {

// These methods are used to pack and unpack the page_url/storage_area_id into
// source strings to/from the browser.
std::string PackSource(const GURL& page_url,
                       const std::string& storage_area_id) {
  return page_url.spec() + "\n" + storage_area_id;
}

void UnpackSource(const mojo::String& source,
                  GURL* page_url,
                  std::string* storage_area_id) {
  std::vector<std::string> result = base::SplitString(
      source.To<std::string>(), "\n", base::KEEP_WHITESPACE,
      base::SPLIT_WANT_ALL);
  DCHECK_EQ(result.size(), 2u);
  *page_url = GURL(result[0]);
  *storage_area_id = result[1];
}

LocalStorageCachedArea::LocalStorageCachedArea(
    const url::Origin& origin,
    mojom::StoragePartitionService* storage_partition_service,
    LocalStorageCachedAreas* cached_areas)
    : origin_(origin), binding_(this),
      cached_areas_(cached_areas), weak_factory_(this) {
  storage_partition_service->OpenLocalStorage(
      origin_, binding_.CreateInterfacePtrAndBind(), mojo::GetProxy(&leveldb_));
}

LocalStorageCachedArea::~LocalStorageCachedArea() {
  cached_areas_->CacheAreaClosed(this);
}

unsigned LocalStorageCachedArea::GetLength() {
  EnsureLoaded();
  return map_->Length();
}

base::NullableString16 LocalStorageCachedArea::GetKey(unsigned index) {
  EnsureLoaded();
  return map_->Key(index);
}

base::NullableString16 LocalStorageCachedArea::GetItem(
    const base::string16& key) {
  EnsureLoaded();
  return map_->GetItem(key);
}

bool LocalStorageCachedArea::SetItem(const base::string16& key,
                                     const base::string16& value,
                                     const GURL& page_url,
                                     const std::string& storage_area_id) {
  // A quick check to reject obviously overbudget items to avoid priming the
  // cache.
  if (key.length() + value.length() > kPerStorageAreaQuota)
    return false;

  EnsureLoaded();
  base::NullableString16 unused;
  if (!map_->SetItem(key, value, &unused))
    return false;

  // Ignore mutations to |key| until OnSetItemComplete.
  ignore_key_mutations_[key]++;
  leveldb_->Put(mojo::Array<uint8_t>::From(key),
                mojo::Array<uint8_t>::From(value),
                PackSource(page_url, storage_area_id),
                base::Bind(&LocalStorageCachedArea::OnSetItemComplete,
                           weak_factory_.GetWeakPtr(), key));
  return true;
}

void LocalStorageCachedArea::RemoveItem(const base::string16& key,
                                        const GURL& page_url,
                                        const std::string& storage_area_id) {
  EnsureLoaded();
  base::string16 unused;
  if (!map_->RemoveItem(key, &unused))
    return;

  // Ignore mutations to |key| until OnRemoveItemComplete.
  ignore_key_mutations_[key]++;
  leveldb_->Delete(mojo::Array<uint8_t>::From(key),
                   PackSource(page_url, storage_area_id),
                   base::Bind(&LocalStorageCachedArea::OnRemoveItemComplete,
                              weak_factory_.GetWeakPtr(), key));
}

void LocalStorageCachedArea::Clear(const GURL& page_url,
                                   const std::string& storage_area_id) {
  // No need to prime the cache in this case.

  Reset();
  map_ = new DOMStorageMap(kPerStorageAreaQuota);
  ignore_all_mutations_ = true;
  leveldb_->DeleteAll(PackSource(page_url, storage_area_id),
                      base::Bind(&LocalStorageCachedArea::OnClearComplete,
                                  weak_factory_.GetWeakPtr()));
}

void LocalStorageCachedArea::AreaCreated(LocalStorageArea* area) {
  areas_[area->id()] = area;
}

void LocalStorageCachedArea::AreaDestroyed(LocalStorageArea* area) {
  areas_.erase(area->id());
}

void LocalStorageCachedArea::KeyAdded(mojo::Array<uint8_t> key,
                                      mojo::Array<uint8_t> value,
                                      const mojo::String& source) {
  base::NullableString16 null_value;
  KeyAddedOrChanged(std::move(key), std::move(value),
                    null_value, source);
}

void LocalStorageCachedArea::KeyChanged(mojo::Array<uint8_t> key,
                                        mojo::Array<uint8_t> new_value,
                                        mojo::Array<uint8_t> old_value,
                                        const mojo::String& source) {
  base::NullableString16 old_value_str(old_value.To<base::string16>(), false);
  KeyAddedOrChanged(std::move(key), std::move(new_value),
                    old_value_str, source);
}

void LocalStorageCachedArea::KeyDeleted(mojo::Array<uint8_t> key,
                                        mojo::Array<uint8_t> old_value,
                                        const mojo::String& source) {
  GURL page_url;
  std::string storage_area_id;
  UnpackSource(source, &page_url, &storage_area_id);

  base::string16 key_string = key.To<base::string16>();

  blink::WebStorageArea* originating_area = nullptr;
  if (areas_.find(storage_area_id) != areas_.end()) {
    // The source storage area is in this process.
    originating_area = areas_[storage_area_id];
  } else if (map_ && !ignore_all_mutations_) {
    // This was from another process or the storage area is gone. If the former,
    // remove it from our cache if we haven't already changed it and are waiting
    // for the confirmation callback. In the latter case, we won't do anything
    // because ignore_key_mutations_ won't be updated until the callback runs.
    if (ignore_key_mutations_.find(key_string) != ignore_key_mutations_.end()) {
      base::string16 unused;
      map_->RemoveItem(key_string, &unused);
    }
  }

  blink::WebStorageEventDispatcher::dispatchLocalStorageEvent(
      key_string, old_value.To<base::string16>(), base::NullableString16(),
      GURL(origin_.Serialize()), page_url, originating_area);
}

void LocalStorageCachedArea::AllDeleted(const mojo::String& source) {
  GURL page_url;
  std::string storage_area_id;
  UnpackSource(source, &page_url, &storage_area_id);

  blink::WebStorageArea* originating_area = nullptr;
  if (areas_.find(storage_area_id) != areas_.end()) {
    // The source storage area is in this process.
    originating_area = areas_[storage_area_id];
  } else if (map_ && !ignore_all_mutations_) {
    scoped_refptr<DOMStorageMap> old = map_;
    map_ = new DOMStorageMap(kPerStorageAreaQuota);

    // We have to retain local additions which happened after this clear
    // operation from another process.
    auto iter = ignore_key_mutations_.begin();
    while (iter != ignore_key_mutations_.end()) {
      base::NullableString16 value = old->GetItem(iter->first);
      if (!value.is_null()) {
        base::NullableString16 unused;
        map_->SetItem(iter->first, value.string(), &unused);
      }
      ++iter;
    }
  }

  blink::WebStorageEventDispatcher::dispatchLocalStorageEvent(
      base::NullableString16(), base::NullableString16(),
      base::NullableString16(), GURL(origin_.Serialize()), page_url,
      originating_area);
}

void LocalStorageCachedArea::GetAllComplete(const mojo::String& source) {
  // Since the GetAll method is synchronous, we need this asynchronously
  // delivered notification to avoid applying changes to the returned array
  // that we already have.
  if (source.To<std::string>() == get_all_request_id_) {
    DCHECK(ignore_all_mutations_);
    DCHECK(!get_all_request_id_.empty());
    ignore_all_mutations_ = false;
    get_all_request_id_.clear();
  }
}

void LocalStorageCachedArea::KeyAddedOrChanged(
    mojo::Array<uint8_t> key,
    mojo::Array<uint8_t> new_value,
    const base::NullableString16& old_value,
    const mojo::String& source) {
  GURL page_url;
  std::string storage_area_id;
  UnpackSource(source, &page_url, &storage_area_id);

  base::string16 key_string = key.To<base::string16>();
  base::string16 new_value_string = new_value.To<base::string16>();

  blink::WebStorageArea* originating_area = nullptr;
  if (areas_.find(storage_area_id) != areas_.end()) {
    // The source storage area is in this process.
    originating_area = areas_[storage_area_id];
  } else if (map_ && !ignore_all_mutations_) {
    // This was from another process or the storage area is gone. If the former,
    // apply it to our cache if we haven't already changed it and are waiting
    // for the confirmation callback. In the latter case, we won't do anything
    // because ignore_key_mutations_ won't be updated until the callback runs.
    if (ignore_key_mutations_.find(key_string) != ignore_key_mutations_.end()) {
      // We turn off quota checking here to accomodate the over budget allowance
      // that's provided in the browser process.
      base::NullableString16 unused;
      map_->set_quota(std::numeric_limits<int32_t>::max());
      map_->SetItem(key_string, new_value_string, &unused);
      map_->set_quota(kPerStorageAreaQuota);
    }
  }

  blink::WebStorageEventDispatcher::dispatchLocalStorageEvent(
      key_string, old_value, new_value_string,
      GURL(origin_.Serialize()), page_url, originating_area);

}

void LocalStorageCachedArea::EnsureLoaded() {
  if (map_)
    return;

  base::TimeTicks before = base::TimeTicks::Now();
  ignore_all_mutations_ = true;
  get_all_request_id_ = base::Uint64ToString(base::RandUint64());
  leveldb::mojom::DatabaseError status = leveldb::mojom::DatabaseError::OK;
  mojo::Array<content::mojom::KeyValuePtr> data;
  leveldb_->GetAll(get_all_request_id_, &status, &data);

  DOMStorageValuesMap values;
  for (size_t i = 0; i < data.size(); ++i) {
    values[data[i]->key.To<base::string16>()] =
        base::NullableString16(data[i]->value.To<base::string16>(), false);
  }

  map_ = new DOMStorageMap(kPerStorageAreaQuota);
  map_->SwapValues(&values);

  base::TimeDelta time_to_prime = base::TimeTicks::Now() - before;
  UMA_HISTOGRAM_TIMES("LocalStorage.MojoTimeToPrime", time_to_prime);

  size_t local_storage_size_kb = map_->bytes_used() / 1024;
  // Track localStorage size, from 0-6MB. Note that the maximum size should be
  // 5MB, but we add some slop since we want to make sure the max size is always
  // above what we see in practice, since histograms can't change.
  UMA_HISTOGRAM_CUSTOM_COUNTS("LocalStorage.MojoSizeInKB",
                              local_storage_size_kb,
                              0, 6 * 1024, 50);
  if (local_storage_size_kb < 100) {
    UMA_HISTOGRAM_TIMES("LocalStorage.MojoTimeToPrimeForUnder100KB",
                        time_to_prime);
  } else if (local_storage_size_kb < 1000) {
    UMA_HISTOGRAM_TIMES("LocalStorage.MojoTimeToPrimeFor100KBTo1MB",
                        time_to_prime);
  } else {
    UMA_HISTOGRAM_TIMES("LocalStorage.MojoTimeToPrimeFor1MBTo5MB",
                        time_to_prime);
  }
}

void LocalStorageCachedArea::OnSetItemComplete(const base::string16& key,
                                               bool success) {
  if (!success) {
    Reset();
    return;
  }

  auto found = ignore_key_mutations_.find(key);
  DCHECK(found != ignore_key_mutations_.end());
  if (--found->second == 0)
    ignore_key_mutations_.erase(found);
}

void LocalStorageCachedArea::OnRemoveItemComplete(
    const base::string16& key, bool success) {
  DCHECK(success);
  auto found = ignore_key_mutations_.find(key);
  DCHECK(found != ignore_key_mutations_.end());
  if (--found->second == 0)
    ignore_key_mutations_.erase(found);
}

void LocalStorageCachedArea::OnClearComplete(bool success) {
  DCHECK(success);
  DCHECK(ignore_all_mutations_);
  ignore_all_mutations_ = false;
}

void LocalStorageCachedArea::Reset() {
  map_ = NULL;
  ignore_key_mutations_.clear();
  ignore_all_mutations_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace content
