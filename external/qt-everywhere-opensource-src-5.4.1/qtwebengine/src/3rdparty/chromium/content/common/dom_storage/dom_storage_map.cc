// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/dom_storage/dom_storage_map.h"

#include "base/logging.h"

namespace content {

namespace {

size_t size_of_item(const base::string16& key, const base::string16& value) {
  return (key.length() + value.length()) * sizeof(base::char16);
}

size_t CountBytes(const DOMStorageValuesMap& values) {
  if (values.size() == 0)
    return 0;

  size_t count = 0;
  DOMStorageValuesMap::const_iterator it = values.begin();
  for (; it != values.end(); ++it)
    count += size_of_item(it->first, it->second.string());
  return count;
}

}  // namespace

DOMStorageMap::DOMStorageMap(size_t quota)
    : bytes_used_(0),
      quota_(quota) {
  ResetKeyIterator();
}

DOMStorageMap::~DOMStorageMap() {}

unsigned DOMStorageMap::Length() const {
  return values_.size();
}

base::NullableString16 DOMStorageMap::Key(unsigned index) {
  if (index >= values_.size())
    return base::NullableString16();
  while (last_key_index_ != index) {
    if (last_key_index_ > index) {
      --key_iterator_;
      --last_key_index_;
    } else {
      ++key_iterator_;
      ++last_key_index_;
    }
  }
  return base::NullableString16(key_iterator_->first, false);
}

base::NullableString16 DOMStorageMap::GetItem(const base::string16& key) const {
  DOMStorageValuesMap::const_iterator found = values_.find(key);
  if (found == values_.end())
    return base::NullableString16();
  return found->second;
}

bool DOMStorageMap::SetItem(
    const base::string16& key, const base::string16& value,
    base::NullableString16* old_value) {
  DOMStorageValuesMap::const_iterator found = values_.find(key);
  if (found == values_.end())
    *old_value = base::NullableString16();
  else
    *old_value = found->second;

  size_t old_item_size = old_value->is_null() ?
      0 : size_of_item(key, old_value->string());
  size_t new_item_size = size_of_item(key, value);
  size_t new_bytes_used = bytes_used_ - old_item_size + new_item_size;

  // Only check quota if the size is increasing, this allows
  // shrinking changes to pre-existing files that are over budget.
  if (new_item_size > old_item_size && new_bytes_used > quota_)
    return false;

  values_[key] = base::NullableString16(value, false);
  ResetKeyIterator();
  bytes_used_ = new_bytes_used;
  return true;
}

bool DOMStorageMap::RemoveItem(
    const base::string16& key,
    base::string16* old_value) {
  DOMStorageValuesMap::iterator found = values_.find(key);
  if (found == values_.end())
    return false;
  *old_value = found->second.string();
  values_.erase(found);
  ResetKeyIterator();
  bytes_used_ -= size_of_item(key, *old_value);
  return true;
}

void DOMStorageMap::SwapValues(DOMStorageValuesMap* values) {
  // Note: A pre-existing file may be over the quota budget.
  values_.swap(*values);
  bytes_used_ = CountBytes(values_);
  ResetKeyIterator();
}

DOMStorageMap* DOMStorageMap::DeepCopy() const {
  DOMStorageMap* copy = new DOMStorageMap(quota_);
  copy->values_ = values_;
  copy->bytes_used_ = bytes_used_;
  copy->ResetKeyIterator();
  return copy;
}

void DOMStorageMap::ResetKeyIterator() {
  key_iterator_ = values_.begin();
  last_key_index_ = 0;
}

}  // namespace content
