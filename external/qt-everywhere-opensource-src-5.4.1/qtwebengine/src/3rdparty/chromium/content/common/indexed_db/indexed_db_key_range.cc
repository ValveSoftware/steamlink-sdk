// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/indexed_db/indexed_db_key_range.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebIDBTypes.h"

namespace content {

IndexedDBKeyRange::IndexedDBKeyRange()
    : lower_(blink::WebIDBKeyTypeNull),
      upper_(blink::WebIDBKeyTypeNull),
      lower_open_(false),
      upper_open_(false) {}

IndexedDBKeyRange::IndexedDBKeyRange(const IndexedDBKey& lower,
                                     const IndexedDBKey& upper,
                                     bool lower_open,
                                     bool upper_open)
    : lower_(lower),
      upper_(upper),
      lower_open_(lower_open),
      upper_open_(upper_open) {}

IndexedDBKeyRange::IndexedDBKeyRange(const IndexedDBKey& key)
    : lower_(key), upper_(key), lower_open_(false), upper_open_(false) {}

IndexedDBKeyRange::~IndexedDBKeyRange() {}

bool IndexedDBKeyRange::IsOnlyKey() const {
  if (lower_open_ || upper_open_)
    return false;

  return lower_.Equals(upper_);
}

}  // namespace content
