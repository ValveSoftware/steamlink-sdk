// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_DOM_STORAGE_CACHED_AREA_H_
#define CONTENT_RENDERER_DOM_STORAGE_DOM_STORAGE_CACHED_AREA_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/nullable_string16.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class DOMStorageMap;
class DOMStorageProxy;

// Unlike the other classes in the dom_storage library, this one is intended
// for use in renderer processes. It maintains a complete cache of the
// origin's Map of key/value pairs for fast access. The cache is primed on
// first access and changes are written to the backend thru the |proxy|.
// Mutations originating in other processes are applied to the cache via
// the ApplyMutation method.
class CONTENT_EXPORT DOMStorageCachedArea
    : public base::RefCounted<DOMStorageCachedArea> {
 public:
  DOMStorageCachedArea(int64 namespace_id,
                       const GURL& origin,
                       DOMStorageProxy* proxy);

  int64 namespace_id() const { return namespace_id_; }
  const GURL& origin() const { return origin_; }

  unsigned GetLength(int connection_id);
  base::NullableString16 GetKey(int connection_id, unsigned index);
  base::NullableString16 GetItem(int connection_id, const base::string16& key);
  bool SetItem(int connection_id,
               const base::string16& key,
               const base::string16& value,
               const GURL& page_url);
  void RemoveItem(int connection_id,
                  const base::string16& key,
                  const GURL& page_url);
  void Clear(int connection_id, const GURL& page_url);

  void ApplyMutation(const base::NullableString16& key,
                     const base::NullableString16& new_value);

  size_t MemoryBytesUsedByCache() const;

  // Resets the object back to its newly constructed state.
  void Reset();

 private:
  friend class DOMStorageCachedAreaTest;
  friend class base::RefCounted<DOMStorageCachedArea>;
  ~DOMStorageCachedArea();

  // Primes the cache, loading all values for the area.
  void Prime(int connection_id);
  void PrimeIfNeeded(int connection_id) {
    if (!map_.get())
      Prime(connection_id);
  }

  // Async completion callbacks for proxied operations.
  // These are used to maintain cache consistency by preventing
  // mutation events from other processes from overwriting local
  // changes made after the mutation.
  void OnLoadComplete(bool success);
  void OnSetItemComplete(const base::string16& key, bool success);
  void OnClearComplete(bool success);
  void OnRemoveItemComplete(const base::string16& key, bool success);

  bool should_ignore_key_mutation(const base::string16& key) const {
    return ignore_key_mutations_.find(key) != ignore_key_mutations_.end();
  }

  bool ignore_all_mutations_;
  std::map<base::string16, int> ignore_key_mutations_;

  int64 namespace_id_;
  GURL origin_;
  scoped_refptr<DOMStorageMap> map_;
  scoped_refptr<DOMStorageProxy> proxy_;
  // Sometimes, we need to send  messages to the browser for each get access,
  // for logging purposes. However, we only do this for a fixed maximum number
  // of gets. Here, we keep track of how many remaining get log messages we
  // need to send.
  int remaining_log_get_messages_;
  base::WeakPtrFactory<DOMStorageCachedArea> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_DOM_STORAGE_CACHED_AREA_H_
