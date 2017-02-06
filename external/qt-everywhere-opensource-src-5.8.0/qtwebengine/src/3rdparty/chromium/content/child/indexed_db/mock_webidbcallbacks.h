// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_MOCK_WEBIDBCALLBACKS_H_
#define CONTENT_CHILD_INDEXED_DB_MOCK_WEBIDBCALLBACKS_H_

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebBlobInfo.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseError.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBMetadata.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBValue.h"
#include "third_party/WebKit/public/web/WebHeap.h"

namespace blink {
class WebData;
}  // namespace blink

namespace content {

class MockWebIDBCallbacks : public blink::WebIDBCallbacks {
 public:
  MockWebIDBCallbacks();
  ~MockWebIDBCallbacks();
  MOCK_METHOD1(onError, void(const blink::WebIDBDatabaseError&));
  MOCK_METHOD3(onSuccess,
               void(const blink::WebIDBKey& key,
                    const blink::WebIDBKey& primaryKey,
                    const blink::WebIDBValue& value));
  MOCK_METHOD1(onSuccess, void(const blink::WebVector<blink::WebString>&));
  MOCK_METHOD4(onSuccess,
               void(blink::WebIDBCursor*,
                    const blink::WebIDBKey&,
                    const blink::WebIDBKey& primaryKey,
                    const blink::WebIDBValue&));
  MOCK_METHOD2(onSuccess,
               void(blink::WebIDBDatabase*, const blink::WebIDBMetadata&));
  MOCK_METHOD1(onSuccess, void(const blink::WebIDBKey&));
  MOCK_METHOD1(onSuccess, void(const blink::WebIDBValue&));
  MOCK_METHOD1(onSuccess, void(const blink::WebVector<blink::WebIDBValue>&));
  MOCK_METHOD1(onSuccess, void(long long));
  MOCK_METHOD0(onSuccess, void());
  MOCK_METHOD1(onBlocked, void(long long oldVersion));
  MOCK_METHOD5(onUpgradeNeeded,
               void(long long oldVersion,
                    blink::WebIDBDatabase*,
                    const blink::WebIDBMetadata&,
                    unsigned short dataLoss,
                    blink::WebString dataLossMessage));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebIDBCallbacks);
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_MOCK_WEBIDBCALLBACKS_H_
