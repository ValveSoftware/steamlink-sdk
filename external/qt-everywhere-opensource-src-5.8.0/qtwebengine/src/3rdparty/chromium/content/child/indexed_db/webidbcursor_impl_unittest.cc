// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/webidbcursor_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/indexed_db_key_builders.h"
#include "content/child/indexed_db/mock_webidbcallbacks.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "ipc/ipc_sync_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebData.h"

using blink::WebBlobInfo;
using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBKey;
using blink::WebIDBKeyTypeNumber;
using blink::WebIDBValue;
using blink::WebVector;
using testing::StrictMock;

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

  void RequestIDBCursorPrefetch(int n,
                                WebIDBCallbacks* callbacks,
                                int32_t ipc_cursor_id) override {
    ++prefetch_calls_;
    last_prefetch_count_ = n;
    callbacks_.reset(callbacks);
  }

  void RequestIDBCursorPrefetchReset(int used_prefetches,
                                     int unused_prefetches,
                                     int32_t ipc_cursor_id) override {
    ++reset_calls_;
    last_used_count_ = used_prefetches;
  }

  void RequestIDBCursorAdvance(unsigned long count,
                               WebIDBCallbacks* callbacks,
                               int32_t ipc_cursor_id,
                               int64_t transaction_id) override {
    ++advance_calls_;
    callbacks_.reset(callbacks);
  }

  void RequestIDBCursorContinue(const IndexedDBKey& key,
                                const IndexedDBKey& primary_key,
                                WebIDBCallbacks* callbacks,
                                int32_t ipc_cursor_id,
                                int64_t transaction_id) override {
    ++continue_calls_;
    callbacks_.reset(callbacks);
  }

  void CursorDestroyed(int32_t ipc_cursor_id) override {
    destroyed_cursor_id_ = ipc_cursor_id;
  }

  int prefetch_calls() { return prefetch_calls_; }
  int last_prefetch_count() { return last_prefetch_count_; }
  int reset_calls() { return reset_calls_; }
  int last_used_count() { return last_used_count_; }
  int advance_calls() { return advance_calls_; }
  int continue_calls() { return continue_calls_; }
  int32_t destroyed_cursor_id() { return destroyed_cursor_id_; }

 private:
  int prefetch_calls_;
  int last_prefetch_count_;
  int reset_calls_;
  int last_used_count_;
  int advance_calls_;
  int continue_calls_;
  int32_t destroyed_cursor_id_;
  std::unique_ptr<WebIDBCallbacks> callbacks_;
};

class MockContinueCallbacks : public StrictMock<MockWebIDBCallbacks> {
 public:
  MockContinueCallbacks(IndexedDBKey* key = 0,
                        WebVector<WebBlobInfo>* webBlobInfo = 0)
      : key_(key), web_blob_info_(webBlobInfo) {}

  void onSuccess(const WebIDBKey& key,
                 const WebIDBKey& primaryKey,
                 const WebIDBValue& value) override {
    if (key_)
      *key_ = IndexedDBKeyBuilder::Build(key);
    if (web_blob_info_)
      *web_blob_info_ = value.webBlobInfo;
  }

 private:
  IndexedDBKey* key_;
  WebVector<WebBlobInfo>* web_blob_info_;
};

class MockSyncMessageFilter : public IPC::SyncMessageFilter {
 public:
  MockSyncMessageFilter()
      : SyncMessageFilter(nullptr, false /* is_channel_send_thread_safe */) {}

 private:
  ~MockSyncMessageFilter() override {}
};

}  // namespace

class WebIDBCursorImplTest : public testing::Test {
 public:
  WebIDBCursorImplTest() {
    null_key_.assignNull();
    thread_safe_sender_ = new ThreadSafeSender(
        base::ThreadTaskRunnerHandle::Get(), new MockSyncMessageFilter);
    dispatcher_ =
        base::WrapUnique(new MockDispatcher(thread_safe_sender_.get()));
  }

 protected:
  base::MessageLoop message_loop_;
  WebIDBKey null_key_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  std::unique_ptr<MockDispatcher> dispatcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebIDBCursorImplTest);
};

TEST_F(WebIDBCursorImplTest, PrefetchTest) {
  const int64_t transaction_id = 1;
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
      std::vector<WebIDBValue> values(prefetch_count);
      for (int i = 0; i < prefetch_count; ++i) {
        keys.push_back(IndexedDBKey(expected_key + i, WebIDBKeyTypeNumber));
        values[i].webBlobInfo =
            WebVector<WebBlobInfo>(static_cast<size_t>(expected_key + i));
      }
      cursor.SetPrefetchData(keys, primary_keys, values);

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
  const int64_t transaction_id = 1;
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
  std::vector<WebIDBValue> values(prefetch_count);
  for (int i = 0; i < prefetch_count; ++i) {
    keys.push_back(IndexedDBKey(expected_key + i, WebIDBKeyTypeNumber));
    values[i].webBlobInfo =
        WebVector<WebBlobInfo>(static_cast<size_t>(expected_key + i));
  }
  cursor.SetPrefetchData(keys, primary_keys, values);

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
  const int64_t transaction_id = 1;
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
  std::vector<WebIDBValue> values(prefetch_count);
  cursor.SetPrefetchData(keys, primary_keys, values);

  // No reset should have been sent since prefetch data hasn't been used.
  EXPECT_EQ(0, dispatcher_->reset_calls());

  // The real dispatcher would call cursor->CachedContinue(), so do that:
  MockContinueCallbacks callbacks;
  cursor.CachedContinue(&callbacks);

  // Now the cursor should have reset the rest of the cache.
  EXPECT_EQ(1, dispatcher_->reset_calls());
  EXPECT_EQ(1, dispatcher_->last_used_count());
}

}  // namespace content
