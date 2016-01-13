// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/worker_task_runner.h"

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class WorkerTaskRunnerTest : public testing::Test {
 public:
  void FakeStart() {
    task_runner_.OnWorkerRunLoopStarted(blink::WebWorkerRunLoop());
  }
  void FakeStop() {
    task_runner_.OnWorkerRunLoopStopped(blink::WebWorkerRunLoop());
  }
  WorkerTaskRunner task_runner_;
};

class MockObserver : public WorkerTaskRunner::Observer {
 public:
  MOCK_METHOD0(OnWorkerRunLoopStopped, void());
  void RemoveSelfOnNotify() {
    ON_CALL(*this, OnWorkerRunLoopStopped()).WillByDefault(
        testing::Invoke(this, &MockObserver::RemoveSelf));
  }
  void RemoveSelf() {
    runner_->RemoveStopObserver(this);
  }
  WorkerTaskRunner* runner_;
};

TEST_F(WorkerTaskRunnerTest, BasicObservingAndWorkerId) {
  ASSERT_EQ(0, task_runner_.CurrentWorkerId());
  MockObserver o;
  EXPECT_CALL(o, OnWorkerRunLoopStopped()).Times(1);
  FakeStart();
  task_runner_.AddStopObserver(&o);
  ASSERT_LT(0, task_runner_.CurrentWorkerId());
  FakeStop();
}

TEST_F(WorkerTaskRunnerTest, CanRemoveSelfDuringNotification) {
  MockObserver o;
  o.RemoveSelfOnNotify();
  o.runner_ = &task_runner_;
  EXPECT_CALL(o, OnWorkerRunLoopStopped()).Times(1);
  FakeStart();
  task_runner_.AddStopObserver(&o);
  FakeStop();
}

}  // namespace content
