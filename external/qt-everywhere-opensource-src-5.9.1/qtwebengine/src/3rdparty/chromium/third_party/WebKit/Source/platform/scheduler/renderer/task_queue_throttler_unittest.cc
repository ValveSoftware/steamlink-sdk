// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/task_queue_throttler.h"

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/test_time_source.h"
#include "platform/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace blink {
namespace scheduler {

namespace {
void RunTenTimesTask(size_t* count, scoped_refptr<TaskQueue> timer_queue) {
  if (++(*count) < 10) {
    timer_queue->PostTask(FROM_HERE,
                          base::Bind(&RunTenTimesTask, count, timer_queue));
  }
}

bool IsQueueBlocked(TaskQueue* task_queue) {
  internal::TaskQueueImpl* task_queue_impl =
      reinterpret_cast<internal::TaskQueueImpl*>(task_queue);
  if (!task_queue_impl->IsQueueEnabled())
    return true;
  return task_queue_impl->GetFenceForTest() ==
         static_cast<internal::EnqueueOrder>(
             internal::EnqueueOrderValues::BLOCKING_FENCE);
}
}

class TaskQueueThrottlerTest : public testing::Test {
 public:
  TaskQueueThrottlerTest() {}
  ~TaskQueueThrottlerTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        make_scoped_refptr(new cc::OrderedSimpleTaskRunner(clock_.get(), true));
    delegate_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, base::MakeUnique<TestTimeSource>(clock_.get()));
    scheduler_.reset(new RendererSchedulerImpl(delegate_));
    task_queue_throttler_ = scheduler_->task_queue_throttler();
    timer_queue_ = scheduler_->NewTimerTaskRunner(TaskQueue::QueueType::TEST);
  }

  void TearDown() override {
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  void ExpectThrottled(scoped_refptr<TaskQueue> timer_queue) {
    size_t count = 0;
    timer_queue->PostTask(FROM_HERE,
                          base::Bind(&RunTenTimesTask, &count, timer_queue));

    mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
    EXPECT_LE(count, 1u);

    // Make sure the rest of the tasks run or we risk a UAF on |count|.
    mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(10));
    EXPECT_EQ(10u, count);
  }

  void ExpectUnthrottled(scoped_refptr<TaskQueue> timer_queue) {
    size_t count = 0;
    timer_queue->PostTask(FROM_HERE,
                          base::Bind(&RunTenTimesTask, &count, timer_queue));

    mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(10u, count);
    mock_task_runner_->RunUntilIdle();
  }

 protected:
  std::unique_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delegate_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  scoped_refptr<TaskQueue> timer_queue_;
  TaskQueueThrottler* task_queue_throttler_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(TaskQueueThrottlerTest);
};

TEST_F(TaskQueueThrottlerTest, AlignedThrottledRunTime) {
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.0)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.1)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.2)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.5)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.8)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.9)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(2.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(2.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(1.1)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(9.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(8.0)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(9.0),
            TaskQueueThrottler::AlignedThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(8.1)));
}

namespace {
void TestTask(std::vector<base::TimeTicks>* run_times,
              base::SimpleTestTickClock* clock) {
  run_times->push_back(clock->NowTicks());
}
}  // namespace

