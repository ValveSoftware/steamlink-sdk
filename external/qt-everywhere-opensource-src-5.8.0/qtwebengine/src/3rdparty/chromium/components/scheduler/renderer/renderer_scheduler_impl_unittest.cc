// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/scheduler/renderer/renderer_scheduler_impl.h"

#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/output/begin_frame_args.h"
#include "cc/test/ordered_simple_task_runner.h"
#include "components/scheduler/base/test_time_source.h"
#include "components/scheduler/child/scheduler_tqm_delegate_for_test.h"
#include "components/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scheduler {

namespace {
class FakeInputEvent : public blink::WebInputEvent {
 public:
  explicit FakeInputEvent(blink::WebInputEvent::Type event_type)
      : WebInputEvent(sizeof(FakeInputEvent)) {
    type = event_type;
  }

  FakeInputEvent(blink::WebInputEvent::Type event_type, int event_modifiers)
      : WebInputEvent(sizeof(FakeInputEvent)) {
    type = event_type;
    modifiers = event_modifiers;
  }
};

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
                           base::TimeTicks deadline) {
  if ((*run_count + 1) < max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE, base::Bind(&RepostingIdleTestTask,
                              base::Unretained(idle_task_runner), run_count));
  }
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

void WillBeginFrameIdleTask(RendererScheduler* scheduler,
                            base::SimpleTestTickClock* clock,
                            base::TimeTicks deadline) {
  scheduler->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
}

void UpdateClockToDeadlineIdleTestTask(base::SimpleTestTickClock* clock,
                                       int* run_count,
                                       base::TimeTicks deadline) {
  clock->Advance(deadline - clock->NowTicks());
  (*run_count)++;
}

void PostingYieldingTestTask(RendererSchedulerImpl* scheduler,
                             base::SingleThreadTaskRunner* task_runner,
                             bool simulate_input,
                             bool* should_yield_before,
                             bool* should_yield_after) {
  *should_yield_before = scheduler->ShouldYieldForHighPriorityWork();
  task_runner->PostTask(FROM_HERE, base::Bind(NullTask));
  if (simulate_input) {
    scheduler->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchMove),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  }
  *should_yield_after = scheduler->ShouldYieldForHighPriorityWork();
}

enum class SimulateInputType {
  None,
  TouchStart,
  TouchEnd,
  GestureScrollBegin,
  GestureScrollEnd
};

void AnticipationTestTask(RendererSchedulerImpl* scheduler,
                          SimulateInputType simulate_input,
                          bool* is_anticipated_before,
                          bool* is_anticipated_after) {
  *is_anticipated_before = scheduler->IsHighPriorityWorkAnticipated();
  switch (simulate_input) {
    case SimulateInputType::None:
      break;

    case SimulateInputType::TouchStart:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::TouchStart),
          RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;

    case SimulateInputType::TouchEnd:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::TouchEnd),
          RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;

    case SimulateInputType::GestureScrollBegin:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::GestureScrollBegin),
          RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;

    case SimulateInputType::GestureScrollEnd:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::GestureScrollEnd),
          RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;
  }
  *is_anticipated_after = scheduler->IsHighPriorityWorkAnticipated();
}

};  // namespace

class RendererSchedulerImplForTest : public RendererSchedulerImpl {
 public:
  using RendererSchedulerImpl::OnIdlePeriodEnded;
  using RendererSchedulerImpl::OnIdlePeriodStarted;
  using RendererSchedulerImpl::EstimateLongestJankFreeTaskDuration;

  RendererSchedulerImplForTest(
      scoped_refptr<SchedulerTqmDelegate> main_task_runner)
      : RendererSchedulerImpl(main_task_runner), update_policy_count_(0) {}

  void UpdatePolicyLocked(UpdateType update_type) override {
    update_policy_count_++;
    RendererSchedulerImpl::UpdatePolicyLocked(update_type);

    std::string use_case = RendererSchedulerImpl::UseCaseToString(
        MainThreadOnly().current_use_case);
    if (MainThreadOnly().touchstart_expected_soon) {
      use_cases_.push_back(use_case + " touchstart expected");
    } else {
      use_cases_.push_back(use_case);
    }
  }

  void EnsureUrgentPolicyUpdatePostedOnMainThread() {
    base::AutoLock lock(any_thread_lock_);
    RendererSchedulerImpl::EnsureUrgentPolicyUpdatePostedOnMainThread(
        FROM_HERE);
  }

  void ScheduleDelayedPolicyUpdate(base::TimeTicks now, base::TimeDelta delay) {
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, delay, now);
  }

  bool BeginMainFrameOnCriticalPath() {
    base::AutoLock lock(any_thread_lock_);
    return AnyThread().begin_main_frame_on_critical_path;
  }

  int update_policy_count_;
  std::vector<std::string> use_cases_;
};

// Lets gtest print human readable Policy values.
::std::ostream& operator<<(::std::ostream& os,
                           const RendererSchedulerImpl::UseCase& use_case) {
  return os << RendererSchedulerImpl::UseCaseToString(use_case);
}

class RendererSchedulerImplTest : public testing::Test {
 public:
  using UseCase = RendererSchedulerImpl::UseCase;

  RendererSchedulerImplTest() : clock_(new base::SimpleTestTickClock()) {
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
  }

  RendererSchedulerImplTest(base::MessageLoop* message_loop)
      : clock_(new base::SimpleTestTickClock()), message_loop_(message_loop) {
    clock_->Advance(base::TimeDelta::FromMicroseconds(5000));
  }

  ~RendererSchedulerImplTest() override {}

  void SetUp() override {
    if (message_loop_) {
      main_task_runner_ = SchedulerTqmDelegateImpl::Create(
          message_loop_.get(),
          base::WrapUnique(new TestTimeSource(clock_.get())));
    } else {
      mock_task_runner_ = make_scoped_refptr(
          new cc::OrderedSimpleTaskRunner(clock_.get(), false));
      main_task_runner_ = SchedulerTqmDelegateForTest::Create(
          mock_task_runner_,
          base::WrapUnique(new TestTimeSource(clock_.get())));
    }
    Initialize(
        base::WrapUnique(new RendererSchedulerImplForTest(main_task_runner_)));
  }

  void Initialize(std::unique_ptr<RendererSchedulerImplForTest> scheduler) {
    scheduler_ = std::move(scheduler);
    default_task_runner_ = scheduler_->DefaultTaskRunner();
    compositor_task_runner_ = scheduler_->CompositorTaskRunner();
    loading_task_runner_ = scheduler_->LoadingTaskRunner();
    idle_task_runner_ = scheduler_->IdleTaskRunner();
    timer_task_runner_ = scheduler_->TimerTaskRunner();
  }

  void TearDown() override {
    DCHECK(!mock_task_runner_.get() || !message_loop_.get());
    scheduler_->Shutdown();
    if (mock_task_runner_.get()) {
      // Check that all tests stop posting tasks.
      mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
      while (mock_task_runner_->RunUntilIdle()) {
      }
    } else {
      message_loop_->RunUntilIdle();
    }
    scheduler_.reset();
  }

  void RunUntilIdle() {
    // Only one of mock_task_runner_ or message_loop_ should be set.
    DCHECK(!mock_task_runner_.get() || !message_loop_.get());
    if (mock_task_runner_.get())
      mock_task_runner_->RunUntilIdle();
    else
      message_loop_->RunUntilIdle();
  }

  void DoMainFrame() {
    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = false;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidCommitFrameToCompositor();
  }

  void DoMainFrameOnCriticalPath() {
    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
  }

