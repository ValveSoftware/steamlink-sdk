// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key_path.h"

#include "base/logging.h"

namespace content {

using blink::WebIDBKeyPathTypeArray;
using blink::WebIDBKeyPathTypeNull;
using blink::WebIDBKeyPathTypeString;

IndexedDBKeyPath::IndexedDBKeyPath() : type_(WebIDBKeyPathTypeNull) {}

IndexedDBKeyPath::IndexedDBKeyPath(const base::string16& string)
    : type_(WebIDBKeyPathTypeString), string_(string) {}

IndexedDBKeyPath::IndexedDBKeyPath(const std::vector<base::string16>& array)
    : type_(WebIDBKeyPathTypeArray), array_(array) {}

IndexedDBKeyPath::IndexedDBKeyPath(const IndexedDBKeyPath& other) = default;
IndexedDBKeyPath::~IndexedDBKeyPath() = default;
IndexedDBKeyPath& IndexedDBKeyPath::operator=(const IndexedDBKeyPath& other) =
    default;

const std::vector<base::string16>& IndexedDBKeyPath::array() const {
  DCHECK(type_ == blink::WebIDBKeyPathTypeArray);
  return array_;
}

const base::string16& IndexedDBKeyPath::string() const {
  DCHECK(type_ == blink::WebIDBKeyPathTypeString);
  return string_;
}

bool IndexedDBKeyPath::operator==(const IndexedDBKeyPath& other) const {
  if (type_ != other.type_)
    return false;

  switch (type_) {
    case WebIDBKeyPathTypeNull:
      return true;
    case WebIDBKeyPathTypeString:
      return string_ == other.string_;
    case WebIDBKeyPathTypeArray:
      return array_ == other.array_;
  }
  NOTREACHED();
  return false;
}

}  // namespace content
