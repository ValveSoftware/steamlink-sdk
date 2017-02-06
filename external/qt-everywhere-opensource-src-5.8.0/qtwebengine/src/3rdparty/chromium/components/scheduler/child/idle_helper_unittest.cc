// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/child/idle_helper.h"

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "components/scheduler/base/real_time_domain.h"
#include "components/scheduler/base/task_queue.h"
#include "components/scheduler/base/task_queue_manager.h"
#include "components/scheduler/base/test_time_source.h"
#include "components/scheduler/child/scheduler_helper.h"
#include "components/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "components/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Exactly;
using testing::Invoke;
using testing::Return;

namespace scheduler {

namespace {
void AppendToVectorTestTask(std::vector<std::string>* vector,
                            std::string value) {
  vector->push_back(value);
}

void AppendToVectorIdleTestTask(std::vector<std::string>* vector,
                                std::string value,
                                base::TimeTicks deadline) {
  AppendToVectorTestTask(vector, value);
}

void NullTask() {
}

void NullIdleTask(base::TimeTicks deadline) {
}

void AppendToVectorReentrantTask(base::SingleThreadTaskRunner* task_runner,
                                 std::vector<int>* vector,
                                 int* reentrant_count,
                                 int max_reentrant_count) {
  vector->push_back((*reentrant_count)++);
  if (*reentrant_count < max_reentrant_count) {
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(AppendToVectorReentrantTask, base::Unretained(task_runner),
                   vector, reentrant_count, max_reentrant_count));
  }
}

void IdleTestTask(int* run_count,
                  base::TimeTicks* deadline_out,
                  base::TimeTicks deadline) {
  (*run_count)++;
  *deadline_out = deadline;
}

int max_idle_task_reposts = 2;

void RepostingIdleTestTask(SingleThreadIdleTaskRunner* idle_task_runner,
                           int* run_count,
                           base::TimeTicks* deadline_out,
                           base::TimeTicks deadline) {
  if ((*run_count + 1) < max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE,
        base::Bind(&RepostingIdleTestTask, base::Unretained(idle_task_runner),
                   run_count, deadline_out));
  }
  *deadline_out = deadline;
  (*run_count)++;
}

void RepostingUpdateClockIdleTestTask(
    SingleThreadIdleTaskRunner* idle_task_runner,
    int* run_count,
    base::SimpleTestTickClock* clock,
    base::TimeDelta advance_time,
    std::vector<base::TimeTicks>* deadlines,
    base::TimeTicks deadline) {
  if ((*run_count + 1) < max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE, base::Bind(&RepostingUpdateClockIdleTestTask,
                              base::Unretained(idle_task_runner), run_count,
                              clock, advance_time, deadlines));
  }
  deadlines->push_back(deadline);
  (*run_count)++;
  clock->Advance(advance_time);
}

void RepeatingTask(base::SingleThreadTaskRunner* task_runner,
                   int num_repeats,
                   base::TimeDelta delay) {
  if (num_repeats > 1) {
    task_runner->PostDelayedTask(
        FROM_HERE, base::Bind(&RepeatingTask, base::Unretained(task_runner),
                              num_repeats - 1, delay),
        delay);
  }
}

void UpdateClockIdleTestTask(base::SimpleTestTickClock* clock,
                             int* run_count,
                             base::TimeTicks set_time,
                             base::TimeTicks deadline) {
  clock->Advance(set_time - clock->NowTicks());
  (*run_count)++;
}

void UpdateClockToDeadlineIdleTestTask(base::SimpleTestTickClock* clock,
                                       int* run_count,
                                       base::TimeTicks deadline) {
  UpdateClockIdleTestTask(clock, run_count, deadline, deadline);
}

void EndIdlePeriodIdleTask(IdleHelper* idle_helper, base::TimeTicks deadline) {
  idle_helper->EndIdlePeriod();
}

