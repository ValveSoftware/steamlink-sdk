// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/screen_orientation_dispatcher.h"

#include <list>
#include <memory>
#include <tuple>

#include "base/logging.h"
#include "content/common/screen_orientation_messages.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebLockOrientationCallback.h"

namespace content {

// MockLockOrientationCallback is an implementation of
// WebLockOrientationCallback and takes a LockOrientationResultHolder* as a
// parameter when being constructed. The |results_| pointer is owned by the
// caller and not by the callback object. The intent being that as soon as the
// callback is resolved, it will be killed so we use the
// LockOrientationResultHolder to know in which state the callback object is at
// any time.
class MockLockOrientationCallback : public blink::WebLockOrientationCallback {
 public:
  struct LockOrientationResultHolder {
    LockOrientationResultHolder()
        : succeeded_(false), failed_(false) {}

    bool succeeded_;
    bool failed_;
    blink::WebLockOrientationError error_;
  };

  explicit MockLockOrientationCallback(LockOrientationResultHolder* results)
      : results_(results) {}

  void onSuccess() override { results_->succeeded_ = true; }

  void onError(blink::WebLockOrientationError error) override {
    results_->failed_ = true;
    results_->error_ = error;
  }

 private:
  ~MockLockOrientationCallback() override {}

  LockOrientationResultHolder* results_;
};

class ScreenOrientationDispatcherWithSink : public ScreenOrientationDispatcher {
 public:
  explicit ScreenOrientationDispatcherWithSink(IPC::TestSink* sink)
      :ScreenOrientationDispatcher(NULL) , sink_(sink) {
  }

  bool Send(IPC::Message* message) override { return sink_->Send(message); }

  IPC::TestSink* sink_;
};

class ScreenOrientationDispatcherTest : public testing::Test {
 protected:
  void SetUp() override {
    dispatcher_.reset(new ScreenOrientationDispatcherWithSink(&sink_));
  }

  int GetFirstLockRequestIdFromSink() {
    const IPC::Message* msg = sink().GetFirstMessageMatching(
        ScreenOrientationHostMsg_LockRequest::ID);
    EXPECT_TRUE(msg != NULL);

    std::tuple<blink::WebScreenOrientationLockType, int> params;
    ScreenOrientationHostMsg_LockRequest::Read(msg, &params);
    return std::get<1>(params);
  }

  IPC::TestSink& sink() {
    return sink_;
  }

  void LockOrientation(blink::WebScreenOrientationLockType orientation,
                       blink::WebLockOrientationCallback* callback) {
    dispatcher_->lockOrientation(orientation, callback);
  }

  void UnlockOrientation() {
    dispatcher_->unlockOrientation();
  }

  void OnMessageReceived(const IPC::Message& message) {
    dispatcher_->OnMessageReceived(message);
  }

  int routing_id() const {
    // We return a fake routing_id() in the context of this test.
    return 0;
  }

  IPC::TestSink sink_;
  std::unique_ptr<ScreenOrientationDispatcher> dispatcher_;
};

// Test that calling lockOrientation() followed by unlockOrientation() cancel
// the lockOrientation().
TEST_F(ScreenOrientationDispatcherTest, CancelPending_Unlocking) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  LockOrientation(blink::WebScreenOrientationLockPortraitPrimary,
                  new MockLockOrientationCallback(&callback_results));
  UnlockOrientation();

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_TRUE(callback_results.failed_);
  EXPECT_EQ(blink::WebLockOrientationErrorCanceled, callback_results.error_);
}

// Test that calling lockOrientation() twice cancel the first lockOrientation().
TEST_F(ScreenOrientationDispatcherTest, CancelPending_DoubleLock) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  // We create the object to prevent leaks but never actually use it.
  MockLockOrientationCallback::LockOrientationResultHolder callback_results2;

  LockOrientation(blink::WebScreenOrientationLockPortraitPrimary,
                  new MockLockOrientationCallback(&callback_results));
  LockOrientation(blink::WebScreenOrientationLockPortraitPrimary,
                  new MockLockOrientationCallback(&callback_results2));

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_TRUE(callback_results.failed_);
  EXPECT_EQ(blink::WebLockOrientationErrorCanceled, callback_results.error_);
}

