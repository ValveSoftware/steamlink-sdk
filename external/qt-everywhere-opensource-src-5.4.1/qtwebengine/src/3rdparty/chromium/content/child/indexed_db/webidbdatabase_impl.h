// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_WEBIDBDATABASE_IMPL_H_
#define CONTENT_CHILD_INDEXED_DB_WEBIDBDATABASE_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebIDBCursor.h"
#include "third_party/WebKit/public/platform/WebIDBDatabase.h"

namespace blink {
class WebBlobInfo;
class WebIDBCallbacks;
class WebIDBDatabaseCallbacks;
class WebString;
}

namespace content {
class ThreadSafeSender;

class WebIDBDatabaseImpl : public blink::WebIDBDatabase {
 public:
  WebIDBDatabaseImpl(int32 ipc_database_id,
                     int32 ipc_database_callbacks_id,
                     ThreadSafeSender* thread_safe_sender);
  virtual ~WebIDBDatabaseImpl();

  // blink::WebIDBDatabase
  virtual void createObjectStore(long long transaction_id,
                                 long long objectstore_id,
                                 const blink::WebString& name,
                                 const blink::WebIDBKeyPath& key_path,
                                 bool auto_increment);
  virtual void deleteObjectStore(long long transaction_id,
                                 long long object_store_id);
  virtual void createTransaction(long long transaction_id,
                                 blink::WebIDBDatabaseCallbacks* callbacks,
                                 const blink::WebVector<long long>& scope,
                                 blink::WebIDBDatabase::TransactionMode mode);
  virtual void close();
  virtual void get(long long transactionId,
                   long long objectStoreId,
                   long long indexId,
                   const blink::WebIDBKeyRange&,
                   bool keyOnly,
                   blink::WebIDBCallbacks*);
  virtual void put(long long transactionId,
                   long long objectStoreId,
                   const blink::WebData& value,
                   const blink::WebVector<blink::WebBlobInfo>& webBlobInfo,
                   const blink::WebIDBKey&,
                   PutMode,
                   blink::WebIDBCallbacks*,
                   const blink::WebVector<long long>& indexIds,
                   const blink::WebVector<WebIndexKeys>&);
  virtual void setIndexKeys(long long transactionId,
                            long long objectStoreId,
                            const blink::WebIDBKey&,
                            const blink::WebVector<long long>& indexIds,
                            const blink::WebVector<WebIndexKeys>&);
  virtual void setIndexesReady(long long transactionId,
                               long long objectStoreId,
                               const blink::WebVector<long long>& indexIds);
  virtual void openCursor(long long transactionId,
                          long long objectStoreId,
                          long long indexId,
                          const blink::WebIDBKeyRange&,
                          blink::WebIDBCursor::Direction direction,
                          bool keyOnly,
                          TaskType,
                          blink::WebIDBCallbacks*);
  virtual void count(long long transactionId,
                     long long objectStoreId,
                     long long indexId,
                     const blink::WebIDBKeyRange&,
                     blink::WebIDBCallbacks*);
  virtual void deleteRange(long long transactionId,
                           long long objectStoreId,
                           const blink::WebIDBKeyRange&,
                           blink::WebIDBCallbacks*);
  virtual void clear(long long transactionId,
                     long long objectStoreId,
                     blink::WebIDBCallbacks*);
  virtual void createIndex(long long transactionId,
                           long long objectStoreId,
                           long long indexId,
                           const blink::WebString& name,
                           const blink::WebIDBKeyPath&,
                           bool unique,
                           bool multiEntry);
  virtual void deleteIndex(long long transactionId,
                           long long objectStoreId,
                           long long indexId);
  virtual void abort(long long transaction_id);
  virtual void commit(long long transaction_id);
  virtual void ackReceivedBlobs(
      const blink::WebVector<blink::WebString>& uuids);

 private:
  int32 ipc_database_id_;
  int32 ipc_database_callbacks_id_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_WEBIDBDATABASE_IMPL_H_
