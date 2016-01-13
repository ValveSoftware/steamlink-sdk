// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_key_builders.h"
#include "content/child/indexed_db/webidbcursor_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "ipc/ipc_sync_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebIDBCallbacks.h"

using blink::WebBlobInfo;
using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBKey;
using blink::WebIDBKeyTypeNumber;
using blink::WebVector;

namespace content {

namespace {

class MockDispatcher : public IndexedDBDispatcher {
 public:
  explicit MockDispatcher(ThreadSafeSender* thread_safe_sender)
      : IndexedDBDispatcher(thread_safe_sender),
        prefetch_calls_(0),
        last_prefetch_count_(0),
        reset_calls_(0),
        last_used_count_(0),
        advance_calls_(0),
        continue_calls_(0),
        destroyed_cursor_id_(0) {}

  virtual void RequestIDBCursorPrefetch(int n,
                                        WebIDBCallbacks* callbacks,
                                        int32 ipc_cursor_id) OVERRIDE {
    ++prefetch_calls_;
    last_prefetch_count_ = n;
    callbacks_.reset(callbacks);
  }

  virtual void RequestIDBCursorPrefetchReset(int used_prefetches,
                                             int unused_prefetches,
                                             int32 ipc_cursor_id) OVERRIDE {
    ++reset_calls_;
    last_used_count_ = used_prefetches;
  }

  virtual void RequestIDBCursorAdvance(unsigned long count,
                                       WebIDBCallbacks* callbacks,
                                       int32 ipc_cursor_id,
                                       int64 transaction_id) OVERRIDE {
    ++advance_calls_;
    callbacks_.reset(callbacks);
  }

  virtual void RequestIDBCursorContinue(const IndexedDBKey& key,
                                        const IndexedDBKey& primary_key,
                                        WebIDBCallbacks* callbacks,
                                        int32 ipc_cursor_id,
                                        int64 transaction_id) OVERRIDE {
    ++continue_calls_;
    callbacks_.reset(callbacks);
  }

  virtual void CursorDestroyed(int32 ipc_cursor_id) OVERRIDE {
    destroyed_cursor_id_ = ipc_cursor_id;
  }

  int prefetch_calls() { return prefetch_calls_; }
  int last_prefetch_count() { return last_prefetch_count_; }
  int reset_calls() { return reset_calls_; }
  int last_used_count() { return last_used_count_; }
  int advance_calls() { return advance_calls_; }
  int continue_calls() { return continue_calls_; }
  int32 destroyed_cursor_id() { return destroyed_cursor_id_; }

 private:
  int prefetch_calls_;
  int last_prefetch_count_;
  int reset_calls_;
  int last_used_count_;
  int advance_calls_;
  int continue_calls_;
  int32 destroyed_cursor_id_;
  scoped_ptr<WebIDBCallbacks> callbacks_;
};

class MockContinueCallbacks : public WebIDBCallbacks {
 public:
  MockContinueCallbacks(IndexedDBKey* key = 0,
                        WebVector<WebBlobInfo>* webBlobInfo = 0)
      : key_(key), webBlobInfo_(webBlobInfo) {}

  virtual void onSuccess(const WebIDBKey& key,
                         const WebIDBKey& primaryKey,
                         const WebData& value,
                         const WebVector<WebBlobInfo>& webBlobInfo) OVERRIDE {
    if (key_)
      *key_ = IndexedDBKeyBuilder::Build(key);
    if (webBlobInfo_)
      *webBlobInfo_ = webBlobInfo;
  }

 private:
  IndexedDBKey* key_;
  WebVector<WebBlobInfo>* webBlobInfo_;
};

}  // namespace

class WebIDBCursorImplTest : public testing::Test {
 public:
  WebIDBCursorImplTest() {
    null_key_.assignNull();
    sync_message_filter_ = new IPC::SyncMessageFilter(NULL);
    thread_safe_sender_ = new ThreadSafeSender(
        base::MessageLoopProxy::current(), sync_message_filter_.get());
    dispatcher_ =
        make_scoped_ptr(new MockDispatcher(thread_safe_sender_.get()));
  }

 protected:
  WebIDBKey null_key_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_ptr<MockDispatcher> dispatcher_;

