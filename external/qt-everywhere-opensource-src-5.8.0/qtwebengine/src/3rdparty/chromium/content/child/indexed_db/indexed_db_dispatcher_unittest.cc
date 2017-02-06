// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/indexed_db_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "content/child/indexed_db/mock_webidbcallbacks.h"
#include "content/child/indexed_db/webidbcursor_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "ipc/ipc_sync_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebBlobInfo.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using blink::WebBlobInfo;
using blink::WebData;
using blink::WebIDBCursor;
using blink::WebIDBKey;
using blink::WebVector;
using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace content {
namespace {

class MockDispatcher : public IndexedDBDispatcher {
 public:
  explicit MockDispatcher(ThreadSafeSender* sender)
      : IndexedDBDispatcher(sender) {}

  bool Send(IPC::Message* msg) override {
    delete msg;
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDispatcher);
};

class MockSyncMessageFilter : public IPC::SyncMessageFilter {
 public:
  MockSyncMessageFilter()
      : SyncMessageFilter(nullptr, false /* is_channel_send_thread_safe */) {}

 private:
  ~MockSyncMessageFilter() override {}
};

}  // namespace

class IndexedDBDispatcherTest : public testing::Test {
 public:
  IndexedDBDispatcherTest()
      : thread_safe_sender_(new ThreadSafeSender(
            base::ThreadTaskRunnerHandle::Get(), new MockSyncMessageFilter)) {}

  void TearDown() override { blink::WebHeap::collectAllGarbageForTesting(); }

 protected:
  base::MessageLoop message_loop_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcherTest);
};

TEST_F(IndexedDBDispatcherTest, ValueSizeTest) {
  // For testing use a much smaller maximum size to prevent allocating >100 MB
  // of memory, which crashes on memory-constrained systems.
  const size_t kMaxValueSizeForTesting = 10 * 1024 * 1024;  // 10 MB

  const std::vector<char> data(kMaxValueSizeForTesting + 1);
  const WebData value(&data.front(), data.size());
  const WebVector<WebBlobInfo> web_blob_info;
  const int32_t ipc_dummy_id = -1;
  const int64_t transaction_id = 1;
  const int64_t object_store_id = 2;

  StrictMock<MockWebIDBCallbacks> callbacks;
  EXPECT_CALL(callbacks, onError(_)).Times(1);

  IndexedDBDispatcher dispatcher(thread_safe_sender_.get());
  dispatcher.max_put_value_size_ = kMaxValueSizeForTesting;
  IndexedDBKey key(0, blink::WebIDBKeyTypeNumber);
  dispatcher.RequestIDBDatabasePut(ipc_dummy_id,
                                   transaction_id,
                                   object_store_id,
                                   value,
                                   web_blob_info,
                                   key,
                                   blink::WebIDBPutModeAddOrUpdate,
                                   &callbacks,
                                   WebVector<long long>(),
                                   WebVector<WebVector<WebIDBKey> >());
}

TEST_F(IndexedDBDispatcherTest, KeyAndValueSizeTest) {
  // For testing use a much smaller maximum size to prevent allocating >100 MB
  // of memory, which crashes on memory-constrained systems.
  const size_t kMaxValueSizeForTesting = 10 * 1024 * 1024;  // 10 MB
  const size_t kKeySize = 1024 * 1024;

  const std::vector<char> data(kMaxValueSizeForTesting - kKeySize);
  const WebData value(&data.front(), data.size());
  const WebVector<WebBlobInfo> web_blob_info;
  const IndexedDBKey key(
      base::string16(kKeySize / sizeof(base::string16::value_type), 'x'));

  const int32_t ipc_dummy_id = -1;
  const int64_t transaction_id = 1;
  const int64_t object_store_id = 2;

  StrictMock<MockWebIDBCallbacks> callbacks;
  EXPECT_CALL(callbacks, onError(_)).Times(1);

  IndexedDBDispatcher dispatcher(thread_safe_sender_.get());
  dispatcher.max_put_value_size_ = kMaxValueSizeForTesting;
  dispatcher.RequestIDBDatabasePut(ipc_dummy_id,
                                   transaction_id,
                                   object_store_id,
                                   value,
                                   web_blob_info,
                                   key,
                                   blink::WebIDBPutModeAddOrUpdate,
                                   &callbacks,
                                   WebVector<long long>(),
                                   WebVector<WebVector<WebIDBKey> >());
}

