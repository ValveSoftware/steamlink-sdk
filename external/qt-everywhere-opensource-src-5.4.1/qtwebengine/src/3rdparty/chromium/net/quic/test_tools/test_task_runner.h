// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common utilities for Quic tests

#ifndef NET_QUIC_TEST_TOOLS_TEST_TASK_RUNNER_H_
#define NET_QUIC_TEST_TOOLS_TEST_TASK_RUNNER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/task_runner.h"
#include "base/test/test_pending_task.h"

namespace net {

class MockClock;

namespace test {

typedef base::TestPendingTask PostedTask;

class TestTaskRunner : public base::TaskRunner {
 public:
  explicit TestTaskRunner(MockClock* clock);

  // base::TaskRunner implementation.
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE;
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;

  const std::vector<PostedTask>& GetPostedTasks() const;

  void RunNextTask();

 protected:
  virtual ~TestTaskRunner();

 private:
  std::vector<PostedTask>::iterator FindNextTask();

  MockClock* const clock_;
  std::vector<PostedTask> tasks_;

  DISALLOW_COPY_AND_ASSIGN(TestTaskRunner);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_TEST_TASK_RUNNER_H_
