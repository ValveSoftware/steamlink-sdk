// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/queueing_time_estimator.h"
#include "platform/scheduler/base/test_time_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

using QueueingTimeEstimatorTest = testing::Test;

class TestQueueingTimeEstimatorClient : public QueueingTimeEstimator::Client {
 public:
  void OnQueueingTimeForWindowEstimated(
      base::TimeDelta queueing_time) override {
    expected_queueing_times_.push_back(queueing_time);
  }
  const std::vector<base::TimeDelta>& expected_queueing_times() {
    return expected_queueing_times_;
  }

 private:
  std::vector<base::TimeDelta> expected_queueing_times_;
};

class QueueingTimeEstimatorForTest : public QueueingTimeEstimator {
 public:
  QueueingTimeEstimatorForTest(TestQueueingTimeEstimatorClient* client,
                               base::TimeDelta window_duration)
      : QueueingTimeEstimator(client, window_duration) {}
};

// Three tasks of one second each, all within a 5 second window. Expected
// queueing time is the probability of falling into one of these tasks (3/5),
// multiplied by the expected queueing time within a task (0.5 seconds). Thus we
// expect a queueing time of 0.3 seconds.
TEST_F(QueueingTimeEstimatorTest, AllTasksWithinWindow) {
  base::TimeTicks time;
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5));
  for (int i = 0; i < 3; ++i) {
    estimator.OnToplevelTaskCompleted(
        time, time + base::TimeDelta::FromMilliseconds(1000));
    time += base::TimeDelta::FromMilliseconds(1500);
  }

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnToplevelTaskCompleted(
      time, time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(300)));
}

// One 20 second long task, starting 3 seconds into the first window.
// Window 1: Probability of being within task = 2/5. Expected delay within task:
// avg(20, 18). Total expected queueing time = 7.6s.
// Window 2: Probability of being within task = 1. Expected delay within task:
// avg(18, 13). Total expected queueing time = 15.5s.
// Window 5: Probability of being within task = 3/5. Expected delay within task:
// avg(3, 0). Total expected queueing time = 0.9s.
TEST_F(QueueingTimeEstimatorTest, MultiWindowTask) {
  TestQueueingTimeEstimatorClient client;
  QueueingTimeEstimatorForTest estimator(&client,
                                         base::TimeDelta::FromSeconds(5));
  base::TimeTicks time;
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnToplevelTaskCompleted(time, time);

  time += base::TimeDelta::FromMilliseconds(3000);

  estimator.OnToplevelTaskCompleted(
      time, time + base::TimeDelta::FromMilliseconds(20000));

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(25000);

  estimator.OnToplevelTaskCompleted(
      time, time + base::TimeDelta::FromMilliseconds(500));

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(7600),
                                   base::TimeDelta::FromMilliseconds(15500),
                                   base::TimeDelta::FromMilliseconds(10500),
                                   base::TimeDelta::FromMilliseconds(5500),
                                   base::TimeDelta::FromMilliseconds(900)));
}

}  // namespace scheduler
}  // namespace blink