// Test that when a LockError message is received, the request is set as failed
// with the correct values.
TEST_F(ScreenOrientationDispatcherTest, LockRequest_Error) {
  std::list<blink::WebLockOrientationError> errors;
  errors.push_back(blink::WebLockOrientationErrorNotAvailable);
  errors.push_back(
      blink::WebLockOrientationErrorFullScreenRequired);
  errors.push_back(blink::WebLockOrientationErrorCanceled);

  for (std::list<blink::WebLockOrientationError>::const_iterator
          it = errors.begin(); it != errors.end(); ++it) {
    MockLockOrientationCallback::LockOrientationResultHolder callback_results;
    LockOrientation(blink::WebScreenOrientationLockPortraitPrimary,
                    new MockLockOrientationCallback(&callback_results));

    int request_id = GetFirstLockRequestIdFromSink();
    OnMessageReceived(
        ScreenOrientationMsg_LockError(routing_id(), request_id, *it));

    EXPECT_FALSE(callback_results.succeeded_);
    EXPECT_TRUE(callback_results.failed_);
    EXPECT_EQ(*it, callback_results.error_);

    sink().ClearMessages();
  }
}

// Test that when a LockSuccess message is received, the request is set as
// succeeded.
TEST_F(ScreenOrientationDispatcherTest, LockRequest_Success) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  LockOrientation(blink::WebScreenOrientationLockPortraitPrimary,
                  new MockLockOrientationCallback(&callback_results));

  int request_id = GetFirstLockRequestIdFromSink();
  OnMessageReceived(ScreenOrientationMsg_LockSuccess(routing_id(),
                                                     request_id));

  EXPECT_TRUE(callback_results.succeeded_);
  EXPECT_FALSE(callback_results.failed_);

  sink().ClearMessages();
}

// Test an edge case: a LockSuccess is received but it matches no pending
// callback.
TEST_F(ScreenOrientationDispatcherTest, SuccessForUnknownRequest) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  LockOrientation(blink::WebScreenOrientationLockPortraitPrimary,
                  new MockLockOrientationCallback(&callback_results));

  int request_id = GetFirstLockRequestIdFromSink();
  OnMessageReceived(ScreenOrientationMsg_LockSuccess(routing_id(),
                                                     request_id + 1));

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_FALSE(callback_results.failed_);
}

// Test an edge case: a LockError is received but it matches no pending
// callback.
TEST_F(ScreenOrientationDispatcherTest, ErrorForUnknownRequest) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results;
  LockOrientation(blink::WebScreenOrientationLockPortraitPrimary,
                  new MockLockOrientationCallback(&callback_results));

  int request_id = GetFirstLockRequestIdFromSink();
  OnMessageReceived(ScreenOrientationMsg_LockError(
      routing_id(), request_id + 1, blink::WebLockOrientationErrorCanceled));

  EXPECT_FALSE(callback_results.succeeded_);
  EXPECT_FALSE(callback_results.failed_);
}

// Test the following scenario:
// - request1 is received by the dispatcher;
// - request2 is received by the dispatcher;
// - request1 is rejected;
// - request1 success response is received.
// Expected: request1 is still rejected, request2 has not been set as succeeded.
TEST_F(ScreenOrientationDispatcherTest, RaceScenario) {
  MockLockOrientationCallback::LockOrientationResultHolder callback_results1;
  MockLockOrientationCallback::LockOrientationResultHolder callback_results2;

  LockOrientation(blink::WebScreenOrientationLockPortraitPrimary,
                  new MockLockOrientationCallback(&callback_results1));
  int request_id1 = GetFirstLockRequestIdFromSink();

  LockOrientation(blink::WebScreenOrientationLockLandscapePrimary,
                  new MockLockOrientationCallback(&callback_results2));

  // callback_results1 must be rejected, tested in CancelPending_DoubleLock.

  OnMessageReceived(ScreenOrientationMsg_LockSuccess(routing_id(),
                                                     request_id1));

  // First request is still rejected.
  EXPECT_FALSE(callback_results1.succeeded_);
  EXPECT_TRUE(callback_results1.failed_);
  EXPECT_EQ(blink::WebLockOrientationErrorCanceled, callback_results1.error_);

  // Second request is still pending.
  EXPECT_FALSE(callback_results2.succeeded_);
  EXPECT_FALSE(callback_results2.failed_);
}

}  // namespace content
