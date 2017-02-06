// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/delay_based_time_source.h"

#include <stdint.h>

#include "base/test/test_simple_task_runner.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

base::TimeDelta Interval() {
  return base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                           60);
}

TEST(DelayBasedTimeSourceTest, TaskPostedAndTickCalled) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);
  EXPECT_TRUE(timer->Active());
  EXPECT_TRUE(task_runner->HasPendingTask());

  timer->SetNow(timer->Now() + base::TimeDelta::FromMilliseconds(16));
  task_runner->RunPendingTasks();
  EXPECT_TRUE(timer->Active());
  EXPECT_TRUE(client.TickCalled());
}

TEST(DelayBasedTimeSourceTest, TickNotCalledWithTaskPosted) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);
  EXPECT_TRUE(task_runner->HasPendingTask());
  timer->SetActive(false);
  task_runner->RunPendingTasks();
  EXPECT_FALSE(client.TickCalled());
}

TEST(DelayBasedTimeSourceTest, StartTwiceEnqueuesOneTask) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);
  EXPECT_TRUE(task_runner->HasPendingTask());
  task_runner->ClearPendingTasks();
  timer->SetActive(true);
  EXPECT_FALSE(task_runner->HasPendingTask());
}

TEST(DelayBasedTimeSourceTest, StartWhenRunningDoesntTick) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);
  EXPECT_TRUE(task_runner->HasPendingTask());
  task_runner->RunPendingTasks();
  task_runner->ClearPendingTasks();
  timer->SetActive(true);
  EXPECT_FALSE(task_runner->HasPendingTask());
}

// At 60Hz, when the tick returns at exactly the requested next time, make sure
// a 16ms next delay is posted.
TEST(DelayBasedTimeSourceTest, NextDelaySaneWhenExactlyOnRequestedTime) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);
  // Run the first tick.
  task_runner->RunPendingTasks();

  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  timer->SetNow(timer->Now() + Interval());
  task_runner->RunPendingTasks();

  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());
}

// At 60Hz, when the tick returns at slightly after the requested next time,
// make sure a 16ms next delay is posted.
TEST(DelayBasedTimeSourceTest, NextDelaySaneWhenSlightlyAfterRequestedTime) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);
  // Run the first tick.
  task_runner->RunPendingTasks();

  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  timer->SetNow(timer->Now() + Interval() +
                base::TimeDelta::FromMicroseconds(1));
  task_runner->RunPendingTasks();

  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());
}

// At 60Hz, when the tick returns at exactly 2*interval after the requested next
// time, make sure we don't tick unnecessarily.
TEST(DelayBasedTimeSourceTest,
     NextDelaySaneWhenExactlyTwiceAfterRequestedTime) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);
  // Run the first tick.
  task_runner->RunPendingTasks();

  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  timer->SetNow(timer->Now() + 2 * Interval());
  task_runner->RunPendingTasks();

  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());
}

// At 60Hz, when the tick returns at 2*interval and a bit after the requested
// next time, make sure a 16ms next delay is posted.
TEST(DelayBasedTimeSourceTest,
     NextDelaySaneWhenSlightlyAfterTwiceRequestedTime) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);
  // Run the first tick.
  task_runner->RunPendingTasks();

  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  timer->SetNow(timer->Now() + 2 * Interval() +
                base::TimeDelta::FromMicroseconds(1));
  task_runner->RunPendingTasks();

  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());
}

// At 60Hz, when the tick returns halfway to the next frame time, make sure
// a correct next delay value is posted.
TEST(DelayBasedTimeSourceTest, NextDelaySaneWhenHalfAfterRequestedTime) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);
  // Run the first tick.
  task_runner->RunPendingTasks();

  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  timer->SetNow(timer->Now() + Interval() +
                base::TimeDelta::FromMilliseconds(8));
  task_runner->RunPendingTasks();

  EXPECT_EQ(8, task_runner->NextPendingTaskDelay().InMilliseconds());
}