TEST_F(IndexedDBDispatcherTest, CursorTransactionId) {
  const int32_t ipc_database_id = -1;
  const int64_t transaction_id = 1234;
  const int64_t object_store_id = 2;
  const int32_t index_id = 3;
  const blink::WebIDBCursorDirection direction =
      blink::WebIDBCursorDirectionNext;
  const bool key_only = false;

  MockDispatcher dispatcher(thread_safe_sender_.get());

  // First case: successful cursor open.
  {
    std::unique_ptr<WebIDBCursor> cursor;
    EXPECT_EQ(0UL, dispatcher.cursor_transaction_ids_.size());

    auto callbacks = new StrictMock<MockWebIDBCallbacks>();
    // Reference first param (cursor) to keep it alive.
    // TODO(cmumford): Cleanup (and below) once std::addressof() is allowed.
    ON_CALL(*callbacks, onSuccess(testing::A<WebIDBCursor*>(), _, _, _))
        .WillByDefault(
            WithArgs<0>(Invoke(&cursor.operator=(nullptr),
                               &std::unique_ptr<WebIDBCursor>::reset)));
    EXPECT_CALL(*callbacks, onSuccess(testing::A<WebIDBCursor*>(), _, _, _))
        .Times(1);

    // Make a cursor request. This should record the transaction id.
    dispatcher.RequestIDBDatabaseOpenCursor(
        ipc_database_id, transaction_id, object_store_id, index_id,
        IndexedDBKeyRange(), direction, key_only, blink::WebIDBTaskTypeNormal,
        callbacks);

    // Verify that the transaction id was captured.
    EXPECT_EQ(1UL, dispatcher.cursor_transaction_ids_.size());
    EXPECT_FALSE(cursor.get());

    int32_t ipc_callbacks_id =
        dispatcher.cursor_transaction_ids_.begin()->first;

    IndexedDBMsg_CallbacksSuccessIDBCursor_Params params;
    params.ipc_thread_id = dispatcher.CurrentWorkerId();
    params.ipc_callbacks_id = ipc_callbacks_id;

    // Now simululate the cursor response.
    params.ipc_cursor_id = WebIDBCursorImpl::kInvalidCursorId;
    dispatcher.OnSuccessOpenCursor(params);

    EXPECT_EQ(0UL, dispatcher.cursor_transaction_ids_.size());

    EXPECT_TRUE(cursor.get());

    WebIDBCursorImpl* impl = static_cast<WebIDBCursorImpl*>(cursor.get());

    // This is the primary expectation of this test: the transaction id was
    // applied to the cursor.
    EXPECT_EQ(transaction_id, impl->transaction_id());
  }

  // Second case: null cursor (no data in range)
  {
    EXPECT_EQ(0UL, dispatcher.cursor_transaction_ids_.size());

    auto callbacks = new StrictMock<MockWebIDBCallbacks>();
    EXPECT_CALL(*callbacks, onSuccess(testing::A<const blink::WebIDBValue&>()))
        .Times(1);

    // Make a cursor request. This should record the transaction id.
    dispatcher.RequestIDBDatabaseOpenCursor(
        ipc_database_id, transaction_id, object_store_id, index_id,
        IndexedDBKeyRange(), direction, key_only, blink::WebIDBTaskTypeNormal,
        callbacks);

    // Verify that the transaction id was captured.
    EXPECT_EQ(1UL, dispatcher.cursor_transaction_ids_.size());

    int32_t ipc_callbacks_id =
        dispatcher.cursor_transaction_ids_.begin()->first;

    // Now simululate a "null cursor" response.
    IndexedDBMsg_CallbacksSuccessValue_Params params;
    params.ipc_thread_id = dispatcher.CurrentWorkerId();
    params.ipc_callbacks_id = ipc_callbacks_id;
    dispatcher.OnSuccessValue(params);

    // Ensure the map result was deleted.
    EXPECT_EQ(0UL, dispatcher.cursor_transaction_ids_.size());
  }
}