scoped_refptr<SchedulerTqmDelegate> CreateTaskRunnerDelegate(
    base::MessageLoop* message_loop,
    scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner,
    std::unique_ptr<TestTimeSource> test_time_source) {
  if (message_loop)
    return SchedulerTqmDelegateImpl::Create(message_loop,
                                            std::move(test_time_source));

  return SchedulerTqmDelegateForTest::Create(mock_task_runner,
                                             std::move(test_time_source));
}

};  // namespace

class IdleHelperForTest : public IdleHelper, public IdleHelper::Delegate {
 public:
  explicit IdleHelperForTest(
      SchedulerHelper* scheduler_helper,
      base::TimeDelta required_quiescence_duration_before_long_idle_period)
      : IdleHelper(scheduler_helper,
                   this,
                   "test.idle",
                   TRACE_DISABLED_BY_DEFAULT("test.idle"),
                   "TestSchedulerIdlePeriod",
                   required_quiescence_duration_before_long_idle_period) {}

  ~IdleHelperForTest() override {}

  // SchedulerHelperDelegate implementation:
  MOCK_METHOD2(CanEnterLongIdlePeriod,
               bool(base::TimeTicks now,
                    base::TimeDelta* next_long_idle_period_delay_out));

  MOCK_METHOD0(IsNotQuiescent, void());
  MOCK_METHOD0(OnIdlePeriodStarted, void());
  MOCK_METHOD0(OnIdlePeriodEnded, void());
};

class BaseIdleHelperTest : public testing::Test {
 public:
  BaseIdleHelperTest(
      base::MessageLoop* message_loop,
      base::TimeDelta required_quiescence_duration_before_long_idle_period)
      : clock_(new base::SimpleTestTickClock()),
        mock_task_runner_(
            message_loop
                ? nullptr
                : new cc::OrderedSimpleTaskRunner(clock_.get(), false)),
        message_loop_(message_loop),
        main_task_runner_(CreateTaskRunnerDelegate(
            message_loop,
            mock_task_runner_,
            base::WrapUnique(new TestTimeSource(clock_.get())))),
        scheduler_helper_(
            new SchedulerHelper(main_task_runner_,
                                "test.idle",
                                TRACE_DISABLED_BY_DEFAULT("test.idle"),
                                TRACE_DISABLED_BY_DEFAULT("test.idle.debug"))),
        idle_helper_(new IdleHelperForTest(
            scheduler_helper_.get(),
            required_quiescence_duration_before_long_idle_period)),
        default_task_runner_(scheduler_helper_->DefaultTaskRunner()),
        idle_task_runner_(idle_helper_->IdleTaskRunner()) {
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
  }

  ~BaseIdleHelperTest() override {}

  void SetUp() override {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
  }

  void TearDown() override {
    DCHECK(!mock_task_runner_.get() || !message_loop_.get());
    if (mock_task_runner_.get()) {
      // Check that all tests stop posting tasks.
      mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
      while (mock_task_runner_->RunUntilIdle()) {
      }
    } else {
      message_loop_->RunUntilIdle();
    }
  }

  void RunUntilIdle() {
    // Only one of mock_task_runner_ or message_loop_ should be set.
    DCHECK(!mock_task_runner_.get() || !message_loop_.get());
    if (mock_task_runner_.get())
      mock_task_runner_->RunUntilIdle();
    else
      message_loop_->RunUntilIdle();
  }

  template <typename E>
  static void CallForEachEnumValue(E first,
                                   E last,
                                   const char* (*function)(E)) {
    for (E val = first; val < last;
         val = static_cast<E>(static_cast<int>(val) + 1)) {
      (*function)(val);
    }
  }

  static void CheckAllTaskQueueIdToString() {
    CallForEachEnumValue<IdleHelper::IdlePeriodState>(
        IdleHelper::IdlePeriodState::FIRST_IDLE_PERIOD_STATE,
        IdleHelper::IdlePeriodState::IDLE_PERIOD_STATE_COUNT,
        &IdleHelper::IdlePeriodStateToString);
  }