  void ForceTouchStartToBeExpectedSoon() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollUpdate),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollEnd),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    clock_->Advance(priority_escalation_after_input_duration() * 2);
    scheduler_->ForceUpdatePolicy();
  }

  void SimulateExpensiveTasks(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
    // RunUntilIdle won't actually run all of the SimpleTestTickClock::Advance
    // tasks unless we set AutoAdvanceNow to true :/
    mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

    // Simulate a bunch of expensive tasks
    for (int i = 0; i < 10; i++) {
      task_runner->PostTask(FROM_HERE,
                            base::Bind(&base::SimpleTestTickClock::Advance,
                                       base::Unretained(clock_.get()),
                                       base::TimeDelta::FromMilliseconds(500)));
    }

    RunUntilIdle();

    // Switch back to not auto-advancing because we want to be in control of
    // when time advances.
    mock_task_runner_->SetAutoAdvanceNowToPendingTasks(false);
  }

  enum class TouchEventPolicy {
    SEND_TOUCH_START,
    DONT_SEND_TOUCH_START,
  };

  void SimulateCompositorGestureStart(TouchEventPolicy touch_event_policy) {
    if (touch_event_policy == TouchEventPolicy::SEND_TOUCH_START) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::TouchStart),
          RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::TouchMove),
          RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::TouchMove),
          RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    }
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollBegin),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollUpdate),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  }

  // Simulate a gesture where there is an active compositor scroll, but no
  // scroll updates are generated. Instead, the main thread handles
  // non-canceleable touch events, making this an effectively main thread
  // driven gesture.
  void SimulateMainThreadGestureWithoutScrollUpdates() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchStart),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchMove),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollBegin),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchMove),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  }

  // Simulate a gesture where the main thread handles touch events but does not
  // preventDefault(), allowing the gesture to turn into a compositor driven
  // gesture. This function also verifies the necessary policy updates are
  // scheduled.
  void SimulateMainThreadGestureWithoutPreventDefault() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchStart),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    // Touchstart policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::TOUCHSTART, ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());

    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchMove),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureTapCancel),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollBegin),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    // Main thread gesture policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
              ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());

    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollUpdate),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchScrollStarted),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchMove),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    // Compositor thread gesture policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::COMPOSITOR_GESTURE,
              ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());
  }

  void SimulateMainThreadGestureStart(TouchEventPolicy touch_event_policy,
                                      blink::WebInputEvent::Type gesture_type) {
    if (touch_event_policy == TouchEventPolicy::SEND_TOUCH_START) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::TouchStart),
          RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(blink::WebInputEvent::TouchStart));

      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::TouchMove),
          RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(blink::WebInputEvent::TouchMove));

      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::TouchMove),
          RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(blink::WebInputEvent::TouchMove));
    }
    if (gesture_type != blink::WebInputEvent::Undefined) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(gesture_type),
          RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(FakeInputEvent(gesture_type));
    }
  }

  void SimulateMainThreadInputHandlingCompositorTask(
      base::TimeDelta begin_main_frame_duration) {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchMove),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    clock_->Advance(begin_main_frame_duration);
    scheduler_->DidHandleInputEventOnMainThread(
        FakeInputEvent(blink::WebInputEvent::TouchMove));
    scheduler_->DidCommitFrameToCompositor();
  }

  void SimulateMainThreadCompositorTask(
      base::TimeDelta begin_main_frame_duration) {
    clock_->Advance(begin_main_frame_duration);
    scheduler_->DidCommitFrameToCompositor();
    simulate_compositor_task_ran_ = true;
  }

  bool SimulatedCompositorTaskPending() const {
    return !simulate_compositor_task_ran_;
  }

  void SimulateTimerTask(base::TimeDelta duration) {
    clock_->Advance(duration);
    simulate_timer_task_ran_ = true;
  }

  void EnableIdleTasks() { DoMainFrame(); }

  UseCase CurrentUseCase() {
    return scheduler_->MainThreadOnly().current_use_case;
  }

  UseCase ForceUpdatePolicyAndGetCurrentUseCase() {
    scheduler_->ForceUpdatePolicy();
    return scheduler_->MainThreadOnly().current_use_case;
  }

  v8::RAILMode RAILMode() {
    return scheduler_->MainThreadOnly().current_policy.rail_mode;
  }

  bool BeginFrameNotExpectedSoon() {
    return scheduler_->MainThreadOnly().begin_frame_not_expected_soon;
  }

  bool TouchStartExpectedSoon() {
    return scheduler_->MainThreadOnly().touchstart_expected_soon;
  }

  bool HaveSeenABeginMainframe() {
    return scheduler_->MainThreadOnly().have_seen_a_begin_main_frame;
  }

  bool LoadingTasksSeemExpensive() {
    return scheduler_->MainThreadOnly().loading_tasks_seem_expensive;
  }

  bool TimerTasksSeemExpensive() {
    return scheduler_->MainThreadOnly().timer_tasks_seem_expensive;
  }

  base::TimeTicks EstimatedNextFrameBegin() {
    return scheduler_->MainThreadOnly().estimated_next_frame_begin;
  }

  int NavigationTaskExpectedCount() {
    return scheduler_->MainThreadOnly().navigation_task_expected_count;
  }

  // Helper for posting several tasks of specific types. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task type:
  // - 'D': Default task
  // - 'C': Compositor task
  // - 'L': Loading task
  // - 'I': Idle task
  // - 'T': Timer task
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
        case 'C':
          compositor_task_runner_->PostTask(
              FROM_HERE, base::Bind(&AppendToVectorTestTask, run_order, task));
          break;
        case 'L':
          loading_task_runner_->PostTask(
              FROM_HERE, base::Bind(&AppendToVectorTestTask, run_order, task));
          break;
        case 'I':
          idle_task_runner_->PostIdleTask(
              FROM_HERE,
              base::Bind(&AppendToVectorIdleTestTask, run_order, task));
          break;
        case 'T':
          timer_task_runner_->PostTask(
              FROM_HERE, base::Bind(&AppendToVectorTestTask, run_order, task));
          break;
        default:
          NOTREACHED();
      }
    }
  }

 protected:
  static base::TimeDelta priority_escalation_after_input_duration() {
    return base::TimeDelta::FromMilliseconds(
        UserModel::kGestureEstimationLimitMillis);
  }

  static base::TimeDelta subsequent_input_expected_after_input_duration() {
    return base::TimeDelta::FromMilliseconds(
        UserModel::kExpectSubsequentGestureMillis);
  }

  static base::TimeDelta maximum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kMaximumIdlePeriodMillis);
  }

  static base::TimeDelta end_idle_when_hidden_delay() {
    return base::TimeDelta::FromMilliseconds(
        RendererSchedulerImpl::kEndIdleWhenHiddenDelayMillis);
  }

  static base::TimeDelta idle_period_starvation_threshold() {
    return base::TimeDelta::FromMilliseconds(
        RendererSchedulerImpl::kIdlePeriodStarvationThresholdMillis);
  }

  static base::TimeDelta suspend_timers_when_backgrounded_delay() {
    return base::TimeDelta::FromMilliseconds(
        RendererSchedulerImpl::kSuspendTimersWhenBackgroundedDelayMillis);
  }

  static base::TimeDelta rails_response_time() {
    return base::TimeDelta::FromMilliseconds(
        RendererSchedulerImpl::kRailsResponseTimeMillis);
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

  static void CheckAllUseCaseToString() {
    CallForEachEnumValue<RendererSchedulerImpl::UseCase>(
        RendererSchedulerImpl::UseCase::FIRST_USE_CASE,
        RendererSchedulerImpl::UseCase::USE_CASE_COUNT,
        &RendererSchedulerImpl::UseCaseToString);
  }

  std::unique_ptr<base::SimpleTestTickClock> clock_;
  // Only one of mock_task_runner_ or message_loop_ will be set.
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  std::unique_ptr<base::MessageLoop> message_loop_;

  scoped_refptr<SchedulerTqmDelegate> main_task_runner_;
  std::unique_ptr<RendererSchedulerImplForTest> scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;
  bool simulate_timer_task_ran_;
  bool simulate_compositor_task_ran_;

  DISALLOW_COPY_AND_ASSIGN(RendererSchedulerImplTest);
};

TEST_F(RendererSchedulerImplTest, TestPostDefaultTask) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 D4");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("D3"), std::string("D4")));
}

TEST_F(RendererSchedulerImplTest, TestPostDefaultAndCompositor) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1");
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::Contains("D1"));
  EXPECT_THAT(run_order, testing::Contains("C1"));
}

TEST_F(RendererSchedulerImplTest, TestRentrantTask) {
  int count = 0;
  std::vector<int> run_order;
  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(AppendToVectorReentrantTask,
                            base::RetainedRef(default_task_runner_), &run_order,
                            &count, 5));
  RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(RendererSchedulerImplTest, TestPostIdleTask) {
  int run_count = 0;
  base::TimeTicks expected_deadline =
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(2300);
  base::TimeTicks deadline_in_task;

  clock_->Advance(base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no WillBeginFrame.

  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run as no DidCommitFrameToCompositor.

  clock_->Advance(base::TimeDelta::FromMilliseconds(1200));
  scheduler_->DidCommitFrameToCompositor();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // We missed the deadline.

  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
  clock_->Advance(base::TimeDelta::FromMilliseconds(800));
  scheduler_->DidCommitFrameToCompositor();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(RendererSchedulerImplTest, TestRepostingIdleTask) {
  int run_count = 0;

  max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&RepostingIdleTestTask,
                            base::RetainedRef(idle_task_runner_), &run_count));
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);

  // Reposted tasks shouldn't run until next idle period.
  RunUntilIdle();
  EXPECT_EQ(1, run_count);

  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
}

TEST_F(RendererSchedulerImplTest, TestIdleTaskExceedsDeadline) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  int run_count = 0;

  // Post two UpdateClockToDeadlineIdleTestTask tasks.
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&UpdateClockToDeadlineIdleTestTask, clock_.get(), &run_count));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&UpdateClockToDeadlineIdleTestTask, clock_.get(), &run_count));

  EnableIdleTasks();
  RunUntilIdle();
  // Only the first idle task should execute since it's used up the deadline.
  EXPECT_EQ(1, run_count);

  EnableIdleTasks();
  RunUntilIdle();
  // Second task should be run on the next idle period.
  EXPECT_EQ(2, run_count);
}

TEST_F(RendererSchedulerImplTest, TestPostIdleTaskAfterWakeup) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  EnableIdleTasks();
  RunUntilIdle();
  // Shouldn't run yet as no other task woke up the scheduler.
  EXPECT_EQ(0, run_count);

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  EnableIdleTasks();
  RunUntilIdle();
  // Another after wakeup idle task shouldn't wake the scheduler.
  EXPECT_EQ(0, run_count);

  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));

  RunUntilIdle();
  EnableIdleTasks();  // Must start a new idle period before idle task runs.
  RunUntilIdle();
  // Execution of default task queue task should trigger execution of idle task.
  EXPECT_EQ(2, run_count);
}

TEST_F(RendererSchedulerImplTest, TestPostIdleTaskAfterWakeupWhileAwake) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));

  RunUntilIdle();
  EnableIdleTasks();  // Must start a new idle period before idle task runs.
  RunUntilIdle();
  // Should run as the scheduler was already awakened by the normal task.
  EXPECT_EQ(1, run_count);
}

TEST_F(RendererSchedulerImplTest, TestPostIdleTaskWakesAfterWakeupIdleTask) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  EnableIdleTasks();
  RunUntilIdle();
  // Must start a new idle period before after-wakeup idle task runs.
  EnableIdleTasks();
  RunUntilIdle();
  // Normal idle task should wake up after-wakeup idle task.
  EXPECT_EQ(2, run_count);
}

TEST_F(RendererSchedulerImplTest, TestDelayedEndIdlePeriodCanceled) {
  int run_count = 0;

  base::TimeTicks deadline_in_task;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  // Trigger the beginning of an idle period for 1000ms.
  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
  DoMainFrame();

  // End the idle period early (after 500ms), and send a WillBeginFrame which
  // specifies that the next idle period should end 1000ms from now.
  clock_->Advance(base::TimeDelta::FromMilliseconds(500));
  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Not currently in an idle period.

  // Trigger the start of the idle period before the task to end the previous
  // idle period has been triggered.
  clock_->Advance(base::TimeDelta::FromMilliseconds(400));
  scheduler_->DidCommitFrameToCompositor();

  // Post a task which simulates running until after the previous end idle
  // period delayed task was scheduled for
  scheduler_->DefaultTaskRunner()->PostTask(FROM_HERE, base::Bind(NullTask));
  clock_->Advance(base::TimeDelta::FromMilliseconds(300));

  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // We should still be in the new idle period.
}

