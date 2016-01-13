// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/indexed_db/webidbcursor_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "ipc/ipc_sync_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebBlobInfo.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebIDBCallbacks.h"

using blink::WebBlobInfo;
using blink::WebData;
using blink::WebIDBCallbacks;
using blink::WebIDBCursor;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseError;
using blink::WebIDBKey;
using blink::WebVector;

namespace content {
namespace {

class MockCallbacks : public WebIDBCallbacks {
 public:
  MockCallbacks() : error_seen_(false) {}

  virtual void onError(const WebIDBDatabaseError&) { error_seen_ = true; }

  bool error_seen() const { return error_seen_; }

 private:
  bool error_seen_;

  DISALLOW_COPY_AND_ASSIGN(MockCallbacks);
};

class MockDispatcher : public IndexedDBDispatcher {
 public:
  explicit MockDispatcher(ThreadSafeSender* sender)
      : IndexedDBDispatcher(sender) {}

  virtual bool Send(IPC::Message* msg) OVERRIDE {
    delete msg;
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDispatcher);
};

}  // namespace

class IndexedDBDispatcherTest : public testing::Test {
 public:
  IndexedDBDispatcherTest()
      : message_loop_proxy_(base::MessageLoopProxy::current()),
        sync_message_filter_(new IPC::SyncMessageFilter(NULL)),
        thread_safe_sender_(new ThreadSafeSender(message_loop_proxy_.get(),
                                                 sync_message_filter_.get())) {}

 protected:
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBDispatcherTest);
};

TEST_F(IndexedDBDispatcherTest, ValueSizeTest) {
  const std::vector<char> data(kMaxIDBValueSizeInBytes + 1);
  const WebData value(&data.front(), data.size());
  const WebVector<WebBlobInfo> web_blob_info;
  const int32 ipc_dummy_id = -1;
  const int64 transaction_id = 1;
  const int64 object_store_id = 2;

  MockCallbacks callbacks;
  IndexedDBDispatcher dispatcher(thread_safe_sender_.get());
  IndexedDBKey key(0, blink::WebIDBKeyTypeNumber);
  dispatcher.RequestIDBDatabasePut(ipc_dummy_id,
                                   transaction_id,
                                   object_store_id,
                                   value,
                                   web_blob_info,
                                   key,
                                   WebIDBDatabase::AddOrUpdate,
                                   &callbacks,
                                   WebVector<long long>(),
                                   WebVector<WebVector<WebIDBKey> >());

  EXPECT_TRUE(callbacks.error_seen());
}

TEST_F(IndexedDBDispatcherTest, KeyAndValueSizeTest) {
  const size_t kKeySize = 1024 * 1024;

  const std::vector<char> data(kMaxIDBValueSizeInBytes - kKeySize);
  const WebData value(&data.front(), data.size());
  const WebVector<WebBlobInfo> web_blob_info;
  const IndexedDBKey key(
      base::string16(kKeySize / sizeof(base::string16::value_type), 'x'));

  const int32 ipc_dummy_id = -1;
  const int64 transaction_id = 1;
  const int64 object_store_id = 2;

  MockCallbacks callbacks;
  IndexedDBDispatcher dispatcher(thread_safe_sender_.get());
  dispatcher.RequestIDBDatabasePut(ipc_dummy_id,
                                   transaction_id,
                                   object_store_id,
                                   value,
                                   web_blob_info,
                                   key,
                                   WebIDBDatabase::AddOrUpdate,
                                   &callbacks,
                                   WebVector<long long>(),
                                   WebVector<WebVector<WebIDBKey> >());

  EXPECT_TRUE(callbacks.error_seen());
}

namespace {

class CursorCallbacks : public WebIDBCallbacks {
 public:
  explicit CursorCallbacks(scoped_ptr<WebIDBCursor>* cursor)
      : cursor_(cursor) {}

  virtual void onSuccess(const WebData&,
                         const WebVector<WebBlobInfo>&) OVERRIDE {}
  virtual void onSuccess(WebIDBCursor* cursor,
                         const WebIDBKey& key,
                         const WebIDBKey& primaryKey,
                         const WebData& value,
                         const WebVector<WebBlobInfo>&) OVERRIDE {
    cursor_->reset(cursor);
  }

 private:
  scoped_ptr<WebIDBCursor>* cursor_;

  DISALLOW_COPY_AND_ASSIGN(CursorCallbacks);
};

}  // namespace

