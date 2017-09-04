// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class makes the blink::IDBObserver visible to the content layer by
// holding a reference to it.

#ifndef WebIDBObserver_h
#define WebIDBObserver_h

#include "public/platform/WebVector.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include <bitset>

namespace blink {

struct WebIDBObservation;

class WebIDBObserver {
 public:
  virtual ~WebIDBObserver() {}

  virtual bool transaction() const = 0;
  virtual bool noRecords() const = 0;
  virtual bool values() const = 0;
  virtual const std::bitset<WebIDBOperationTypeCount>& operationTypes()
      const = 0;
  virtual void onChange(const WebVector<WebIDBObservation>&,
                        const WebVector<int32_t>& observationIndex) = 0;
};

}  // namespace blink

#endif  // WebIDBObserver_h