TEST_F(TaskQueueThrottlerTest, TimerAlignment) {
  std::vector<base::TimeTicks> run_times;
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(200.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(800.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(1200.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(8300.0));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  mock_task_runner_->RunUntilIdle();

  // Times are aligned to a multipple of 1000 milliseconds.
  EXPECT_THAT(
      run_times,
      ElementsAre(
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(2000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(9000.0)));
}

TEST_F(TaskQueueThrottlerTest, TimerAlignment_Unthrottled) {
  std::vector<base::TimeTicks> run_times;
  base::TimeTicks start_time = clock_->NowTicks();
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(200.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(800.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(1200.0));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(8300.0));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());

  mock_task_runner_->RunUntilIdle();

  // Times are not aligned.
  EXPECT_THAT(
      run_times,
      ElementsAre(start_time + base::TimeDelta::FromMilliseconds(200.0),
                  start_time + base::TimeDelta::FromMilliseconds(800.0),
                  start_time + base::TimeDelta::FromMilliseconds(1200.0),
                  start_time + base::TimeDelta::FromMilliseconds(8300.0)));
}

TEST_F(TaskQueueThrottlerTest, Refcount) {
  ExpectUnthrottled(timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  ExpectUnthrottled(timer_queue_);

  // Should be a NOP.
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  ExpectUnthrottled(timer_queue_);

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);
}

TEST_F(TaskQueueThrottlerTest,
       ThrotlingAnEmptyQueueDoesNotPostPumpThrottledTasksLocked) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  EXPECT_TRUE(task_queue_throttler_->task_runner()->IsEmpty());
}

TEST_F(TaskQueueThrottlerTest, WakeUpForNonDelayedTask) {
  std::vector<base::TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Posting a task should trigger the pump.
  timer_queue_->PostTask(FROM_HERE,
                         base::Bind(&TestTask, &run_times, clock_.get()));

  mock_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() +
                          base::TimeDelta::FromMilliseconds(1000.0)));
}

TEST_F(TaskQueueThrottlerTest, WakeUpForDelayedTask) {
  std::vector<base::TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Posting a task should trigger the pump.
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(1200.0));

  mock_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() +
                          base::TimeDelta::FromMilliseconds(2000.0)));
}

namespace {
bool MessageLoopTaskCounter(size_t* count) {
  *count = *count + 1;
  return true;
}

void NopTask() {}

void AddOneTask(size_t* count) {
  (*count)++;
}

}  // namespace

TEST_F(TaskQueueThrottlerTest,
       SingleThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  timer_queue_->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay);

  size_t task_count = 0;
  mock_task_runner_->RunTasksWhile(
      base::Bind(&MessageLoopTaskCounter, &task_count));

  // Run the task.
  EXPECT_EQ(1u, task_count);
}

TEST_F(TaskQueueThrottlerTest,
       SingleFutureThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  base::TimeDelta delay(base::TimeDelta::FromSecondsD(15.5));
  timer_queue_->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay);

  size_t task_count = 0;
  mock_task_runner_->RunTasksWhile(
      base::Bind(&MessageLoopTaskCounter, &task_count));

  // Run the delayed task.
  EXPECT_EQ(1u, task_count);
}

TEST_F(TaskQueueThrottlerTest,
       TwoFutureThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  std::vector<base::TimeTicks> run_times;

  base::TimeDelta delay(base::TimeDelta::FromSecondsD(15.5));
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&TestTask, &run_times, clock_.get()), delay);

  base::TimeDelta delay2(base::TimeDelta::FromSecondsD(5.5));
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&TestTask, &run_times, clock_.get()), delay2);

  size_t task_count = 0;
  mock_task_runner_->RunTasksWhile(
      base::Bind(&MessageLoopTaskCounter, &task_count));

  // Run both delayed tasks.
  EXPECT_EQ(2u, task_count);

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(6),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(16)));
}

TEST_F(TaskQueueThrottlerTest, TaskDelayIsBasedOnRealTime) {
  std::vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Post an initial task that should run at the first aligned time period.
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(900.0));

  mock_task_runner_->RunUntilIdle();

  // Advance realtime.
  clock_->Advance(base::TimeDelta::FromMilliseconds(250));

  // Post a task that due to real time + delay must run in the third aligned
  // time period.
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(900.0));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000.0),
          base::TimeTicks() + base::TimeDelta::FromMilliseconds(3000.0)));
}

TEST_F(TaskQueueThrottlerTest, ThrottledTasksReportRealTime) {
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(), clock_->NowTicks());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(), clock_->NowTicks());

  clock_->Advance(base::TimeDelta::FromMilliseconds(250));
  // Make sure the throttled time domain's Now() reports the same as the
  // underlying clock.
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(), clock_->NowTicks());
}

