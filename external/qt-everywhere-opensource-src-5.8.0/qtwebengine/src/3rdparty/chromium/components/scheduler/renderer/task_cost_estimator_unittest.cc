// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/task_cost_estimator.h"

#include <memory>

#include "base/test/simple_test_tick_clock.h"
#include "components/scheduler/base/test_time_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduler {

class TaskCostEstimatorTest : public testing::Test {
 public:
  TaskCostEstimatorTest() {}
  ~TaskCostEstimatorTest() override {}

  void SetUp() override {
    test_time_source_.reset(new TestTimeSource(&clock_));
  }

  base::SimpleTestTickClock clock_;
  std::unique_ptr<TestTimeSource> test_time_source_;
};

class TaskCostEstimatorForTest : public TaskCostEstimator {
 public:
  TaskCostEstimatorForTest(TestTimeSource* test_time_source,
                           int sample_count,
                           double estimation_percentile)
      : TaskCostEstimator(test_time_source,
                          sample_count,
                          estimation_percentile) {}
};

TEST_F(TaskCostEstimatorTest, BasicEstimation) {
  TaskCostEstimatorForTest estimator(test_time_source_.get(), 1, 100);
  base::PendingTask task(FROM_HERE, base::Closure());

  estimator.WillProcessTask(task);
  clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  estimator.DidProcessTask(task);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(500),
            estimator.expected_task_duration());
}

TEST_F(TaskCostEstimatorTest, Clear) {
  TaskCostEstimatorForTest estimator(test_time_source_.get(), 1, 100);
  base::PendingTask task(FROM_HERE, base::Closure());

  estimator.WillProcessTask(task);
  clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  estimator.DidProcessTask(task);

  estimator.Clear();

  EXPECT_EQ(base::TimeDelta(), estimator.expected_task_duration());
}

TEST_F(TaskCostEstimatorTest, NestedRunLoop) {
  TaskCostEstimatorForTest estimator(test_time_source_.get(), 1, 100);
  base::PendingTask task(FROM_HERE, base::Closure());

  // Make sure we ignore the tasks inside the nested run loop.
  estimator.WillProcessTask(task);
  estimator.WillProcessTask(task);
  clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  estimator.DidProcessTask(task);
  clock_.Advance(base::TimeDelta::FromMilliseconds(500));
  estimator.DidProcessTask(task);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1000),
            estimator.expected_task_duration());
}

}  // namespace scheduler