TEST_F(RendererSchedulerImplTest, TestDefaultPolicy) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("C1"), std::string("D2"),
                                   std::string("C2"), std::string("I1")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::NONE, CurrentUseCase());
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_WithTouchHandler) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1"),
                                   std::string("C1"), std::string("C2")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::COMPOSITOR_GESTURE,
            CurrentUseCase());
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutScrollUpdates) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureWithoutScrollUpdates();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
            CurrentUseCase());
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutPreventDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureWithoutPreventDefault();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1"),
                                   std::string("C1"), std::string("C2")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::COMPOSITOR_GESTURE,
            CurrentUseCase());
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_LongGestureDuration) {
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);

  base::TimeTicks loop_end_time =
      clock_->NowTicks() + base::TimeDelta::FromMilliseconds(
                               UserModel::kMedianGestureDurationMillis * 2);

  // The UseCase::COMPOSITOR_GESTURE usecase initially deprioritizes compositor
  // tasks (see TestCompositorPolicy_CompositorHandlesInput_WithTouchHandler)
  // but if the gesture is long enough, compositor tasks get prioritized again.
  while (clock_->NowTicks() < loop_end_time) {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchMove),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    clock_->Advance(base::TimeDelta::FromMilliseconds(16));
    RunUntilIdle();
  }

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("L1"), std::string("D1"),
                                   std::string("D2")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::COMPOSITOR_GESTURE,
            CurrentUseCase());
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_WithoutTouchHandler) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::DONT_SEND_TOUCH_START);
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1"),
                                   std::string("C1"), std::string("C2")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::COMPOSITOR_GESTURE,
            CurrentUseCase());
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithTouchHandler) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureStart(TouchEventPolicy::SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollBegin);
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
            CurrentUseCase());
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart));
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutTouchHandler) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateMainThreadGestureStart(TouchEventPolicy::DONT_SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollBegin);
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
            CurrentUseCase());
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart));
}

TEST_F(RendererSchedulerImplTest, TestCompositorPolicy_DidAnimateForInput) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  scheduler_->DidAnimateForInputOnCompositorThread();
  // Note DidAnimateForInputOnCompositorThread does not by itself trigger a
  // policy update.
  EXPECT_EQ(RendererSchedulerImpl::UseCase::COMPOSITOR_GESTURE,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("I1"), std::string("C1"),
                                   std::string("C2")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::COMPOSITOR_GESTURE,
            CurrentUseCase());
}

TEST_F(RendererSchedulerImplTest, Navigation_ResetsTaskCostEstimations) {
  std::vector<std::string> run_order;

  SimulateExpensiveTasks(timer_task_runner_);
  scheduler_->OnNavigationStarted();
  PostTestTasks(&run_order, "C1 T1");

  SimulateMainThreadGestureStart(TouchEventPolicy::DONT_SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollBegin);
  scheduler_->DidCommitFrameToCompositor();  // Starts Idle Period
  RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveTimersDontRunWhenMainThreadScrolling) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(timer_task_runner_);
  DoMainFrame();
  SimulateMainThreadGestureStart(TouchEventPolicy::SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollUpdate);

  PostTestTasks(&run_order, "C1 T1");

  RunUntilIdle();
  EXPECT_FALSE(TouchStartExpectedSoon());
  EXPECT_EQ(RendererSchedulerImpl::UseCase::MAIN_THREAD_GESTURE,
            CurrentUseCase());

  EXPECT_THAT(run_order, testing::ElementsAre(std::string("C1")));
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveTimersDoRunWhenMainThreadInputHandling) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(timer_task_runner_);
  DoMainFrame();
  SimulateMainThreadGestureStart(TouchEventPolicy::SEND_TOUCH_START,
                                 blink::WebInputEvent::Undefined);

  PostTestTasks(&run_order, "C1 T1");

  RunUntilIdle();
  EXPECT_FALSE(TouchStartExpectedSoon());
  EXPECT_EQ(RendererSchedulerImpl::UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
            CurrentUseCase());

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveTimersDoRunWhenMainThreadScrolling_AndOnCriticalPath) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(timer_task_runner_);
  DoMainFrameOnCriticalPath();
  SimulateMainThreadGestureStart(TouchEventPolicy::SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollBegin);

  PostTestTasks(&run_order, "C1 T1");

  RunUntilIdle();
  EXPECT_FALSE(TouchStartExpectedSoon());
  EXPECT_EQ(RendererSchedulerImpl::UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
            CurrentUseCase());

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));
}

TEST_F(RendererSchedulerImplTest, TestTouchstartPolicy_Compositor) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2 T1 T2");

  // Observation of touchstart should defer execution of timer, idle and loading
  // tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  // Animation or meta events like TapDown/FlingCancel shouldn't affect the
  // priority.
  run_order.clear();
  scheduler_->DidAnimateForInputOnCompositorThread();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingCancel),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureTapDown),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Action events like ScrollBegin will kick us back into compositor priority,
  // allowing service of the timer, loading and idle queues.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureScrollBegin),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("T1"),
                                   std::string("T2")));
}

TEST_F(RendererSchedulerImplTest, TestTouchstartPolicy_MainThread) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2 T1 T2");

  // Observation of touchstart should defer execution of timer, idle and loading
  // tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart));
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  // Meta events like TapDown/FlingCancel shouldn't affect the priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingCancel),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingCancel));
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureTapDown),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureTapDown));
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Action events like ScrollBegin will kick us back into compositor priority,
  // allowing service of the timer, loading and idle queues.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureScrollBegin),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureScrollBegin));
  RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("T1"),
                                   std::string("T2")));
}

// TODO(alexclarke): Reenable once we've reinstaed the Loading UseCase.
TEST_F(RendererSchedulerImplTest, DISABLED_LoadingUseCase) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 T1 L1 D2 C2 T2 L2");

  scheduler_->OnNavigationStarted();
  EnableIdleTasks();
  RunUntilIdle();

  // In loading policy, loading tasks are prioritized other others.
  std::string loading_policy_expected[] = {
      std::string("D1"), std::string("L1"), std::string("D2"),
      std::string("L2"), std::string("C1"), std::string("T1"),
      std::string("C2"), std::string("T2"), std::string("I1")};
  EXPECT_THAT(run_order, testing::ElementsAreArray(loading_policy_expected));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::LOADING, CurrentUseCase());

  // Advance 15s and try again, the loading policy should have ended and the
  // task order should return to the NONE use case where loading tasks are no
  // longer prioritized.
  clock_->Advance(base::TimeDelta::FromMilliseconds(150000));
  run_order.clear();
  PostTestTasks(&run_order, "I1 D1 C1 T1 L1 D2 C2 T2 L2");
  EnableIdleTasks();
  RunUntilIdle();

  std::string default_order_expected[] = {
      std::string("D1"), std::string("C1"), std::string("T1"),
      std::string("L1"), std::string("D2"), std::string("C2"),
      std::string("T2"), std::string("L2"), std::string("I1")};
  EXPECT_THAT(run_order, testing::ElementsAreArray(default_order_expected));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::NONE, CurrentUseCase());
}

TEST_F(RendererSchedulerImplTest,
       EventConsumedOnCompositorThread_IgnoresMouseMove_WhenMouseUp) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseMove),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_EQ(RendererSchedulerImpl::UseCase::NONE, CurrentUseCase());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
}

TEST_F(RendererSchedulerImplTest,
       EventForwardedToMainThread_IgnoresMouseMove_WhenMouseUp) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseMove),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_EQ(RendererSchedulerImpl::UseCase::NONE, CurrentUseCase());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
}

TEST_F(RendererSchedulerImplTest,
       EventConsumedOnCompositorThread_MouseMove_WhenMouseDown) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseMove,
                     blink::WebInputEvent::LeftButtonDown),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
}

TEST_F(RendererSchedulerImplTest,
       EventForwardedToMainThread_MouseMove_WhenMouseDown) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseMove,
                     blink::WebInputEvent::LeftButtonDown),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
  scheduler_->DidHandleInputEventOnMainThread(FakeInputEvent(
      blink::WebInputEvent::MouseMove, blink::WebInputEvent::LeftButtonDown));
}

TEST_F(RendererSchedulerImplTest, EventConsumedOnCompositorThread_MouseWheel) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseWheel),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
}

TEST_F(RendererSchedulerImplTest, EventForwardedToMainThread_MouseWheel) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::MouseWheel),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
            CurrentUseCase());
}

TEST_F(RendererSchedulerImplTest,
       EventConsumedOnCompositorThread_IgnoresKeyboardEvents) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::KeyDown),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::NONE, CurrentUseCase());
}

TEST_F(RendererSchedulerImplTest,
       EventForwardedToMainThread_IgnoresKeyboardEvents) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::KeyDown),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
  EXPECT_EQ(RendererSchedulerImpl::UseCase::NONE, CurrentUseCase());
  // Note compositor tasks are not prioritized.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::KeyDown));
}

TEST_F(RendererSchedulerImplTest,
       TestMainthreadScrollingUseCaseDoesNotStarveDefaultTasks) {
  SimulateMainThreadGestureStart(TouchEventPolicy::DONT_SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollBegin);
  EnableIdleTasks();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1");

  for (int i = 0; i < 20; i++) {
    compositor_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));
  }
  PostTestTasks(&run_order, "C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  // Ensure that the default D1 task gets to run at some point before the final
  // C2 compositor task.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("D1"),
                                   std::string("C2")));
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicyEnds_CompositorHandlesInput) {
  SimulateCompositorGestureStart(TouchEventPolicy::DONT_SEND_TOUCH_START);
  EXPECT_EQ(UseCase::COMPOSITOR_GESTURE,
            ForceUpdatePolicyAndGetCurrentUseCase());

  clock_->Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(RendererSchedulerImplTest,
       TestCompositorPolicyEnds_MainThreadHandlesInput) {
  SimulateMainThreadGestureStart(TouchEventPolicy::DONT_SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollBegin);
  EXPECT_EQ(UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
            ForceUpdatePolicyAndGetCurrentUseCase());

  clock_->Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(RendererSchedulerImplTest, TestTouchstartPolicyEndsAfterTimeout) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  run_order.clear();
  clock_->Advance(base::TimeDelta::FromMilliseconds(1000));

  // Don't post any compositor tasks to simulate a very long running event
  // handler.
  PostTestTasks(&run_order, "D1 D2");

  // Touchstart policy mode should have ended now that the clock has advanced.
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("D2")));
}