TEST(DelayBasedTimeSourceTest, JitteryRuntimeWithFutureTimebases) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);

  // Run the first tick.
  task_runner->RunPendingTasks();
  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  base::TimeTicks future_timebase = timer->Now() + Interval() * 10;

  // 1ms jitter
  base::TimeDelta jitter1 = base::TimeDelta::FromMilliseconds(1);

  // Tick with +1ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() + jitter1);
  task_runner->RunPendingTasks();
  EXPECT_EQ(15, task_runner->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() - jitter1);
  task_runner->RunPendingTasks();
  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  // Tick with -1ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() - jitter1);
  task_runner->RunPendingTasks();
  EXPECT_EQ(1, task_runner->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() + jitter1);
  task_runner->RunPendingTasks();
  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  // 8 ms jitter
  base::TimeDelta jitter8 = base::TimeDelta::FromMilliseconds(8);

  // Tick with +8ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() + jitter8);
  task_runner->RunPendingTasks();
  EXPECT_EQ(8, task_runner->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() - jitter8);
  task_runner->RunPendingTasks();
  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  // Tick with -8ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() - jitter8);
  task_runner->RunPendingTasks();
  EXPECT_EQ(8, task_runner->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() + jitter8);
  task_runner->RunPendingTasks();
  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  // 15 ms jitter
  base::TimeDelta jitter15  = base::TimeDelta::FromMilliseconds(15);

  // Tick with +15ms jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() + jitter15);
  task_runner->RunPendingTasks();
  EXPECT_EQ(1, task_runner->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() - jitter15);
  task_runner->RunPendingTasks();
  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());

  // Tick with -15ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() - jitter15);
  task_runner->RunPendingTasks();
  EXPECT_EQ(15, task_runner->NextPendingTaskDelay().InMilliseconds());

  // Tick with 0ms of jitter
  future_timebase += Interval();
  timer->SetTimebaseAndInterval(future_timebase, Interval());
  timer->SetNow(timer->Now() + Interval() + jitter15);
  task_runner->RunPendingTasks();
  EXPECT_EQ(16, task_runner->NextPendingTaskDelay().InMilliseconds());
}

TEST(DelayBasedTimeSourceTest, AchievesTargetRateWithNoNoise) {
  int num_iterations = 10;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);

  double total_frame_time = 0.0;
  for (int i = 0; i < num_iterations; ++i) {
    int64_t delay_ms = task_runner->NextPendingTaskDelay().InMilliseconds();

    // accumulate the "delay"
    total_frame_time += delay_ms / 1000.0;

    // Run the callback exactly when asked
    timer->SetNow(timer->Now() + base::TimeDelta::FromMilliseconds(delay_ms));
    task_runner->RunPendingTasks();
  }
  double average_interval =
      total_frame_time / static_cast<double>(num_iterations);
  EXPECT_NEAR(1.0 / 60.0, average_interval, 0.1);
}

TEST(DelayBasedTimeSourceTest, TestDeactivateWhilePending) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());
  timer->SetActive(true);  // Should post a task.
  timer->SetActive(false);
  timer = NULL;
  // Should run the posted task without crashing.
  EXPECT_TRUE(task_runner->HasPendingTask());
  task_runner->RunPendingTasks();
}

TEST(DelayBasedTimeSourceTest, TestDeactivateAndReactivateBeforeNextTickTime) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());

  // Should run the activate task, and pick up a new timebase.
  timer->SetActive(true);
  task_runner->RunPendingTasks();

  // Stop the timer
  timer->SetActive(false);

  // Task will be pending anyway, run it
  task_runner->RunPendingTasks();

  // Start the timer again, but before the next tick time the timer previously
  // planned on using. That same tick time should still be targeted.
  timer->SetNow(timer->Now() + base::TimeDelta::FromMilliseconds(4));
  timer->SetActive(true);
  EXPECT_EQ(12, task_runner->NextPendingTaskDelay().InMilliseconds());
}

TEST(DelayBasedTimeSourceTest, TestDeactivateAndReactivateAfterNextTickTime) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      new base::TestSimpleTaskRunner;
  FakeDelayBasedTimeSourceClient client;
  std::unique_ptr<FakeDelayBasedTimeSource> timer(
      new FakeDelayBasedTimeSource(task_runner.get()));
  timer->SetClient(&client);
  timer->SetTimebaseAndInterval(base::TimeTicks(), Interval());

  // Should run the activate task, and pick up a new timebase.
  timer->SetActive(true);
  task_runner->RunPendingTasks();

  // Stop the timer.
  timer->SetActive(false);

  // Task will be pending anyway, run it.
  task_runner->RunPendingTasks();

  // Start the timer again, but before the next tick time the timer previously
  // planned on using. That same tick time should still be targeted.
  timer->SetNow(timer->Now() + base::TimeDelta::FromMilliseconds(20));
  timer->SetActive(true);
  EXPECT_EQ(13, task_runner->NextPendingTaskDelay().InMilliseconds());
}

}  // namespace
}  // namespace cc
