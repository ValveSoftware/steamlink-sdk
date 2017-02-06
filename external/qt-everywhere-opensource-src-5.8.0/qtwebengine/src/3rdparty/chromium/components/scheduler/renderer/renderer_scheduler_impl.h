// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_
#define COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "components/scheduler/base/pollable_thread_safe_flag.h"
#include "components/scheduler/child/idle_helper.h"
#include "components/scheduler/child/scheduler_helper.h"
#include "components/scheduler/renderer/deadline_task_runner.h"
#include "components/scheduler/renderer/idle_time_estimator.h"
#include "components/scheduler/renderer/render_widget_signals.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/renderer/task_cost_estimator.h"
#include "components/scheduler/renderer/throttling_helper.h"
#include "components/scheduler/renderer/user_model.h"
#include "components/scheduler/renderer/web_view_scheduler_impl.h"
#include "components/scheduler/scheduler_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace scheduler {
class RenderWidgetSchedulingState;
class WebViewSchedulerImpl;
class ThrottlingHelper;

class SCHEDULER_EXPORT RendererSchedulerImpl
    : public RendererScheduler,
      public IdleHelper::Delegate,
      public SchedulerHelper::Observer,
      public RenderWidgetSignals::Observer {
 public:
  // Keep RendererScheduler::UseCaseToString in sync with this enum.
  enum class UseCase {
    // No active use case detected.
    NONE,
    // A continuous gesture (e.g., scroll, pinch) which is being driven by the
    // compositor thread.
    COMPOSITOR_GESTURE,
    // An unspecified touch gesture which is being handled by the main thread.
    // Note that since we don't have a full view of the use case, we should be
    // careful to prioritize all work equally.
    MAIN_THREAD_CUSTOM_INPUT_HANDLING,
    // A continuous gesture (e.g., scroll, pinch) which is being driven by the
    // compositor thread but also observed by the main thread. An example is
    // synchronized scrolling where a scroll listener on the main thread changes
    // page layout based on the current scroll position.
    SYNCHRONIZED_GESTURE,
    // A gesture has recently started and we are about to run main thread touch
    // listeners to find out the actual gesture type. To minimize touch latency,
    // only input handling work should run in this state.
    TOUCHSTART,
    // The page is loading.
    LOADING,
    // A continuous gesture (e.g., scroll) which is being handled by the main
    // thread.
    MAIN_THREAD_GESTURE,
    // Must be the last entry.
    USE_CASE_COUNT,
    FIRST_USE_CASE = NONE,
  };
  static const char* UseCaseToString(UseCase use_case);
  static const char* RAILModeToString(v8::RAILMode rail_mode);

  RendererSchedulerImpl(scoped_refptr<SchedulerTqmDelegate> main_task_runner);
  ~RendererSchedulerImpl() override;

  // RendererScheduler implementation:
  std::unique_ptr<blink::WebThread> CreateMainThread() override;
  scoped_refptr<TaskQueue> DefaultTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  scoped_refptr<TaskQueue> CompositorTaskRunner() override;
  scoped_refptr<TaskQueue> LoadingTaskRunner() override;
  scoped_refptr<TaskQueue> TimerTaskRunner() override;
  scoped_refptr<TaskQueue> NewLoadingTaskRunner(const char* name) override;
  scoped_refptr<TaskQueue> NewTimerTaskRunner(const char* name) override;
  std::unique_ptr<RenderWidgetSchedulingState> NewRenderWidgetSchedulingState()
      override;
  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const blink::WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void DidHandleInputEventOnMainThread(
      const blink::WebInputEvent& web_input_event) override;
  void DidAnimateForInputOnCompositorThread() override;
  void OnRendererBackgrounded() override;
  void OnRendererForegrounded() override;
  void SuspendRenderer() override;
  void AddPendingNavigation(
      blink::WebScheduler::NavigatingFrameType type) override;
  void RemovePendingNavigation(
      blink::WebScheduler::NavigatingFrameType type) override;
  void OnNavigationStarted() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool ShouldYieldForHighPriorityWork() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SuspendTimerQueue() override;
  void ResumeTimerQueue() override;
  void SetTimerQueueSuspensionWhenBackgroundedEnabled(bool enabled) override;
  void SetTopLevelBlameContext(
      base::trace_event::BlameContext* blame_context) override;
  void SetRAILModeObserver(RAILModeObserver* observer) override;

  // RenderWidgetSignals::Observer implementation:
  void SetAllRenderWidgetsHidden(bool hidden) override;
  void SetHasVisibleRenderWidgetWithTouchHandler(
      bool has_visible_render_widget_with_touch_handler) override;

  // SchedulerHelper::Observer implementation:
  void OnUnregisterTaskQueue(const scoped_refptr<TaskQueue>& queue) override;
  void OnTriedToExecuteBlockedTask(const TaskQueue& queue,
                                   const base::PendingTask& task) override;

  // Returns a task runner where tasks run at the highest possible priority.
  scoped_refptr<TaskQueue> ControlTaskRunner();

  void RegisterTimeDomain(TimeDomain* time_domain);
  void UnregisterTimeDomain(TimeDomain* time_domain);

  void SetExpensiveTaskBlockingAllowed(bool allowed);

  void AddWebViewScheduler(WebViewSchedulerImpl* web_view_scheduler);
  void RemoveWebViewScheduler(WebViewSchedulerImpl* web_view_scheduler);

  // Test helpers.
  SchedulerHelper* GetSchedulerHelperForTesting();
  TaskCostEstimator* GetLoadingTaskCostEstimatorForTesting();
  TaskCostEstimator* GetTimerTaskCostEstimatorForTesting();
  IdleTimeEstimator* GetIdleTimeEstimatorForTesting();
  base::TimeTicks CurrentIdleTaskDeadlineForTesting() const;
  void RunIdleTasksForTesting(const base::Closure& callback);
  void EndIdlePeriodForTesting(const base::Closure& callback,
                               base::TimeTicks time_remaining);
  bool PolicyNeedsUpdateForTesting();

  base::TickClock* tick_clock() const;

  RealTimeDomain* real_time_domain() const {
    return helper_.real_time_domain();
  }

  ThrottlingHelper* throttling_helper() { return throttling_helper_.get(); }

 private:
  friend class RendererSchedulerImplTest;
  friend class RendererSchedulerImplForTest;
  friend class RenderWidgetSchedulingState;

  enum class ExpensiveTaskPolicy { RUN, BLOCK, THROTTLE };

  enum class TimeDomainType {
    REAL,
    THROTTLED,
  };

  struct TaskQueuePolicy {
    TaskQueuePolicy()
        : is_enabled(true),
          priority(TaskQueue::NORMAL_PRIORITY),
          time_domain_type(TimeDomainType::REAL) {}

    bool is_enabled;
    TaskQueue::QueuePriority priority;
    TimeDomainType time_domain_type;

    bool operator==(const TaskQueuePolicy& other) const {
      return is_enabled == other.is_enabled && priority == other.priority &&
             time_domain_type == other.time_domain_type;
    }
  };

  struct Policy {
    TaskQueuePolicy compositor_queue_policy;
    TaskQueuePolicy loading_queue_policy;
    TaskQueuePolicy timer_queue_policy;
    TaskQueuePolicy default_queue_policy;
    v8::RAILMode rail_mode = v8::PERFORMANCE_ANIMATION;

    bool operator==(const Policy& other) const {
      return compositor_queue_policy == other.compositor_queue_policy &&
             loading_queue_policy == other.loading_queue_policy &&
             timer_queue_policy == other.timer_queue_policy &&
             default_queue_policy == other.default_queue_policy &&
             rail_mode == other.rail_mode;
    }
  };

  class PollableNeedsUpdateFlag {
   public:
    PollableNeedsUpdateFlag(base::Lock* write_lock);
    ~PollableNeedsUpdateFlag();

    // Set the flag. May only be called if |write_lock| is held.
    void SetWhileLocked(bool value);

    // Returns true iff the flag is set to true.
    bool IsSet() const;

   private:
    base::subtle::Atomic32 flag_;
    base::Lock* write_lock_;  // Not owned.

    DISALLOW_COPY_AND_ASSIGN(PollableNeedsUpdateFlag);
  };

  // IdleHelper::Delegate implementation:
  bool CanEnterLongIdlePeriod(
      base::TimeTicks now,
      base::TimeDelta* next_long_idle_period_delay_out) override;
  void IsNotQuiescent() override {}
  void OnIdlePeriodStarted() override;
  void OnIdlePeriodEnded() override;

  void EndIdlePeriod();

  // Returns the serialized scheduler state for tracing.
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue(
      base::TimeTicks optional_now) const;
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValueLocked(
      base::TimeTicks optional_now) const;

  static bool ShouldPrioritizeInputEvent(
      const blink::WebInputEvent& web_input_event);

  // The amount of time which idle periods can continue being scheduled when the
  // renderer has been hidden, before going to sleep for good.
  static const int kEndIdleWhenHiddenDelayMillis = 10000;

  // The amount of time for which loading tasks will be prioritized over
  // other tasks during the initial page load.
  static const int kRailsInitialLoadingPrioritizationMillis = 1000;

  // The amount of time in milliseconds we have to respond to user input as
  // defined by RAILS.
  static const int kRailsResponseTimeMillis = 50;

  // For the purposes of deciding whether or not it's safe to turn timers and
  // loading tasks on only in idle periods, we regard the system as being as
  // being "idle period" starved if there hasn't been an idle period in the last
  // 10 seconds. This was chosen to be long enough to cover most anticipated
  // user gestures.
  static const int kIdlePeriodStarvationThresholdMillis = 10000;

  // The amount of time to wait before suspending shared timers after the
  // renderer has been backgrounded. This is used only if background suspension
  // of shared timers is enabled.
  static const int kSuspendTimersWhenBackgroundedDelayMillis = 5 * 60 * 1000;

  // The time we should stay in a priority-escalated mode after a call to
  // DidAnimateForInputOnCompositorThread().
  static const int kFlingEscalationLimitMillis = 100;

  // Schedules an immediate PolicyUpdate, if there isn't one already pending and
  // sets |policy_may_need_update_|. Note |any_thread_lock_| must be
  // locked.
  void EnsureUrgentPolicyUpdatePostedOnMainThread(
      const tracked_objects::Location& from_here);

  // Update the policy if a new signal has arrived. Must be called from the main
  // thread.
  void MaybeUpdatePolicy();

  // Locks |any_thread_lock_| and updates the scheduler policy.  May early
  // out if the policy is unchanged. Must be called from the main thread.
  void UpdatePolicy();

  // Like UpdatePolicy, except it doesn't early out.
  void ForceUpdatePolicy();

  enum class UpdateType {
    MAY_EARLY_OUT_IF_POLICY_UNCHANGED,
    FORCE_UPDATE,
  };

  // The implelemtation of UpdatePolicy & ForceUpdatePolicy.  It is allowed to
  // early out if |update_type| is MAY_EARLY_OUT_IF_POLICY_UNCHANGED.
  virtual void UpdatePolicyLocked(UpdateType update_type);

  // Helper for computing the use case. |expected_usecase_duration| will be
  // filled with the amount of time after which the use case should be updated
  // again. If the duration is zero, a new use case update should not be
  // scheduled. Must be called with |any_thread_lock_| held. Can be called from
  // any thread.
  UseCase ComputeCurrentUseCase(
      base::TimeTicks now,
      base::TimeDelta* expected_use_case_duration) const;

  // An input event of some sort happened, the policy may need updating.
  void UpdateForInputEventOnCompositorThread(blink::WebInputEvent::Type type,
                                             InputEventState input_event_state);

  // Returns true if there has been at least one idle period in the last
  // |kIdlePeriodStarvationThresholdMillis|.
  bool HadAnIdlePeriodRecently(base::TimeTicks now) const;

  // Helpers for safely suspending/resuming the timer queue after a
  // background/foreground signal.
  void SuspendTimerQueueWhenBackgrounded();
  void ResumeTimerQueueWhenForegrounded();

  // The task cost estimators and the UserModel need to be reset upon page
  // nagigation. This function does that. Must be called from the main thread.
  void ResetForNavigationLocked();

  // Estimates the maximum task length that won't cause a jank based on the
  // current system state. Must be called from the main thread.
  base::TimeDelta EstimateLongestJankFreeTaskDuration() const;

  // Log a console warning message to all WebViews in this process.
  void BroadcastConsoleWarning(const std::string& message);

  void ApplyTaskQueuePolicy(TaskQueue* task_queue,
                            const TaskQueuePolicy& old_task_queue_policy,
                            const TaskQueuePolicy& new_task_queue_policy) const;

  static const char* ExpensiveTaskPolicyToString(
      ExpensiveTaskPolicy expensive_task_policy);

  SchedulerHelper helper_;
  IdleHelper idle_helper_;
  std::unique_ptr<ThrottlingHelper> throttling_helper_;
  RenderWidgetSignals render_widget_scheduler_signals_;

  const scoped_refptr<TaskQueue> control_task_runner_;
  const scoped_refptr<TaskQueue> compositor_task_runner_;
  std::set<scoped_refptr<TaskQueue>> loading_task_runners_;
  std::set<scoped_refptr<TaskQueue>> timer_task_runners_;
  scoped_refptr<TaskQueue> default_loading_task_runner_;
  scoped_refptr<TaskQueue> default_timer_task_runner_;

  base::Closure update_policy_closure_;
  DeadlineTaskRunner delayed_update_policy_runner_;
  CancelableClosureHolder end_renderer_hidden_idle_period_closure_;
  CancelableClosureHolder suspend_timers_when_backgrounded_closure_;

  // We have decided to improve thread safety at the cost of some boilerplate
  // (the accessors) for the following data members.

  struct MainThreadOnly {
    MainThreadOnly(const scoped_refptr<TaskQueue>& compositor_task_runner,
                   base::TickClock* time_source);
    ~MainThreadOnly();

    TaskCostEstimator loading_task_cost_estimator;
    TaskCostEstimator timer_task_cost_estimator;
    IdleTimeEstimator idle_time_estimator;
    UseCase current_use_case;
    Policy current_policy;
    base::TimeTicks current_policy_expiration_time;
    base::TimeTicks estimated_next_frame_begin;
    base::TimeDelta compositor_frame_interval;
    base::TimeDelta longest_jank_free_task_duration;
    int timer_queue_suspend_count;  // TIMER_TASK_QUEUE suspended if non-zero.
    int navigation_task_expected_count;
    ExpensiveTaskPolicy expensive_task_policy;
    bool renderer_hidden;
    bool renderer_backgrounded;
    bool renderer_suspended;
    bool timer_queue_suspension_when_backgrounded_enabled;
    bool timer_queue_suspended_when_backgrounded;
    bool was_shutdown;
    bool loading_tasks_seem_expensive;
    bool timer_tasks_seem_expensive;
    bool touchstart_expected_soon;
    bool have_seen_a_begin_main_frame;
    bool have_reported_blocking_intervention_in_current_policy;
    bool have_reported_blocking_intervention_since_navigation;
    bool has_visible_render_widget_with_touch_handler;
    bool begin_frame_not_expected_soon;
    bool expensive_task_blocking_allowed;
    bool in_idle_period_for_testing;
    std::set<WebViewSchedulerImpl*> web_view_schedulers;  // Not owned.
    RAILModeObserver* rail_mode_observer;                 // Not owned.
  };

  struct AnyThread {
    AnyThread();
    ~AnyThread();

    base::TimeTicks last_idle_period_end_time;
    base::TimeTicks rails_loading_priority_deadline;
    base::TimeTicks fling_compositor_escalation_deadline;
    UserModel user_model;
    bool awaiting_touch_start_response;
    bool in_idle_period;
    bool begin_main_frame_on_critical_path;
    bool last_gesture_was_compositor_driven;
    bool default_gesture_prevented;
    bool have_seen_touchstart;
  };

  struct CompositorThreadOnly {
    CompositorThreadOnly();
    ~CompositorThreadOnly();

    blink::WebInputEvent::Type last_input_type;
    std::unique_ptr<base::ThreadChecker> compositor_thread_checker;

    void CheckOnValidThread() {
#if DCHECK_IS_ON()
      // We don't actually care which thread this called from, just so long as
      // its consistent.
      if (!compositor_thread_checker)
        compositor_thread_checker.reset(new base::ThreadChecker());
      DCHECK(compositor_thread_checker->CalledOnValidThread());
#endif
    }
  };

  // Don't access main_thread_only_, instead use MainThreadOnly().
  MainThreadOnly main_thread_only_;
  MainThreadOnly& MainThreadOnly() {
    helper_.CheckOnValidThread();
    return main_thread_only_;
  }
  const struct MainThreadOnly& MainThreadOnly() const {
    helper_.CheckOnValidThread();
    return main_thread_only_;
  }

  mutable base::Lock any_thread_lock_;
  // Don't access any_thread_, instead use AnyThread().
  AnyThread any_thread_;
  AnyThread& AnyThread() {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }
  const struct AnyThread& AnyThread() const {
    any_thread_lock_.AssertAcquired();
    return any_thread_;
  }

  // Don't access compositor_thread_only_, instead use CompositorThreadOnly().
  CompositorThreadOnly compositor_thread_only_;
  CompositorThreadOnly& CompositorThreadOnly() {
    compositor_thread_only_.CheckOnValidThread();
    return compositor_thread_only_;
  }

  PollableThreadSafeFlag policy_may_need_update_;
  base::WeakPtrFactory<RendererSchedulerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererSchedulerImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_RENDERER_SCHEDULER_IMPL_H_