  bool IsInIdlePeriod() const {
    return idle_helper_->IsInIdlePeriod(
        idle_helper_->SchedulerIdlePeriodState());
  }

 protected:
  static base::TimeDelta maximum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kMaximumIdlePeriodMillis);
  }

  static base::TimeDelta retry_enable_long_idle_period_delay() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kRetryEnableLongIdlePeriodDelayMillis);
  }

  static base::TimeDelta minimum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kMinimumIdlePeriodDurationMillis);
  }

  base::TimeTicks CurrentIdleTaskDeadline() {
    return idle_helper_->CurrentIdleTaskDeadline();
  }

  void CheckIdlePeriodStateIs(const char* expected) {
    EXPECT_STREQ(expected, IdleHelper::IdlePeriodStateToString(
                               idle_helper_->SchedulerIdlePeriodState()));
  }

  std::unique_ptr<base::SimpleTestTickClock> clock_;
  // Only one of mock_task_runner_ or message_loop_ will be set.
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  std::unique_ptr<base::MessageLoop> message_loop_;

  scoped_refptr<SchedulerTqmDelegate> main_task_runner_;
  std::unique_ptr<SchedulerHelper> scheduler_helper_;
  std::unique_ptr<IdleHelperForTest> idle_helper_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BaseIdleHelperTest);
};

class IdleHelperTest : public BaseIdleHelperTest {
 public:
  IdleHelperTest() : BaseIdleHelperTest(nullptr, base::TimeDelta()) {}

  ~IdleHelperTest() override {}

  TaskQueueManager* task_queue_manager() const {
    return scheduler_helper_->GetTaskQueueManagerForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleHelperTest);
};

TEST_F(IdleHelperTest, TestPostIdleTask) {
  int run_count = 0;
  base::TimeTicks expected_deadline =
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(2300);
  base::TimeTicks deadline_in_task;

  clock_->Advance(base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      expected_deadline);
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(IdleHelperTest, TestPostIdleTask_EndIdlePeriod) {
  int run_count = 0;
  base::TimeTicks deadline_in_task;

  clock_->Advance(base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  idle_helper_->EndIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);
}

TEST_F(IdleHelperTest, TestRepostingIdleTask) {
  base::TimeTicks actual_deadline;
  int run_count = 0;

  max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingIdleTestTask, base::RetainedRef(idle_task_runner_),
                 &run_count, &actual_deadline));
  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  EXPECT_EQ(1, run_count);

  // Reposted tasks shouldn't run until next idle period.
  RunUntilIdle();
  EXPECT_EQ(1, run_count);

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
}

TEST_F(IdleHelperTest, TestIdleTaskExceedsDeadline) {
  int run_count = 0;

  // Post two UpdateClockToDeadlineIdleTestTask tasks.
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&UpdateClockToDeadlineIdleTestTask, clock_.get(), &run_count));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&UpdateClockToDeadlineIdleTestTask, clock_.get(), &run_count));

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  // Only the first idle task should execute since it's used up the deadline.
  EXPECT_EQ(1, run_count);

  idle_helper_->EndIdlePeriod();
  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  // Second task should be run on the next idle period.
  EXPECT_EQ(2, run_count);
}

TEST_F(IdleHelperTest, TestPostIdleTaskAfterWakeup) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  // Shouldn't run yet as no other task woke up the scheduler.
  EXPECT_EQ(0, run_count);

  // Must start a new idle period before idle task runs.
  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  // Another after wakeup idle task shouldn't wake the scheduler.
  EXPECT_EQ(0, run_count);

  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));

  RunUntilIdle();
  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  // Execution of default task queue task should trigger execution of idle task.
  EXPECT_EQ(2, run_count);
}