namespace {

class MockCursor : public WebIDBCursorImpl {
 public:
  MockCursor(int32_t ipc_cursor_id,
             int64_t transaction_id,
             ThreadSafeSender* thread_safe_sender)
      : WebIDBCursorImpl(ipc_cursor_id, transaction_id, thread_safe_sender),
        reset_count_(0) {}

  // This method is virtual so it can be overridden in unit tests.
  void ResetPrefetchCache() override { ++reset_count_; }

  int reset_count() const { return reset_count_; }

 private:
  int reset_count_;

  DISALLOW_COPY_AND_ASSIGN(MockCursor);
};

}  // namespace

TEST_F(IndexedDBDispatcherTest, CursorReset) {
  std::unique_ptr<WebIDBCursor> cursor;
  MockDispatcher dispatcher(thread_safe_sender_.get());

  const int32_t ipc_database_id = 0;
  const int32_t object_store_id = 0;
  const int32_t index_id = 0;
  const bool key_only = false;
  const int cursor1_ipc_id = 1;
  const int cursor2_ipc_id = 2;
  const int other_cursor_ipc_id = 2;
  const int cursor1_transaction_id = 1;
  const int cursor2_transaction_id = 2;
  const int other_transaction_id = 3;

  std::unique_ptr<MockCursor> cursor1(
      new MockCursor(WebIDBCursorImpl::kInvalidCursorId, cursor1_transaction_id,
                     thread_safe_sender_.get()));

  std::unique_ptr<MockCursor> cursor2(
      new MockCursor(WebIDBCursorImpl::kInvalidCursorId, cursor2_transaction_id,
                     thread_safe_sender_.get()));

  dispatcher.cursors_[cursor1_ipc_id] = cursor1.get();
  dispatcher.cursors_[cursor2_ipc_id] = cursor2.get();

  EXPECT_EQ(0, cursor1->reset_count());
  EXPECT_EQ(0, cursor2->reset_count());

  // Other transaction:
  dispatcher.RequestIDBDatabaseGet(
      ipc_database_id, other_transaction_id, object_store_id, index_id,
      IndexedDBKeyRange(), key_only, new StrictMock<MockWebIDBCallbacks>());

  EXPECT_EQ(0, cursor1->reset_count());
  EXPECT_EQ(0, cursor2->reset_count());

  // Same transaction:
  dispatcher.RequestIDBDatabaseGet(
      ipc_database_id, cursor1_transaction_id, object_store_id, index_id,
      IndexedDBKeyRange(), key_only, new StrictMock<MockWebIDBCallbacks>());

  EXPECT_EQ(1, cursor1->reset_count());
  EXPECT_EQ(0, cursor2->reset_count());

  // Same transaction and same cursor:
  dispatcher.RequestIDBCursorContinue(IndexedDBKey(), IndexedDBKey(),
                                      new StrictMock<MockWebIDBCallbacks>(),
                                      cursor1_ipc_id, cursor1_transaction_id);

  EXPECT_EQ(1, cursor1->reset_count());
  EXPECT_EQ(0, cursor2->reset_count());

  // Same transaction and different cursor:
  dispatcher.RequestIDBCursorContinue(
      IndexedDBKey(), IndexedDBKey(), new StrictMock<MockWebIDBCallbacks>(),
      other_cursor_ipc_id, cursor1_transaction_id);

  EXPECT_EQ(2, cursor1->reset_count());
  EXPECT_EQ(0, cursor2->reset_count());

  cursor1.reset();
  cursor2.reset();
}

}  // namespace content