TEST_F(RendererSchedulerImplTest,
       TestTouchstartPolicyEndsAfterConsecutiveTouchmoves) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2");

  // Observation of touchstart should defer execution of idle and loading tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  // Receiving the first touchmove will not affect scheduler priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  // Receiving the second touchmove will kick us back into compositor priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("L1")));
}

TEST_F(RendererSchedulerImplTest, TestIsHighPriorityWorkAnticipated) {
  bool is_anticipated_before = false;
  bool is_anticipated_after = false;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AnticipationTestTask, scheduler_.get(),
                            SimulateInputType::None, &is_anticipated_before,
                            &is_anticipated_after));
  RunUntilIdle();
  // In its default state, without input receipt, the scheduler should indicate
  // that no high-priority is anticipated.
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_FALSE(is_anticipated_after);

  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AnticipationTestTask, scheduler_.get(),
                            SimulateInputType::TouchStart,
                            &is_anticipated_before, &is_anticipated_after));
  bool dummy;
  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AnticipationTestTask, scheduler_.get(),
                            SimulateInputType::TouchEnd, &dummy, &dummy));
  default_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AnticipationTestTask, scheduler_.get(),
                 SimulateInputType::GestureScrollBegin, &dummy, &dummy));
  default_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AnticipationTestTask, scheduler_.get(),
                 SimulateInputType::GestureScrollEnd, &dummy, &dummy));

  RunUntilIdle();
  // When input is received, the scheduler should indicate that high-priority
  // work is anticipated.
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_TRUE(is_anticipated_after);

  clock_->Advance(priority_escalation_after_input_duration() * 2);
  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AnticipationTestTask, scheduler_.get(),
                            SimulateInputType::None, &is_anticipated_before,
                            &is_anticipated_after));
  RunUntilIdle();
  // Without additional input, the scheduler should go into NONE
  // use case but with scrolling expected where high-priority work is still
  // anticipated.
  EXPECT_EQ(UseCase::NONE, CurrentUseCase());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_TRUE(is_anticipated_before);
  EXPECT_TRUE(is_anticipated_after);

  clock_->Advance(subsequent_input_expected_after_input_duration() * 2);
  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AnticipationTestTask, scheduler_.get(),
                            SimulateInputType::None, &is_anticipated_before,
                            &is_anticipated_after));
  RunUntilIdle();
  // Eventually the scheduler should go into the default use case where
  // high-priority work is no longer anticipated.
  EXPECT_EQ(UseCase::NONE, CurrentUseCase());
  EXPECT_FALSE(TouchStartExpectedSoon());
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_FALSE(is_anticipated_after);
}

TEST_F(RendererSchedulerImplTest, TestShouldYield) {
  bool should_yield_before = false;
  bool should_yield_after = false;

  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PostingYieldingTestTask, scheduler_.get(),
                            base::RetainedRef(default_task_runner_), false,
                            &should_yield_before, &should_yield_after));
  RunUntilIdle();
  // Posting to default runner shouldn't cause yielding.
  EXPECT_FALSE(should_yield_before);
  EXPECT_FALSE(should_yield_after);

  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PostingYieldingTestTask, scheduler_.get(),
                            base::RetainedRef(compositor_task_runner_), false,
                            &should_yield_before, &should_yield_after));
  RunUntilIdle();
  // Posting while not mainthread scrolling shouldn't cause yielding.
  EXPECT_FALSE(should_yield_before);
  EXPECT_FALSE(should_yield_after);

  default_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PostingYieldingTestTask, scheduler_.get(),
                            base::RetainedRef(compositor_task_runner_), true,
                            &should_yield_before, &should_yield_after));
  RunUntilIdle();
  // We should be able to switch to compositor priority mid-task.
  EXPECT_FALSE(should_yield_before);
  EXPECT_TRUE(should_yield_after);
}

TEST_F(RendererSchedulerImplTest, TestShouldYield_TouchStart) {
  // Receiving a touchstart should immediately trigger yielding, even if
  // there's no immediately pending work in the compositor queue.
  EXPECT_FALSE(scheduler_->ShouldYieldForHighPriorityWork());
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_TRUE(scheduler_->ShouldYieldForHighPriorityWork());
  RunUntilIdle();
}

TEST_F(RendererSchedulerImplTest, SlowMainThreadInputEvent) {
  EXPECT_EQ(UseCase::NONE, CurrentUseCase());

  // An input event should bump us into input priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  RunUntilIdle();
  EXPECT_EQ(UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING, CurrentUseCase());

  // Simulate the input event being queued for a very long time. The compositor
  // task we post here represents the enqueued input task.
  clock_->Advance(priority_escalation_after_input_duration() * 2);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart));
  RunUntilIdle();

  // Even though we exceeded the input priority escalation period, we should
  // still be in main thread gesture since the input remains queued.
  EXPECT_EQ(UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING, CurrentUseCase());

  // After the escalation period ends we should go back into normal mode.
  clock_->Advance(priority_escalation_after_input_duration() * 2);
  RunUntilIdle();
  EXPECT_EQ(UseCase::NONE, CurrentUseCase());
}

class RendererSchedulerImplWithMockSchedulerTest
    : public RendererSchedulerImplTest {
 public:
  void SetUp() override {
    mock_task_runner_ = make_scoped_refptr(
        new cc::OrderedSimpleTaskRunner(clock_.get(), false));
    main_task_runner_ = SchedulerTqmDelegateForTest::Create(
        mock_task_runner_, base::WrapUnique(new TestTimeSource(clock_.get())));
    mock_scheduler_ = new RendererSchedulerImplForTest(main_task_runner_);
    Initialize(base::WrapUnique(mock_scheduler_));
  }

 protected:
  RendererSchedulerImplForTest* mock_scheduler_;
};

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       OnlyOnePendingUrgentPolicyUpdatey) {
  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();

  RunUntilIdle();

  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       OnePendingDelayedAndOneUrgentUpdatePolicy) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  mock_scheduler_->ScheduleDelayedPolicyUpdate(
      clock_->NowTicks(), base::TimeDelta::FromMilliseconds(1));
  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();

  RunUntilIdle();

  // We expect both the urgent and the delayed updates to run.
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       OneUrgentAndOnePendingDelayedUpdatePolicy) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  mock_scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  mock_scheduler_->ScheduleDelayedPolicyUpdate(
      clock_->NowTicks(), base::TimeDelta::FromMilliseconds(1));

  RunUntilIdle();

  // We expect both the urgent and the delayed updates to run.
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       UpdatePolicyCountTriggeredByOneInputEvent) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, mock_scheduler_->update_policy_count_);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart));
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  clock_->Advance(base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();

  // We finally expect a delayed policy update 100ms later.
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       UpdatePolicyCountTriggeredByThreeInputEvents) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, mock_scheduler_->update_policy_count_);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart));
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  // The second call to DidHandleInputEventOnCompositorThread should not post a
  // policy update because we are already in compositor priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  // We expect DidHandleInputEvent to trigger a policy update.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove));
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  // The third call to DidHandleInputEventOnCompositorThread should post a
  // policy update because the awaiting_touch_start_response_ flag changed.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);

  // We expect DidHandleInputEvent to trigger a policy update.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove));
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);

  clock_->Advance(base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();

  // We finally expect a delayed policy update.
  EXPECT_EQ(3, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       UpdatePolicyCountTriggeredByTwoInputEventsWithALongSeparatingDelay) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, mock_scheduler_->update_policy_count_);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart));
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  clock_->Advance(base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();
  // We expect a delayed policy update.
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);

  // We expect the second call to DidHandleInputEventOnCompositorThread to post
  // an urgent policy update because we are no longer in compositor priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);
  mock_task_runner_->RunPendingTasks();
  EXPECT_EQ(3, mock_scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove));
  EXPECT_EQ(3, mock_scheduler_->update_policy_count_);

  clock_->Advance(base::TimeDelta::FromMilliseconds(1000));
  RunUntilIdle();

  // We finally expect a delayed policy update.
  EXPECT_EQ(4, mock_scheduler_->update_policy_count_);
}

TEST_F(RendererSchedulerImplWithMockSchedulerTest,
       EnsureUpdatePolicyNotTriggeredTooOften) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);

  EXPECT_EQ(0, mock_scheduler_->update_policy_count_);
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);

  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);

  // We expect the first call to IsHighPriorityWorkAnticipated to be called
  // after receiving an input event (but before the UpdateTask was processed) to
  // call UpdatePolicy.
  EXPECT_EQ(1, mock_scheduler_->update_policy_count_);
  scheduler_->IsHighPriorityWorkAnticipated();
  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);
  // Subsequent calls should not call UpdatePolicy.
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureScrollEnd),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchEnd),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchMove));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchEnd));

  EXPECT_EQ(2, mock_scheduler_->update_policy_count_);

  // We expect both the urgent and the delayed updates to run in addition to the
  // earlier updated cause by IsHighPriorityWorkAnticipated, a final update
  // transitions from 'not_scrolling touchstart expected' to 'not_scrolling'.
  RunUntilIdle();
  EXPECT_THAT(
      mock_scheduler_->use_cases_,
      testing::ElementsAre(
          std::string("none"), std::string("compositor_gesture"),
          std::string("compositor_gesture touchstart expected"),
          std::string("none touchstart expected"), std::string("none")));
}

class RendererSchedulerImplWithMessageLoopTest
    : public RendererSchedulerImplTest {
 public:
  RendererSchedulerImplWithMessageLoopTest()
      : RendererSchedulerImplTest(new base::MessageLoop()) {}
  ~RendererSchedulerImplWithMessageLoopTest() override {}

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
    EnableIdleTasks();
    message_loop_->RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererSchedulerImplWithMessageLoopTest);
};

