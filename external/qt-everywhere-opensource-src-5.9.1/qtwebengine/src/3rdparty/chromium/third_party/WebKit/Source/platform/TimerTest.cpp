// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/Timer.h"

#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/child/web_task_runner_impl.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebViewScheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefCounted.h"
#include <memory>
#include <queue>

using testing::ElementsAre;

namespace blink {
namespace {

class TimerTest : public testing::Test {
 public:
  void SetUp() override {
    m_runTimes.clear();
    m_platform.advanceClockSeconds(10.0);
    m_startTime = monotonicallyIncreasingTime();
  }

  void countingTask(TimerBase*) {
    m_runTimes.append(monotonicallyIncreasingTime());
  }

  void recordNextFireTimeTask(TimerBase* timer) {
    m_nextFireTimes.append(monotonicallyIncreasingTime() +
                           timer->nextFireInterval());
  }

  void runUntilDeadline(double deadline) {
    double period = deadline - monotonicallyIncreasingTime();
    EXPECT_GE(period, 0.0);
    m_platform.runForPeriodSeconds(period);
  }

  // Returns false if there are no pending delayed tasks, otherwise sets |time|
  // to the delay in seconds till the next pending delayed task is scheduled to
  // fire.
  bool timeTillNextDelayedTask(double* time) const {
    base::TimeTicks nextRunTime;
    if (!m_platform.rendererScheduler()
             ->TimerTaskRunner()
             ->GetTimeDomain()
             ->NextScheduledRunTime(&nextRunTime))
      return false;
    *time = (nextRunTime -
             m_platform.rendererScheduler()
                 ->TimerTaskRunner()
                 ->GetTimeDomain()
                 ->Now())
                .InSecondsF();
    return true;
  }

 protected:
  double m_startTime;
  WTF::Vector<double> m_runTimes;
  WTF::Vector<double> m_nextFireTimes;
  TestingPlatformSupportWithMockScheduler m_platform;
};

TEST_F(TimerTest, StartOneShot_Zero) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(0, BLINK_FROM_HERE);

  double runTime;
  EXPECT_FALSE(timeTillNextDelayedTask(&runTime));

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime));
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancel) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(0, BLINK_FROM_HERE);

  double runTime;
  EXPECT_FALSE(timeTillNextDelayedTask(&runTime));

  timer.stop();

  m_platform.runUntilIdle();
  EXPECT_FALSE(m_runTimes.size());
}

TEST_F(TimerTest, StartOneShot_ZeroAndCancelThenRepost) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(0, BLINK_FROM_HERE);

  double runTime;
  EXPECT_FALSE(timeTillNextDelayedTask(&runTime));

  timer.stop();

  m_platform.runUntilIdle();
  EXPECT_FALSE(m_runTimes.size());

  timer.startOneShot(0, BLINK_FROM_HERE);

  EXPECT_FALSE(timeTillNextDelayedTask(&runTime));

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime));
}

TEST_F(TimerTest, StartOneShot_Zero_RepostingAfterRunning) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(0, BLINK_FROM_HERE);

  double runTime;
  EXPECT_FALSE(timeTillNextDelayedTask(&runTime));

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime));

  timer.startOneShot(0, BLINK_FROM_HERE);

  EXPECT_FALSE(timeTillNextDelayedTask(&runTime));

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime, m_startTime));
}

TEST_F(TimerTest, StartOneShot_NonZero) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10.0, BLINK_FROM_HERE);

  double runTime;
  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));
  EXPECT_FLOAT_EQ(10.0, runTime);

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancel) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10, BLINK_FROM_HERE);

  double runTime;
  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));
  EXPECT_FLOAT_EQ(10.0, runTime);

  timer.stop();
  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));

  m_platform.runUntilIdle();
  EXPECT_FALSE(m_runTimes.size());
}

TEST_F(TimerTest, StartOneShot_NonZeroAndCancelThenRepost) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10, BLINK_FROM_HERE);

  double runTime;
  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));
  EXPECT_FLOAT_EQ(10.0, runTime);

  timer.stop();
  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));

  m_platform.runUntilIdle();
  EXPECT_FALSE(m_runTimes.size());

  double secondPostTime = monotonicallyIncreasingTime();
  timer.startOneShot(10, BLINK_FROM_HERE);

  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));
  EXPECT_FLOAT_EQ(10.0, runTime);

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(secondPostTime + 10.0));
}