TEST_F(TaskQueueThrottlerTest, TaskQueueDisabledTillPump) {
  size_t count = 0;
  timer_queue_->PostTask(FROM_HERE, base::Bind(&AddOneTask, &count));

  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(IsQueueBlocked(timer_queue_.get()));

  mock_task_runner_->RunUntilIdle();  // Wait until the pump.
  EXPECT_EQ(1u, count);               // The task got run
  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
}

TEST_F(TaskQueueThrottlerTest, DoubleIncrementDoubleDecrement) {
  timer_queue_->PostTask(FROM_HERE, base::Bind(&NopTask));

  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
}

TEST_F(TaskQueueThrottlerTest, EnableVirtualTimeThenIncrement) {
  timer_queue_->PostTask(FROM_HERE, base::Bind(&NopTask));

  scheduler_->EnableVirtualTime();
  EXPECT_EQ(timer_queue_->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());

  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  EXPECT_EQ(timer_queue_->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
}

TEST_F(TaskQueueThrottlerTest, IncrementThenEnableVirtualTime) {
  timer_queue_->PostTask(FROM_HERE, base::Bind(&NopTask));

  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(IsQueueBlocked(timer_queue_.get()));

  scheduler_->EnableVirtualTime();
  EXPECT_FALSE(IsQueueBlocked(timer_queue_.get()));
  EXPECT_EQ(timer_queue_->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
}

TEST_F(TaskQueueThrottlerTest, TimeBudgetPool) {
  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);

  base::TimeTicks time_zero = clock_->NowTicks();

  pool->SetTimeBudgetRecoveryRate(time_zero, 0.1);

  EXPECT_TRUE(pool->HasEnoughBudgetToRun(time_zero));
  EXPECT_EQ(time_zero, pool->GetNextAllowedRunTime());

  // Run an expensive task and make sure that we're throttled.
  pool->RecordTaskRunTime(time_zero,
                          time_zero + base::TimeDelta::FromMilliseconds(100));

  EXPECT_FALSE(pool->HasEnoughBudgetToRun(
      time_zero + base::TimeDelta::FromMilliseconds(500)));
  EXPECT_EQ(time_zero + base::TimeDelta::FromMilliseconds(1000),
            pool->GetNextAllowedRunTime());
  EXPECT_TRUE(pool->HasEnoughBudgetToRun(
      time_zero + base::TimeDelta::FromMilliseconds(1000)));

  // Run a cheap task and make sure that it doesn't affect anything.
  EXPECT_TRUE(pool->HasEnoughBudgetToRun(
      time_zero + base::TimeDelta::FromMilliseconds(2000)));
  pool->RecordTaskRunTime(time_zero + base::TimeDelta::FromMilliseconds(2000),
                          time_zero + base::TimeDelta::FromMilliseconds(2020));
  EXPECT_TRUE(pool->HasEnoughBudgetToRun(
      time_zero + base::TimeDelta::FromMilliseconds(2020)));
  EXPECT_EQ(time_zero + base::TimeDelta::FromMilliseconds(2020),
            pool->GetNextAllowedRunTime());

  pool->Close();
}

namespace {

void ExpensiveTestTask(std::vector<base::TimeTicks>* run_times,
                       base::SimpleTestTickClock* clock) {
  run_times->push_back(clock->NowTicks());
  clock->Advance(base::TimeDelta::FromMilliseconds(250));
}

}  // namespace

TEST_F(TaskQueueThrottlerTest, TimeBasedThrottling) {
  std::vector<base::TimeTicks> run_times;

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Submit two tasks. They should be aligned, and second one should be
  // throttled.
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(200));
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(3)));

  pool->RemoveQueue(clock_->NowTicks(), timer_queue_.get());
  run_times.clear();

  // Queue was removed from TimeBudgetPool, only timer alignment should be
  // active now.
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(200));
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(4000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(4250)));

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_F(TaskQueueThrottlerTest, EnableAndDisableTimeBudgetPool) {
  std::vector<base::TimeTicks> run_times;

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);
  EXPECT_TRUE(pool->IsThrottlingEnabled());

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Post an expensive task. Pool is now throttled.
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(1000)));
  run_times.clear();

  LazyNow lazy_now(clock_.get());
  pool->DisableThrottling(&lazy_now);
  EXPECT_FALSE(pool->IsThrottlingEnabled());

  // Pool should not be throttled now.
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(2000)));
  run_times.clear();

  lazy_now = LazyNow(clock_.get());
  pool->EnableThrottling(&lazy_now);
  EXPECT_TRUE(pool->IsThrottlingEnabled());

  // Because time pool was disabled, time budget level did not replenish
  // and queue is throttled.
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(4000)));
  run_times.clear();

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());

  pool->RemoveQueue(clock_->NowTicks(), timer_queue_.get());
  pool->Close();
}

