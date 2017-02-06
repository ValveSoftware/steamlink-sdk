// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/throttling_helper.h"

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "components/scheduler/base/test_time_source.h"
#include "components/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/web_frame_scheduler_impl.h"
#include "components/scheduler/renderer/web_view_scheduler_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace scheduler {

namespace {
void CountingTask(size_t* count, scoped_refptr<TaskQueue> timer_queue) {
  if (++(*count) < 10) {
    timer_queue->PostTask(FROM_HERE,
                          base::Bind(&CountingTask, count, timer_queue));
  }
}
}

class ThrottlingHelperTest : public testing::Test {
 public:
  ThrottlingHelperTest() {}
  ~ThrottlingHelperTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    mock_task_runner_ =
        make_scoped_refptr(new cc::OrderedSimpleTaskRunner(clock_.get(), true));
    delegate_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, base::WrapUnique(new TestTimeSource(clock_.get())));
    scheduler_.reset(new RendererSchedulerImpl(delegate_));
    throttling_helper_ = scheduler_->throttling_helper();
    timer_queue_ = scheduler_->NewTimerTaskRunner("test_queue");
  }

  void TearDown() override {
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  void ExpectThrottled(scoped_refptr<TaskQueue> timer_queue) {
    size_t count = 0;
    timer_queue->PostTask(FROM_HERE,
                          base::Bind(&CountingTask, &count, timer_queue));

    mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
    EXPECT_LT(count, 10u);
    mock_task_runner_->RunUntilIdle();
  }

  void ExpectUnthrottled(scoped_refptr<TaskQueue> timer_queue) {
    size_t count = 0;
    timer_queue->PostTask(FROM_HERE,
                          base::Bind(&CountingTask, &count, timer_queue));

    mock_task_runner_->RunForPeriod(base::TimeDelta::FromSeconds(1));
    EXPECT_EQ(count, 10u);
    mock_task_runner_->RunUntilIdle();
  }

 protected:
  std::unique_ptr<base::SimpleTestTickClock> clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> delegate_;
  std::unique_ptr<RendererSchedulerImpl> scheduler_;
  scoped_refptr<TaskQueue> timer_queue_;
  ThrottlingHelper* throttling_helper_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(ThrottlingHelperTest);
};

TEST_F(ThrottlingHelperTest, ThrottledRunTime) {
  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            ThrottlingHelper::ThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.0)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            ThrottlingHelper::ThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.1)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            ThrottlingHelper::ThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.2)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            ThrottlingHelper::ThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.5)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            ThrottlingHelper::ThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.8)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0),
            ThrottlingHelper::ThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(0.9)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(2.0),
            ThrottlingHelper::ThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(1.0)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(2.0),
            ThrottlingHelper::ThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(1.1)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(9.0),
            ThrottlingHelper::ThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(8.0)));

  EXPECT_EQ(base::TimeTicks() + base::TimeDelta::FromSecondsD(9.0),
            ThrottlingHelper::ThrottledRunTime(
                base::TimeTicks() + base::TimeDelta::FromSecondsD(8.1)));
}

namespace {
void TestTask(std::vector<base::TimeTicks>* run_times,
              base::SimpleTestTickClock* clock) {
  run_times->push_back(clock->NowTicks());
}
}  // namespace

TEST_F(ThrottlingHelperTest, TimerAlignment) {
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

  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());

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

TEST_F(ThrottlingHelperTest, TimerAlignment_Unthrottled) {
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

  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  throttling_helper_->DecreaseThrottleRefCount(timer_queue_.get());

  mock_task_runner_->RunUntilIdle();

  // Times are not aligned.
  EXPECT_THAT(
      run_times,
      ElementsAre(start_time + base::TimeDelta::FromMilliseconds(200.0),
                  start_time + base::TimeDelta::FromMilliseconds(800.0),
                  start_time + base::TimeDelta::FromMilliseconds(1200.0),
                  start_time + base::TimeDelta::FromMilliseconds(8300.0)));
}

TEST_F(ThrottlingHelperTest, Refcount) {
  ExpectUnthrottled(timer_queue_.get());

  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);

  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);

  throttling_helper_->DecreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);

  throttling_helper_->DecreaseThrottleRefCount(timer_queue_.get());
  ExpectUnthrottled(timer_queue_);

  // Should be a NOP.
  throttling_helper_->DecreaseThrottleRefCount(timer_queue_.get());
  ExpectUnthrottled(timer_queue_);

  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  ExpectThrottled(timer_queue_);
}

TEST_F(ThrottlingHelperTest,
       ThrotlingAnEmptyQueueDoesNotPostPumpThrottledTasksLocked) {
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());

  EXPECT_TRUE(throttling_helper_->task_runner()->IsEmpty());
}

TEST_F(ThrottlingHelperTest, WakeUpForNonDelayedTask) {
  std::vector<base::TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());

  // Posting a task should trigger the pump.
  timer_queue_->PostTask(FROM_HERE,
                         base::Bind(&TestTask, &run_times, clock_.get()));

  mock_task_runner_->RunUntilIdle();
  EXPECT_THAT(run_times,
              ElementsAre(base::TimeTicks() +
                          base::TimeDelta::FromMilliseconds(1000.0)));
}

TEST_F(ThrottlingHelperTest, WakeUpForDelayedTask) {
  std::vector<base::TimeTicks> run_times;

  // Nothing is posted on timer_queue_ so PumpThrottledTasks will not tick.
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());

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

}  // namespace

TEST_F(ThrottlingHelperTest,
       SingleThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());

  base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
  timer_queue_->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay);

  size_t task_count = 0;
  mock_task_runner_->RunTasksWhile(
      base::Bind(&MessageLoopTaskCounter, &task_count));

  EXPECT_EQ(1u, task_count);
}

