// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/memory_pressure_monitor.h"

#include <memory>

#include "base/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/tracked_objects.h"
#include "components/memory_pressure/memory_pressure_stats_collector.h"
#include "components/memory_pressure/test_memory_pressure_calculator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

namespace {

using MemoryPressureLevel = MemoryPressureMonitor::MemoryPressureLevel;
const MemoryPressureLevel MEMORY_PRESSURE_LEVEL_NONE =
    MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
const MemoryPressureLevel MEMORY_PRESSURE_LEVEL_MODERATE =
    MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
const MemoryPressureLevel MEMORY_PRESSURE_LEVEL_CRITICAL =
    MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;

using testing::_;

}  // namespace

// A mock task runner. This isn't directly a TaskRunner as the reference
// counting confuses gmock.
class LenientMockTaskRunner {
 public:
  MOCK_METHOD3(PostDelayedTask,
               bool(const tracked_objects::Location&,
                    const base::Closure&,
                    base::TimeDelta));
};
using MockTaskRunner = testing::StrictMock<LenientMockTaskRunner>;

// A TaskRunner implementation that wraps a MockTaskRunner.
class TaskRunnerProxy : public base::TaskRunner {
 public:
  // The provided |mock| must outlive this object.
  explicit TaskRunnerProxy(MockTaskRunner* mock) : mock_(mock) {}
  bool RunsTasksOnCurrentThread() const override { return true; }
  bool PostDelayedTask(const tracked_objects::Location& location,
                       const base::Closure& closure,
                       base::TimeDelta delta) override {
    return mock_->PostDelayedTask(location, closure, delta);
  }

 private:
  MockTaskRunner* mock_;
  ~TaskRunnerProxy() override {}
};

class TestMemoryPressureMonitor : public MemoryPressureMonitor {
 public:
  // Expose the callback that is used for scheduled checks.
  using MemoryPressureMonitor::CheckPressureAndUpdateStats;

#if !defined(MEMORY_PRESSURE_IS_POLLING)
  using MemoryPressureMonitor::OnMemoryPressureChanged;
#endif

#if defined(MEMORY_PRESSURE_IS_POLLING)
  TestMemoryPressureMonitor(scoped_refptr<base::TaskRunner> task_runner,
                            base::TickClock* tick_clock,
                            MemoryPressureStatsCollector* stats_collector,
                            MemoryPressureCalculator* calculator,
                            DispatchCallback dispatch_callback)
      : MemoryPressureMonitor(task_runner,
                              tick_clock,
                              stats_collector,
                              calculator,
                              dispatch_callback) {}
#else  // MEMORY_PRESSURE_IS_POLLING
  TestMemoryPressureMonitor(scoped_refptr<base::TaskRunner> task_runner,
                            base::TickClock* tick_clock,
                            MemoryPressureStatsCollector* stats_collector,
                            DispatchCallback dispatch_callback,
                            MemoryPressureLevel initial_level)
      : MemoryPressureMonitor(task_runner,
                              tick_clock,
                              stats_collector,
                              dispatch_callback,
                              initial_level) {}
#endif  // !MEMORY_PRESSURE_IS_POLLING

  // A handful of accessors for unittesting.
  MemoryPressureLevel current_memory_pressure_level() const {
    return current_memory_pressure_level_;
  }
  const std::map<int, base::TimeTicks>& scheduled_checks() const {
    return scheduled_checks_;
  }
  int serial_number() const { return serial_number_; }
};

// A mock dispatch class.
class LenientMockDispatch {
 public:
  MOCK_METHOD1(Dispatch, void(MemoryPressureLevel));
};
using MockDispatch = testing::StrictMock<LenientMockDispatch>;