 private:
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(WebIDBCursorImplTest);
};

TEST_F(WebIDBCursorImplTest, PrefetchTest) {
  const int64 transaction_id = 1;
  {
    WebIDBCursorImpl cursor(WebIDBCursorImpl::kInvalidCursorId,
                            transaction_id,
                            thread_safe_sender_.get());

    // Call continue() until prefetching should kick in.
    int continue_calls = 0;
    EXPECT_EQ(dispatcher_->continue_calls(), 0);
    for (int i = 0; i < WebIDBCursorImpl::kPrefetchContinueThreshold; ++i) {
      cursor.continueFunction(null_key_, new MockContinueCallbacks());
      EXPECT_EQ(++continue_calls, dispatcher_->continue_calls());
      EXPECT_EQ(0, dispatcher_->prefetch_calls());
    }

    // Do enough repetitions to verify that the count grows each time,
    // but not so many that the maximum limit is hit.
    const int kPrefetchRepetitions = 5;

    int expected_key = 0;
    int last_prefetch_count = 0;
    for (int repetitions = 0; repetitions < kPrefetchRepetitions;
         ++repetitions) {
      // Initiate the prefetch
      cursor.continueFunction(null_key_, new MockContinueCallbacks());
      EXPECT_EQ(continue_calls, dispatcher_->continue_calls());
      EXPECT_EQ(repetitions + 1, dispatcher_->prefetch_calls());

      // Verify that the requested count has increased since last time.
      int prefetch_count = dispatcher_->last_prefetch_count();
      EXPECT_GT(prefetch_count, last_prefetch_count);
      last_prefetch_count = prefetch_count;

      // Fill the prefetch cache as requested.
      std::vector<IndexedDBKey> keys;
      std::vector<IndexedDBKey> primary_keys(prefetch_count);
      std::vector<WebData> values(prefetch_count);
      std::vector<WebVector<WebBlobInfo> > blob_info;
      for (int i = 0; i < prefetch_count; ++i) {
        keys.push_back(IndexedDBKey(expected_key + i, WebIDBKeyTypeNumber));
        blob_info.push_back(
            WebVector<WebBlobInfo>(static_cast<size_t>(expected_key + i)));
      }
      cursor.SetPrefetchData(keys, primary_keys, values, blob_info);

      // Note that the real dispatcher would call cursor->CachedContinue()
      // immediately after cursor->SetPrefetchData() to service the request
      // that initiated the prefetch.

      // Verify that the cache is used for subsequent continue() calls.
      for (int i = 0; i < prefetch_count; ++i) {
        IndexedDBKey key;
        WebVector<WebBlobInfo> web_blob_info;
        cursor.continueFunction(
            null_key_, new MockContinueCallbacks(&key, &web_blob_info));
        EXPECT_EQ(continue_calls, dispatcher_->continue_calls());
        EXPECT_EQ(repetitions + 1, dispatcher_->prefetch_calls());

        EXPECT_EQ(WebIDBKeyTypeNumber, key.type());
        EXPECT_EQ(expected_key, static_cast<int>(web_blob_info.size()));
        EXPECT_EQ(expected_key++, key.number());
      }
    }
  }

  EXPECT_EQ(dispatcher_->destroyed_cursor_id(),
            WebIDBCursorImpl::kInvalidCursorId);
}

TEST_F(WebIDBCursorImplTest, AdvancePrefetchTest) {
  const int64 transaction_id = 1;
  WebIDBCursorImpl cursor(WebIDBCursorImpl::kInvalidCursorId,
                          transaction_id,
                          thread_safe_sender_.get());

  // Call continue() until prefetching should kick in.
  EXPECT_EQ(0, dispatcher_->continue_calls());
  for (int i = 0; i < WebIDBCursorImpl::kPrefetchContinueThreshold; ++i) {
    cursor.continueFunction(null_key_, new MockContinueCallbacks());
  }
  EXPECT_EQ(0, dispatcher_->prefetch_calls());

  // Initiate the prefetch
  cursor.continueFunction(null_key_, new MockContinueCallbacks());

  EXPECT_EQ(1, dispatcher_->prefetch_calls());
  EXPECT_EQ(static_cast<int>(WebIDBCursorImpl::kPrefetchContinueThreshold),
            dispatcher_->continue_calls());
  EXPECT_EQ(0, dispatcher_->advance_calls());

  const int prefetch_count = dispatcher_->last_prefetch_count();

  // Fill the prefetch cache as requested.
  int expected_key = 0;
  std::vector<IndexedDBKey> keys;
  std::vector<IndexedDBKey> primary_keys(prefetch_count);
  std::vector<WebData> values(prefetch_count);
  std::vector<WebVector<WebBlobInfo> > blob_info;
  for (int i = 0; i < prefetch_count; ++i) {
    keys.push_back(IndexedDBKey(expected_key + i, WebIDBKeyTypeNumber));
    blob_info.push_back(
        WebVector<WebBlobInfo>(static_cast<size_t>(expected_key + i)));
  }
  cursor.SetPrefetchData(keys, primary_keys, values, blob_info);

  // Note that the real dispatcher would call cursor->CachedContinue()
  // immediately after cursor->SetPrefetchData() to service the request
  // that initiated the prefetch.

  // Need at least this many in the cache for the test steps.
  ASSERT_GE(prefetch_count, 5);

  // IDBCursor.continue()
  IndexedDBKey key;
  cursor.continueFunction(null_key_, new MockContinueCallbacks(&key));
  EXPECT_EQ(0, key.number());

  // IDBCursor.advance(1)
  cursor.advance(1, new MockContinueCallbacks(&key));
  EXPECT_EQ(1, key.number());

  // IDBCursor.continue()
  cursor.continueFunction(null_key_, new MockContinueCallbacks(&key));
  EXPECT_EQ(2, key.number());

  // IDBCursor.advance(2)
  cursor.advance(2, new MockContinueCallbacks(&key));
  EXPECT_EQ(4, key.number());

  EXPECT_EQ(0, dispatcher_->advance_calls());

  // IDBCursor.advance(lots) - beyond the fetched amount
  cursor.advance(WebIDBCursorImpl::kMaxPrefetchAmount,
                 new MockContinueCallbacks(&key));
  EXPECT_EQ(1, dispatcher_->advance_calls());
  EXPECT_EQ(1, dispatcher_->prefetch_calls());
  EXPECT_EQ(static_cast<int>(WebIDBCursorImpl::kPrefetchContinueThreshold),
            dispatcher_->continue_calls());
}

TEST_F(WebIDBCursorImplTest, PrefetchReset) {
  const int64 transaction_id = 1;
  WebIDBCursorImpl cursor(WebIDBCursorImpl::kInvalidCursorId,
                          transaction_id,
                          thread_safe_sender_.get());

  // Call continue() until prefetching should kick in.
  int continue_calls = 0;
  EXPECT_EQ(dispatcher_->continue_calls(), 0);
  for (int i = 0; i < WebIDBCursorImpl::kPrefetchContinueThreshold; ++i) {
    cursor.continueFunction(null_key_, new MockContinueCallbacks());
    EXPECT_EQ(++continue_calls, dispatcher_->continue_calls());
    EXPECT_EQ(0, dispatcher_->prefetch_calls());
  }

  // Initiate the prefetch
  cursor.continueFunction(null_key_, new MockContinueCallbacks());
  EXPECT_EQ(continue_calls, dispatcher_->continue_calls());
  EXPECT_EQ(1, dispatcher_->prefetch_calls());
  EXPECT_EQ(0, dispatcher_->reset_calls());

  // Now invalidate it
  cursor.ResetPrefetchCache();

  // No reset should have been sent since nothing has been received yet.
  EXPECT_EQ(0, dispatcher_->reset_calls());

  // Fill the prefetch cache as requested.
  int prefetch_count = dispatcher_->last_prefetch_count();
  std::vector<IndexedDBKey> keys(prefetch_count);
  std::vector<IndexedDBKey> primary_keys(prefetch_count);
  std::vector<WebData> values(prefetch_count);
  std::vector<WebVector<WebBlobInfo> > blob_info(prefetch_count);
  cursor.SetPrefetchData(keys, primary_keys, values, blob_info);

  // No reset should have been sent since prefetch data hasn't been used.
  EXPECT_EQ(0, dispatcher_->reset_calls());

  // The real dispatcher would call cursor->CachedContinue(), so do that:
  scoped_ptr<WebIDBCallbacks> callbacks(new MockContinueCallbacks());
  cursor.CachedContinue(callbacks.get());

  // Now the cursor should have reset the rest of the cache.
  EXPECT_EQ(1, dispatcher_->reset_calls());
  EXPECT_EQ(1, dispatcher_->last_used_count());
}

}  // namespace content
