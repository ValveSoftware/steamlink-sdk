// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockWebIDBDatabase_h
#define MockWebIDBDatabase_h

#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBKeyRange.h"
#include <gmock/gmock.h>
#include <memory>

namespace blink {

class MockWebIDBDatabase : public testing::StrictMock<WebIDBDatabase> {
public:
    virtual ~MockWebIDBDatabase();

    static std::unique_ptr<MockWebIDBDatabase> create();

    MOCK_METHOD5(createObjectStore, void(long long transactionId, long long objectStoreId, const WebString& name, const WebIDBKeyPath&, bool autoIncrement));
    MOCK_METHOD2(deleteObjectStore, void(long long transactionId, long long objectStoreId));
    MOCK_METHOD4(createTransaction, void(long long id, WebIDBDatabaseCallbacks*, const WebVector<long long>& scope, WebIDBTransactionMode));
    MOCK_METHOD0(close, void());
    MOCK_METHOD0(versionChangeIgnored, void());
    MOCK_METHOD1(abort, void(long long transactionId));
    MOCK_METHOD1(commit, void(long long transactionId));
    MOCK_METHOD7(createIndex, void(long long transactionId, long long objectStoreId, long long indexId, const WebString& name, const WebIDBKeyPath&, bool unique, bool multiEntry));
    MOCK_METHOD3(deleteIndex, void(long long transactionId, long long objectStoreId, long long indexId));
    MOCK_METHOD6(get, void(long long transactionId, long long objectStoreId, long long indexId, const WebIDBKeyRange&, bool keyOnly, WebIDBCallbacks*));
    MOCK_METHOD7(getAll, void(long long transactionId, long long objectStoreId, long long indexId, const WebIDBKeyRange&, long long maxCount, bool keyOnly, WebIDBCallbacks*));
    MOCK_METHOD9(put, void(long long transactionId, long long objectStoreId, const WebData& value, const WebVector<WebBlobInfo>&, const WebIDBKey&, WebIDBPutMode, WebIDBCallbacks*, const WebVector<long long>& indexIds, const WebVector<WebIndexKeys>&));
    MOCK_METHOD5(setIndexKeys, void(long long transactionId, long long objectStoreId, const WebIDBKey&, const WebVector<long long>& indexIds, const WebVector<WebIndexKeys>&));
    MOCK_METHOD3(setIndexesReady, void(long long transactionId, long long objectStoreId, const WebVector<long long>& indexIds));
    MOCK_METHOD8(openCursor, void(long long transactionId, long long objectStoreId, long long indexId, const WebIDBKeyRange&, WebIDBCursorDirection, bool keyOnly, WebIDBTaskType, WebIDBCallbacks*));
    MOCK_METHOD5(count, void(long long transactionId, long long objectStoreId, long long indexId, const WebIDBKeyRange&, WebIDBCallbacks*));
    MOCK_METHOD4(deleteRange, void(long long transactionId, long long objectStoreId, const WebIDBKeyRange&, WebIDBCallbacks*));
    MOCK_METHOD3(clear, void(long long transactionId, long long objectStoreId, WebIDBCallbacks*));
    MOCK_METHOD1(ackReceivedBlobs, void(const WebVector<WebString>& uuids));

private:
    MockWebIDBDatabase();
};

} // namespace blink

#endif // MockWebIDBDatabase_h