TEST_F(IdleHelperTest, TestPostIdleTaskAfterWakeupWhileAwake) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));

  RunUntilIdle();
  // Must start a new idle period before idle task runs.
  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  // Should run as the scheduler was already awakened by the normal task.
  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperTest, TestPostIdleTaskWakesAfterWakeupIdleTask) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  // Must start a new idle period before after-wakeup idle task runs.
  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  // Normal idle task should wake up after-wakeup idle task.
  EXPECT_EQ(2, run_count);
}

class IdleHelperTestWithIdlePeriodObserver : public BaseIdleHelperTest {
 public:
  IdleHelperTestWithIdlePeriodObserver()
      : BaseIdleHelperTest(nullptr, base::TimeDelta()) {}

  ~IdleHelperTestWithIdlePeriodObserver() override {}

  void SetUp() override {
    // Don't set expectations on IdleHelper::Delegate.
  }

  TaskQueueManager* task_queue_manager() const {
    return scheduler_helper_->GetTaskQueueManagerForTesting();
  }

  void ExpectIdlePeriodStartsButNeverEnds() {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(1);
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(0);
  }

  void ExpectIdlePeriodStartsAndEnds(const testing::Cardinality& cardinality) {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(cardinality);
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(cardinality);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleHelperTestWithIdlePeriodObserver);
};

TEST_F(IdleHelperTestWithIdlePeriodObserver, TestEnterButNotExitIdlePeriod) {
  ExpectIdlePeriodStartsButNeverEnds();

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
}

TEST_F(IdleHelperTestWithIdlePeriodObserver, TestEnterAndExitIdlePeriod) {
  BaseIdleHelperTest* fixture = this;
  ON_CALL(*idle_helper_, OnIdlePeriodStarted())
      .WillByDefault(
          Invoke([fixture]() { EXPECT_TRUE(fixture->IsInIdlePeriod()); }));
  ON_CALL(*idle_helper_, OnIdlePeriodEnded())
      .WillByDefault(
          Invoke([fixture]() { EXPECT_FALSE(fixture->IsInIdlePeriod()); }));

  ExpectIdlePeriodStartsAndEnds(Exactly(1));

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  idle_helper_->EndIdlePeriod();
}

class IdleHelperWithMessageLoopTest : public BaseIdleHelperTest {
 public:
  IdleHelperWithMessageLoopTest()
      : BaseIdleHelperTest(new base::MessageLoop(), base::TimeDelta()) {}
  ~IdleHelperWithMessageLoopTest() override {}

  void PostFromNestedRunloop(std::vector<
      std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>>* tasks) {
    base::MessageLoop::ScopedNestableTaskAllower allow(message_loop_.get());
    for (std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>& pair : *tasks) {
      if (pair.second) {
        idle_task_runner_->PostIdleTask(FROM_HERE, pair.first);
      } else {
        idle_task_runner_->PostNonNestableIdleTask(FROM_HERE, pair.first);
      }
    }
    idle_helper_->StartIdlePeriod(
        IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
        clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
    message_loop_->RunUntilIdle();
  }

  void SetUp() override {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(AnyNumber());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleHelperWithMessageLoopTest);
};

TEST_F(IdleHelperWithMessageLoopTest,
       NonNestableIdleTaskDoesntExecuteInNestedLoop) {
  std::vector<std::string> order;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&AppendToVectorIdleTestTask, &order, std::string("1")));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&AppendToVectorIdleTestTask, &order, std::string("2")));

  std::vector<std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::Bind(&AppendToVectorIdleTestTask, &order, std::string("3")),
      false));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::Bind(&AppendToVectorIdleTestTask, &order, std::string("4")), true));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::Bind(&AppendToVectorIdleTestTask, &order, std::string("5")), true));

  default_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&IdleHelperWithMessageLoopTest::PostFromNestedRunloop,
                 base::Unretained(this),
                 base::Unretained(&tasks_to_post_from_nested_loop)));

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(order, testing::ElementsAre(std::string("1"), std::string("2"),
                                          std::string("4"), std::string("5"),
                                          std::string("3")));
}