TEST_F(TimerTest, StartOneShot_NonZero_RepostingAfterRunning) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10, BLINK_FROM_HERE);

  double runTime;
  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));
  EXPECT_FLOAT_EQ(10.0, runTime);

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));

  timer.startOneShot(20, BLINK_FROM_HERE);

  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));
  EXPECT_FLOAT_EQ(20.0, runTime);

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0, m_startTime + 30.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithSameRunTimeDoesNothing) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10, BLINK_FROM_HERE);
  timer.startOneShot(10, BLINK_FROM_HERE);

  double runTime;
  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));
  EXPECT_FLOAT_EQ(10.0, runTime);

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithNewerRunTimeCancelsOriginalTask) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10, BLINK_FROM_HERE);
  timer.startOneShot(0, BLINK_FROM_HERE);

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 0.0));
}

TEST_F(TimerTest, PostingTimerTwiceWithLaterRunTimeCancelsOriginalTask) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(0, BLINK_FROM_HERE);
  timer.startOneShot(10, BLINK_FROM_HERE);

  m_platform.runUntilIdle();
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 10.0));
}

TEST_F(TimerTest, StartRepeatingTask) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startRepeating(1.0, BLINK_FROM_HERE);

  double runTime;
  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));
  EXPECT_FLOAT_EQ(1.0, runTime);

  runUntilDeadline(m_startTime + 5.5);
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0,
                                      m_startTime + 3.0, m_startTime + 4.0,
                                      m_startTime + 5.0));
}

TEST_F(TimerTest, StartRepeatingTask_ThenCancel) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startRepeating(1.0, BLINK_FROM_HERE);

  double runTime;
  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));
  EXPECT_FLOAT_EQ(1.0, runTime);

  runUntilDeadline(m_startTime + 2.5);
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0));

  timer.stop();
  m_platform.runUntilIdle();

  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0));
}

TEST_F(TimerTest, StartRepeatingTask_ThenPostOneShot) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startRepeating(1.0, BLINK_FROM_HERE);

  double runTime;
  EXPECT_TRUE(timeTillNextDelayedTask(&runTime));
  EXPECT_FLOAT_EQ(1.0, runTime);

  runUntilDeadline(m_startTime + 2.5);
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0));

  timer.startOneShot(0, BLINK_FROM_HERE);
  m_platform.runUntilIdle();

  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 1.0, m_startTime + 2.0,
                                      m_startTime + 2.5));
}

TEST_F(TimerTest, IsActive_NeverPosted) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);

  EXPECT_FALSE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotZero) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(0, BLINK_FROM_HERE);

  EXPECT_TRUE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_OneShotNonZero) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10, BLINK_FROM_HERE);

  EXPECT_TRUE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterPosting_Repeating) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startRepeating(1.0, BLINK_FROM_HERE);

  EXPECT_TRUE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotZero) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(0, BLINK_FROM_HERE);

  m_platform.runUntilIdle();
  EXPECT_FALSE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_OneShotNonZero) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10, BLINK_FROM_HERE);

  m_platform.runUntilIdle();
  EXPECT_FALSE(timer.isActive());
}

TEST_F(TimerTest, IsActive_AfterRunning_Repeating) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startRepeating(1.0, BLINK_FROM_HERE);

  runUntilDeadline(m_startTime + 10);
  EXPECT_TRUE(timer.isActive());  // It should run until cancelled.
}

TEST_F(TimerTest, NextFireInterval_OneShotZero) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(0, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(0.0, timer.nextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(10.0, timer.nextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_OneShotNonZero_AfterAFewSeconds) {
  m_platform.setAutoAdvanceNowToPendingTasks(false);

  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10, BLINK_FROM_HERE);

  m_platform.advanceClockSeconds(2.0);
  EXPECT_FLOAT_EQ(8.0, timer.nextFireInterval());
}

TEST_F(TimerTest, NextFireInterval_Repeating) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startRepeating(20, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(20.0, timer.nextFireInterval());
}

TEST_F(TimerTest, RepeatInterval_NeverStarted) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);

  EXPECT_FLOAT_EQ(0.0, timer.repeatInterval());
}

TEST_F(TimerTest, RepeatInterval_OneShotZero) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(0, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(0.0, timer.repeatInterval());
}

TEST_F(TimerTest, RepeatInterval_OneShotNonZero) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startOneShot(10, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(0.0, timer.repeatInterval());
}

TEST_F(TimerTest, RepeatInterval_Repeating) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startRepeating(20, BLINK_FROM_HERE);

  EXPECT_FLOAT_EQ(20.0, timer.repeatInterval());
}