class MemoryPressureMonitorTest : public testing::Test {
 public:
  MemoryPressureMonitorTest()
      : task_runner_proxy_(new TaskRunnerProxy(&mock_task_runner_)),
        stats_collector_(&tick_clock_) {}

#if defined(MEMORY_PRESSURE_IS_POLLING)
  // Creates a monitor in the given initial pressure level and validates its
  // state.
  void CreateMonitor(MemoryPressureLevel initial_level) {
    calculator_.SetLevel(initial_level);
    Tick(1);  // Advance the clock so it doesn't return zero.

    // Determine the delay with which we expect the task to be posted.
    int delay = MemoryPressureMonitor::kDefaultPollingIntervalMs;
    if (initial_level == MEMORY_PRESSURE_LEVEL_MODERATE)
      delay = MemoryPressureMonitor::kNotificationIntervalPressureModerateMs;
    else if (initial_level == MEMORY_PRESSURE_LEVEL_CRITICAL)
      delay = MemoryPressureMonitor::kNotificationIntervalPressureCriticalMs;

    // The monitor will make one call to the task runner during construction.
    ExpectTaskPosted(delay);
    monitor_.reset(new TestMemoryPressureMonitor(
        task_runner_proxy_, &tick_clock_, &stats_collector_, &calculator_,
        base::Bind(&MockDispatch::Dispatch,
                   base::Unretained(&mock_dispatch_))));
    VerifyAndClearExpectations();
    EXPECT_EQ(1u, monitor_->scheduled_checks().size());

    // The monitor should have made one call immediately to the calculator_.
    EXPECT_EQ(1, calculator_.calls());
    ExpectPressure(initial_level);
    calculator_.ResetCalls();
  }
#else  // MEMORY_PRESSURE_IS_POLLING
  void CreateMonitor(MemoryPressureLevel initial_level) {
    Tick(1);  // Advance the clock so it doesn't return zero.

    // The monitor will make one call to the task runner during construction.
    ExpectTaskPosted(MemoryPressureMonitor::kDefaultPollingIntervalMs);
    monitor_.reset(new TestMemoryPressureMonitor(
        task_runner_proxy_, &tick_clock_, &stats_collector_,
        base::Bind(&MockDispatch::Dispatch, base::Unretained(&mock_dispatch_)),
        initial_level));
    VerifyAndClearExpectations();
    EXPECT_EQ(1u, monitor_->scheduled_checks().size());

    // The monitor should have made one call immediately to the calculator_.
    ExpectPressure(initial_level);
  }
#endif  // !MEMORY_PRESSURE_IS_POLLING

  // Advances the tick clock by the given number of milliseconds.
  void Tick(int ms) {
    tick_clock_.Advance(base::TimeDelta::FromMilliseconds(ms));
  }

  // Sets expectations for tasks scheduled via |mock_task_runner_|.
  void ExpectTaskPosted(int delay_ms) {
    base::TimeDelta delay = base::TimeDelta::FromMilliseconds(delay_ms);
    EXPECT_CALL(mock_task_runner_, PostDelayedTask(_, _, delay))
        .WillOnce(testing::Return(true));
  }

  // Sets up expectations for calls to |mock_dispatch_|.
  void ExpectDispatch(MemoryPressureLevel level) {
    EXPECT_CALL(mock_dispatch_, Dispatch(level));
  }
  void ExpectDispatchModerate() {
    EXPECT_CALL(mock_dispatch_, Dispatch(MEMORY_PRESSURE_LEVEL_MODERATE));
  }
  void ExpectDispatchCritical() {
    EXPECT_CALL(mock_dispatch_, Dispatch(MEMORY_PRESSURE_LEVEL_CRITICAL));
  }

  // Verifies and clears expectations for both |mock_task_runner_| and
  // |mock_dispatch_|.
  void VerifyAndClearExpectations() {
    testing::Mock::VerifyAndClearExpectations(&mock_task_runner_);
    testing::Mock::VerifyAndClearExpectations(&mock_dispatch_);
  }

  // Checks expectations on |monitor_->current_memory_pressure_level()|.
  void ExpectPressure(MemoryPressureLevel level) {
    EXPECT_EQ(level, monitor_->current_memory_pressure_level());
  }
  void ExpectNone() {
    EXPECT_EQ(MEMORY_PRESSURE_LEVEL_NONE,
              monitor_->current_memory_pressure_level());
  }
  void ExpectModerate() {
    EXPECT_EQ(MEMORY_PRESSURE_LEVEL_MODERATE,
              monitor_->current_memory_pressure_level());
  }
  void ExpectCritical() {
    EXPECT_EQ(MEMORY_PRESSURE_LEVEL_CRITICAL,
              monitor_->current_memory_pressure_level());
  }