TEST_F(IdleHelperTestWithIdlePeriodObserver, TestLongIdlePeriod) {
  base::TimeTicks expected_deadline =
      clock_->NowTicks() + maximum_idle_period_duration();
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(1)
      .WillRepeatedly(Return(true));
  ExpectIdlePeriodStartsButNeverEnds();

  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no idle period.

  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(IdleHelperTest, TestLongIdlePeriodWithPendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(30);
  base::TimeTicks expected_deadline = clock_->NowTicks() + pending_task_delay;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        pending_task_delay);

  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(IdleHelperTest, TestLongIdlePeriodWithLatePendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        pending_task_delay);

  // Advance clock until after delayed task was meant to be run.
  clock_->Advance(base::TimeDelta::FromMilliseconds(20));

  // Post an idle task and then EnableLongIdlePeriod. Since there is a late
  // pending delayed task this shouldn't actually start an idle period.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // After the delayed task has been run we should trigger an idle period.
  clock_->Advance(maximum_idle_period_duration());
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperTestWithIdlePeriodObserver, TestLongIdlePeriodRepeating) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  std::vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _))
      .Times(4)
      .WillRepeatedly(Return(true));
  ExpectIdlePeriodStartsAndEnds(AtLeast(2));

  max_idle_task_reposts = 3;
  base::TimeTicks clock_before(clock_->NowTicks());
  base::TimeDelta idle_task_runtime(base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingUpdateClockIdleTestTask,
                 base::RetainedRef(idle_task_runner_), &run_count, clock_.get(),
                 idle_task_runtime, &actual_deadlines));

  // Check each idle task runs in their own idle period.
  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(3, run_count);
  EXPECT_THAT(
      actual_deadlines,
      testing::ElementsAre(
          clock_before + maximum_idle_period_duration(),
          clock_before + idle_task_runtime + maximum_idle_period_duration(),
          clock_before + (2 * idle_task_runtime) +
              maximum_idle_period_duration()));

  max_idle_task_reposts = 5;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingUpdateClockIdleTestTask,
                 base::RetainedRef(idle_task_runner_), &run_count, clock_.get(),
                 idle_task_runtime, &actual_deadlines));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&EndIdlePeriodIdleTask, base::Unretained(idle_helper_.get())));

  // Ensure that reposting tasks stop after EndIdlePeriod is called.
  RunUntilIdle();
  EXPECT_EQ(4, run_count);
}

