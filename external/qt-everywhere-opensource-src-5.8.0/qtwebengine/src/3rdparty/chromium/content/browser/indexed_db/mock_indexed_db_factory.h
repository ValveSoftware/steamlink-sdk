// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_FACTORY_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockIndexedDBFactory : public IndexedDBFactory {
 public:
  MockIndexedDBFactory();
  MOCK_METHOD2(ReleaseDatabase,
               void(const IndexedDBDatabase::Identifier& identifier,
                    bool forced_close));
  MOCK_METHOD4(GetDatabaseNames,
               void(scoped_refptr<IndexedDBCallbacks> callbacks,
                    const url::Origin& origin,
                    const base::FilePath& data_directory,
                    net::URLRequestContext* request_context));
  MOCK_METHOD5(Open,
               void(const base::string16& name,
                    const IndexedDBPendingConnection& connection,
                    net::URLRequestContext* request_context,
                    const url::Origin& origin,
                    const base::FilePath& data_directory));
  MOCK_METHOD5(DeleteDatabase,
               void(const base::string16& name,
                    net::URLRequestContext* request_context,
                    scoped_refptr<IndexedDBCallbacks> callbacks,
                    const url::Origin& origin,
                    const base::FilePath& data_directory));
  MOCK_METHOD1(HandleBackingStoreFailure, void(const url::Origin& origin));
  MOCK_METHOD2(HandleBackingStoreCorruption,
               void(const url::Origin& origin,
                    const IndexedDBDatabaseError& error));
  // The Android NDK implements a subset of STL, and the gtest templates can't
  // deal with std::pair's. This means we can't use GoogleMock for this method
  OriginDBs GetOpenDatabasesForOrigin(const url::Origin& origin) const override;
  MOCK_METHOD1(ForceClose, void(const url::Origin& origin));
  MOCK_METHOD0(ContextDestroyed, void());
  MOCK_METHOD1(DatabaseDeleted,
               void(const IndexedDBDatabase::Identifier& identifier));
  MOCK_CONST_METHOD1(GetConnectionCount, size_t(const url::Origin& origin));

  MOCK_METHOD2(ReportOutstandingBlobs,
               void(const url::Origin& origin, bool blobs_outstanding));

 protected:
  virtual ~MockIndexedDBFactory();

  MOCK_METHOD7(OpenBackingStore,
               scoped_refptr<IndexedDBBackingStore>(
                   const url::Origin& origin,
                   const base::FilePath& data_directory,
                   net::URLRequestContext* request_context,
                   blink::WebIDBDataLoss* data_loss,
                   std::string* data_loss_reason,
                   bool* disk_full,
                   leveldb::Status* s));

  MOCK_METHOD8(OpenBackingStoreHelper,
               scoped_refptr<IndexedDBBackingStore>(
                   const url::Origin& origin,
                   const base::FilePath& data_directory,
                   net::URLRequestContext* request_context,
                   blink::WebIDBDataLoss* data_loss,
                   std::string* data_loss_message,
                   bool* disk_full,
                   bool first_time,
                   leveldb::Status* s));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIndexedDBFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_INDEXED_DB_FACTORY_H_
