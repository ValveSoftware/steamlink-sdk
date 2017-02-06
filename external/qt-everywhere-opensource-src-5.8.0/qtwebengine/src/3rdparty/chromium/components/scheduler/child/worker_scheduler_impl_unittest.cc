// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/worker_scheduler_impl.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "components/scheduler/base/test_time_source.h"
#include "components/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;

namespace scheduler {

namespace {
void NopTask() {
}

int TimeTicksToIntMs(const base::TimeTicks& time) {
  return static_cast<int>((time - base::TimeTicks()).InMilliseconds());
}

void RecordTimelineTask(std::vector<std::string>* timeline,
                        base::SimpleTestTickClock* clock) {
  timeline->push_back(base::StringPrintf("run RecordTimelineTask @ %d",
                                         TimeTicksToIntMs(clock->NowTicks())));
}

void AppendToVectorTestTask(std::vector<std::string>* vector,
                            std::string value) {
  vector->push_back(value);
}

void AppendToVectorIdleTestTask(std::vector<std::string>* vector,
                                std::string value,
                                base::TimeTicks deadline) {
  AppendToVectorTestTask(vector, value);
}

void TimelineIdleTestTask(std::vector<std::string>* timeline,
                          base::TimeTicks deadline) {
  timeline->push_back(base::StringPrintf("run TimelineIdleTestTask deadline %d",
                                         TimeTicksToIntMs(deadline)));
}

};  // namespace

class WorkerSchedulerImplForTest : public WorkerSchedulerImpl {
 public:
  WorkerSchedulerImplForTest(
      scoped_refptr<SchedulerTqmDelegate> main_task_runner,
      base::SimpleTestTickClock* clock_)
      : WorkerSchedulerImpl(main_task_runner),
        clock_(clock_),
        timeline_(nullptr) {}

  void RecordTimelineEvents(std::vector<std::string>* timeline) {
    timeline_ = timeline;
  }

 private:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override {
    if (timeline_) {
      timeline_->push_back(base::StringPrintf("CanEnterLongIdlePeriod @ %d",
                                              TimeTicksToIntMs(now)));
    }
    return WorkerSchedulerImpl::CanEnterLongIdlePeriod(
        now, next_long_idle_period_delay_out);
  }

  void IsNotQuiescent() override {
    if (timeline_) {
      timeline_->push_back(base::StringPrintf(
          "IsNotQuiescent @ %d", TimeTicksToIntMs(clock_->NowTicks())));
    }
    WorkerSchedulerImpl::IsNotQuiescent();
  }

  base::SimpleTestTickClock* clock_;    // NOT OWNED
  std::vector<std::string>* timeline_;  // NOT OWNED
};

class WorkerSchedulerImplTest : public testing::Test {
 public:
  WorkerSchedulerImplTest()
      : clock_(new base::SimpleTestTickClock()),
        mock_task_runner_(new cc::OrderedSimpleTaskRunner(clock_.get(), true)),
        main_task_runner_(SchedulerTqmDelegateForTest::Create(
            mock_task_runner_,
            base::WrapUnique(new TestTimeSource(clock_.get())))),
        scheduler_(
            new WorkerSchedulerImplForTest(main_task_runner_, clock_.get())),
        timeline_(nullptr) {
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
  }

  ~WorkerSchedulerImplTest() override {}

  void TearDown() override {
    // Check that all tests stop posting tasks.
    while (mock_task_runner_->RunUntilIdle()) {
    }
  }

  void Init() {
    scheduler_->Init();
    default_task_runner_ = scheduler_->DefaultTaskRunner();
    idle_task_runner_ = scheduler_->IdleTaskRunner();
    timeline_ = nullptr;
  }

  void RecordTimelineEvents(std::vector<std::string>* timeline) {
    timeline_ = timeline;
    scheduler_->RecordTimelineEvents(timeline);
  }