TEST_F(IdleHelperTest, TestLongIdlePeriodDoesNotWakeScheduler) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  // Start a long idle period and get the time it should end.
  idle_helper_->EnableLongIdlePeriod();
  // The scheduler should not run the enable_next_long_idle_period task if
  // there are no idle tasks and no other task woke up the scheduler, thus
  // the idle period deadline shouldn't update at the end of the current long
  // idle period.
  base::TimeTicks idle_period_deadline = CurrentIdleTaskDeadline();
  clock_->Advance(maximum_idle_period_duration());
  RunUntilIdle();

  base::TimeTicks new_idle_period_deadline = CurrentIdleTaskDeadline();
  EXPECT_EQ(idle_period_deadline, new_idle_period_deadline);

  // Posting a after-wakeup idle task also shouldn't wake the scheduler or
  // initiate the next long idle period.
  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  RunUntilIdle();
  new_idle_period_deadline = CurrentIdleTaskDeadline();
  EXPECT_EQ(idle_period_deadline, new_idle_period_deadline);
  EXPECT_EQ(0, run_count);

  // Running a normal task should initiate a new long idle period though.
  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));
  RunUntilIdle();
  new_idle_period_deadline = CurrentIdleTaskDeadline();
  EXPECT_EQ(idle_period_deadline + maximum_idle_period_duration(),
            new_idle_period_deadline);

  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperTestWithIdlePeriodObserver,
       TestLongIdlePeriodWhenNotCanEnterLongIdlePeriod) {
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(1000);
  base::TimeDelta halfDelay = base::TimeDelta::FromMilliseconds(500);
  base::TimeTicks delayOver = clock_->NowTicks() + delay;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  ON_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _))
      .WillByDefault(Invoke(
          [delay, delayOver](base::TimeTicks now,
                             base::TimeDelta* next_long_idle_period_delay_out) {
            if (now >= delayOver)
              return true;
            *next_long_idle_period_delay_out = delay;
            return false;
          }));

  EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _)).Times(2);
  EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(AnyNumber());

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  // Make sure Idle tasks don't run until the delay has occurred.
  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  clock_->Advance(halfDelay);
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // Delay is finished, idle task should run.
  clock_->Advance(halfDelay);
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperTest, TestLongIdlePeriodImmediatelyRestartsIfMaxDeadline) {
  std::vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  base::TimeTicks clock_before(clock_->NowTicks());
  base::TimeDelta idle_task_runtime(base::TimeDelta::FromMilliseconds(10));

  // The second idle period should happen immediately after the first the
  // they have max deadlines.
  max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingUpdateClockIdleTestTask,
                 base::RetainedRef(idle_task_runner_), &run_count, clock_.get(),
                 idle_task_runtime, &actual_deadlines));

  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
  EXPECT_THAT(
      actual_deadlines,
      testing::ElementsAre(
          clock_before + maximum_idle_period_duration(),
          clock_before + idle_task_runtime + maximum_idle_period_duration()));
}

TEST_F(IdleHelperTest, TestLongIdlePeriodRestartWaitsIfNotMaxDeadline) {
  base::TimeTicks actual_deadline;
  int run_count = 0;

  base::TimeDelta pending_task_delay(base::TimeDelta::FromMilliseconds(20));
  base::TimeDelta idle_task_duration(base::TimeDelta::FromMilliseconds(10));
  base::TimeTicks expected_deadline(clock_->NowTicks() + pending_task_delay +
                                    maximum_idle_period_duration() +
                                    retry_enable_long_idle_period_delay());

  // Post delayed task to ensure idle period doesn't have a max deadline.
  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        pending_task_delay);

  max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingIdleTestTask, base::RetainedRef(idle_task_runner_),
                 &run_count, &actual_deadline));
  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
  clock_->Advance(idle_task_duration);

  // Next idle period shouldn't happen until the pending task has been run.
  RunUntilIdle();
  EXPECT_EQ(1, run_count);

  // Once the pending task is run the new idle period should start.
  clock_->Advance(pending_task_delay - idle_task_duration);

  // Since the idle period tried to start before the pending task ran we have to
  // wait for the idle helper to retry starting the long idle period.
  clock_->Advance(retry_enable_long_idle_period_delay());
  RunUntilIdle();

  EXPECT_EQ(2, run_count);
  EXPECT_EQ(expected_deadline, actual_deadline);
}

TEST_F(IdleHelperTest, TestLongIdlePeriodPaused) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  std::vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  // If there are no idle tasks posted we should start in the paused state.
  idle_helper_->EnableLongIdlePeriod();
  CheckIdlePeriodStateIs("in_long_idle_period_paused");
  // There shouldn't be any delayed tasks posted by the idle helper when paused.
  base::TimeTicks next_pending_delayed_task;
  EXPECT_FALSE(scheduler_helper_->real_time_domain()->NextScheduledRunTime(
      &next_pending_delayed_task));

  // Posting a task should transition us to the an active state.
  max_idle_task_reposts = 2;
  base::TimeTicks clock_before(clock_->NowTicks());
  base::TimeDelta idle_task_runtime(base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingUpdateClockIdleTestTask,
                 base::RetainedRef(idle_task_runner_), &run_count, clock_.get(),
                 idle_task_runtime, &actual_deadlines));
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
  EXPECT_THAT(
      actual_deadlines,
      testing::ElementsAre(
          clock_before + maximum_idle_period_duration(),
          clock_before + idle_task_runtime + maximum_idle_period_duration()));

  // Once all task have been run we should go back to the paused state.
  CheckIdlePeriodStateIs("in_long_idle_period_paused");
  EXPECT_FALSE(scheduler_helper_->real_time_domain()->NextScheduledRunTime(
      &next_pending_delayed_task));

  idle_helper_->EndIdlePeriod();
  CheckIdlePeriodStateIs("not_in_idle_period");
}

