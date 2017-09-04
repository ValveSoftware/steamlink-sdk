// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/long_task_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class LongTaskTrackerTest : public ::testing::Test {
 protected:
  LongTaskTracker long_task_tracker_;

  size_t NumLongTasks() { return long_task_tracker_.long_task_times_.size(); }
};

TEST_F(LongTaskTrackerTest, RecordAndReport) {
  EXPECT_EQ(0U, NumLongTasks());

  // Add long tasks above 50ms threshold
  long_task_tracker_.RecordLongTask(
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(4500),
      base::TimeDelta::FromMilliseconds(51));
  EXPECT_EQ(1U, NumLongTasks());

  long_task_tracker_.RecordLongTask(
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(5500),
      base::TimeDelta::FromMilliseconds(75));
  EXPECT_EQ(2U, NumLongTasks());

  // Add task below 50ms threshold
  long_task_tracker_.RecordLongTask(
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(6500),
      base::TimeDelta::FromMilliseconds(49));
  EXPECT_EQ(2U, NumLongTasks());

  LongTaskTracker::LongTaskTiming long_task_times =
      long_task_tracker_.GetLongTaskTiming();
  // GetLongTaskTiming clears the internal vector
  EXPECT_EQ(0U, NumLongTasks());
  // Validate recorded contents
  EXPECT_EQ(2U, long_task_times.size());
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromMilliseconds(4500),
            long_task_times[0].first);
  EXPECT_EQ(51, long_task_times[0].second.InMilliseconds());
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromMilliseconds(5500),
            long_task_times[1].first);
  EXPECT_EQ(75, long_task_times[1].second.InMilliseconds());
}

}  // namespace scheduler
}  // namespace blink