  void RunUntilIdle() {
    if (timeline_) {
      timeline_->push_back(base::StringPrintf(
          "RunUntilIdle begin @ %d", TimeTicksToIntMs(clock_->NowTicks())));
    }
    mock_task_runner_->RunUntilIdle();
    if (timeline_) {
      timeline_->push_back(base::StringPrintf(
          "RunUntilIdle end @ %d", TimeTicksToIntMs(clock_->NowTicks())));
    }
  }

  // Helper for posting several tasks of specific types. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task type:
  // - 'D': Default task
  // - 'I': Idle task
  void PostTestTasks(std::vector<std::string>* run_order,
                     const std::string& task_descriptor) {
    std::istringstream stream(task_descriptor);
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      switch (task[0]) {
        case 'D':
          default_task_runner_->PostTask(
              FROM_HERE, base::Bind(&AppendToVectorTestTask, run_order, task));
          break;
        case 'I':
          idle_task_runner_->PostIdleTask(
              FROM_HERE,
              base::Bind(&AppendToVectorIdleTestTask, run_order, task));
          break;
        default:
          NOTREACHED();
      }
    }
  }

  static base::TimeDelta maximum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kMaximumIdlePeriodMillis);
  }

 protected:
  std::unique_ptr<base::SimpleTestTickClock> clock_;
  // Only one of mock_task_runner_ or message_loop_ will be set.
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;

  scoped_refptr<SchedulerTqmDelegate> main_task_runner_;
  std::unique_ptr<WorkerSchedulerImplForTest> scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  std::vector<std::string>* timeline_;  // NOT OWNED

  DISALLOW_COPY_AND_ASSIGN(WorkerSchedulerImplTest);
};

TEST_F(WorkerSchedulerImplTest, TestPostDefaultTask) {
  Init();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("D3"), std::string("D4")));
}

TEST_F(WorkerSchedulerImplTest, TestPostIdleTask) {
  Init();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1");

  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("I1")));
}

TEST_F(WorkerSchedulerImplTest, TestPostDefaultAndIdleTasks) {
  Init();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D2"), std::string("D3"),
                                   std::string("D4"), std::string("I1")));
}

TEST_F(WorkerSchedulerImplTest, TestPostDefaultDelayedAndIdleTasks) {
  Init();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D2 D3 D4");

  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&AppendToVectorTestTask, &run_order, "DELAYED"),
      base::TimeDelta::FromMilliseconds(1000));

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D2"), std::string("D3"),
                                   std::string("D4"), std::string("I1"),
                                   std::string("DELAYED")));
}

TEST_F(WorkerSchedulerImplTest, TestIdleTaskWhenIsNotQuiescent) {
  std::vector<std::string> timeline;
  RecordTimelineEvents(&timeline);
  Init();

  timeline.push_back("Post default task");
  // Post a delayed task timed to occur mid way during the long idle period.
  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RecordTimelineTask, base::Unretained(&timeline),
                            base::Unretained(clock_.get())));
  RunUntilIdle();

  timeline.push_back("Post idle task");
  idle_task_runner_->PostIdleTask(FROM_HERE,
                                  base::Bind(&TimelineIdleTestTask, &timeline));

  RunUntilIdle();

  std::string expected_timeline[] = {"CanEnterLongIdlePeriod @ 5",
                                     "Post default task",
                                     "run RecordTimelineTask @ 5",
                                     "Post idle task",
                                     "IsNotQuiescent @ 5",
                                     "CanEnterLongIdlePeriod @ 305",
                                     "run TimelineIdleTestTask deadline 355"};

  EXPECT_THAT(timeline, ElementsAreArray(expected_timeline));
}