TEST_F(IdleHelperTest, TestLongIdlePeriodWhenShutdown) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  scheduler_helper_->Shutdown();

  // We shouldn't be able to enter a long idle period when shutdown
  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  CheckIdlePeriodStateIs("not_in_idle_period");
  EXPECT_EQ(0, run_count);
}

void TestCanExceedIdleDeadlineIfRequiredTask(IdleHelperForTest* idle_helper,
                                             bool* can_exceed_idle_deadline_out,
                                             int* run_count,
                                             base::TimeTicks deadline) {
  *can_exceed_idle_deadline_out =
      idle_helper->CanExceedIdleDeadlineIfRequired();
  (*run_count)++;
}

TEST_F(IdleHelperTest, CanExceedIdleDeadlineIfRequired) {
  int run_count = 0;
  bool can_exceed_idle_deadline = false;

  // Should return false if not in an idle period.
  EXPECT_FALSE(idle_helper_->CanExceedIdleDeadlineIfRequired());

  // Should return false for short idle periods.
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask, idle_helper_.get(),
                 &can_exceed_idle_deadline, &run_count));
  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Should return false for a long idle period which is shortened due to a
  // pending delayed task.
  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask, idle_helper_.get(),
                 &can_exceed_idle_deadline, &run_count));
  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  clock_->Advance(maximum_idle_period_duration());
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask, idle_helper_.get(),
                 &can_exceed_idle_deadline, &run_count));
  RunUntilIdle();
  EXPECT_EQ(3, run_count);
  EXPECT_TRUE(can_exceed_idle_deadline);
}

class IdleHelperWithQuiescencePeriodTest : public BaseIdleHelperTest {
 public:
  enum {
    kQuiescenceDelayMs = 100,
    kLongIdlePeriodMs = 50,
  };

  IdleHelperWithQuiescencePeriodTest()
      : BaseIdleHelperTest(
            nullptr,
            base::TimeDelta::FromMilliseconds(kQuiescenceDelayMs)) {}

  ~IdleHelperWithQuiescencePeriodTest() override {}

  void SetUp() override {
    EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(AnyNumber());
    EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*idle_helper_, IsNotQuiescent()).Times(AnyNumber());
  }

  void MakeNonQuiescent() {
    // Run an arbitrary task so we're deemed to be not quiescent.
    default_task_runner_->PostTask(FROM_HERE, base::Bind(NullTask));
    RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleHelperWithQuiescencePeriodTest);
};

