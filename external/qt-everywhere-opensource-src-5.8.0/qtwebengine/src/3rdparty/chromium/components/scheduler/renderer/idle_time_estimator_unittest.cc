// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/idle_time_estimator.h"

#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "components/scheduler/base/task_queue_manager.h"
#include "components/scheduler/base/test_time_source.h"
#include "components/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduler {

class IdleTimeEstimatorForTest : public IdleTimeEstimator {
 public:
  IdleTimeEstimatorForTest(
      const scoped_refptr<TaskQueue>& compositor_task_runner,
      TestTimeSource* test_time_source,
      int sample_count,
      double estimation_percentile)
      : IdleTimeEstimator(compositor_task_runner,
                          test_time_source,
                          sample_count,
                          estimation_percentile) {}
};

class IdleTimeEstimatorTest : public testing::Test {
 public:
  IdleTimeEstimatorTest()
      : frame_length_(base::TimeDelta::FromMilliseconds(16)) {}

  ~IdleTimeEstimatorTest() override {}

  void SetUp() override {
    clock_.reset(new base::SimpleTestTickClock());
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
    test_time_source_.reset(new TestTimeSource(clock_.get()));
    mock_task_runner_ = make_scoped_refptr(
        new cc::OrderedSimpleTaskRunner(clock_.get(), false));
    main_task_runner_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, base::WrapUnique(new TestTimeSource(clock_.get())));
    manager_ = base::WrapUnique(
        new TaskQueueManager(main_task_runner_, "test.scheduler",
                             "test.scheduler", "test.scheduler.debug"));
    compositor_task_runner_ =
        manager_->NewTaskQueue(TaskQueue::Spec("compositor_tq"));
    estimator_.reset(new IdleTimeEstimatorForTest(
        compositor_task_runner_, test_time_source_.get(), 10, 50));
  }

  void SimulateFrameWithOneCompositorTask(int compositor_time) {
    base::TimeDelta non_idle_time =
        base::TimeDelta::FromMilliseconds(compositor_time);
    base::PendingTask task(FROM_HERE, base::Closure());
    estimator_->WillProcessTask(task);
    clock_->Advance(non_idle_time);
    estimator_->DidCommitFrameToCompositor();
    estimator_->DidProcessTask(task);
    if (non_idle_time < frame_length_)
      clock_->Advance(frame_length_ - non_idle_time);
  }

  void SimulateFrameWithTwoCompositorTasks(int compositor_time1,
                                           int compositor_time2) {
    base::TimeDelta non_idle_time1 =
        base::TimeDelta::FromMilliseconds(compositor_time1);
    base::TimeDelta non_idle_time2 =
        base::TimeDelta::FromMilliseconds(compositor_time2);
    base::PendingTask task(FROM_HERE, base::Closure());
    estimator_->WillProcessTask(task);
    clock_->Advance(non_idle_time1);
    estimator_->DidProcessTask(task);

    estimator_->WillProcessTask(task);
    clock_->Advance(non_idle_time2);
    estimator_->DidCommitFrameToCompositor();
    estimator_->DidProcessTask(task);

    base::TimeDelta idle_time = frame_length_ - non_idle_time1 - non_idle_time2;
    clock_->Advance(idle_time);
  }

  std::unique_ptr<base::SimpleTestTickClock> clock_;
  std::unique_ptr<TestTimeSource> test_time_source_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  scoped_refptr<SchedulerTqmDelegate> main_task_runner_;
  std::unique_ptr<TaskQueueManager> manager_;
  scoped_refptr<TaskQueue> compositor_task_runner_;
  std::unique_ptr<IdleTimeEstimatorForTest> estimator_;
  const base::TimeDelta frame_length_;
};

TEST_F(IdleTimeEstimatorTest, InitialTimeEstimateWithNoData) {
  EXPECT_EQ(frame_length_, estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, BasicEstimation_SteadyState) {
  SimulateFrameWithOneCompositorTask(5);
  SimulateFrameWithOneCompositorTask(5);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, BasicEstimation_Variable) {
  SimulateFrameWithOneCompositorTask(5);
  SimulateFrameWithOneCompositorTask(6);
  SimulateFrameWithOneCompositorTask(7);
  SimulateFrameWithOneCompositorTask(7);
  SimulateFrameWithOneCompositorTask(7);
  SimulateFrameWithOneCompositorTask(8);

  // We expect it to return the median.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(9),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, NoIdleTime) {
  SimulateFrameWithOneCompositorTask(100);
  SimulateFrameWithOneCompositorTask(100);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, Clear) {
  SimulateFrameWithOneCompositorTask(5);
  SimulateFrameWithOneCompositorTask(5);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            estimator_->GetExpectedIdleDuration(frame_length_));
  estimator_->Clear();

  EXPECT_EQ(frame_length_, estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, Estimation_MultipleTasks) {
  SimulateFrameWithTwoCompositorTasks(1, 4);
  SimulateFrameWithTwoCompositorTasks(1, 4);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            estimator_->GetExpectedIdleDuration(frame_length_));
}

TEST_F(IdleTimeEstimatorTest, IgnoresNestedTasks) {
  SimulateFrameWithOneCompositorTask(5);
  SimulateFrameWithOneCompositorTask(5);

  base::PendingTask task(FROM_HERE, base::Closure());
  estimator_->WillProcessTask(task);
  SimulateFrameWithTwoCompositorTasks(4, 4);
  SimulateFrameWithTwoCompositorTasks(4, 4);
  SimulateFrameWithTwoCompositorTasks(4, 4);
  SimulateFrameWithTwoCompositorTasks(4, 4);
  estimator_->DidCommitFrameToCompositor();
  estimator_->DidProcessTask(task);

  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            estimator_->GetExpectedIdleDuration(frame_length_));
}


}  // namespace scheduler