TEST_F(TimerTest, AugmentRepeatInterval) {
  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startRepeating(10, BLINK_FROM_HERE);
  EXPECT_FLOAT_EQ(10.0, timer.repeatInterval());
  EXPECT_FLOAT_EQ(10.0, timer.nextFireInterval());

  m_platform.advanceClockSeconds(2.0);
  timer.augmentRepeatInterval(10);

  EXPECT_FLOAT_EQ(20.0, timer.repeatInterval());
  EXPECT_FLOAT_EQ(18.0, timer.nextFireInterval());

  // NOTE setAutoAdvanceNowToPendingTasks(true) (which uses
  // cc::OrderedSimpleTaskRunner) results in somewhat strange behavior of the
  // test clock which breaks this test.  Specifically the test clock advancing
  // logic ignores newly posted delayed tasks and advances too far.
  runUntilDeadline(m_startTime + 50.0);
  EXPECT_THAT(m_runTimes, ElementsAre(m_startTime + 20.0, m_startTime + 40.0));
}

TEST_F(TimerTest, AugmentRepeatInterval_TimerFireDelayed) {
  m_platform.setAutoAdvanceNowToPendingTasks(false);

  Timer<TimerTest> timer(this, &TimerTest::countingTask);
  timer.startRepeating(10, BLINK_FROM_HERE);
  EXPECT_FLOAT_EQ(10.0, timer.repeatInterval());
  EXPECT_FLOAT_EQ(10.0, timer.nextFireInterval());

  m_platform.advanceClockSeconds(123.0);  // Make the timer long overdue.
  timer.augmentRepeatInterval(10);

  EXPECT_FLOAT_EQ(20.0, timer.repeatInterval());
  // The timer is overdue so it should be scheduled to fire immediatly.
  EXPECT_FLOAT_EQ(0.0, timer.nextFireInterval());
}

TEST_F(TimerTest, RepeatingTimerDoesNotDrift) {
  m_platform.setAutoAdvanceNowToPendingTasks(false);

  Timer<TimerTest> timer(this, &TimerTest::recordNextFireTimeTask);
  timer.startRepeating(2.0, BLINK_FROM_HERE);

  recordNextFireTimeTask(
      &timer);  // Next scheduled task to run at m_startTime + 2.0

  // Simulate timer firing early. Next scheduled task to run at
  // m_startTime + 4.0
  m_platform.advanceClockSeconds(1.9);
  runUntilDeadline(monotonicallyIncreasingTime() + 0.2);

  // Next scheduled task to run at m_startTime + 6.0
  m_platform.runForPeriodSeconds(2.0);
  // Next scheduled task to run at m_startTime + 8.0
  m_platform.runForPeriodSeconds(2.1);
  // Next scheduled task to run at m_startTime + 10.0
  m_platform.runForPeriodSeconds(2.9);
  // Next scheduled task to run at m_startTime + 14.0 (skips a beat)
  m_platform.advanceClockSeconds(3.1);
  m_platform.runUntilIdle();
  // Next scheduled task to run at m_startTime + 18.0 (skips a beat)
  m_platform.advanceClockSeconds(4.0);
  m_platform.runUntilIdle();
  // Next scheduled task to run at m_startTime + 28.0 (skips 5 beats)
  m_platform.advanceClockSeconds(10.0);
  m_platform.runUntilIdle();

  EXPECT_THAT(
      m_nextFireTimes,
      ElementsAre(m_startTime + 2.0, m_startTime + 4.0, m_startTime + 6.0,
                  m_startTime + 8.0, m_startTime + 10.0, m_startTime + 14.0,
                  m_startTime + 18.0, m_startTime + 28.0));
}

template <typename TimerFiredClass>
class TimerForTest : public TaskRunnerTimer<TimerFiredClass> {
 public:
  using TimerFiredFunction =
      typename TaskRunnerTimer<TimerFiredClass>::TimerFiredFunction;

  ~TimerForTest() override {}

  TimerForTest(WebTaskRunner* webTaskRunner,
               TimerFiredClass* timerFiredClass,
               TimerFiredFunction timerFiredFunction)
      : TaskRunnerTimer<TimerFiredClass>(webTaskRunner,
                                         timerFiredClass,
                                         timerFiredFunction) {}
};

TEST_F(TimerTest, UserSuppliedWebTaskRunner) {
  scoped_refptr<scheduler::TaskQueue> taskRunner(
      m_platform.rendererScheduler()->NewTimerTaskRunner(
          scheduler::TaskQueue::QueueType::TEST));
  scheduler::WebTaskRunnerImpl webTaskRunner(taskRunner);
  TimerForTest<TimerTest> timer(&webTaskRunner, this, &TimerTest::countingTask);
  timer.startOneShot(0, BLINK_FROM_HERE);

  // Make sure the task was posted on taskRunner.
  EXPECT_FALSE(taskRunner->IsEmpty());
}

}  // namespace
}  // namespace blink