class IdleHelperWithQuiescencePeriodTestWithIdlePeriodObserver :
    public IdleHelperWithQuiescencePeriodTest {
 public:

  IdleHelperWithQuiescencePeriodTestWithIdlePeriodObserver()
      : IdleHelperWithQuiescencePeriodTest() {}

  ~IdleHelperWithQuiescencePeriodTestWithIdlePeriodObserver() override {}

  void SetUp() override {
    // Don't set expectations on IdleHelper::Delegate.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      IdleHelperWithQuiescencePeriodTestWithIdlePeriodObserver);
};


TEST_F(IdleHelperWithQuiescencePeriodTest,
       LongIdlePeriodStartsImmediatelyIfQuiescent) {
  base::TimeTicks actual_deadline;
  int run_count = 0;
  max_idle_task_reposts = 1;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingIdleTestTask, base::RetainedRef(idle_task_runner_),
                 &run_count, &actual_deadline));

  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();

  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperWithQuiescencePeriodTestWithIdlePeriodObserver,
       LongIdlePeriodDoesNotStartsImmediatelyIfBusy) {
  MakeNonQuiescent();
  EXPECT_CALL(*idle_helper_, OnIdlePeriodStarted()).Times(0);
  EXPECT_CALL(*idle_helper_, OnIdlePeriodEnded()).Times(0);
  EXPECT_CALL(*idle_helper_, CanEnterLongIdlePeriod(_, _)).Times(0);
  EXPECT_CALL(*idle_helper_, IsNotQuiescent()).Times(AtLeast(1));

  base::TimeTicks actual_deadline;
  int run_count = 0;
  max_idle_task_reposts = 1;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingIdleTestTask, base::RetainedRef(idle_task_runner_),
                 &run_count, &actual_deadline));

  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();

  EXPECT_EQ(0, run_count);

  scheduler_helper_->Shutdown();
}

TEST_F(IdleHelperWithQuiescencePeriodTest,
       LongIdlePeriodStartsAfterQuiescence) {
  MakeNonQuiescent();
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  // Run a repeating task so we're deemed to be busy for the next 400ms.
  default_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RepeatingTask, base::Unretained(default_task_runner_.get()),
                 10, base::TimeDelta::FromMilliseconds(40)));

  int run_count = 0;
  // In this scenario EnableLongIdlePeriod deems us not to be quiescent 5x in
  // a row.
  base::TimeTicks expected_deadline =
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(
                               5 * kQuiescenceDelayMs + kLongIdlePeriodMs);
  base::TimeTicks deadline_in_task;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(IdleHelperWithQuiescencePeriodTest,
       QuescienceCheckedForAfterLongIdlePeriodEnds) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  idle_task_runner_->PostIdleTask(FROM_HERE, base::Bind(&NullIdleTask));
  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();

  // Post a normal task to make the scheduler non-quiescent.
  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));
  RunUntilIdle();

  // Post an idle task. The idle task won't run initially because the system is
  // not judged to be quiescent, but should be run after the quiescence delay.
  int run_count = 0;
  base::TimeTicks deadline_in_task;
  base::TimeTicks expected_deadline =
      clock_->NowTicks() +
      base::TimeDelta::FromMilliseconds(kQuiescenceDelayMs + kLongIdlePeriodMs);
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();

  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(IdleHelperTest, NoShortIdlePeriodWhenDeadlineTooClose) {
  int run_count = 0;
  base::TimeTicks deadline_in_task;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  base::TimeDelta half_a_ms(base::TimeDelta::FromMicroseconds(50));
  base::TimeTicks less_than_min_deadline(
      clock_->NowTicks() + minimum_idle_period_duration() - half_a_ms);
  base::TimeTicks more_than_min_deadline(
      clock_->NowTicks() + minimum_idle_period_duration() + half_a_ms);

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      less_than_min_deadline);
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  idle_helper_->StartIdlePeriod(
      IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, clock_->NowTicks(),
      more_than_min_deadline);
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

TEST_F(IdleHelperTest, NoLongIdlePeriodWhenDeadlineTooClose) {
  int run_count = 0;
  base::TimeTicks deadline_in_task;

  base::TimeDelta half_a_ms(base::TimeDelta::FromMicroseconds(50));
  base::TimeDelta less_than_min_deadline_duration(
      minimum_idle_period_duration() - half_a_ms);
  base::TimeDelta more_than_min_deadline_duration(
      minimum_idle_period_duration() + half_a_ms);

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        less_than_min_deadline_duration);

  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  idle_helper_->EndIdlePeriod();
  clock_->Advance(maximum_idle_period_duration());
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        more_than_min_deadline_duration);
  idle_helper_->EnableLongIdlePeriod();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

}  // namespace scheduler
