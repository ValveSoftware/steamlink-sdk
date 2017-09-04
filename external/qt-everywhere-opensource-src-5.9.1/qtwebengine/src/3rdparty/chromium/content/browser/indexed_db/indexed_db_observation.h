// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVATION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVATION_H_

#include "base/macros.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

namespace content {

// Represents a single observed operation (put/add/delete/clear) of a
// transaction.
class IndexedDBObservation {
 public:
  using OperationType = blink::WebIDBOperationType;

  IndexedDBObservation(int32_t object_store_id, OperationType type);
  IndexedDBObservation(int32_t object_store_id,
                       OperationType type,
                       IndexedDBKeyRange key_range);
  IndexedDBObservation(int32_t object_store_id,
                       OperationType type,
                       IndexedDBKeyRange key_range,
                       IndexedDBValue value);

  ~IndexedDBObservation();

  int64_t object_store_id() const { return object_store_id_; }
  OperationType type() const { return type_; }
  const IndexedDBKeyRange key_range() const { return key_range_; }
  const IndexedDBValue value() const { return value_; }

 private:
  int64_t object_store_id_;
  OperationType type_;
  // Populated for ADD, PUT, and DELETE operations.
  IndexedDBKeyRange key_range_;
  // Populated for ADD and PUT operations if values are requested.
  IndexedDBValue value_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBObservation);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVATION_H_
