// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <bitset>
#include <set>

#include "base/macros.h"
#include "base/stl_util.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

namespace content {

class CONTENT_EXPORT IndexedDBObserver {
 public:
  struct CONTENT_EXPORT Options {
    explicit Options(bool include_transaction,
                     bool no_records,
                     bool values,
                     unsigned short types);
    Options(const Options&);
    ~Options();

    bool include_transaction;
    bool no_records;
    bool values;
    // Operation type bits are set corresponding to WebIDBOperationType.
    std::bitset<blink::WebIDBOperationTypeCount> operation_types;
  };
  IndexedDBObserver(int32_t observer_id,
                    std::set<int64_t> object_store_ids,
                    const Options& options);
  ~IndexedDBObserver();

  int32_t id() const { return id_; }
  const std::set<int64_t>& object_store_ids() const {
    return object_store_ids_;
  }

  bool IsRecordingType(blink::WebIDBOperationType type) const {
    DCHECK_NE(type, blink::WebIDBOperationTypeCount);
    return options_.operation_types[type];
  }
  bool IsRecordingObjectStore(int64_t object_store_id) const {
    return base::ContainsValue(object_store_ids_, object_store_id);
  }
  bool include_transaction() const { return options_.include_transaction; }
  bool no_records() const { return options_.no_records; }
  bool values() const { return options_.values; }

 private:
  int32_t id_;
  std::set<int64_t> object_store_ids_;
  Options options_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVER_H_