TEST_F(RendererSchedulerImplWithMessageLoopTest,
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
      base::Bind(
          &RendererSchedulerImplWithMessageLoopTest::PostFromNestedRunloop,
          base::Unretained(this),
          base::Unretained(&tasks_to_post_from_nested_loop)));

  EnableIdleTasks();
  RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(order, testing::ElementsAre(std::string("1"), std::string("2"),
                                          std::string("4"), std::string("5"),
                                          std::string("3")));
}

TEST_F(RendererSchedulerImplTest, TestLongIdlePeriod) {
  base::TimeTicks expected_deadline =
      clock_->NowTicks() + maximum_idle_period_duration();
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no idle period.

  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(RendererSchedulerImplTest, TestLongIdlePeriodWithPendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(30);
  base::TimeTicks expected_deadline = clock_->NowTicks() + pending_task_delay;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        pending_task_delay);

  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(RendererSchedulerImplTest,
       TestLongIdlePeriodWithLatePendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        pending_task_delay);

  // Advance clock until after delayed task was meant to be run.
  clock_->Advance(base::TimeDelta::FromMilliseconds(20));

  // Post an idle task and BeginFrameNotExpectedSoon to initiate a long idle
  // period. Since there is a late pending delayed task this shouldn't actually
  // start an idle period.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // After the delayed task has been run we should trigger an idle period.
  clock_->Advance(maximum_idle_period_duration());
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

TEST_F(RendererSchedulerImplTest, TestLongIdlePeriodRepeating) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(true);
  std::vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  max_idle_task_reposts = 3;
  base::TimeTicks clock_before(clock_->NowTicks());
  base::TimeDelta idle_task_runtime(base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingUpdateClockIdleTestTask,
                 base::RetainedRef(idle_task_runner_), &run_count, clock_.get(),
                 idle_task_runtime, &actual_deadlines));
  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(3, run_count);
  EXPECT_THAT(
      actual_deadlines,
      testing::ElementsAre(
          clock_before + maximum_idle_period_duration(),
          clock_before + idle_task_runtime + maximum_idle_period_duration(),
          clock_before + (2 * idle_task_runtime) +
              maximum_idle_period_duration()));

  // Check that idle tasks don't run after the idle period ends with a
  // new BeginMainFrame.
  max_idle_task_reposts = 5;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&RepostingUpdateClockIdleTestTask,
                 base::RetainedRef(idle_task_runner_), &run_count, clock_.get(),
                 idle_task_runtime, &actual_deadlines));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&WillBeginFrameIdleTask,
                            base::Unretained(scheduler_.get()), clock_.get()));
  RunUntilIdle();
  EXPECT_EQ(4, run_count);
}

TEST_F(RendererSchedulerImplTest, TestLongIdlePeriodDoesNotWakeScheduler) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  // Start a long idle period and get the time it should end.
  scheduler_->BeginFrameNotExpectedSoon();
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
  idle_task_runner_->PostIdleTaskAfterWakeup(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));
  RunUntilIdle();
  new_idle_period_deadline = scheduler_->CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline, new_idle_period_deadline);
  EXPECT_EQ(0, run_count);

  // Running a normal task should initiate a new long idle period though.
  default_task_runner_->PostTask(FROM_HERE, base::Bind(&NullTask));
  RunUntilIdle();
  new_idle_period_deadline = scheduler_->CurrentIdleTaskDeadlineForTesting();
  EXPECT_EQ(idle_period_deadline + maximum_idle_period_duration(),
            new_idle_period_deadline);

  EXPECT_EQ(1, run_count);
}

TEST_F(RendererSchedulerImplTest, TestLongIdlePeriodInTouchStartPolicy) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&IdleTestTask, &run_count, &deadline_in_task));

  // Observation of touchstart should defer the start of the long idle period.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // The long idle period should start after the touchstart policy has finished.
  clock_->Advance(priority_escalation_after_input_duration());
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
}

void TestCanExceedIdleDeadlineIfRequiredTask(RendererScheduler* scheduler,
                                             bool* can_exceed_idle_deadline_out,
                                             int* run_count,
                                             base::TimeTicks deadline) {
  *can_exceed_idle_deadline_out = scheduler->CanExceedIdleDeadlineIfRequired();
  (*run_count)++;
}

TEST_F(RendererSchedulerImplTest, CanExceedIdleDeadlineIfRequired) {
  int run_count = 0;
  bool can_exceed_idle_deadline = false;

  // Should return false if not in an idle period.
  EXPECT_FALSE(scheduler_->CanExceedIdleDeadlineIfRequired());

  // Should return false for short idle periods.
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                 &can_exceed_idle_deadline, &run_count));
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Should return false for a long idle period which is shortened due to a
  // pending delayed task.
  default_task_runner_->PostDelayedTask(FROM_HERE, base::Bind(&NullTask),
                                        base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                 &can_exceed_idle_deadline, &run_count));
  scheduler_->BeginFrameNotExpectedSoon();
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  clock_->Advance(maximum_idle_period_duration());
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::Bind(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                 &can_exceed_idle_deadline, &run_count));
  RunUntilIdle();
  EXPECT_EQ(3, run_count);
  EXPECT_TRUE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  scheduler_->WillBeginFrame(cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL));
  EXPECT_FALSE(scheduler_->CanExceedIdleDeadlineIfRequired());
}

TEST_F(RendererSchedulerImplTest, TestRendererHiddenIdlePeriod) {
  int run_count = 0;

  max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&RepostingIdleTestTask,
                            base::RetainedRef(idle_task_runner_), &run_count));

  // Renderer should start in visible state.
  RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // When we hide the renderer it should start a max deadline idle period, which
  // will run an idle task and then immediately start a new idle period, which
  // runs the second idle task.
  scheduler_->SetAllRenderWidgetsHidden(true);
  RunUntilIdle();
  EXPECT_EQ(2, run_count);

  // Advance time by amount of time by the maximum amount of time we execute
  // idle tasks when hidden (plus some slack) - idle period should have ended.
  max_idle_task_reposts = 3;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::Bind(&RepostingIdleTestTask,
                            base::RetainedRef(idle_task_runner_), &run_count));
  clock_->Advance(end_idle_when_hidden_delay() +
                  base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  EXPECT_EQ(2, run_count);
}

TEST_F(RendererSchedulerImplTest, TimerQueueEnabledByDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2")));
}

TEST_F(RendererSchedulerImplTest, SuspendAndResumeTimerQueue) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  scheduler_->SuspendTimerQueue();
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  scheduler_->ResumeTimerQueue();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2")));
}

TEST_F(RendererSchedulerImplTest, SuspendAndThrottleTimerQueue) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  scheduler_->SuspendTimerQueue();
  RunUntilIdle();
  scheduler_->throttling_helper()->IncreaseThrottleRefCount(
      static_cast<TaskQueue*>(timer_task_runner_.get()));
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

TEST_F(RendererSchedulerImplTest, ThrottleAndSuspendTimerQueue) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  scheduler_->throttling_helper()->IncreaseThrottleRefCount(
      static_cast<TaskQueue*>(timer_task_runner_.get()));
  RunUntilIdle();
  scheduler_->SuspendTimerQueue();
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

TEST_F(RendererSchedulerImplTest, MultipleSuspendsNeedMultipleResumes) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  scheduler_->SuspendTimerQueue();
  scheduler_->SuspendTimerQueue();
  scheduler_->SuspendTimerQueue();
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  scheduler_->ResumeTimerQueue();
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  scheduler_->ResumeTimerQueue();
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  scheduler_->ResumeTimerQueue();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2")));
}

TEST_F(RendererSchedulerImplTest, SuspendRenderer) {
  // Assume that the renderer is backgrounded.
  scheduler_->OnRendererBackgrounded();

  // Tasks in some queues don't fire when the renderer is suspended.
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1 L1 I1 T1");
  scheduler_->SuspendRenderer();
  EnableIdleTasks();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("I1")));

  // The rest queued tasks fire when the tab goes foregrounded.
  run_order.clear();
  scheduler_->OnRendererForegrounded();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("T1")));
}

TEST_F(RendererSchedulerImplTest, UseCaseToString) {
  CheckAllUseCaseToString();
}

TEST_F(RendererSchedulerImplTest, MismatchedDidHandleInputEventOnMainThread) {
  // This should not DCHECK because there was no corresponding compositor side
  // call to DidHandleInputEventOnCompositorThread with
  // INPUT_EVENT_ACK_STATE_NOT_CONSUMED. There are legitimate reasons for the
  // compositor to not be there and we don't want to make debugging impossible.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::GestureFlingStart));
}

TEST_F(RendererSchedulerImplTest, BeginMainFrameOnCriticalPath) {
  ASSERT_FALSE(scheduler_->BeginMainFrameOnCriticalPath());

  cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(1000), cc::BeginFrameArgs::NORMAL);
  scheduler_->WillBeginFrame(begin_frame_args);
  ASSERT_TRUE(scheduler_->BeginMainFrameOnCriticalPath());

  begin_frame_args.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args);
  ASSERT_FALSE(scheduler_->BeginMainFrameOnCriticalPath());
}

TEST_F(RendererSchedulerImplTest, ShutdownPreventsPostingOfNewTasks) {
  scheduler_->Shutdown();
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1");
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());
}