TEST_F(ThrottlingHelperTest,
       SingleFutureThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());

  base::TimeDelta delay(base::TimeDelta::FromSecondsD(15.5));
  timer_queue_->PostDelayedTask(FROM_HERE, base::Bind(&NopTask), delay);

  size_t task_count = 0;
  mock_task_runner_->RunTasksWhile(
      base::Bind(&MessageLoopTaskCounter, &task_count));

  EXPECT_EQ(1u, task_count);
}

TEST_F(ThrottlingHelperTest,
       TwoFutureThrottledTaskPumpedAndRunWithNoExtraneousMessageLoopTasks) {
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  std::vector<base::TimeTicks> run_times;

  base::TimeDelta delay(base::TimeDelta::FromSecondsD(15.5));
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                delay);

  base::TimeDelta delay2(base::TimeDelta::FromSecondsD(5.5));
  timer_queue_->PostDelayedTask(FROM_HERE,
                                base::Bind(&TestTask, &run_times, clock_.get()),
                                delay2);

  size_t task_count = 0;
  mock_task_runner_->RunTasksWhile(
      base::Bind(&MessageLoopTaskCounter, &task_count));

  EXPECT_EQ(2u, task_count);  // There are two since the cancelled task runs in
  // the same DoWork batch.

  EXPECT_THAT(
      run_times,
      ElementsAre(base::TimeTicks() + base::TimeDelta::FromSeconds(6),
                  base::TimeTicks() + base::TimeDelta::FromSeconds(16)));
}

TEST_F(ThrottlingHelperTest, TaskDelayIsBasedOnRealTime) {
  std::vector<base::TimeTicks> run_times;

  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());

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

TEST_F(ThrottlingHelperTest, ThrottledTasksReportRealTime) {
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(), clock_->NowTicks());

  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(), clock_->NowTicks());

  clock_->Advance(base::TimeDelta::FromMilliseconds(250));
  // Make sure the throttled time domain's Now() reports the same as the
  // underlying clock.
  EXPECT_EQ(timer_queue_->GetTimeDomain()->Now(), clock_->NowTicks());
}

TEST_F(ThrottlingHelperTest, TaskQueueDisabledTillPump) {
  timer_queue_->PostTask(FROM_HERE, base::Bind(&NopTask));

  EXPECT_TRUE(timer_queue_->IsQueueEnabled());
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(timer_queue_->IsQueueEnabled());

  mock_task_runner_->RunUntilIdle();  // Wait until the pump.
  EXPECT_TRUE(timer_queue_->IsQueueEnabled());
}

TEST_F(ThrottlingHelperTest, TaskQueueUnthrottle_InitiallyEnabled) {
  timer_queue_->PostTask(FROM_HERE, base::Bind(&NopTask));

  timer_queue_->SetQueueEnabled(true);  // NOP
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(timer_queue_->IsQueueEnabled());

  throttling_helper_->DecreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(timer_queue_->IsQueueEnabled());
}

TEST_F(ThrottlingHelperTest, TaskQueueUnthrottle_InitiallyDisabled) {
  timer_queue_->PostTask(FROM_HERE, base::Bind(&NopTask));

  timer_queue_->SetQueueEnabled(false);
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(timer_queue_->IsQueueEnabled());

  throttling_helper_->DecreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(timer_queue_->IsQueueEnabled());
}

TEST_F(ThrottlingHelperTest, SetQueueEnabled_Unthrottled) {
  timer_queue_->PostTask(FROM_HERE, base::Bind(&NopTask));

  throttling_helper_->SetQueueEnabled(timer_queue_.get(), false);
  EXPECT_FALSE(timer_queue_->IsQueueEnabled());

  throttling_helper_->SetQueueEnabled(timer_queue_.get(), true);
  EXPECT_TRUE(timer_queue_->IsQueueEnabled());
}

TEST_F(ThrottlingHelperTest, SetQueueEnabled_DisabledWhileThrottled) {
  timer_queue_->PostTask(FROM_HERE, base::Bind(&NopTask));

  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(timer_queue_->IsQueueEnabled());

  throttling_helper_->SetQueueEnabled(timer_queue_.get(), false);
  throttling_helper_->DecreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(timer_queue_->IsQueueEnabled());
}

TEST_F(ThrottlingHelperTest, TaskQueueDisabledTillPump_ThenManuallyDisabled) {
  timer_queue_->PostTask(FROM_HERE, base::Bind(&NopTask));

  EXPECT_TRUE(timer_queue_->IsQueueEnabled());
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(timer_queue_->IsQueueEnabled());

  mock_task_runner_->RunUntilIdle();  // Wait until the pump.
  EXPECT_TRUE(timer_queue_->IsQueueEnabled());

  throttling_helper_->SetQueueEnabled(timer_queue_.get(), false);
  EXPECT_FALSE(timer_queue_->IsQueueEnabled());
}

TEST_F(ThrottlingHelperTest, DoubleIncrementDoubleDecrement) {
  timer_queue_->PostTask(FROM_HERE, base::Bind(&NopTask));

  EXPECT_TRUE(timer_queue_->IsQueueEnabled());
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  throttling_helper_->IncreaseThrottleRefCount(timer_queue_.get());
  EXPECT_FALSE(timer_queue_->IsQueueEnabled());
  throttling_helper_->DecreaseThrottleRefCount(timer_queue_.get());
  throttling_helper_->DecreaseThrottleRefCount(timer_queue_.get());
  EXPECT_TRUE(timer_queue_->IsQueueEnabled());
}

}  // namespace scheduler