TEST_F(TaskQueueThrottlerTest, ImmediateTasksTimeBudgetThrottling) {
  std::vector<base::TimeTicks> run_times;

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Submit two tasks. They should be aligned, and second one should be
  // throttled.
  timer_queue_->PostTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()));
  timer_queue_->PostTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(3)));

  pool->RemoveQueue(clock_->NowTicks(), timer_queue_.get());
  run_times.clear();

  // Queue was removed from TimeBudgetPool, only timer alignment should be
  // active now.
  timer_queue_->PostTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()));
  timer_queue_->PostTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(4000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(4250)));

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_F(TaskQueueThrottlerTest, TwoQueuesTimeBudgetThrottling) {
  std::vector<base::TimeTicks> run_times;

  scoped_refptr<TaskQueue> second_queue =
      scheduler_->NewTimerTaskRunner(TaskQueue::QueueType::TEST);

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());
  pool->AddQueue(base::TimeTicks(), second_queue.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->IncreaseThrottleRefCount(second_queue.get());

  timer_queue_->PostTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()));
  second_queue->PostTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(3)));

  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(second_queue.get());

  pool->RemoveQueue(clock_->NowTicks(), timer_queue_.get());
  pool->RemoveQueue(clock_->NowTicks(), second_queue.get());

  pool->Close();
}

TEST_F(TaskQueueThrottlerTest, DisabledTimeBudgetDoesNotAffectThrottledQueues) {
  std::vector<base::TimeTicks> run_times;
  LazyNow lazy_now(clock_.get());

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);
  pool->SetTimeBudgetRecoveryRate(lazy_now.Now(), 0.1);
  pool->DisableThrottling(&lazy_now);

  pool->AddQueue(lazy_now.Now(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(100));
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(100));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1250)));
}

TEST_F(TaskQueueThrottlerTest,
       TimeBudgetThrottlingDoesNotAffectUnthrottledQueues) {
  std::vector<base::TimeTicks> run_times;

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);
  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);

  LazyNow lazy_now(clock_.get());
  pool->DisableThrottling(&lazy_now);

  pool->AddQueue(clock_->NowTicks(), timer_queue_.get());

  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(100));
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(100));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(105),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(355)));
}

TEST_F(TaskQueueThrottlerTest, MaxThrottlingDuration) {
  std::vector<base::TimeTicks> run_times;

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool(
          "test", base::nullopt, base::TimeDelta::FromMinutes(1));

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.001);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  for (int i = 0; i < 5; ++i) {
    timer_queue_->PostDelayedTask(
        FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
        base::TimeDelta::FromMilliseconds(200));
  }

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(62),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(123),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(184),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(245)));
}

TEST_F(TaskQueueThrottlerTest, EnableAndDisableThrottling) {
  std::vector<base::TimeTicks> run_times;

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(300));

  // Disable throttling - task should run immediately.
  task_queue_throttler_->DisableThrottling();

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(500));

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(300)));
  run_times.clear();

  // Schedule a task at 900ms. It should proceed as normal.
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(400));

  // Schedule a task at 1200ms. It should proceed as normal.
  // PumpThrottledTasks was scheduled at 1000ms, so it needs to be checked
  // that it was cancelled and it does not interfere with tasks posted before
  // 1s mark and scheduled to run after 1s mark.
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(700));

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(1300));

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(900),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1200)));
  run_times.clear();

  // Schedule a task at 1500ms. It should be throttled because of enabled
  // throttling.
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(1400));

  // Throttling is enabled and new task should be aligned.
  task_queue_throttler_->EnableThrottling();

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(2000)));
}