TEST_F(IndexedDBDispatcherTest, CursorTransactionId) {
  const int32 ipc_database_id = -1;
  const int64 transaction_id = 1234;
  const int64 object_store_id = 2;
  const int32 index_id = 3;
  const WebIDBCursor::Direction direction = WebIDBCursor::Next;
  const bool key_only = false;

  MockDispatcher dispatcher(thread_safe_sender_.get());

  // First case: successful cursor open.
  {
    scoped_ptr<WebIDBCursor> cursor;
    EXPECT_EQ(0UL, dispatcher.cursor_transaction_ids_.size());

    // Make a cursor request. This should record the transaction id.
    dispatcher.RequestIDBDatabaseOpenCursor(ipc_database_id,
                                            transaction_id,
                                            object_store_id,
                                            index_id,
                                            IndexedDBKeyRange(),
                                            direction,
                                            key_only,
                                            blink::WebIDBDatabase::NormalTask,
                                            new CursorCallbacks(&cursor));

    // Verify that the transaction id was captured.
    EXPECT_EQ(1UL, dispatcher.cursor_transaction_ids_.size());
    EXPECT_FALSE(cursor.get());

    int32 ipc_callbacks_id = dispatcher.cursor_transaction_ids_.begin()->first;

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
    scoped_ptr<WebIDBCursor> cursor;
    EXPECT_EQ(0UL, dispatcher.cursor_transaction_ids_.size());

    // Make a cursor request. This should record the transaction id.
    dispatcher.RequestIDBDatabaseOpenCursor(ipc_database_id,
                                            transaction_id,
                                            object_store_id,
                                            index_id,
                                            IndexedDBKeyRange(),
                                            direction,
                                            key_only,
                                            blink::WebIDBDatabase::NormalTask,
                                            new CursorCallbacks(&cursor));

    // Verify that the transaction id was captured.
    EXPECT_EQ(1UL, dispatcher.cursor_transaction_ids_.size());
    EXPECT_FALSE(cursor.get());

    int32 ipc_callbacks_id = dispatcher.cursor_transaction_ids_.begin()->first;

    // Now simululate a "null cursor" response.
    IndexedDBMsg_CallbacksSuccessValue_Params params;
    params.ipc_thread_id = dispatcher.CurrentWorkerId();
    params.ipc_callbacks_id = ipc_callbacks_id;
    dispatcher.OnSuccessValue(params);

    // Ensure the map result was deleted.
    EXPECT_EQ(0UL, dispatcher.cursor_transaction_ids_.size());
    EXPECT_FALSE(cursor.get());
  }
}

namespace {

class MockCursor : public WebIDBCursorImpl {
 public:
  MockCursor(int32 ipc_cursor_id,
             int64 transaction_id,
             ThreadSafeSender* thread_safe_sender)
      : WebIDBCursorImpl(ipc_cursor_id, transaction_id, thread_safe_sender),
        reset_count_(0) {}

  // This method is virtual so it can be overridden in unit tests.
  virtual void ResetPrefetchCache() OVERRIDE { ++reset_count_; }

  int reset_count() const { return reset_count_; }

 private:
  int reset_count_;

  DISALLOW_COPY_AND_ASSIGN(MockCursor);
};

}  // namespace

TEST_F(IndexedDBDispatcherTest, CursorReset) {
  scoped_ptr<WebIDBCursor> cursor;
  MockDispatcher dispatcher(thread_safe_sender_.get());

  const int32 ipc_database_id = 0;
  const int32 object_store_id = 0;
  const int32 index_id = 0;
  const bool key_only = false;
  const int cursor1_ipc_id = 1;
  const int cursor2_ipc_id = 2;
  const int other_cursor_ipc_id = 2;
  const int cursor1_transaction_id = 1;
  const int cursor2_transaction_id = 2;
  const int other_transaction_id = 3;

  scoped_ptr<MockCursor> cursor1(
      new MockCursor(WebIDBCursorImpl::kInvalidCursorId,
                     cursor1_transaction_id,
                     thread_safe_sender_.get()));

  scoped_ptr<MockCursor> cursor2(
      new MockCursor(WebIDBCursorImpl::kInvalidCursorId,
                     cursor2_transaction_id,
                     thread_safe_sender_.get()));

  dispatcher.cursors_[cursor1_ipc_id] = cursor1.get();
  dispatcher.cursors_[cursor2_ipc_id] = cursor2.get();

  EXPECT_EQ(0, cursor1->reset_count());
  EXPECT_EQ(0, cursor2->reset_count());

  // Other transaction:
  dispatcher.RequestIDBDatabaseGet(ipc_database_id,
                                   other_transaction_id,
                                   object_store_id,
                                   index_id,
                                   IndexedDBKeyRange(),
                                   key_only,
                                   new MockCallbacks());

  EXPECT_EQ(0, cursor1->reset_count());
  EXPECT_EQ(0, cursor2->reset_count());

  // Same transaction:
  dispatcher.RequestIDBDatabaseGet(ipc_database_id,
                                   cursor1_transaction_id,
                                   object_store_id,
                                   index_id,
                                   IndexedDBKeyRange(),
                                   key_only,
                                   new MockCallbacks());

  EXPECT_EQ(1, cursor1->reset_count());
  EXPECT_EQ(0, cursor2->reset_count());

  // Same transaction and same cursor:
  dispatcher.RequestIDBCursorContinue(IndexedDBKey(),
                                      IndexedDBKey(),
                                      new MockCallbacks(),
                                      cursor1_ipc_id,
                                      cursor1_transaction_id);

  EXPECT_EQ(1, cursor1->reset_count());
  EXPECT_EQ(0, cursor2->reset_count());

  // Same transaction and different cursor:
  dispatcher.RequestIDBCursorContinue(IndexedDBKey(),
                                      IndexedDBKey(),
                                      new MockCallbacks(),
                                      other_cursor_ipc_id,
                                      cursor1_transaction_id);

  EXPECT_EQ(2, cursor1->reset_count());
  EXPECT_EQ(0, cursor2->reset_count());

  cursor1.reset();
  cursor2.reset();
}

}  // namespace content