TEST_F(WorkerSchedulerImplTest, TestIdleDeadlineWithPendingDelayedTask) {
  std::vector<std::string> timeline;
  RecordTimelineEvents(&timeline);
  Init();

  timeline.push_back("Post delayed and idle tasks");
  // Post a delayed task timed to occur mid way during the long idle period.
  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&RecordTimelineTask, base::Unretained(&timeline),
                            base::Unretained(clock_.get())),
      base::TimeDelta::FromMilliseconds(20));
  idle_task_runner_->PostIdleTask(FROM_HERE,
                                  base::Bind(&TimelineIdleTestTask, &timeline));

  RunUntilIdle();

  std::string expected_timeline[] = {
      "CanEnterLongIdlePeriod @ 5",
      "Post delayed and idle tasks",
      "CanEnterLongIdlePeriod @ 5",
      "run TimelineIdleTestTask deadline 25",  // Note the short 20ms deadline.
      "run RecordTimelineTask @ 25"};

  EXPECT_THAT(timeline, ElementsAreArray(expected_timeline));
}

TEST_F(WorkerSchedulerImplTest,
       TestIdleDeadlineWithPendingDelayedTaskFarInTheFuture) {
  std::vector<std::string> timeline;
  RecordTimelineEvents(&timeline);
  Init();

  timeline.push_back("Post delayed and idle tasks");
  // Post a delayed task timed to occur well after the long idle period.
  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&RecordTimelineTask, base::Unretained(&timeline),
                            base::Unretained(clock_.get())),
      base::TimeDelta::FromMilliseconds(500));
  idle_task_runner_->PostIdleTask(FROM_HERE,
                                  base::Bind(&TimelineIdleTestTask, &timeline));

  RunUntilIdle();

  std::string expected_timeline[] = {
      "CanEnterLongIdlePeriod @ 5",
      "Post delayed and idle tasks",
      "CanEnterLongIdlePeriod @ 5",
      "run TimelineIdleTestTask deadline 55",  // Note the full 50ms deadline.
      "run RecordTimelineTask @ 505"};

  EXPECT_THAT(timeline, ElementsAreArray(expected_timeline));
}

TEST_F(WorkerSchedulerImplTest, TestPostIdleTaskAfterRunningUntilIdle) {
  Init();

  default_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&NopTask), base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 I2 D3");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D3"), std::string("I1"),
                                   std::string("I2")));
}

TEST_F(WorkerSchedulerImplTest, TestLongIdlePeriodTimeline) {
  Init();

  std::vector<std::string> timeline;
  RecordTimelineEvents(&timeline);

  // The scheduler should not run the initiate_next_long_idle_period task if
  // there are no idle tasks and no other task woke up the scheduler, thus
  // the idle period deadline shouldn't update at the end of the current long
  // idle period.
  base::TimeTicks idle_period_deadline =
      scheduler_->CurrentIdleTaskDeadlineForTesting();
  clock_->Advance(maximum_idle_period_duration());
  RunUntilIdle();

  base::TimeTicks new_idle_period_deadline =
      scheduler_->CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline, new_idle_period_deadline);

  // Posting a after-wakeup idle task also shouldn't wake the scheduler or
  // initiate the next long idle period.
  timeline.push_back("PostIdleTaskAfterWakeup");
  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&TimelineIdleTestTask, &timeline));
  RunUntilIdle();
  new_idle_period_deadline = scheduler_->CurrentIdleTaskDeadlineForTesting();

  // Running a normal task should initiate a new long idle period after waiting
  // 300ms for quiescence.
  timeline.push_back("Post RecordTimelineTask");
  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RecordTimelineTask, base::Unretained(&timeline),
                            base::Unretained(clock_.get())));
  RunUntilIdle();

  std::string expected_timeline[] = {
      "RunUntilIdle begin @ 55",
      "RunUntilIdle end @ 55",
      "PostIdleTaskAfterWakeup",
      "RunUntilIdle begin @ 55",  // NOTE idle task doesn't run till later.
      "RunUntilIdle end @ 55",
      "Post RecordTimelineTask",
      "RunUntilIdle begin @ 55",
      "run RecordTimelineTask @ 55",
      "IsNotQuiescent @ 55",  // NOTE we have to wait for quiescence.
      "CanEnterLongIdlePeriod @ 355",
      "run TimelineIdleTestTask deadline 405",
      "RunUntilIdle end @ 355"};

  EXPECT_THAT(timeline, ElementsAreArray(expected_timeline));
}

}  // namespace scheduler
