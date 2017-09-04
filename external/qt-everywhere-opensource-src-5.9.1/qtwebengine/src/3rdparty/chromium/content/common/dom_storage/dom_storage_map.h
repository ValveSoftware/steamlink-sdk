// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DOM_STORAGE_DOM_STORAGE_MAP_H_
#define CONTENT_COMMON_DOM_STORAGE_DOM_STORAGE_MAP_H_

#include <stddef.h>

#include <map>

#include "base/memory/ref_counted.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/dom_storage/dom_storage_types.h"

namespace content {

// A wrapper around a std::map that adds refcounting and
// tracks the size in bytes of the keys/values, enforcing a quota.
// See class comments for DOMStorageContextImpl for a larger overview.
class CONTENT_EXPORT DOMStorageMap
    : public base::RefCountedThreadSafe<DOMStorageMap> {
 public:
  explicit DOMStorageMap(size_t quota);

  unsigned Length() const;
  base::NullableString16 Key(unsigned index);
  base::NullableString16 GetItem(const base::string16& key) const;
  bool SetItem(const base::string16& key, const base::string16& value,
               base::NullableString16* old_value);
  bool RemoveItem(const base::string16& key, base::string16* old_value);

  // Swaps this instances values_ with |map|.
  // Note: to grandfather in pre-existing files that are overbudget,
  // this method does not do quota checking.
  void SwapValues(DOMStorageValuesMap* map);

  // Writes a copy of the current set of values_ to the |map|.
  void ExtractValues(DOMStorageValuesMap* map) const { *map = values_; }

  // Creates a new instance of DOMStorageMap containing
  // a deep copy of values_.
  DOMStorageMap* DeepCopy() const;

  size_t bytes_used() const { return bytes_used_; }
  size_t quota() const { return quota_; }
  void set_quota(size_t quota) { quota_ = quota; }

  static size_t CountBytes(const DOMStorageValuesMap& values);

 private:
  friend class base::RefCountedThreadSafe<DOMStorageMap>;
  ~DOMStorageMap();

  void ResetKeyIterator();

  DOMStorageValuesMap values_;
  DOMStorageValuesMap::const_iterator key_iterator_;
  unsigned last_key_index_;
  size_t bytes_used_;
  size_t quota_;
};

}  // namespace content

#endif  // CONTENT_COMMON_DOM_STORAGE_DOM_STORAGE_MAP_H_
