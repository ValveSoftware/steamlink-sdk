// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_WEBIDBCURSOR_IMPL_H_
#define CONTENT_CHILD_INDEXED_DB_WEBIDBCURSOR_IMPL_H_

#include <stdint.h>

#include <deque>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCursor.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBKey.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBValue.h"

namespace content {
class ThreadSafeSender;

class CONTENT_EXPORT WebIDBCursorImpl
    : NON_EXPORTED_BASE(public blink::WebIDBCursor) {
 public:
  WebIDBCursorImpl(int32_t ipc_cursor_id,
                   int64_t transaction_id,
                   ThreadSafeSender* thread_safe_sender);
  ~WebIDBCursorImpl() override;

  void advance(unsigned long count, blink::WebIDBCallbacks* callback) override;
  virtual void continueFunction(const blink::WebIDBKey& key,
                                blink::WebIDBCallbacks* callback);
  void continueFunction(const blink::WebIDBKey& key,
                        const blink::WebIDBKey& primary_key,
                        blink::WebIDBCallbacks* callback) override;
  void postSuccessHandlerCallback() override;

  void SetPrefetchData(const std::vector<IndexedDBKey>& keys,
                       const std::vector<IndexedDBKey>& primary_keys,
                       const std::vector<blink::WebIDBValue>& values);

  void CachedAdvance(unsigned long count, blink::WebIDBCallbacks* callbacks);
  void CachedContinue(blink::WebIDBCallbacks* callbacks);

  // This method is virtual so it can be overridden in unit tests.
  virtual void ResetPrefetchCache();

  int64_t transaction_id() const { return transaction_id_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorReset);
  FRIEND_TEST_ALL_PREFIXES(IndexedDBDispatcherTest, CursorTransactionId);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, AdvancePrefetchTest);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, PrefetchReset);
  FRIEND_TEST_ALL_PREFIXES(WebIDBCursorImplTest, PrefetchTest);

  enum { kInvalidCursorId = -1 };
  enum { kPrefetchContinueThreshold = 2 };
  enum { kMinPrefetchAmount = 5 };
  enum { kMaxPrefetchAmount = 100 };

  int32_t ipc_cursor_id_;
  int64_t transaction_id_;

  // Prefetch cache.
  std::deque<IndexedDBKey> prefetch_keys_;
  std::deque<IndexedDBKey> prefetch_primary_keys_;
  std::deque<blink::WebIDBValue> prefetch_values_;

  // Number of continue calls that would qualify for a pre-fetch.
  int continue_count_;

  // Number of items used from the last prefetch.
  int used_prefetches_;

  // Number of onsuccess handlers we are waiting for.
  int pending_onsuccess_callbacks_;

  // Number of items to request in next prefetch.
  int prefetch_amount_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_WEBIDBCURSOR_IMPL_H_
