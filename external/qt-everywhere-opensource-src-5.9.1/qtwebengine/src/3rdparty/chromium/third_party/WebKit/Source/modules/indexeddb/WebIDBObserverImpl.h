// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebIDBObserverImpl_h
#define WebIDBObserverImpl_h

#include <bitset>

#include "modules/indexeddb/IDBObserver.h"
#include "platform/heap/Persistent.h"
#include "public/platform/modules/indexeddb/WebIDBObserver.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {

class IDBObserver;
struct WebIDBObservation;

class WebIDBObserverImpl final : public WebIDBObserver {
  USING_FAST_MALLOC(WebIDBObserverImpl);

 public:
  static std::unique_ptr<WebIDBObserverImpl> create(
      IDBObserver*,
      bool transaction,
      bool values,
      bool noRecords,
      std::bitset<WebIDBOperationTypeCount> operationTypes);

  ~WebIDBObserverImpl() override;

  void setId(int32_t);

  bool transaction() const { return m_transaction; }
  bool noRecords() const { return m_noRecords; }
  bool values() const { return m_values; }
  const std::bitset<WebIDBOperationTypeCount>& operationTypes() const {
    return m_operationTypes;
  }

  void onChange(const WebVector<WebIDBObservation>&,
                const WebVector<int32_t>& observationIndex);

 private:
  enum { kInvalidObserverId = -1 };

  WebIDBObserverImpl(IDBObserver*,
                     bool transaction,
                     bool values,
                     bool noRecords,
                     std::bitset<WebIDBOperationTypeCount> operationTypes);

  int32_t m_id;
  bool m_transaction;
  bool m_values;
  bool m_noRecords;
  // Operation type bits are set corresponding to WebIDBOperationType.
  std::bitset<WebIDBOperationTypeCount> m_operationTypes;
  Persistent<IDBObserver> m_observer;
};

}  // namespace blink

#endif  // WebIDBObserverImpl_h