TEST_F(RendererSchedulerImplTest, TestRendererBackgroundedTimerSuspension) {
  scheduler_->SetTimerQueueSuspensionWhenBackgroundedEnabled(true);

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  // The background signal will not immediately suspend the timer queue.
  scheduler_->OnRendererBackgrounded();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2")));

  run_order.clear();
  PostTestTasks(&run_order, "T3");
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("T3")));

  // Advance the time until after the scheduled timer queue suspension.
  run_order.clear();
  clock_->Advance(suspend_timers_when_backgrounded_delay() +
                  base::TimeDelta::FromMilliseconds(10));
  RunUntilIdle();
  ASSERT_TRUE(run_order.empty());

  // Timer tasks should be suspended until the foregrounded signal.
  PostTestTasks(&run_order, "T4 T5");
  RunUntilIdle();
  EXPECT_TRUE(run_order.empty());

  scheduler_->OnRendererForegrounded();
  RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T4"), std::string("T5")));

  // Subsequent timer tasks should fire as usual.
  run_order.clear();
  PostTestTasks(&run_order, "T6");
  RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("T6")));
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveLoadingTasksNotBlockedTillFirstBeginMainFrame) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(loading_task_runner_);
  ForceTouchStartToBeExpectedSoon();
  PostTestTasks(&run_order, "L1 D1");
  RunUntilIdle();

  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_FALSE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1")));

  // Emit a BeginMainFrame, and the loading task should get blocked.
  DoMainFrame();
  run_order.clear();

  PostTestTasks(&run_order, "L1 D1");
  RunUntilIdle();

  EXPECT_EQ(RendererSchedulerImpl::UseCase::NONE, CurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, RAILMode());
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveLoadingTasksNotBlockedIfNoTouchHandler) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(false);
  DoMainFrame();
  SimulateExpensiveTasks(loading_task_runner_);
  ForceTouchStartToBeExpectedSoon();
  PostTestTasks(&run_order, "L1 D1");
  RunUntilIdle();

  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_FALSE(TouchStartExpectedSoon());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, RAILMode());
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveTimerTaskBlocked_UseCase_NONE_PreviousCompositorGesture) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  ForceTouchStartToBeExpectedSoon();

  PostTestTasks(&run_order, "T1 D1");
  RunUntilIdle();

  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_FALSE(LoadingTasksSeemExpensive());
  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, RAILMode());
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveTimerTaskNotBlocked_UseCase_NONE_PreviousMainThreadGesture) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);

  SimulateMainThreadGestureStart(TouchEventPolicy::SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollBegin);
  EXPECT_EQ(UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
            ForceUpdatePolicyAndGetCurrentUseCase());

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchEnd),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::TouchEnd));

  clock_->Advance(priority_escalation_after_input_duration() * 2);
  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());

  PostTestTasks(&run_order, "T1 D1");
  RunUntilIdle();

  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_FALSE(LoadingTasksSeemExpensive());
  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, RAILMode());
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveTimerTaskBlocked_UseCase_COMPOSITOR_GESTURE) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  ForceTouchStartToBeExpectedSoon();
  scheduler_->DidAnimateForInputOnCompositorThread();

  PostTestTasks(&run_order, "T1 D1");
  RunUntilIdle();

  EXPECT_EQ(UseCase::COMPOSITOR_GESTURE,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_FALSE(LoadingTasksSeemExpensive());
  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, RAILMode());
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveTimerTaskNotBlockedIfDisallowed_UseCase_COMPOSITOR_GESTURE) {
  std::vector<std::string> run_order;

  scheduler_->SetExpensiveTaskBlockingAllowed(false);
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  ForceTouchStartToBeExpectedSoon();
  scheduler_->DidAnimateForInputOnCompositorThread();

  PostTestTasks(&run_order, "T1 D1");
  RunUntilIdle();

  EXPECT_EQ(UseCase::COMPOSITOR_GESTURE,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_FALSE(LoadingTasksSeemExpensive());
  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("T1"),
                                              std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, RAILMode());
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveTimerTaskBlocked_EvenIfBeginMainFrameNotExpectedSoon) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  ForceTouchStartToBeExpectedSoon();
  scheduler_->BeginFrameNotExpectedSoon();

  PostTestTasks(&run_order, "T1 D1");
  RunUntilIdle();

  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_FALSE(LoadingTasksSeemExpensive());
  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, RAILMode());
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveLoadingTasksBlockedIfChildFrameNavigationExpected) {
  std::vector<std::string> run_order;

  DoMainFrame();
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(loading_task_runner_);
  ForceTouchStartToBeExpectedSoon();
  scheduler_->AddPendingNavigation(
      blink::WebScheduler::NavigatingFrameType::kChildFrame);

  PostTestTasks(&run_order, "L1 D1");
  RunUntilIdle();

  // The expensive loading task gets blocked.
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, RAILMode());
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveLoadingTasksNotBlockedIfMainFrameNavigationExpected) {
  std::vector<std::string> run_order;

  DoMainFrame();
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(loading_task_runner_);
  ForceTouchStartToBeExpectedSoon();
  scheduler_->AddPendingNavigation(
      blink::WebScheduler::NavigatingFrameType::kMainFrame);

  PostTestTasks(&run_order, "L1 D1");
  RunUntilIdle();

  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_EQ(1, NavigationTaskExpectedCount());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1")));

  // After the nagigation has been cancelled, the expensive loading tasks should
  // get blocked.
  scheduler_->RemovePendingNavigation(
      blink::WebScheduler::NavigatingFrameType::kMainFrame);
  run_order.clear();

  PostTestTasks(&run_order, "L1 D1");
  RunUntilIdle();

  EXPECT_EQ(RendererSchedulerImpl::UseCase::NONE, CurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_EQ(0, NavigationTaskExpectedCount());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, RAILMode());
}

TEST_F(
    RendererSchedulerImplTest,
    ExpensiveLoadingTasksNotBlockedIfMainFrameNavigationExpected_Multiple) {
  std::vector<std::string> run_order;

  DoMainFrame();
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(loading_task_runner_);
  ForceTouchStartToBeExpectedSoon();
  scheduler_->AddPendingNavigation(
      blink::WebScheduler::NavigatingFrameType::kMainFrame);
  scheduler_->AddPendingNavigation(
      blink::WebScheduler::NavigatingFrameType::kMainFrame);

  PostTestTasks(&run_order, "L1 D1");
  RunUntilIdle();

  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_EQ(2, NavigationTaskExpectedCount());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1")));


  run_order.clear();
  scheduler_->RemovePendingNavigation(
      blink::WebScheduler::NavigatingFrameType::kMainFrame);
  // Navigation task expected ref count non-zero so expensive tasks still not
  // blocked.
  PostTestTasks(&run_order, "L1 D1");
  RunUntilIdle();

  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_EQ(1, NavigationTaskExpectedCount());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1")));


  run_order.clear();
  scheduler_->RemovePendingNavigation(
      blink::WebScheduler::NavigatingFrameType::kMainFrame);
  // Navigation task expected ref count is now zero, the expensive loading tasks
  // should get blocked.
  PostTestTasks(&run_order, "L1 D1");
  RunUntilIdle();

  EXPECT_EQ(RendererSchedulerImpl::UseCase::NONE, CurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_EQ(0, NavigationTaskExpectedCount());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, RAILMode());
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveLoadingTasksNotBlockedDuringMainThreadGestures) {
  std::vector<std::string> run_order;

  SimulateExpensiveTasks(loading_task_runner_);

  // Loading tasks should not be disabled during main thread user interactions.
  PostTestTasks(&run_order, "C1 L1");

  // Trigger main_thread_gesture UseCase
  SimulateMainThreadGestureStart(TouchEventPolicy::SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollBegin);
  RunUntilIdle();
  EXPECT_EQ(RendererSchedulerImpl::UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
            CurrentUseCase());

  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("L1")));
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, RAILMode());
}

TEST_F(RendererSchedulerImplTest, ModeratelyExpensiveTimer_NotBlocked) {
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateMainThreadGestureStart(TouchEventPolicy::SEND_TOUCH_START,
                                 blink::WebInputEvent::TouchMove);
  RunUntilIdle();
  for (int i = 0; i < 20; i++) {
    simulate_timer_task_ran_ = false;

    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = false;
    scheduler_->WillBeginFrame(begin_frame_args);

    compositor_task_runner_->PostTask(
        FROM_HERE, base::Bind(&RendererSchedulerImplTest::
                                  SimulateMainThreadInputHandlingCompositorTask,
                              base::Unretained(this),
                              base::TimeDelta::FromMilliseconds(8)));
    timer_task_runner_->PostTask(
        FROM_HERE, base::Bind(&RendererSchedulerImplTest::SimulateTimerTask,
                              base::Unretained(this),
                              base::TimeDelta::FromMilliseconds(4)));

    RunUntilIdle();
    EXPECT_TRUE(simulate_timer_task_ran_) << " i = " << i;
    EXPECT_EQ(RendererSchedulerImpl::UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
              CurrentUseCase())
        << " i = " << i;
    EXPECT_FALSE(LoadingTasksSeemExpensive()) << " i = " << i;
    EXPECT_FALSE(TimerTasksSeemExpensive()) << " i = " << i;

    base::TimeDelta time_till_next_frame =
        EstimatedNextFrameBegin() - clock_->NowTicks();
    if (time_till_next_frame > base::TimeDelta())
      clock_->Advance(time_till_next_frame);
  }
}

TEST_F(RendererSchedulerImplTest,
       FourtyMsTimer_NotBlocked_CompositorScrolling) {
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  RunUntilIdle();
  for (int i = 0; i < 20; i++) {
    simulate_timer_task_ran_ = false;

    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = false;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidAnimateForInputOnCompositorThread();

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RendererSchedulerImplTest::SimulateMainThreadCompositorTask,
                   base::Unretained(this),
                   base::TimeDelta::FromMilliseconds(8)));
    timer_task_runner_->PostTask(
        FROM_HERE, base::Bind(&RendererSchedulerImplTest::SimulateTimerTask,
                              base::Unretained(this),
                              base::TimeDelta::FromMilliseconds(40)));

    RunUntilIdle();
    EXPECT_TRUE(simulate_timer_task_ran_) << " i = " << i;
    EXPECT_EQ(RendererSchedulerImpl::UseCase::COMPOSITOR_GESTURE,
              CurrentUseCase())
        << " i = " << i;
    EXPECT_FALSE(LoadingTasksSeemExpensive()) << " i = " << i;
    EXPECT_FALSE(TimerTasksSeemExpensive()) << " i = " << i;

    base::TimeDelta time_till_next_frame =
        EstimatedNextFrameBegin() - clock_->NowTicks();
    if (time_till_next_frame > base::TimeDelta())
      clock_->Advance(time_till_next_frame);
  }
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveTimer_NotBlocked_UseCase_MAIN_THREAD_CUSTOM_INPUT_HANDLING) {
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateMainThreadGestureStart(TouchEventPolicy::SEND_TOUCH_START,
                                 blink::WebInputEvent::TouchMove);
  RunUntilIdle();
  for (int i = 0; i < 20; i++) {
    simulate_timer_task_ran_ = false;

    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = false;
    scheduler_->WillBeginFrame(begin_frame_args);

    compositor_task_runner_->PostTask(
        FROM_HERE, base::Bind(&RendererSchedulerImplTest::
                                  SimulateMainThreadInputHandlingCompositorTask,
                              base::Unretained(this),
                              base::TimeDelta::FromMilliseconds(8)));
    timer_task_runner_->PostTask(
        FROM_HERE, base::Bind(&RendererSchedulerImplTest::SimulateTimerTask,
                              base::Unretained(this),
                              base::TimeDelta::FromMilliseconds(10)));

    RunUntilIdle();
    EXPECT_EQ(RendererSchedulerImpl::UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING,
              CurrentUseCase())
        << " i = " << i;
    EXPECT_FALSE(LoadingTasksSeemExpensive()) << " i = " << i;
    if (i == 0) {
      EXPECT_FALSE(TimerTasksSeemExpensive()) << " i = " << i;
    } else {
      EXPECT_TRUE(TimerTasksSeemExpensive()) << " i = " << i;
    }
    EXPECT_TRUE(simulate_timer_task_ran_) << " i = " << i;

    base::TimeDelta time_till_next_frame =
        EstimatedNextFrameBegin() - clock_->NowTicks();
    if (time_till_next_frame > base::TimeDelta())
      clock_->Advance(time_till_next_frame);
  }
}

