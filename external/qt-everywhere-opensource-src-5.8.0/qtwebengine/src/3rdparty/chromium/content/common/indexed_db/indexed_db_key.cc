// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key.h"

#include <string>

namespace content {

using blink::WebIDBKey;
using blink::WebIDBKeyType;
using blink::WebIDBKeyTypeArray;
using blink::WebIDBKeyTypeBinary;
using blink::WebIDBKeyTypeDate;
using blink::WebIDBKeyTypeInvalid;
using blink::WebIDBKeyTypeMin;
using blink::WebIDBKeyTypeNull;
using blink::WebIDBKeyTypeNumber;
using blink::WebIDBKeyTypeString;

namespace {

// Very rough estimate of minimum key size overhead.
const size_t kOverheadSize = 16;

static size_t CalculateArraySize(const IndexedDBKey::KeyArray& keys) {
  size_t size(0);
  for (size_t i = 0; i < keys.size(); ++i)
    size += keys[i].size_estimate();
  return size;
}

template<typename T>
int Compare(const T& a, const T& b) {
  // Using '<' for both comparisons here is as generic as possible (for e.g.
  // objects which only define operator<() and not operator>() or operator==())
  // and also allows e.g. floating point NaNs to compare equal.
  if (a < b)
    return -1;
  return (b < a) ? 1 : 0;
}

template <typename T>
static IndexedDBKey::KeyArray CopyKeyArray(const T& array) {
  IndexedDBKey::KeyArray result;
  result.reserve(array.size());
  for (size_t i = 0; i < array.size(); ++i) {
    result.push_back(IndexedDBKey(array[i]));
  }
  return result;
}

}  // namespace

IndexedDBKey::IndexedDBKey()
    : type_(WebIDBKeyTypeNull),
      size_estimate_(kOverheadSize) {}

IndexedDBKey::IndexedDBKey(WebIDBKeyType type)
    : type_(type), size_estimate_(kOverheadSize) {
  DCHECK(type == WebIDBKeyTypeNull || type == WebIDBKeyTypeInvalid);
}

IndexedDBKey::IndexedDBKey(double number, WebIDBKeyType type)
    : type_(type),
      number_(number),
      size_estimate_(kOverheadSize + sizeof(number)) {
  DCHECK(type == WebIDBKeyTypeNumber || type == WebIDBKeyTypeDate);
}

IndexedDBKey::IndexedDBKey(const KeyArray& array)
    : type_(WebIDBKeyTypeArray),
      array_(CopyKeyArray(array)),
      size_estimate_(kOverheadSize + CalculateArraySize(array)) {}

IndexedDBKey::IndexedDBKey(const std::string& binary)
    : type_(WebIDBKeyTypeBinary),
      binary_(binary),
      size_estimate_(kOverheadSize +
                     (binary.length() * sizeof(std::string::value_type))) {}

IndexedDBKey::IndexedDBKey(const base::string16& string)
    : type_(WebIDBKeyTypeString),
      string_(string),
      size_estimate_(kOverheadSize +
                     (string.length() * sizeof(base::string16::value_type))) {}

IndexedDBKey::IndexedDBKey(const IndexedDBKey& other) = default;
IndexedDBKey::~IndexedDBKey() = default;
IndexedDBKey& IndexedDBKey::operator=(const IndexedDBKey& other) = default;

bool IndexedDBKey::IsValid() const {
  if (type_ == WebIDBKeyTypeInvalid || type_ == WebIDBKeyTypeNull)
    return false;

  if (type_ == WebIDBKeyTypeArray) {
    for (size_t i = 0; i < array_.size(); i++) {
      if (!array_[i].IsValid())
        return false;
    }
  }

  return true;
}

bool IndexedDBKey::IsLessThan(const IndexedDBKey& other) const {
  return CompareTo(other) < 0;
}

bool IndexedDBKey::Equals(const IndexedDBKey& other) const {
  return !CompareTo(other);
}

int IndexedDBKey::CompareTo(const IndexedDBKey& other) const {
  DCHECK(IsValid());
  DCHECK(other.IsValid());
  if (type_ != other.type_)
    return type_ > other.type_ ? -1 : 1;

  switch (type_) {
    case WebIDBKeyTypeArray:
      for (size_t i = 0; i < array_.size() && i < other.array_.size(); ++i) {
        int result = array_[i].CompareTo(other.array_[i]);
        if (result != 0)
          return result;
      }
      return Compare(array_.size(), other.array_.size());
    case WebIDBKeyTypeBinary:
      return binary_.compare(other.binary_);
    case WebIDBKeyTypeString:
      return string_.compare(other.string_);
    case WebIDBKeyTypeDate:
    case WebIDBKeyTypeNumber:
      return Compare(number_, other.number_);
    case WebIDBKeyTypeInvalid:
    case WebIDBKeyTypeNull:
    case WebIDBKeyTypeMin:
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace content