  MockTaskRunner mock_task_runner_;
  scoped_refptr<TaskRunnerProxy> task_runner_proxy_;
  base::SimpleTestTickClock tick_clock_;
  MemoryPressureStatsCollector stats_collector_;

#if defined(MEMORY_PRESSURE_IS_POLLING)
  TestMemoryPressureCalculator calculator_;
#endif

  MockDispatch mock_dispatch_;
  std::unique_ptr<TestMemoryPressureMonitor> monitor_;
};

TEST_F(MemoryPressureMonitorTest, NormalScheduling) {
  CreateMonitor(MEMORY_PRESSURE_LEVEL_NONE);

  Tick(MemoryPressureMonitor::kDefaultPollingIntervalMs);
  ExpectTaskPosted(MemoryPressureMonitor::kDefaultPollingIntervalMs);
  monitor_->CheckPressureAndUpdateStats(monitor_->serial_number());
  VerifyAndClearExpectations();

  Tick(MemoryPressureMonitor::kDefaultPollingIntervalMs);
  ExpectTaskPosted(MemoryPressureMonitor::kDefaultPollingIntervalMs);
  monitor_->CheckPressureAndUpdateStats(monitor_->serial_number());
  VerifyAndClearExpectations();
}

#if defined(MEMORY_PRESSURE_IS_POLLING)

TEST_F(MemoryPressureMonitorTest, CalculatorNotAlwaysInvoked) {
  CreateMonitor(MEMORY_PRESSURE_LEVEL_NONE);

  // Callback into the monitor too soon and expect nothing to happen.
  Tick(1);
  EXPECT_EQ(MEMORY_PRESSURE_LEVEL_NONE, monitor_->GetCurrentPressureLevel());
  VerifyAndClearExpectations();
  EXPECT_EQ(0, calculator_.calls());
  ExpectNone();
  EXPECT_EQ(1u, monitor_->scheduled_checks().size());

  // Pass sufficient time that the rate limiter will allow another calculation.
  // Don't expect another scheduled call because there's already one pending.
  Tick(MemoryPressureMonitor::kMinimumTimeBetweenSamplesMs);
  EXPECT_EQ(MEMORY_PRESSURE_LEVEL_NONE, monitor_->GetCurrentPressureLevel());
  VerifyAndClearExpectations();
  EXPECT_EQ(1, calculator_.calls());
  ExpectNone();
  EXPECT_EQ(1u, monitor_->scheduled_checks().size());
}

TEST_F(MemoryPressureMonitorTest, NoPressureScheduling) {
  CreateMonitor(MEMORY_PRESSURE_LEVEL_NONE);

  // Step forward by a polling interval. This should cause another scheduled
  // check to be posted.
  Tick(MemoryPressureMonitor::kDefaultPollingIntervalMs);
  ExpectTaskPosted(MemoryPressureMonitor::kDefaultPollingIntervalMs);
  monitor_->CheckPressureAndUpdateStats(monitor_->serial_number());
  VerifyAndClearExpectations();
  ExpectNone();
  EXPECT_EQ(1u, monitor_->scheduled_checks().size());
}

TEST_F(MemoryPressureMonitorTest, NoPressureToModerateViaScheduling) {
  CreateMonitor(MEMORY_PRESSURE_LEVEL_NONE);

  // Step forward by a polling interval. This should cause another scheduled
  // check to be posted.
  calculator_.SetModerate();
  Tick(MemoryPressureMonitor::kDefaultPollingIntervalMs);
  ExpectTaskPosted(
      MemoryPressureMonitor::kNotificationIntervalPressureModerateMs);
  ExpectDispatchModerate();
  monitor_->CheckPressureAndUpdateStats(monitor_->serial_number());
  VerifyAndClearExpectations();
  ExpectModerate();
  EXPECT_EQ(1u, monitor_->scheduled_checks().size());
}

TEST_F(MemoryPressureMonitorTest, NoPressureToCriticalViaScheduling) {
  CreateMonitor(MEMORY_PRESSURE_LEVEL_NONE);

  // Step forward by a polling interval. This should cause another scheduled
  // check to be posted.
  calculator_.SetCritical();
  Tick(MemoryPressureMonitor::kDefaultPollingIntervalMs);
  ExpectTaskPosted(
      MemoryPressureMonitor::kNotificationIntervalPressureCriticalMs);
  ExpectDispatchCritical();
  monitor_->CheckPressureAndUpdateStats(monitor_->serial_number());
  VerifyAndClearExpectations();
  ExpectCritical();
  EXPECT_EQ(1u, monitor_->scheduled_checks().size());
}