TEST_F(RendererSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_NONE) {
  EXPECT_EQ(UseCase::NONE, CurrentUseCase());
  EXPECT_EQ(rails_response_time(),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(RendererSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_COMPOSITOR_GESTURE) {
  SimulateCompositorGestureStart(TouchEventPolicy::DONT_SEND_TOUCH_START);
  EXPECT_EQ(UseCase::COMPOSITOR_GESTURE,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(rails_response_time(),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

// TODO(alexclarke): Reenable once we've reinstaed the Loading UseCase.
TEST_F(RendererSchedulerImplTest,
       DISABLED_EstimateLongestJankFreeTaskDuration_UseCase_) {
  scheduler_->OnNavigationStarted();
  EXPECT_EQ(UseCase::LOADING, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(rails_response_time(),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(RendererSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_MAIN_THREAD_GESTURE) {
  SimulateMainThreadGestureStart(TouchEventPolicy::SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollUpdate);
  cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args);

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererSchedulerImplTest::
                     SimulateMainThreadInputHandlingCompositorTask,
                 base::Unretained(this), base::TimeDelta::FromMilliseconds(5)));

  RunUntilIdle();
  EXPECT_EQ(UseCase::MAIN_THREAD_GESTURE, CurrentUseCase());

  // 16ms frame - 5ms compositor work = 11ms for other stuff.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(
    RendererSchedulerImplTest,
    EstimateLongestJankFreeTaskDuration_UseCase_MAIN_THREAD_CUSTOM_INPUT_HANDLING) {
  cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args);

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererSchedulerImplTest::
                     SimulateMainThreadInputHandlingCompositorTask,
                 base::Unretained(this), base::TimeDelta::FromMilliseconds(5)));

  RunUntilIdle();
  EXPECT_EQ(UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING, CurrentUseCase());

  // 16ms frame - 5ms compositor work = 11ms for other stuff.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(RendererSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_SYNCHRONIZED_GESTURE) {
  SimulateCompositorGestureStart(TouchEventPolicy::DONT_SEND_TOUCH_START);

  cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = true;
  scheduler_->WillBeginFrame(begin_frame_args);

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&RendererSchedulerImplTest::SimulateMainThreadCompositorTask,
                 base::Unretained(this), base::TimeDelta::FromMilliseconds(5)));

  RunUntilIdle();
  EXPECT_EQ(UseCase::SYNCHRONIZED_GESTURE, CurrentUseCase());

  // 16ms frame - 5ms compositor work = 11ms for other stuff.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

class WebViewSchedulerImplForTest : public WebViewSchedulerImpl {
 public:
  WebViewSchedulerImplForTest(RendererSchedulerImpl* scheduler)
      : WebViewSchedulerImpl(nullptr, scheduler, false) {}
  ~WebViewSchedulerImplForTest() override {}

  void AddConsoleWarning(const std::string& message) override {
    console_warnings_.push_back(message);
  }

  const std::vector<std::string>& console_warnings() const {
    return console_warnings_;
  }

 private:
  std::vector<std::string> console_warnings_;

  DISALLOW_COPY_AND_ASSIGN(WebViewSchedulerImplForTest);
};

TEST_F(RendererSchedulerImplTest, BlockedTimerNotification) {
  // Make sure we see one (and just one) console warning about an expensive
  // timer being deferred.
  WebViewSchedulerImplForTest web_view_scheduler(scheduler_.get());

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  scheduler_->SetExpensiveTaskBlockingAllowed(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);
  ForceTouchStartToBeExpectedSoon();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");
  RunUntilIdle();

  EXPECT_EQ(0u, run_order.size());
  EXPECT_EQ(1u, web_view_scheduler.console_warnings().size());
  EXPECT_NE(std::string::npos,
            web_view_scheduler.console_warnings()[0].find("crbug.com/574343"));
}

TEST_F(RendererSchedulerImplTest,
       BlockedTimerNotification_ExpensiveTaskBlockingNotAllowed) {
  // Make sure we don't report warnings about blocked tasks when expensive task
  // blocking is not allowed.
  WebViewSchedulerImplForTest web_view_scheduler(scheduler_.get());

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  scheduler_->SetExpensiveTaskBlockingAllowed(false);
  scheduler_->SuspendTimerQueue();
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);
  ForceTouchStartToBeExpectedSoon();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");
  RunUntilIdle();

  EXPECT_EQ(0u, run_order.size());
  EXPECT_EQ(0u, web_view_scheduler.console_warnings().size());
}

TEST_F(RendererSchedulerImplTest, BlockedTimerNotification_TimersSuspended) {
  // Make sure we don't report warnings about blocked tasks when timers are
  // being blocked for other reasons.
  WebViewSchedulerImplForTest web_view_scheduler(scheduler_.get());

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  scheduler_->SetExpensiveTaskBlockingAllowed(true);
  scheduler_->SuspendTimerQueue();
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);
  ForceTouchStartToBeExpectedSoon();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");
  RunUntilIdle();

  EXPECT_EQ(0u, run_order.size());
  EXPECT_EQ(0u, web_view_scheduler.console_warnings().size());
}

TEST_F(RendererSchedulerImplTest, BlockedTimerNotification_TOUCHSTART) {
  // Make sure we don't report warnings about blocked tasks during TOUCHSTART.
  WebViewSchedulerImplForTest web_view_scheduler(scheduler_.get());

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_EQ(UseCase::TOUCHSTART, ForceUpdatePolicyAndGetCurrentUseCase());

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");
  RunUntilIdle();

  EXPECT_EQ(0u, run_order.size());
  EXPECT_EQ(0u, web_view_scheduler.console_warnings().size());
}

TEST_F(RendererSchedulerImplTest,
       BlockedTimerNotification_SYNCHRONIZED_GESTURE) {
  // Make sure we only report warnings during a high blocking threshold.
  WebViewSchedulerImplForTest web_view_scheduler(scheduler_.get());

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  SimulateCompositorGestureStart(TouchEventPolicy::DONT_SEND_TOUCH_START);

  cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = true;
  scheduler_->WillBeginFrame(begin_frame_args);

  EXPECT_EQ(UseCase::SYNCHRONIZED_GESTURE,
            ForceUpdatePolicyAndGetCurrentUseCase());

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");
  RunUntilIdle();

  EXPECT_EQ(0u, run_order.size());
  EXPECT_EQ(0u, web_view_scheduler.console_warnings().size());
}

namespace {
void SlowCountingTask(size_t* count,
                      base::SimpleTestTickClock* clock,
                      int task_duration,
                      scoped_refptr<base::SingleThreadTaskRunner> timer_queue) {
  clock->Advance(base::TimeDelta::FromMilliseconds(task_duration));
  if (++(*count) < 500) {
    timer_queue->PostTask(FROM_HERE, base::Bind(SlowCountingTask, count, clock,
                                                task_duration, timer_queue));
  }
}
}

TEST_F(RendererSchedulerImplTest,
       SYNCHRONIZED_GESTURE_TimerTaskThrottling_task_expensive) {
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);

  base::TimeTicks first_throttled_run_time =
      ThrottlingHelper::ThrottledRunTime(clock_->NowTicks());

  size_t count = 0;
  // With the compositor task taking 10ms, there is not enough time to run this
  // 7ms timer task in the 16ms frame.
  scheduler_->TimerTaskRunner()->PostTask(
      FROM_HERE, base::Bind(SlowCountingTask, &count, clock_.get(), 7,
                            scheduler_->TimerTaskRunner()));

  for (int i = 0; i < 1000; i++) {
    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollUpdate),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    simulate_compositor_task_ran_ = false;
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RendererSchedulerImplTest::SimulateMainThreadCompositorTask,
                   base::Unretained(this),
                   base::TimeDelta::FromMilliseconds(10)));

    mock_task_runner_->RunTasksWhile(
        base::Bind(&RendererSchedulerImplTest::SimulatedCompositorTaskPending,
                   base::Unretained(this)));
    EXPECT_EQ(UseCase::SYNCHRONIZED_GESTURE, CurrentUseCase()) << "i = " << i;

    // Before the policy is updated the queue will be enabled. Subsequently it
    // will be disabled until the throttled queue is pumped.
    bool expect_queue_enabled =
        (i == 0) || (clock_->NowTicks() > first_throttled_run_time);
    EXPECT_EQ(expect_queue_enabled,
              scheduler_->TimerTaskRunner()->IsQueueEnabled())
        << "i = " << i;
  }

  // Task is throttled but not completely blocked.
  EXPECT_EQ(12u, count);
}

