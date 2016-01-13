// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key.h"

#include <string>
#include "base/logging.h"

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
      date_(0),
      number_(0),
      size_estimate_(kOverheadSize) {}

IndexedDBKey::IndexedDBKey(WebIDBKeyType type)
    : type_(type), date_(0), number_(0), size_estimate_(kOverheadSize) {
  DCHECK(type == WebIDBKeyTypeNull || type == WebIDBKeyTypeInvalid);
}

IndexedDBKey::IndexedDBKey(double number, WebIDBKeyType type)
    : type_(type),
      date_(number),
      number_(number),
      size_estimate_(kOverheadSize + sizeof(number)) {
  DCHECK(type == WebIDBKeyTypeNumber || type == WebIDBKeyTypeDate);
}

IndexedDBKey::IndexedDBKey(const KeyArray& array)
    : type_(WebIDBKeyTypeArray),
      array_(CopyKeyArray(array)),
      date_(0),
      number_(0),
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

IndexedDBKey::~IndexedDBKey() {}

IndexedDBKey::IndexedDBKey(const IndexedDBKey& other)
    : type_(other.type_),
      array_(other.array_),
      binary_(other.binary_),
      string_(other.string_),
      date_(other.date_),
      number_(other.number_),
      size_estimate_(other.size_estimate_) {
  DCHECK((!IsValid() && !other.IsValid()) || CompareTo(other) == 0);
}

IndexedDBKey& IndexedDBKey::operator=(const IndexedDBKey& other) {
  type_ = other.type_;
  array_ = other.array_;
  binary_ = other.binary_;
  string_ = other.string_;
  date_ = other.date_;
  number_ = other.number_;
  size_estimate_ = other.size_estimate_;
  DCHECK((!IsValid() && !other.IsValid()) || CompareTo(other) == 0);
  return *this;
}

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
      return Compare(date_, other.date_);
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