namespace {

void RecordThrottling(std::vector<base::TimeDelta>* reported_throttling_times,
                      base::TimeDelta throttling_duration) {
  reported_throttling_times->push_back(throttling_duration);
}

}  // namespace

TEST_F(TaskQueueThrottlerTest, ReportThrottling) {
  std::vector<base::TimeTicks> run_times;
  std::vector<base::TimeDelta> reported_throttling_times;

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  pool->SetReportingCallback(
      base::Bind(&RecordThrottling, &reported_throttling_times));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(200));
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(200));
  timer_queue_->PostDelayedTask(
      FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
      base::TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(1),
                          base::TimeTicks() + base::TimeDelta::FromSeconds(3)));

  EXPECT_THAT(reported_throttling_times,
              ElementsAre(base::TimeDelta::FromMilliseconds(1255),
                          base::TimeDelta::FromMilliseconds(1755)));

  pool->RemoveQueue(clock_->NowTicks(), timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_F(TaskQueueThrottlerTest, GrantAdditionalBudget) {
  std::vector<base::TimeTicks> run_times;

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);

  pool->SetTimeBudgetRecoveryRate(base::TimeTicks(), 0.1);
  pool->AddQueue(base::TimeTicks(), timer_queue_.get());
  pool->GrantAdditionalBudget(base::TimeTicks(),
                              base::TimeDelta::FromMilliseconds(500));

  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  // Submit five tasks. First three will not be throttled because they have
  // budget to run.
  for (int i = 0; i < 5; ++i) {
    timer_queue_->PostDelayedTask(
        FROM_HERE, base::Bind(&ExpensiveTestTask, &run_times, clock_.get()),
        base::TimeDelta::FromMilliseconds(200));
  }

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1250),
                  base::TimeTicks() + base::TimeDelta::FromMilliseconds(1500),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(3),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(6)));

  pool->RemoveQueue(clock_->NowTicks(), timer_queue_.get());
  task_queue_throttler_->DecreaseThrottleRefCount(timer_queue_.get());
  pool->Close();
}

TEST_F(TaskQueueThrottlerTest, EnableAndDisableThrottlingAndTimeBudgets) {
  // This test checks that if time budget pool is enabled when throttling
  // is disabled, it does not throttle the queue.
  std::vector<base::TimeTicks> run_times;

  task_queue_throttler_->DisableThrottling();

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  LazyNow lazy_now(clock_.get());
  pool->DisableThrottling(&lazy_now);

  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(100));

  lazy_now = LazyNow(clock_.get());
  pool->EnableThrottling(&lazy_now);

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(200));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(300)));
}

TEST_F(TaskQueueThrottlerTest, AddQueueToBudgetPoolWhenThrottlingDisabled) {
  // This test checks that a task queue is added to time budget pool
  // when throttling is disabled, is does not throttle queue.
  std::vector<base::TimeTicks> run_times;

  task_queue_throttler_->DisableThrottling();

  TaskQueueThrottler::TimeBudgetPool* pool =
      task_queue_throttler_->CreateTimeBudgetPool("test", base::nullopt,
                                                  base::nullopt);
  task_queue_throttler_->IncreaseThrottleRefCount(timer_queue_.get());

  mock_task_runner_->RunUntilTime(base::TimeTicks() +
                                  base::TimeDelta::FromMilliseconds(100));

  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                base::TimeDelta::FromMilliseconds(200));

  pool->AddQueue(base::TimeTicks(), timer_queue_.get());

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_times, ElementsAre(base::TimeTicks() +
                                     base::TimeDelta::FromMilliseconds(300)));
}

}  // namespace scheduler
}  // namespace blink