TEST_F(RendererSchedulerImplTest,
       SYNCHRONIZED_GESTURE_TimerTaskThrottling_TimersSuspended) {
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);

  base::TimeTicks first_throttled_run_time =
      ThrottlingHelper::ThrottledRunTime(clock_->NowTicks());

  size_t count = 0;
  // With the compositor task taking 10ms, there is not enough time to run this
  // 7ms timer task in the 16ms frame.
  scheduler_->TimerTaskRunner()->PostTask(
      FROM_HERE, base::Bind(SlowCountingTask, &count, clock_.get(), 7,
                            scheduler_->TimerTaskRunner()));

  bool suspended = false;
  for (int i = 0; i < 1000; i++) {
    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollUpdate),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    simulate_compositor_task_ran_ = false;
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RendererSchedulerImplTest::SimulateMainThreadCompositorTask,
                   base::Unretained(this),
                   base::TimeDelta::FromMilliseconds(10)));

    mock_task_runner_->RunTasksWhile(
        base::Bind(&RendererSchedulerImplTest::SimulatedCompositorTaskPending,
                   base::Unretained(this)));
    EXPECT_EQ(UseCase::SYNCHRONIZED_GESTURE, CurrentUseCase()) << "i = " << i;

    // Before the policy is updated the queue will be enabled. Subsequently it
    // will be disabled until the throttled queue is pumped.
    bool expect_queue_enabled =
        (i == 0) || (clock_->NowTicks() > first_throttled_run_time);
    if (suspended)
      expect_queue_enabled = false;
    EXPECT_EQ(expect_queue_enabled,
              scheduler_->TimerTaskRunner()->IsQueueEnabled())
        << "i = " << i;

    // After we've run any expensive tasks suspend the queue.  The throttling
    // helper should /not/ re-enable this queue under any circumstances while
    // timers are suspended.
    if (count > 0 && !suspended) {
      EXPECT_EQ(2u, count);
      scheduler_->SuspendTimerQueue();
      suspended = true;
    }
  }

  // Make sure the timer queue stayed suspended!
  EXPECT_EQ(2u, count);
}

TEST_F(RendererSchedulerImplTest,
       SYNCHRONIZED_GESTURE_TimerTaskThrottling_task_not_expensive) {
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);

  size_t count = 0;
  // With the compositor task taking 10ms, there is enough time to run this 6ms
  // timer task in the 16ms frame.
  scheduler_->TimerTaskRunner()->PostTask(
      FROM_HERE, base::Bind(SlowCountingTask, &count, clock_.get(), 6,
                            scheduler_->TimerTaskRunner()));

  for (int i = 0; i < 1000; i++) {
    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollUpdate),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    simulate_compositor_task_ran_ = false;
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RendererSchedulerImplTest::SimulateMainThreadCompositorTask,
                   base::Unretained(this),
                   base::TimeDelta::FromMilliseconds(10)));

    mock_task_runner_->RunTasksWhile(
        base::Bind(&RendererSchedulerImplTest::SimulatedCompositorTaskPending,
                   base::Unretained(this)));
    EXPECT_EQ(UseCase::SYNCHRONIZED_GESTURE, CurrentUseCase()) << "i = " << i;
    EXPECT_TRUE(scheduler_->TimerTaskRunner()->IsQueueEnabled()) << "i = " << i;
  }

  // Task is not throttled.
  EXPECT_EQ(500u, count);
}

TEST_F(RendererSchedulerImplTest,
       ExpensiveTimerTaskBlocked_SYNCHRONIZED_GESTURE_TouchStartExpected) {
  SimulateExpensiveTasks(timer_task_runner_);
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  ForceTouchStartToBeExpectedSoon();

  // Bump us into SYNCHRONIZED_GESTURE.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::GestureScrollUpdate),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

  cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
      base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = true;
  scheduler_->WillBeginFrame(begin_frame_args);

  EXPECT_EQ(UseCase::SYNCHRONIZED_GESTURE,
            ForceUpdatePolicyAndGetCurrentUseCase());

  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(TouchStartExpectedSoon());
  EXPECT_FALSE(scheduler_->TimerTaskRunner()->IsQueueEnabled());
}

TEST_F(RendererSchedulerImplTest, DenyLongIdleDuringTouchStart) {
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_EQ(UseCase::TOUCHSTART, ForceUpdatePolicyAndGetCurrentUseCase());

  // First check that long idle is denied during the TOUCHSTART use case.
  IdleHelper::Delegate* idle_delegate = scheduler_.get();
  base::TimeTicks now;
  base::TimeDelta next_time_to_check;
  EXPECT_FALSE(idle_delegate->CanEnterLongIdlePeriod(now, &next_time_to_check));
  EXPECT_GE(next_time_to_check, base::TimeDelta());

  // Check again at a time past the TOUCHSTART expiration. We should still get a
  // non-negative delay to when to check again.
  now += base::TimeDelta::FromMilliseconds(500);
  EXPECT_FALSE(idle_delegate->CanEnterLongIdlePeriod(now, &next_time_to_check));
  EXPECT_GE(next_time_to_check, base::TimeDelta());
}

TEST_F(RendererSchedulerImplTest, TestCompositorPolicy_TouchStartDuringFling) {
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  scheduler_->DidAnimateForInputOnCompositorThread();
  // Note DidAnimateForInputOnCompositorThread does not by itself trigger a
  // policy update.
  EXPECT_EQ(RendererSchedulerImpl::UseCase::COMPOSITOR_GESTURE,
            ForceUpdatePolicyAndGetCurrentUseCase());

  // Make sure TouchStart causes a policy change.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::TouchStart),
      RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(RendererSchedulerImpl::UseCase::TOUCHSTART,
            ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(RendererSchedulerImplTest, SYNCHRONIZED_GESTURE_CompositingExpensive) {
  SimulateCompositorGestureStart(TouchEventPolicy::SEND_TOUCH_START);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. To avoid starvation, compositing tasks
  // should therefore not get prioritized.
  std::vector<std::string> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollUpdate),
        RendererScheduler::InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    simulate_compositor_task_ran_ = false;
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RendererSchedulerImplTest::SimulateMainThreadCompositorTask,
                   base::Unretained(this),
                   base::TimeDelta::FromMilliseconds(20)));

    mock_task_runner_->RunTasksWhile(
        base::Bind(&RendererSchedulerImplTest::SimulatedCompositorTaskPending,
                   base::Unretained(this)));
    EXPECT_EQ(UseCase::SYNCHRONIZED_GESTURE, CurrentUseCase()) << "i = " << i;
  }

  // Timer tasks should not have been starved by the expensive compositor
  // tasks.
  EXPECT_EQ(TaskQueue::NORMAL_PRIORITY,
            scheduler_->CompositorTaskRunner()->GetQueuePriority());
  EXPECT_EQ(1000u, run_order.size());
}

TEST_F(RendererSchedulerImplTest, MAIN_THREAD_CUSTOM_INPUT_HANDLING) {
  SimulateMainThreadGestureStart(TouchEventPolicy::SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollBegin);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. To avoid starvation, compositing tasks
  // should therefore not get prioritized.
  std::vector<std::string> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::TouchMove),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    simulate_compositor_task_ran_ = false;
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RendererSchedulerImplTest::SimulateMainThreadCompositorTask,
                   base::Unretained(this),
                   base::TimeDelta::FromMilliseconds(20)));

    mock_task_runner_->RunTasksWhile(
        base::Bind(&RendererSchedulerImplTest::SimulatedCompositorTaskPending,
                   base::Unretained(this)));
    EXPECT_EQ(UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING, CurrentUseCase())
        << "i = " << i;
  }

  // Timer tasks should not have been starved by the expensive compositor
  // tasks.
  EXPECT_EQ(TaskQueue::NORMAL_PRIORITY,
            scheduler_->CompositorTaskRunner()->GetQueuePriority());
  EXPECT_EQ(1000u, run_order.size());
}

TEST_F(RendererSchedulerImplTest, MAIN_THREAD_GESTURE) {
  SimulateMainThreadGestureStart(TouchEventPolicy::DONT_SEND_TOUCH_START,
                                 blink::WebInputEvent::GestureScrollBegin);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. However because this is a main thread
  // gesture instead of custom main thread input handling, we allow the timer
  // tasks to be starved.
  std::vector<std::string> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    cc::BeginFrameArgs begin_frame_args = cc::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, clock_->NowTicks(), base::TimeTicks(),
        base::TimeDelta::FromMilliseconds(16), cc::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::GestureScrollUpdate),
        RendererScheduler::InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    simulate_compositor_task_ran_ = false;
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&RendererSchedulerImplTest::SimulateMainThreadCompositorTask,
                   base::Unretained(this),
                   base::TimeDelta::FromMilliseconds(20)));

    mock_task_runner_->RunTasksWhile(
        base::Bind(&RendererSchedulerImplTest::SimulatedCompositorTaskPending,
                   base::Unretained(this)));
    EXPECT_EQ(UseCase::MAIN_THREAD_GESTURE, CurrentUseCase()) << "i = " << i;
  }

  EXPECT_EQ(TaskQueue::HIGH_PRIORITY,
            scheduler_->CompositorTaskRunner()->GetQueuePriority());
  EXPECT_EQ(279u, run_order.size());
}

class MockRAILModeObserver : public RendererScheduler::RAILModeObserver {
 public:
  MOCK_METHOD1(OnRAILModeChanged, void(v8::RAILMode rail_mode));
};

TEST_F(RendererSchedulerImplTest, TestResponseRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->SetRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_RESPONSE));

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  ForceTouchStartToBeExpectedSoon();
  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, RAILMode());
  scheduler_->SetRAILModeObserver(nullptr);
}

TEST_F(RendererSchedulerImplTest, TestAnimateRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->SetRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_ANIMATION)).Times(0);

  EXPECT_FALSE(BeginFrameNotExpectedSoon());
  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, RAILMode());
  scheduler_->SetRAILModeObserver(nullptr);
}

TEST_F(RendererSchedulerImplTest, TestIdleRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->SetRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_ANIMATION));
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_IDLE));

  scheduler_->SetAllRenderWidgetsHidden(true);
  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(v8::PERFORMANCE_IDLE, RAILMode());
  scheduler_->SetAllRenderWidgetsHidden(false);
  EXPECT_EQ(UseCase::NONE, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, RAILMode());
  scheduler_->SetRAILModeObserver(nullptr);
}

}  // namespace scheduler
