// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_WEBIDBDATABASE_IMPL_H_
#define CONTENT_CHILD_INDEXED_DB_WEBIDBDATABASE_IMPL_H_

#include <stdint.h>

#include <set>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db.mojom.h"
#include "content/common/indexed_db/indexed_db_constants.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCursor.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {
class WebBlobInfo;
class WebIDBCallbacks;
class WebIDBObserver;
class WebString;
}

namespace content {
class ThreadSafeSender;

class CONTENT_EXPORT WebIDBDatabaseImpl
    : public NON_EXPORTED_BASE(blink::WebIDBDatabase) {
 public:
  WebIDBDatabaseImpl(indexed_db::mojom::DatabaseAssociatedPtrInfo database,
                     scoped_refptr<base::SingleThreadTaskRunner> io_runner,
                     scoped_refptr<ThreadSafeSender> thread_safe_sender);
  ~WebIDBDatabaseImpl() override;

  // blink::WebIDBDatabase
  void createObjectStore(long long transaction_id,
                         long long objectstore_id,
                         const blink::WebString& name,
                         const blink::WebIDBKeyPath& key_path,
                         bool auto_increment) override;
  void deleteObjectStore(long long transaction_id,
                         long long object_store_id) override;
  void renameObjectStore(long long transaction_id,
                         long long object_store_id,
                         const blink::WebString& new_name) override;
  void createTransaction(long long transaction_id,
                         const blink::WebVector<long long>& scope,
                         blink::WebIDBTransactionMode mode) override;

  void close() override;
  void versionChangeIgnored() override;

  int32_t addObserver(std::unique_ptr<blink::WebIDBObserver>,
                      long long transactionId) override;
  void removeObservers(
      const blink::WebVector<int32_t>& observer_ids_to_remove) override;

  void get(long long transactionId,
           long long objectStoreId,
           long long indexId,
           const blink::WebIDBKeyRange&,
           bool keyOnly,
           blink::WebIDBCallbacks*) override;
  void getAll(long long transactionId,
              long long objectStoreId,
              long long indexId,
              const blink::WebIDBKeyRange&,
              long long maxCount,
              bool keyOnly,
              blink::WebIDBCallbacks*) override;
  void put(long long transactionId,
           long long objectStoreId,
           const blink::WebData& value,
           const blink::WebVector<blink::WebBlobInfo>& webBlobInfo,
           const blink::WebIDBKey&,
           blink::WebIDBPutMode,
           blink::WebIDBCallbacks*,
           const blink::WebVector<long long>& indexIds,
           const blink::WebVector<WebIndexKeys>&) override;
  void setIndexKeys(long long transactionId,
                    long long objectStoreId,
                    const blink::WebIDBKey&,
                    const blink::WebVector<long long>& indexIds,
                    const blink::WebVector<WebIndexKeys>&) override;
  void setIndexesReady(long long transactionId,
                       long long objectStoreId,
                       const blink::WebVector<long long>& indexIds) override;
  void openCursor(long long transactionId,
                  long long objectStoreId,
                  long long indexId,
                  const blink::WebIDBKeyRange&,
                  blink::WebIDBCursorDirection direction,
                  bool keyOnly,
                  blink::WebIDBTaskType,
                  blink::WebIDBCallbacks*) override;
  void count(long long transactionId,
             long long objectStoreId,
             long long indexId,
             const blink::WebIDBKeyRange&,
             blink::WebIDBCallbacks*) override;
  void deleteRange(long long transactionId,
                   long long objectStoreId,
                   const blink::WebIDBKeyRange&,
                   blink::WebIDBCallbacks*) override;
  void clear(long long transactionId,
             long long objectStoreId,
             blink::WebIDBCallbacks*) override;
  void createIndex(long long transactionId,
                   long long objectStoreId,
                   long long indexId,
                   const blink::WebString& name,
                   const blink::WebIDBKeyPath&,
                   bool unique,
                   bool multiEntry) override;
  void deleteIndex(long long transactionId,
                   long long objectStoreId,
                   long long indexId) override;
  void renameIndex(long long transactionId,
                   long long objectStoreId,
                   long long indexId,
                   const blink::WebString& new_name) override;
  void abort(long long transaction_id) override;
  void commit(long long transaction_id) override;
  void ackReceivedBlobs(
      const blink::WebVector<blink::WebString>& uuids) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebIDBDatabaseImplTest, ValueSizeTest);
  FRIEND_TEST_ALL_PREFIXES(WebIDBDatabaseImplTest, KeyAndValueSizeTest);

  class IOThreadHelper;

  // Maximum size (in bytes) of value/key pair allowed for put requests. Any
  // requests larger than this size will be rejected.
  // Used by unit tests to exercise behavior without allocating huge chunks
  // of memory.
  size_t max_put_value_size_ = kMaxIDBMessageSizeInBytes;

  IOThreadHelper* helper_;
  std::set<int32_t> observer_ids_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_WEBIDBDATABASE_IMPL_H_
