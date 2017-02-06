// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_key_builders.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using blink::WebIDBKey;
using blink::WebIDBKeyRange;
using blink::WebIDBKeyTypeArray;
using blink::WebIDBKeyTypeBinary;
using blink::WebIDBKeyTypeDate;
using blink::WebIDBKeyTypeInvalid;
using blink::WebIDBKeyTypeMin;
using blink::WebIDBKeyTypeNull;
using blink::WebIDBKeyTypeNumber;
using blink::WebIDBKeyTypeString;
using blink::WebVector;
using blink::WebString;

static content::IndexedDBKey::KeyArray CopyKeyArray(const WebIDBKey& other) {
  content::IndexedDBKey::KeyArray result;
  if (other.keyType() == WebIDBKeyTypeArray) {
    const WebVector<WebIDBKey>& array = other.array();
    for (size_t i = 0; i < array.size(); ++i)
      result.push_back(content::IndexedDBKeyBuilder::Build(array[i]));
  }
  return result;
}

static std::vector<base::string16> CopyArray(
    const WebVector<WebString>& array) {
  std::vector<base::string16> copy(array.size());
  for (size_t i = 0; i < array.size(); ++i)
    copy[i] = array[i];
  return copy;
}


namespace content {

IndexedDBKey IndexedDBKeyBuilder::Build(const blink::WebIDBKey& key) {
  switch (key.keyType()) {
    case WebIDBKeyTypeArray:
      return IndexedDBKey(CopyKeyArray(key));
    case WebIDBKeyTypeBinary:
      return IndexedDBKey(
          std::string(key.binary().data(), key.binary().size()));
    case WebIDBKeyTypeString:
      return IndexedDBKey(key.string());
    case WebIDBKeyTypeDate:
      return IndexedDBKey(key.date(), WebIDBKeyTypeDate);
    case WebIDBKeyTypeNumber:
      return IndexedDBKey(key.number(), WebIDBKeyTypeNumber);
    case WebIDBKeyTypeNull:
    case WebIDBKeyTypeInvalid:
      return IndexedDBKey(key.keyType());
    case WebIDBKeyTypeMin:
    default:
      NOTREACHED();
      return IndexedDBKey();
  }
}

WebIDBKey WebIDBKeyBuilder::Build(const IndexedDBKey& key) {
  switch (key.type()) {
    case WebIDBKeyTypeArray: {
      const IndexedDBKey::KeyArray& array = key.array();
      blink::WebVector<WebIDBKey> web_array(array.size());
      for (size_t i = 0; i < array.size(); ++i) {
        web_array[i] = Build(array[i]);
      }
      return WebIDBKey::createArray(web_array);
    }
    case WebIDBKeyTypeBinary:
      return WebIDBKey::createBinary(key.binary());
    case WebIDBKeyTypeString:
      return WebIDBKey::createString(key.string());
    case WebIDBKeyTypeDate:
      return WebIDBKey::createDate(key.date());
    case WebIDBKeyTypeNumber:
      return WebIDBKey::createNumber(key.number());
    case WebIDBKeyTypeInvalid:
      return WebIDBKey::createInvalid();
    case WebIDBKeyTypeNull:
      return WebIDBKey::createNull();
    case WebIDBKeyTypeMin:
    default:
      NOTREACHED();
      return WebIDBKey::createInvalid();
  }
}

IndexedDBKeyRange IndexedDBKeyRangeBuilder::Build(
    const WebIDBKeyRange& key_range) {
  return IndexedDBKeyRange(
    IndexedDBKeyBuilder::Build(key_range.lower()),
    IndexedDBKeyBuilder::Build(key_range.upper()),
    key_range.lowerOpen(),
    key_range.upperOpen());
}

IndexedDBKeyPath IndexedDBKeyPathBuilder::Build(
    const blink::WebIDBKeyPath& key_path) {
  switch (key_path.keyPathType()) {
    case blink::WebIDBKeyPathTypeString:
      return IndexedDBKeyPath(key_path.string());
    case blink::WebIDBKeyPathTypeArray:
      return IndexedDBKeyPath(CopyArray(key_path.array()));
    case blink::WebIDBKeyPathTypeNull:
      return IndexedDBKeyPath();
    default:
      NOTREACHED();
      return IndexedDBKeyPath();
  }
}

blink::WebIDBKeyPath WebIDBKeyPathBuilder::Build(
    const IndexedDBKeyPath& key_path) {
  switch (key_path.type()) {
    case blink::WebIDBKeyPathTypeString:
      return blink::WebIDBKeyPath::create(WebString(key_path.string()));
    case blink::WebIDBKeyPathTypeArray:
      return blink::WebIDBKeyPath::create(CopyArray(key_path.array()));
    case blink::WebIDBKeyPathTypeNull:
      return blink::WebIDBKeyPath::createNull();
    default:
      NOTREACHED();
      return blink::WebIDBKeyPath::createNull();
  }
}

}  // namespace content
