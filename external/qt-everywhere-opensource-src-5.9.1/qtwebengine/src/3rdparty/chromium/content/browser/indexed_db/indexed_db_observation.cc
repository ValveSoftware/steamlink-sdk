// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_observation.h"

namespace content {

IndexedDBObservation::IndexedDBObservation(int32_t object_store_id,
                                           OperationType type)
    : object_store_id_(object_store_id), type_(type) {}

IndexedDBObservation::IndexedDBObservation(int32_t object_store_id,
                                           OperationType type,
                                           IndexedDBKeyRange key_range)
    : object_store_id_(object_store_id), type_(type), key_range_(key_range) {
  DCHECK_NE(type_, blink::WebIDBClear);
}

IndexedDBObservation::IndexedDBObservation(int32_t object_store_id,
                                           OperationType type,
                                           IndexedDBKeyRange key_range,
                                           IndexedDBValue value)
    : object_store_id_(object_store_id),
      type_(type),
      key_range_(key_range),
      value_(value) {
  DCHECK(type_ == blink::WebIDBAdd || type_ == blink::WebIDBPut);
}

IndexedDBObservation::~IndexedDBObservation() {}

}  //  namespace content