TEST_F(MemoryPressureMonitorTest, ModeratePressureToCriticalViaScheduling) {
  CreateMonitor(MEMORY_PRESSURE_LEVEL_MODERATE);

  // Step forward by a polling interval. This should cause another scheduled
  // check to be posted.
  calculator_.SetCritical();
  Tick(MemoryPressureMonitor::kNotificationIntervalPressureModerateMs);
  ExpectTaskPosted(
      MemoryPressureMonitor::kNotificationIntervalPressureCriticalMs);
  ExpectDispatchCritical();
  monitor_->CheckPressureAndUpdateStats(monitor_->serial_number());
  VerifyAndClearExpectations();
  ExpectCritical();
  EXPECT_EQ(1u, monitor_->scheduled_checks().size());
}

TEST_F(MemoryPressureMonitorTest, UnscheduledStateChange) {
  CreateMonitor(MEMORY_PRESSURE_LEVEL_NONE);

  // Callback into the monitor directly. This will change the memory pressure
  // and cause a new call to be scheduled as the higher memory pressure has a
  // higher renotification frequency.
  // NOTE: This test relies implicitly on the following facts:
  //   kMinimumTimeBetweenSamplesMs < kDefaultPollingIntervalMs / 2
  //   kNotificationIntervalPressureCriticalMs < kDefaultPollingIntervalMs / 2
  calculator_.SetCritical();
  Tick(MemoryPressureMonitor::kDefaultPollingIntervalMs / 2);
  ExpectTaskPosted(
      MemoryPressureMonitor::kNotificationIntervalPressureCriticalMs);
  ExpectDispatchCritical();
  EXPECT_EQ(MEMORY_PRESSURE_LEVEL_CRITICAL,
            monitor_->GetCurrentPressureLevel());
  VerifyAndClearExpectations();
  EXPECT_EQ(1, calculator_.calls());
  ExpectCritical();
  EXPECT_EQ(2u, monitor_->scheduled_checks().size());
}

#else  // MEMORY_PRESSURE_IS_POLLING

TEST_F(MemoryPressureMonitorTest, PressureChangeNop) {
  CreateMonitor(MEMORY_PRESSURE_LEVEL_NONE);

  // The pressure hasn't changed so this should be a nop.
  Tick(1);
  monitor_->OnMemoryPressureChanged(MEMORY_PRESSURE_LEVEL_NONE);
  VerifyAndClearExpectations();
  ExpectNone();
  EXPECT_EQ(1u, monitor_->scheduled_checks().size());
}

TEST_F(MemoryPressureMonitorTest, PressureChangeNoNewScheduledTask) {
  CreateMonitor(MEMORY_PRESSURE_LEVEL_NONE);

  // The pressure is increasing and the renotification frequency has changed.
  // However, a pending event is already scheduled soon enough so no new one
  // should be scheduled.
  Tick(MemoryPressureMonitor::kDefaultPollingIntervalMs -
       MemoryPressureMonitor::kNotificationIntervalPressureModerateMs / 2);
  ExpectDispatchModerate();
  monitor_->OnMemoryPressureChanged(MEMORY_PRESSURE_LEVEL_MODERATE);
  VerifyAndClearExpectations();
  ExpectModerate();
  EXPECT_EQ(1u, monitor_->scheduled_checks().size());
}

TEST_F(MemoryPressureMonitorTest, PressureChangeNewScheduledTask) {
  CreateMonitor(MEMORY_PRESSURE_LEVEL_NONE);

  // The pressure is increasing and the renotification frequency has changed.
  // The already scheduled event is too far in the future so a new event should
  // be scheduled.
  Tick(1);
  ExpectTaskPosted(
      MemoryPressureMonitor::kNotificationIntervalPressureCriticalMs);
  ExpectDispatchCritical();
  monitor_->OnMemoryPressureChanged(MEMORY_PRESSURE_LEVEL_CRITICAL);
  VerifyAndClearExpectations();
  ExpectCritical();
  EXPECT_EQ(2u, monitor_->scheduled_checks().size());
}

#endif  // !MEMORY_PRESSURE_IS_POLLING

}  // namespace memory_pressure
