// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/renderer/renderer_scheduler_impl.h"

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/output/begin_frame_args.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/base/task_queue_impl.h"
#include "platform/scheduler/base/task_queue_selector.h"
#include "platform/scheduler/base/virtual_time_domain.h"
#include "platform/scheduler/child/scheduler_tqm_delegate.h"
#include "platform/scheduler/renderer/auto_advancing_virtual_time_domain.h"
#include "platform/scheduler/renderer/task_queue_throttler.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "platform/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"

namespace blink {
namespace scheduler {
namespace {
// The run time of loading tasks is strongly bimodal.  The vast majority are
// very cheap, but there are usually a handful of very expensive tasks (e.g ~1
// second on a mobile device) so we take a very pessimistic view when estimating
// the cost of loading tasks.
const int kLoadingTaskEstimationSampleCount = 1000;
const double kLoadingTaskEstimationPercentile = 99;
const int kTimerTaskEstimationSampleCount = 1000;
const double kTimerTaskEstimationPercentile = 99;
const int kShortIdlePeriodDurationSampleCount = 10;
const double kShortIdlePeriodDurationPercentile = 50;
// Amount of idle time left in a frame (as a ratio of the vsync interval) above
// which main thread compositing can be considered fast.
const double kFastCompositingIdleTimeThreshold = .2;
constexpr base::TimeDelta kThreadLoadTrackerReportingInterval =
    base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kThreadLoadTrackerWaitingPeriodBeforeReporting =
    base::TimeDelta::FromMinutes(2);
// We do not throttle anything while audio is played and shortly after that.
constexpr base::TimeDelta kThrottlingDelayAfterAudioIsPlayed =
    base::TimeDelta::FromSeconds(5);

void ReportForegroundRendererTaskLoad(base::TimeTicks time, double load) {
  int load_percentage = static_cast<int>(load * 100);
  UMA_HISTOGRAM_PERCENTAGE("RendererScheduler.ForegroundRendererMainThreadLoad",
                           load_percentage);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.ForegroundRendererLoad", load_percentage);
}

void ReportBackgroundRendererTaskLoad(base::TimeTicks time, double load) {
  int load_percentage = static_cast<int>(load * 100);
  UMA_HISTOGRAM_PERCENTAGE("RendererScheduler.BackgroundRendererMainThreadLoad",
                           load_percentage);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.BackgroundRendererLoad", load_percentage);
}

base::TimeTicks MonotonicTimeInSecondsToTimeTicks(
    double monotonicTimeInSeconds) {
  return base::TimeTicks() + base::TimeDelta::FromSecondsD(
      monotonicTimeInSeconds);
}
}  // namespace

RendererSchedulerImpl::RendererSchedulerImpl(
    scoped_refptr<SchedulerTqmDelegate> main_task_runner)
    : helper_(main_task_runner,
              "renderer.scheduler",
              TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
              TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug")),
      idle_helper_(&helper_,
                   this,
                   "renderer.scheduler",
                   TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                   "RendererSchedulerIdlePeriod",
                   base::TimeDelta()),
      render_widget_scheduler_signals_(this),
      control_task_runner_(helper_.ControlTaskRunner()),
      compositor_task_runner_(
          helper_.NewTaskQueue(TaskQueue::Spec(TaskQueue::QueueType::COMPOSITOR)
                                   .SetShouldMonitorQuiescence(true))),
      delayed_update_policy_runner_(
          base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                     base::Unretained(this)),
          helper_.ControlTaskRunner()),
      main_thread_only_(this,
                        compositor_task_runner_,
                        helper_.scheduler_tqm_delegate().get(),
                        helper_.scheduler_tqm_delegate()->NowTicks()),
      policy_may_need_update_(&any_thread_lock_),
      weak_factory_(this) {
  task_queue_throttler_.reset(
      new TaskQueueThrottler(this, "renderer.scheduler"));
  update_policy_closure_ = base::Bind(&RendererSchedulerImpl::UpdatePolicy,
                                      weak_factory_.GetWeakPtr());
  end_renderer_hidden_idle_period_closure_.Reset(base::Bind(
      &RendererSchedulerImpl::EndIdlePeriod, weak_factory_.GetWeakPtr()));

  suspend_timers_when_backgrounded_closure_.Reset(
      base::Bind(&RendererSchedulerImpl::SuspendTimerQueueWhenBackgrounded,
                 weak_factory_.GetWeakPtr()));

  default_loading_task_runner_ =
      NewLoadingTaskRunner(TaskQueue::QueueType::DEFAULT_LOADING);
  default_timer_task_runner_ =
      NewTimerTaskRunner(TaskQueue::QueueType::DEFAULT_TIMER);

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);

  helper_.SetObserver(this);
  helper_.AddTaskTimeObserver(this);
}

RendererSchedulerImpl::~RendererSchedulerImpl() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this);

  for (const scoped_refptr<TaskQueue>& loading_queue : loading_task_runners_) {
    loading_queue->RemoveTaskObserver(
        &MainThreadOnly().loading_task_cost_estimator);
  }
  for (const scoped_refptr<TaskQueue>& timer_queue : timer_task_runners_) {
    timer_queue->RemoveTaskObserver(
        &MainThreadOnly().timer_task_cost_estimator);
  }

  if (virtual_time_domain_)
    UnregisterTimeDomain(virtual_time_domain_.get());

  helper_.RemoveTaskTimeObserver(this);

  // Ensure the renderer scheduler was shut down explicitly, because otherwise
  // we could end up having stale pointers to the Blink heap which has been
  // terminated by this point.
  DCHECK(MainThreadOnly().was_shutdown);
}

RendererSchedulerImpl::MainThreadOnly::MainThreadOnly(
    RendererSchedulerImpl* renderer_scheduler_impl,
    const scoped_refptr<TaskQueue>& compositor_task_runner,
    base::TickClock* time_source,
    base::TimeTicks now)
    : loading_task_cost_estimator(time_source,
                                  kLoadingTaskEstimationSampleCount,
                                  kLoadingTaskEstimationPercentile),
      timer_task_cost_estimator(time_source,
                                kTimerTaskEstimationSampleCount,
                                kTimerTaskEstimationPercentile),
      queueing_time_estimator(renderer_scheduler_impl,
                              base::TimeDelta::FromSeconds(1)),
      idle_time_estimator(compositor_task_runner,
                          time_source,
                          kShortIdlePeriodDurationSampleCount,
                          kShortIdlePeriodDurationPercentile),
      background_main_thread_load_tracker(
          now,
          base::Bind(&ReportBackgroundRendererTaskLoad),
          kThreadLoadTrackerReportingInterval,
          kThreadLoadTrackerWaitingPeriodBeforeReporting),
      foreground_main_thread_load_tracker(
          now,
          base::Bind(&ReportForegroundRendererTaskLoad),
          kThreadLoadTrackerReportingInterval,
          kThreadLoadTrackerWaitingPeriodBeforeReporting),
      current_use_case(UseCase::NONE),
      timer_queue_suspend_count(0),
      navigation_task_expected_count(0),
      expensive_task_policy(ExpensiveTaskPolicy::RUN),
      renderer_hidden(false),
      renderer_backgrounded(false),
      renderer_suspended(false),
      timer_queue_suspension_when_backgrounded_enabled(false),
      timer_queue_suspended_when_backgrounded(false),
      was_shutdown(false),
      loading_tasks_seem_expensive(false),
      timer_tasks_seem_expensive(false),
      touchstart_expected_soon(false),
      have_seen_a_begin_main_frame(false),
      have_reported_blocking_intervention_in_current_policy(false),
      have_reported_blocking_intervention_since_navigation(false),
      has_visible_render_widget_with_touch_handler(false),
      begin_frame_not_expected_soon(false),
      in_idle_period_for_testing(false),
      use_virtual_time(false),
      is_audio_playing(false),
      rail_mode_observer(nullptr) {}

RendererSchedulerImpl::MainThreadOnly::~MainThreadOnly() {}

RendererSchedulerImpl::AnyThread::AnyThread()
    : awaiting_touch_start_response(false),
      in_idle_period(false),
      begin_main_frame_on_critical_path(false),
      last_gesture_was_compositor_driven(false),
      default_gesture_prevented(true),
      have_seen_touchstart(false) {}

RendererSchedulerImpl::AnyThread::~AnyThread() {}

RendererSchedulerImpl::CompositorThreadOnly::CompositorThreadOnly()
    : last_input_type(blink::WebInputEvent::Undefined) {}

RendererSchedulerImpl::CompositorThreadOnly::~CompositorThreadOnly() {}

void RendererSchedulerImpl::Shutdown() {
  base::TimeTicks now = tick_clock()->NowTicks();
  MainThreadOnly().background_main_thread_load_tracker.RecordIdle(now);
  MainThreadOnly().foreground_main_thread_load_tracker.RecordIdle(now);

  task_queue_throttler_.reset();
  helper_.Shutdown();
  idle_helper_.Shutdown();
  MainThreadOnly().was_shutdown = true;
  MainThreadOnly().rail_mode_observer = nullptr;
}

std::unique_ptr<blink::WebThread> RendererSchedulerImpl::CreateMainThread() {
  return base::MakeUnique<WebThreadImplForRendererScheduler>(this);
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::DefaultTaskRunner() {
  return helper_.DefaultTaskRunner();
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::CompositorTaskRunner() {
  helper_.CheckOnValidThread();
  return compositor_task_runner_;
}

scoped_refptr<SingleThreadIdleTaskRunner>
RendererSchedulerImpl::IdleTaskRunner() {
  return idle_helper_.IdleTaskRunner();
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::LoadingTaskRunner() {
  helper_.CheckOnValidThread();
  return default_loading_task_runner_;
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::TimerTaskRunner() {
  helper_.CheckOnValidThread();
  return default_timer_task_runner_;
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::ControlTaskRunner() {
  helper_.CheckOnValidThread();
  return helper_.ControlTaskRunner();
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::NewLoadingTaskRunner(
    TaskQueue::QueueType queue_type) {
  helper_.CheckOnValidThread();
  scoped_refptr<TaskQueue> loading_task_queue(
      helper_.NewTaskQueue(TaskQueue::Spec(queue_type)
                               .SetShouldMonitorQuiescence(true)
                               .SetTimeDomain(MainThreadOnly().use_virtual_time
                                                  ? GetVirtualTimeDomain()
                                                  : nullptr)));
  loading_task_runners_.insert(loading_task_queue);
  loading_task_queue->SetQueueEnabled(
      MainThreadOnly().current_policy.loading_queue_policy.is_enabled);
  loading_task_queue->SetQueuePriority(
      MainThreadOnly().current_policy.loading_queue_policy.priority);
  if (MainThreadOnly().current_policy.loading_queue_policy.time_domain_type ==
      TimeDomainType::THROTTLED) {
    task_queue_throttler_->IncreaseThrottleRefCount(loading_task_queue.get());
  }
  loading_task_queue->AddTaskObserver(
      &MainThreadOnly().loading_task_cost_estimator);
  return loading_task_queue;
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::NewTimerTaskRunner(
    TaskQueue::QueueType queue_type) {
  helper_.CheckOnValidThread();
  // TODO(alexclarke): Consider using ApplyTaskQueuePolicy() for brevity.
  scoped_refptr<TaskQueue> timer_task_queue(
      helper_.NewTaskQueue(TaskQueue::Spec(queue_type)
                               .SetShouldMonitorQuiescence(true)
                               .SetShouldReportWhenExecutionBlocked(true)
                               .SetTimeDomain(MainThreadOnly().use_virtual_time
                                                  ? GetVirtualTimeDomain()
                                                  : nullptr)));
  timer_task_runners_.insert(timer_task_queue);
  timer_task_queue->SetQueueEnabled(
      MainThreadOnly().current_policy.timer_queue_policy.is_enabled);
  timer_task_queue->SetQueuePriority(
      MainThreadOnly().current_policy.timer_queue_policy.priority);
  if (MainThreadOnly().current_policy.timer_queue_policy.time_domain_type ==
      TimeDomainType::THROTTLED) {
    task_queue_throttler_->IncreaseThrottleRefCount(timer_task_queue.get());
  }
  timer_task_queue->AddTaskObserver(
      &MainThreadOnly().timer_task_cost_estimator);
  return timer_task_queue;
}

scoped_refptr<TaskQueue> RendererSchedulerImpl::NewUnthrottledTaskRunner(
    TaskQueue::QueueType queue_type) {
  helper_.CheckOnValidThread();
  scoped_refptr<TaskQueue> unthrottled_task_queue(
      helper_.NewTaskQueue(TaskQueue::Spec(queue_type)
                               .SetShouldMonitorQuiescence(true)
                               .SetTimeDomain(MainThreadOnly().use_virtual_time
                                                  ? GetVirtualTimeDomain()
                                                  : nullptr)));
  unthrottled_task_runners_.insert(unthrottled_task_queue);
  return unthrottled_task_queue;
}

std::unique_ptr<RenderWidgetSchedulingState>
RendererSchedulerImpl::NewRenderWidgetSchedulingState() {
  return render_widget_scheduler_signals_.NewRenderWidgetSchedulingState();
}

void RendererSchedulerImpl::OnUnregisterTaskQueue(
    const scoped_refptr<TaskQueue>& task_queue) {
  if (task_queue_throttler_)
    task_queue_throttler_->UnregisterTaskQueue(task_queue.get());

  if (loading_task_runners_.find(task_queue) != loading_task_runners_.end()) {
    task_queue->RemoveTaskObserver(
        &MainThreadOnly().loading_task_cost_estimator);
    loading_task_runners_.erase(task_queue);
  } else if (timer_task_runners_.find(task_queue) !=
             timer_task_runners_.end()) {
    task_queue->RemoveTaskObserver(&MainThreadOnly().timer_task_cost_estimator);
    timer_task_runners_.erase(task_queue);
  } else if (unthrottled_task_runners_.find(task_queue) !=
             unthrottled_task_runners_.end()) {
    unthrottled_task_runners_.erase(task_queue);
  }
}

bool RendererSchedulerImpl::CanExceedIdleDeadlineIfRequired() const {
  return idle_helper_.CanExceedIdleDeadlineIfRequired();
}

void RendererSchedulerImpl::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_.AddTaskObserver(task_observer);
}

void RendererSchedulerImpl::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper_.RemoveTaskObserver(task_observer);
}

void RendererSchedulerImpl::WillBeginFrame(const cc::BeginFrameArgs& args) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::WillBeginFrame", "args", args.AsValue());
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  EndIdlePeriod();
  MainThreadOnly().estimated_next_frame_begin = args.frame_time + args.interval;
  MainThreadOnly().have_seen_a_begin_main_frame = true;
  MainThreadOnly().begin_frame_not_expected_soon = false;
  MainThreadOnly().compositor_frame_interval = args.interval;
  {
    base::AutoLock lock(any_thread_lock_);
    AnyThread().begin_main_frame_on_critical_path = args.on_critical_path;
  }
}

void RendererSchedulerImpl::DidCommitFrameToCompositor() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidCommitFrameToCompositor");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now(helper_.scheduler_tqm_delegate()->NowTicks());
  if (now < MainThreadOnly().estimated_next_frame_begin) {
    // TODO(rmcilroy): Consider reducing the idle period based on the runtime of
    // the next pending delayed tasks (as currently done in for long idle times)
    idle_helper_.StartIdlePeriod(
        IdleHelper::IdlePeriodState::IN_SHORT_IDLE_PERIOD, now,
        MainThreadOnly().estimated_next_frame_begin);
  }

  MainThreadOnly().idle_time_estimator.DidCommitFrameToCompositor();
}

void RendererSchedulerImpl::BeginFrameNotExpectedSoon() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::BeginFrameNotExpectedSoon");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return;

  MainThreadOnly().begin_frame_not_expected_soon = true;
  idle_helper_.EnableLongIdlePeriod();
  {
    base::AutoLock lock(any_thread_lock_);
    AnyThread().begin_main_frame_on_critical_path = false;
  }
}

void RendererSchedulerImpl::SetAllRenderWidgetsHidden(bool hidden) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::SetAllRenderWidgetsHidden", "hidden",
               hidden);

  helper_.CheckOnValidThread();

  if (helper_.IsShutdown() || MainThreadOnly().renderer_hidden == hidden)
    return;

  end_renderer_hidden_idle_period_closure_.Cancel();

  if (hidden) {
    idle_helper_.EnableLongIdlePeriod();

    // Ensure that we stop running idle tasks after a few seconds of being
    // hidden.
    base::TimeDelta end_idle_when_hidden_delay =
        base::TimeDelta::FromMilliseconds(kEndIdleWhenHiddenDelayMillis);
    control_task_runner_->PostDelayedTask(
        FROM_HERE, end_renderer_hidden_idle_period_closure_.callback(),
        end_idle_when_hidden_delay);
    MainThreadOnly().renderer_hidden = true;
  } else {
    MainThreadOnly().renderer_hidden = false;
    EndIdlePeriod();
  }

  // TODO(alexclarke): Should we update policy here?
  CreateTraceEventObjectSnapshot();
}

void RendererSchedulerImpl::SetHasVisibleRenderWidgetWithTouchHandler(
    bool has_visible_render_widget_with_touch_handler) {
  helper_.CheckOnValidThread();
  if (has_visible_render_widget_with_touch_handler ==
      MainThreadOnly().has_visible_render_widget_with_touch_handler)
    return;

  MainThreadOnly().has_visible_render_widget_with_touch_handler =
      has_visible_render_widget_with_touch_handler;

  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::FORCE_UPDATE);
}

void RendererSchedulerImpl::OnRendererBackgrounded() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnRendererBackgrounded");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() || MainThreadOnly().renderer_backgrounded)
    return;

  MainThreadOnly().renderer_backgrounded = true;

  base::TimeTicks now = tick_clock()->NowTicks();
  MainThreadOnly().foreground_main_thread_load_tracker.Pause(now);
  MainThreadOnly().background_main_thread_load_tracker.Resume(now);

  if (!MainThreadOnly().timer_queue_suspension_when_backgrounded_enabled)
    return;

  suspend_timers_when_backgrounded_closure_.Cancel();
  base::TimeDelta suspend_timers_when_backgrounded_delay =
      base::TimeDelta::FromMilliseconds(
          kSuspendTimersWhenBackgroundedDelayMillis);
  control_task_runner_->PostDelayedTask(
      FROM_HERE, suspend_timers_when_backgrounded_closure_.callback(),
      suspend_timers_when_backgrounded_delay);
}

void RendererSchedulerImpl::OnRendererForegrounded() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnRendererForegrounded");
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown() || !MainThreadOnly().renderer_backgrounded)
    return;

  MainThreadOnly().renderer_backgrounded = false;
  MainThreadOnly().renderer_suspended = false;

  base::TimeTicks now = tick_clock()->NowTicks();
  MainThreadOnly().foreground_main_thread_load_tracker.Resume(now);
  MainThreadOnly().background_main_thread_load_tracker.Pause(now);

  suspend_timers_when_backgrounded_closure_.Cancel();
  ResumeTimerQueueWhenForegroundedOrResumed();
}

void RendererSchedulerImpl::OnAudioStateChanged() {
  bool is_audio_playing = false;
  for (WebViewSchedulerImpl* web_view_scheduler :
       MainThreadOnly().web_view_schedulers) {
    is_audio_playing = is_audio_playing || web_view_scheduler->IsAudioPlaying();
  }

  if (is_audio_playing == MainThreadOnly().is_audio_playing)
    return;

  MainThreadOnly().last_audio_state_change =
      helper_.scheduler_tqm_delegate()->NowTicks();
  MainThreadOnly().is_audio_playing = is_audio_playing;

  UpdatePolicy();
}

void RendererSchedulerImpl::SuspendRenderer() {
  helper_.CheckOnValidThread();
  DCHECK(MainThreadOnly().renderer_backgrounded);
  if (helper_.IsShutdown())
    return;
  suspend_timers_when_backgrounded_closure_.Cancel();

  UMA_HISTOGRAM_COUNTS("PurgeAndSuspend.PendingTaskCount",
                       helper_.GetNumberOfPendingTasks());

  // TODO(hajimehoshi): We might need to suspend not only timer queue but also
  // e.g. loading tasks or postMessage.
  MainThreadOnly().renderer_suspended = true;
  SuspendTimerQueueWhenBackgrounded();
}

void RendererSchedulerImpl::ResumeRenderer() {
  helper_.CheckOnValidThread();
  DCHECK(MainThreadOnly().renderer_backgrounded);
  if (helper_.IsShutdown())
    return;
  suspend_timers_when_backgrounded_closure_.Cancel();
  MainThreadOnly().renderer_suspended = false;
  ResumeTimerQueueWhenForegroundedOrResumed();
}

void RendererSchedulerImpl::EndIdlePeriod() {
  if (MainThreadOnly().in_idle_period_for_testing)
    return;
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::EndIdlePeriod");
  helper_.CheckOnValidThread();
  idle_helper_.EndIdlePeriod();
}

void RendererSchedulerImpl::EndIdlePeriodForTesting(
    const base::Closure& callback,
    base::TimeTicks time_remaining) {
  MainThreadOnly().in_idle_period_for_testing = false;
  EndIdlePeriod();
  callback.Run();
}

bool RendererSchedulerImpl::PolicyNeedsUpdateForTesting() {
  return policy_may_need_update_.IsSet();
}

// static
bool RendererSchedulerImpl::ShouldPrioritizeInputEvent(
    const blink::WebInputEvent& web_input_event) {
  // We regard MouseMove events with the left mouse button down as a signal
  // that the user is doing something requiring a smooth frame rate.
  if ((web_input_event.type == blink::WebInputEvent::MouseDown ||
       web_input_event.type == blink::WebInputEvent::MouseMove) &&
      (web_input_event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    return true;
  }
  // Ignore all other mouse events because they probably don't signal user
  // interaction needing a smooth framerate. NOTE isMouseEventType returns false
  // for mouse wheel events, hence we regard them as user input.
  // Ignore keyboard events because it doesn't really make sense to enter
  // compositor priority for them.
  if (blink::WebInputEvent::isMouseEventType(web_input_event.type) ||
      blink::WebInputEvent::isKeyboardEventType(web_input_event.type)) {
    return false;
  }
  return true;
}

void RendererSchedulerImpl::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    InputEventState event_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidHandleInputEventOnCompositorThread");
  if (!ShouldPrioritizeInputEvent(web_input_event))
    return;

  UpdateForInputEventOnCompositorThread(web_input_event.type, event_state);
}

void RendererSchedulerImpl::DidAnimateForInputOnCompositorThread() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidAnimateForInputOnCompositorThread");
  base::AutoLock lock(any_thread_lock_);
  AnyThread().fling_compositor_escalation_deadline =
      helper_.scheduler_tqm_delegate()->NowTicks() +
      base::TimeDelta::FromMilliseconds(kFlingEscalationLimitMillis);
}

void RendererSchedulerImpl::UpdateForInputEventOnCompositorThread(
    blink::WebInputEvent::Type type,
    InputEventState input_event_state) {
  base::AutoLock lock(any_thread_lock_);
  base::TimeTicks now = helper_.scheduler_tqm_delegate()->NowTicks();

  // TODO(alexclarke): Move WebInputEventTraits where we can access it from here
  // and record the name rather than the integer representation.
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::UpdateForInputEventOnCompositorThread",
               "type", static_cast<int>(type), "input_event_state",
               InputEventStateToString(input_event_state));

  base::TimeDelta unused_policy_duration;
  UseCase previous_use_case =
      ComputeCurrentUseCase(now, &unused_policy_duration);
  bool was_awaiting_touch_start_response =
      AnyThread().awaiting_touch_start_response;

  AnyThread().user_model.DidStartProcessingInputEvent(type, now);

  if (input_event_state == InputEventState::EVENT_CONSUMED_BY_COMPOSITOR)
    AnyThread().user_model.DidFinishProcessingInputEvent(now);

  switch (type) {
    case blink::WebInputEvent::TouchStart:
      AnyThread().awaiting_touch_start_response = true;
      // This is just a fail-safe to reset the state of
      // |last_gesture_was_compositor_driven| to the default. We don't know
      // yet where the gesture will run.
      AnyThread().last_gesture_was_compositor_driven = false;
      AnyThread().have_seen_touchstart = true;
      // Assume the default gesture is prevented until we see evidence
      // otherwise.
      AnyThread().default_gesture_prevented = true;
      break;

    case blink::WebInputEvent::TouchMove:
      // Observation of consecutive touchmoves is a strong signal that the
      // page is consuming the touch sequence, in which case touchstart
      // response prioritization is no longer necessary. Otherwise, the
      // initial touchmove should preserve the touchstart response pending
      // state.
      if (AnyThread().awaiting_touch_start_response &&
          CompositorThreadOnly().last_input_type ==
              blink::WebInputEvent::TouchMove) {
        AnyThread().awaiting_touch_start_response = false;
      }
      break;

    case blink::WebInputEvent::GesturePinchUpdate:
    case blink::WebInputEvent::GestureScrollUpdate:
      // If we see events for an established gesture, we can lock it to the
      // appropriate thread as the gesture can no longer be cancelled.
      AnyThread().last_gesture_was_compositor_driven =
          input_event_state == InputEventState::EVENT_CONSUMED_BY_COMPOSITOR;
      AnyThread().awaiting_touch_start_response = false;
      AnyThread().default_gesture_prevented = false;
      break;

    case blink::WebInputEvent::GestureFlingCancel:
      AnyThread().fling_compositor_escalation_deadline = base::TimeTicks();
      break;

    case blink::WebInputEvent::GestureTapDown:
    case blink::WebInputEvent::GestureShowPress:
    case blink::WebInputEvent::GestureScrollEnd:
      // With no observable effect, these meta events do not indicate a
      // meaningful touchstart response and should not impact task priority.
      break;

    case blink::WebInputEvent::MouseDown:
      // Reset tracking state at the start of a new mouse drag gesture.
      AnyThread().last_gesture_was_compositor_driven = false;
      AnyThread().default_gesture_prevented = true;
      break;

    case blink::WebInputEvent::MouseMove:
      // Consider mouse movement with the left button held down (see
      // ShouldPrioritizeInputEvent) similarly to a touch gesture.
      AnyThread().last_gesture_was_compositor_driven =
          input_event_state == InputEventState::EVENT_CONSUMED_BY_COMPOSITOR;
      AnyThread().awaiting_touch_start_response = false;
      break;

    case blink::WebInputEvent::MouseWheel:
      AnyThread().last_gesture_was_compositor_driven =
          input_event_state == InputEventState::EVENT_CONSUMED_BY_COMPOSITOR;
      AnyThread().awaiting_touch_start_response = false;
      // If the event was sent to the main thread, assume the default gesture is
      // prevented until we see evidence otherwise.
      AnyThread().default_gesture_prevented =
          !AnyThread().last_gesture_was_compositor_driven;
      break;

    case blink::WebInputEvent::Undefined:
      break;

    default:
      AnyThread().awaiting_touch_start_response = false;
      break;
  }

  // Avoid unnecessary policy updates if the use case did not change.
  UseCase use_case = ComputeCurrentUseCase(now, &unused_policy_duration);

  if (use_case != previous_use_case ||
      was_awaiting_touch_start_response !=
          AnyThread().awaiting_touch_start_response) {
    EnsureUrgentPolicyUpdatePostedOnMainThread(FROM_HERE);
  }
  CompositorThreadOnly().last_input_type = type;
}

void RendererSchedulerImpl::DidHandleInputEventOnMainThread(
    const blink::WebInputEvent& web_input_event) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::DidHandleInputEventOnMainThread");
  helper_.CheckOnValidThread();
  if (ShouldPrioritizeInputEvent(web_input_event)) {
    base::AutoLock lock(any_thread_lock_);
    AnyThread().user_model.DidFinishProcessingInputEvent(
        helper_.scheduler_tqm_delegate()->NowTicks());
  }
}

bool RendererSchedulerImpl::IsHighPriorityWorkAnticipated() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // The touchstart, synchronized gesture and main-thread gesture use cases
  // indicate a strong likelihood of high-priority work in the near future.
  UseCase use_case = MainThreadOnly().current_use_case;
  return MainThreadOnly().touchstart_expected_soon ||
         use_case == UseCase::TOUCHSTART ||
         use_case == UseCase::MAIN_THREAD_GESTURE ||
         use_case == UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING ||
         use_case == UseCase::SYNCHRONIZED_GESTURE;
}

bool RendererSchedulerImpl::ShouldYieldForHighPriorityWork() {
  helper_.CheckOnValidThread();
  if (helper_.IsShutdown())
    return false;

  MaybeUpdatePolicy();
  // We only yield if there's a urgent task to be run now, or we are expecting
  // one soon (touch start).
  // Note: even though the control queue has the highest priority we don't yield
  // for it since these tasks are not user-provided work and they are only
  // intended to run before the next task, not interrupt the tasks.
  switch (MainThreadOnly().current_use_case) {
    case UseCase::COMPOSITOR_GESTURE:
    case UseCase::NONE:
      return MainThreadOnly().touchstart_expected_soon;

    case UseCase::MAIN_THREAD_GESTURE:
    case UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING:
    case UseCase::SYNCHRONIZED_GESTURE:
      return compositor_task_runner_->HasPendingImmediateWork() ||
             MainThreadOnly().touchstart_expected_soon;

    case UseCase::TOUCHSTART:
      return true;

    case UseCase::LOADING:
      return false;

    default:
      NOTREACHED();
      return false;
  }
}

base::TimeTicks RendererSchedulerImpl::CurrentIdleTaskDeadlineForTesting()
    const {
  return idle_helper_.CurrentIdleTaskDeadline();
}

void RendererSchedulerImpl::RunIdleTasksForTesting(
    const base::Closure& callback) {
  MainThreadOnly().in_idle_period_for_testing = true;
  IdleTaskRunner()->PostIdleTask(
      FROM_HERE, base::Bind(&RendererSchedulerImpl::EndIdlePeriodForTesting,
                            weak_factory_.GetWeakPtr(), callback));
  idle_helper_.EnableLongIdlePeriod();
}

void RendererSchedulerImpl::MaybeUpdatePolicy() {
  helper_.CheckOnValidThread();
  if (policy_may_need_update_.IsSet()) {
    UpdatePolicy();
  }
}

void RendererSchedulerImpl::EnsureUrgentPolicyUpdatePostedOnMainThread(
    const tracked_objects::Location& from_here) {
  // TODO(scheduler-dev): Check that this method isn't called from the main
  // thread.
  any_thread_lock_.AssertAcquired();
  if (!policy_may_need_update_.IsSet()) {
    policy_may_need_update_.SetWhileLocked(true);
    control_task_runner_->PostTask(from_here, update_policy_closure_);
  }
}

void RendererSchedulerImpl::UpdatePolicy() {
  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::ForceUpdatePolicy() {
  base::AutoLock lock(any_thread_lock_);
  UpdatePolicyLocked(UpdateType::FORCE_UPDATE);
}

void RendererSchedulerImpl::UpdatePolicyLocked(UpdateType update_type) {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();
  if (helper_.IsShutdown())
    return;

  base::TimeTicks now = helper_.scheduler_tqm_delegate()->NowTicks();
  policy_may_need_update_.SetWhileLocked(false);

  base::TimeDelta expected_use_case_duration;
  UseCase use_case = ComputeCurrentUseCase(now, &expected_use_case_duration);
  MainThreadOnly().current_use_case = use_case;

  base::TimeDelta touchstart_expected_flag_valid_for_duration;
  bool touchstart_expected_soon = false;
  if (MainThreadOnly().has_visible_render_widget_with_touch_handler) {
    touchstart_expected_soon = AnyThread().user_model.IsGestureExpectedSoon(
        now, &touchstart_expected_flag_valid_for_duration);
  }
  MainThreadOnly().touchstart_expected_soon = touchstart_expected_soon;

  base::TimeDelta longest_jank_free_task_duration =
      EstimateLongestJankFreeTaskDuration();
  MainThreadOnly().longest_jank_free_task_duration =
      longest_jank_free_task_duration;

  bool loading_tasks_seem_expensive = false;
  bool timer_tasks_seem_expensive = false;
  loading_tasks_seem_expensive =
      MainThreadOnly().loading_task_cost_estimator.expected_task_duration() >
      longest_jank_free_task_duration;
  timer_tasks_seem_expensive =
      MainThreadOnly().timer_task_cost_estimator.expected_task_duration() >
      longest_jank_free_task_duration;
  MainThreadOnly().timer_tasks_seem_expensive = timer_tasks_seem_expensive;
  MainThreadOnly().loading_tasks_seem_expensive = loading_tasks_seem_expensive;

  // The |new_policy_duration| is the minimum of |expected_use_case_duration|
  // and |touchstart_expected_flag_valid_for_duration| unless one is zero in
  // which case we choose the other.
  base::TimeDelta new_policy_duration = expected_use_case_duration;
  if (new_policy_duration.is_zero() ||
      (touchstart_expected_flag_valid_for_duration > base::TimeDelta() &&
       new_policy_duration > touchstart_expected_flag_valid_for_duration)) {
    new_policy_duration = touchstart_expected_flag_valid_for_duration;
  }

  // Do not throttle while audio is playing or for a short period after that
  // to make sure that pages playing short audio clips powered by timers
  // work.
  if (MainThreadOnly().last_audio_state_change &&
      !MainThreadOnly().is_audio_playing) {
    base::TimeTicks audio_will_expire =
        MainThreadOnly().last_audio_state_change.value() +
        kThrottlingDelayAfterAudioIsPlayed;

    base::TimeDelta audio_will_expire_after = audio_will_expire - now;

    if (audio_will_expire_after > base::TimeDelta()) {
      if (new_policy_duration.is_zero()) {
        new_policy_duration = audio_will_expire_after;
      } else {
        new_policy_duration =
            std::min(new_policy_duration, audio_will_expire_after);
      }
    }
  }

  if (new_policy_duration > base::TimeDelta()) {
    MainThreadOnly().current_policy_expiration_time = now + new_policy_duration;
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, new_policy_duration,
                                              now);
  } else {
    MainThreadOnly().current_policy_expiration_time = base::TimeTicks();
  }

  // Avoid prioritizing main thread compositing (e.g., rAF) if it is extremely
  // slow, because that can cause starvation in other task sources.
  bool main_thread_compositing_is_fast =
      MainThreadOnly().idle_time_estimator.GetExpectedIdleDuration(
          MainThreadOnly().compositor_frame_interval) >
      MainThreadOnly().compositor_frame_interval *
          kFastCompositingIdleTimeThreshold;

  Policy new_policy;
  ExpensiveTaskPolicy expensive_task_policy = ExpensiveTaskPolicy::RUN;
  new_policy.rail_mode = v8::PERFORMANCE_ANIMATION;

  switch (use_case) {
    case UseCase::COMPOSITOR_GESTURE:
      if (touchstart_expected_soon) {
        new_policy.rail_mode = v8::PERFORMANCE_RESPONSE;
        expensive_task_policy = ExpensiveTaskPolicy::BLOCK;
        new_policy.compositor_queue_policy.priority = TaskQueue::HIGH_PRIORITY;
      } else {
        // What we really want to do is priorize loading tasks, but that doesn't
        // seem to be safe. Instead we do that by proxy by deprioritizing
        // compositor tasks. This should be safe since we've already gone to the
        // pain of fixing ordering issues with them.
        new_policy.compositor_queue_policy.priority =
            TaskQueue::BEST_EFFORT_PRIORITY;
      }
      break;

    case UseCase::SYNCHRONIZED_GESTURE:
      new_policy.compositor_queue_policy.priority =
          main_thread_compositing_is_fast ? TaskQueue::HIGH_PRIORITY
                                          : TaskQueue::NORMAL_PRIORITY;
      if (touchstart_expected_soon) {
        new_policy.rail_mode = v8::PERFORMANCE_RESPONSE;
        expensive_task_policy = ExpensiveTaskPolicy::BLOCK;
      } else {
        expensive_task_policy = ExpensiveTaskPolicy::THROTTLE;
      }
      break;

    case UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING:
      // In main thread input handling scenarios we don't have perfect knowledge
      // about which things we should be prioritizing, so we don't attempt to
      // block expensive tasks because we don't know whether they were integral
      // to the page's functionality or not.
      new_policy.compositor_queue_policy.priority =
          main_thread_compositing_is_fast ? TaskQueue::HIGH_PRIORITY
                                          : TaskQueue::NORMAL_PRIORITY;
      break;

    case UseCase::MAIN_THREAD_GESTURE:
      // A main thread gesture is for example a scroll gesture which is handled
      // by the main thread. Since we know the established gesture type, we can
      // be a little more aggressive about prioritizing compositing and input
      // handling over other tasks.
      new_policy.compositor_queue_policy.priority = TaskQueue::HIGH_PRIORITY;
      if (touchstart_expected_soon) {
        new_policy.rail_mode = v8::PERFORMANCE_RESPONSE;
        expensive_task_policy = ExpensiveTaskPolicy::BLOCK;
      } else {
        expensive_task_policy = ExpensiveTaskPolicy::THROTTLE;
      }
      break;

    case UseCase::TOUCHSTART:
      new_policy.rail_mode = v8::PERFORMANCE_RESPONSE;
      new_policy.compositor_queue_policy.priority = TaskQueue::HIGH_PRIORITY;
      new_policy.loading_queue_policy.is_enabled = false;
      new_policy.timer_queue_policy.is_enabled = false;
      // NOTE this is a nop due to the above.
      expensive_task_policy = ExpensiveTaskPolicy::BLOCK;
      break;

    case UseCase::NONE:
      // It's only safe to block tasks that if we are expecting a compositor
      // driven gesture.
      if (touchstart_expected_soon &&
          AnyThread().last_gesture_was_compositor_driven) {
        new_policy.rail_mode = v8::PERFORMANCE_RESPONSE;
        expensive_task_policy = ExpensiveTaskPolicy::BLOCK;
      }
      break;

    case UseCase::LOADING:
      new_policy.rail_mode = v8::PERFORMANCE_LOAD;
      new_policy.loading_queue_policy.priority = TaskQueue::HIGH_PRIORITY;
      new_policy.default_queue_policy.priority = TaskQueue::HIGH_PRIORITY;
      break;

    default:
      NOTREACHED();
  }

  // TODO(skyostil): Add an idle state for foreground tabs too.
  if (MainThreadOnly().renderer_hidden)
    new_policy.rail_mode = v8::PERFORMANCE_IDLE;

  if (expensive_task_policy == ExpensiveTaskPolicy::BLOCK &&
      (!MainThreadOnly().have_seen_a_begin_main_frame ||
       MainThreadOnly().navigation_task_expected_count > 0)) {
    expensive_task_policy = ExpensiveTaskPolicy::RUN;
  }

  switch (expensive_task_policy) {
    case ExpensiveTaskPolicy::RUN:
      break;

    case ExpensiveTaskPolicy::BLOCK:
      if (loading_tasks_seem_expensive)
        new_policy.loading_queue_policy.is_enabled = false;
      if (timer_tasks_seem_expensive)
        new_policy.timer_queue_policy.is_enabled = false;
      break;

    case ExpensiveTaskPolicy::THROTTLE:
      if (loading_tasks_seem_expensive) {
        new_policy.loading_queue_policy.time_domain_type =
            TimeDomainType::THROTTLED;
      }
      if (timer_tasks_seem_expensive) {
        new_policy.timer_queue_policy.time_domain_type =
            TimeDomainType::THROTTLED;
      }
      break;
  }
  MainThreadOnly().expensive_task_policy = expensive_task_policy;

  if (MainThreadOnly().timer_queue_suspend_count != 0 ||
      MainThreadOnly().timer_queue_suspended_when_backgrounded) {
    new_policy.timer_queue_policy.is_enabled = false;
    // TODO(alexclarke): Figure out if we really need to do this.
    new_policy.timer_queue_policy.time_domain_type = TimeDomainType::REAL;
  }

  if (MainThreadOnly().renderer_suspended) {
    new_policy.loading_queue_policy.is_enabled = false;
    DCHECK(!new_policy.timer_queue_policy.is_enabled);
  }

  if (MainThreadOnly().use_virtual_time) {
    new_policy.compositor_queue_policy.time_domain_type =
        TimeDomainType::VIRTUAL;
    new_policy.default_queue_policy.time_domain_type = TimeDomainType::VIRTUAL;
    new_policy.loading_queue_policy.time_domain_type = TimeDomainType::VIRTUAL;
    new_policy.timer_queue_policy.time_domain_type = TimeDomainType::VIRTUAL;
  }

  new_policy.should_disable_throttling =
      ShouldDisableThrottlingBecauseOfAudio(now) ||
      MainThreadOnly().use_virtual_time;

  // Tracing is done before the early out check, because it's quite possible we
  // will otherwise miss this information in traces.
  CreateTraceEventObjectSnapshotLocked();
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "use_case",
                 use_case);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "rail_mode",
                 new_policy.rail_mode);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "touchstart_expected_soon",
                 MainThreadOnly().touchstart_expected_soon);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "expensive_task_policy", expensive_task_policy);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.loading_tasks_seem_expensive",
                 MainThreadOnly().loading_tasks_seem_expensive);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "RendererScheduler.timer_tasks_seem_expensive",
                 MainThreadOnly().timer_tasks_seem_expensive);

  // TODO(alexclarke): Can we get rid of force update now?
  if (update_type == UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED &&
      new_policy == MainThreadOnly().current_policy) {
    return;
  }

  ApplyTaskQueuePolicy(compositor_task_runner_.get(),
                       MainThreadOnly().current_policy.compositor_queue_policy,
                       new_policy.compositor_queue_policy);

  for (const scoped_refptr<TaskQueue>& loading_queue : loading_task_runners_) {
    ApplyTaskQueuePolicy(loading_queue.get(),
                         MainThreadOnly().current_policy.loading_queue_policy,
                         new_policy.loading_queue_policy);
  }

  for (const scoped_refptr<TaskQueue>& timer_queue : timer_task_runners_) {
    ApplyTaskQueuePolicy(timer_queue.get(),
                         MainThreadOnly().current_policy.timer_queue_policy,
                         new_policy.timer_queue_policy);
  }
  MainThreadOnly().have_reported_blocking_intervention_in_current_policy =
      false;

  // TODO(alexclarke): We shouldn't have to prioritize the default queue, but it
  // appears to be necessary since the order of loading tasks and IPCs (which
  // are mostly dispatched on the default queue) need to be preserved.
  ApplyTaskQueuePolicy(helper_.DefaultTaskRunner().get(),
                       MainThreadOnly().current_policy.default_queue_policy,
                       new_policy.default_queue_policy);
  if (MainThreadOnly().rail_mode_observer &&
      new_policy.rail_mode != MainThreadOnly().current_policy.rail_mode) {
    MainThreadOnly().rail_mode_observer->OnRAILModeChanged(
        new_policy.rail_mode);
  }

  if (new_policy.should_disable_throttling !=
      MainThreadOnly().current_policy.should_disable_throttling) {
    if (new_policy.should_disable_throttling) {
      task_queue_throttler()->DisableThrottling();
    } else {
      task_queue_throttler()->EnableThrottling();
    }
  }

  DCHECK(compositor_task_runner_->IsQueueEnabled());
  MainThreadOnly().current_policy = new_policy;
}

void RendererSchedulerImpl::ApplyTaskQueuePolicy(
    TaskQueue* task_queue,
    const TaskQueuePolicy& old_task_queue_policy,
    const TaskQueuePolicy& new_task_queue_policy) const {
  if (old_task_queue_policy.is_enabled != new_task_queue_policy.is_enabled) {
    task_queue->SetQueueEnabled(new_task_queue_policy.is_enabled);
  }

  if (old_task_queue_policy.priority != new_task_queue_policy.priority)
    task_queue->SetQueuePriority(new_task_queue_policy.priority);

  if (old_task_queue_policy.time_domain_type !=
      new_task_queue_policy.time_domain_type) {
    if (old_task_queue_policy.time_domain_type == TimeDomainType::THROTTLED) {
      task_queue_throttler_->DecreaseThrottleRefCount(task_queue);
    } else if (new_task_queue_policy.time_domain_type ==
               TimeDomainType::THROTTLED) {
      task_queue_throttler_->IncreaseThrottleRefCount(task_queue);
    } else if (new_task_queue_policy.time_domain_type ==
               TimeDomainType::VIRTUAL) {
      DCHECK(virtual_time_domain_);
      task_queue->SetTimeDomain(virtual_time_domain_.get());
    }
  }
}

RendererSchedulerImpl::UseCase RendererSchedulerImpl::ComputeCurrentUseCase(
    base::TimeTicks now,
    base::TimeDelta* expected_use_case_duration) const {
  any_thread_lock_.AssertAcquired();
  // Special case for flings. This is needed because we don't get notification
  // of a fling ending (although we do for cancellation).
  if (AnyThread().fling_compositor_escalation_deadline > now &&
      !AnyThread().awaiting_touch_start_response) {
    *expected_use_case_duration =
        AnyThread().fling_compositor_escalation_deadline - now;
    return UseCase::COMPOSITOR_GESTURE;
  }
  // Above all else we want to be responsive to user input.
  *expected_use_case_duration =
      AnyThread().user_model.TimeLeftInUserGesture(now);
  if (*expected_use_case_duration > base::TimeDelta()) {
    // Has a gesture been fully established?
    if (AnyThread().awaiting_touch_start_response) {
      // No, so arrange for compositor tasks to be run at the highest priority.
      return UseCase::TOUCHSTART;
    }

    // Yes a gesture has been established.  Based on how the gesture is handled
    // we need to choose between one of four use cases:
    // 1. COMPOSITOR_GESTURE where the gesture is processed only on the
    //    compositor thread.
    // 2. MAIN_THREAD_GESTURE where the gesture is processed only on the main
    //    thread.
    // 3. MAIN_THREAD_CUSTOM_INPUT_HANDLING where the main thread processes a
    //    stream of input events and has prevented a default gesture from being
    //    started.
    // 4. SYNCHRONIZED_GESTURE where the gesture is processed on both threads.
    if (AnyThread().last_gesture_was_compositor_driven) {
      if (AnyThread().begin_main_frame_on_critical_path) {
        return UseCase::SYNCHRONIZED_GESTURE;
      } else {
        return UseCase::COMPOSITOR_GESTURE;
      }
    }
    if (AnyThread().default_gesture_prevented) {
      return UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING;
    } else {
      return UseCase::MAIN_THREAD_GESTURE;
    }
  }

  // TODO(alexclarke): return UseCase::LOADING if signals suggest the system is
  // in the initial 1s of RAIL loading.
  return UseCase::NONE;
}

base::TimeDelta RendererSchedulerImpl::EstimateLongestJankFreeTaskDuration()
    const {
  switch (MainThreadOnly().current_use_case) {
    case UseCase::TOUCHSTART:
    case UseCase::COMPOSITOR_GESTURE:
    case UseCase::LOADING:
    case UseCase::NONE:
      return base::TimeDelta::FromMilliseconds(kRailsResponseTimeMillis);

    case UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING:
    case UseCase::MAIN_THREAD_GESTURE:
    case UseCase::SYNCHRONIZED_GESTURE:
      return MainThreadOnly().idle_time_estimator.GetExpectedIdleDuration(
          MainThreadOnly().compositor_frame_interval);

    default:
      NOTREACHED();
      return base::TimeDelta::FromMilliseconds(kRailsResponseTimeMillis);
  }
}

bool RendererSchedulerImpl::CanEnterLongIdlePeriod(
    base::TimeTicks now,
    base::TimeDelta* next_long_idle_period_delay_out) {
  helper_.CheckOnValidThread();

  MaybeUpdatePolicy();
  if (MainThreadOnly().current_use_case == UseCase::TOUCHSTART) {
    // Don't start a long idle task in touch start priority, try again when
    // the policy is scheduled to end.
    *next_long_idle_period_delay_out =
        std::max(base::TimeDelta(),
                 MainThreadOnly().current_policy_expiration_time - now);
    return false;
  }
  return true;
}

SchedulerHelper* RendererSchedulerImpl::GetSchedulerHelperForTesting() {
  return &helper_;
}

TaskCostEstimator*
RendererSchedulerImpl::GetLoadingTaskCostEstimatorForTesting() {
  return &MainThreadOnly().loading_task_cost_estimator;
}

TaskCostEstimator*
RendererSchedulerImpl::GetTimerTaskCostEstimatorForTesting() {
  return &MainThreadOnly().timer_task_cost_estimator;
}

IdleTimeEstimator* RendererSchedulerImpl::GetIdleTimeEstimatorForTesting() {
  return &MainThreadOnly().idle_time_estimator;
}

void RendererSchedulerImpl::SuspendTimerQueue() {
  MainThreadOnly().timer_queue_suspend_count++;
  ForceUpdatePolicy();
#ifndef NDEBUG
  DCHECK(!default_timer_task_runner_->IsQueueEnabled());
  for (const auto& runner : timer_task_runners_) {
    DCHECK(!runner->IsQueueEnabled());
  }
#endif
}

void RendererSchedulerImpl::ResumeTimerQueue() {
  MainThreadOnly().timer_queue_suspend_count--;
  DCHECK_GE(MainThreadOnly().timer_queue_suspend_count, 0);
  ForceUpdatePolicy();
}

void RendererSchedulerImpl::SetTimerQueueSuspensionWhenBackgroundedEnabled(
    bool enabled) {
  // Note that this will only take effect for the next backgrounded signal.
  MainThreadOnly().timer_queue_suspension_when_backgrounded_enabled = enabled;
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValue(base::TimeTicks optional_now) const {
  base::AutoLock lock(any_thread_lock_);
  return AsValueLocked(optional_now);
}

void RendererSchedulerImpl::CreateTraceEventObjectSnapshot() const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValue(helper_.scheduler_tqm_delegate()->NowTicks()));
}

void RendererSchedulerImpl::CreateTraceEventObjectSnapshotLocked() const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"), "RendererScheduler",
      this, AsValueLocked(helper_.scheduler_tqm_delegate()->NowTicks()));
}

// static
const char* RendererSchedulerImpl::ExpensiveTaskPolicyToString(
    ExpensiveTaskPolicy expensive_task_policy) {
  switch (expensive_task_policy) {
    case ExpensiveTaskPolicy::RUN:
      return "RUN";
    case ExpensiveTaskPolicy::BLOCK:
      return "BLOCK";
    case ExpensiveTaskPolicy::THROTTLE:
      return "THROTTLE";
    default:
      NOTREACHED();
      return nullptr;
  }
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
RendererSchedulerImpl::AsValueLocked(base::TimeTicks optional_now) const {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();

  if (optional_now.is_null())
    optional_now = helper_.scheduler_tqm_delegate()->NowTicks();
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  state->SetBoolean(
      "has_visible_render_widget_with_touch_handler",
      MainThreadOnly().has_visible_render_widget_with_touch_handler);
  state->SetString("current_use_case",
                   UseCaseToString(MainThreadOnly().current_use_case));
  state->SetString("rail_mode",
                   RAILModeToString(MainThreadOnly().current_policy.rail_mode));
  state->SetBoolean("loading_tasks_seem_expensive",
                    MainThreadOnly().loading_tasks_seem_expensive);
  state->SetBoolean("timer_tasks_seem_expensive",
                    MainThreadOnly().timer_tasks_seem_expensive);
  state->SetBoolean("begin_frame_not_expected_soon",
                    MainThreadOnly().begin_frame_not_expected_soon);
  state->SetBoolean("touchstart_expected_soon",
                    MainThreadOnly().touchstart_expected_soon);
  state->SetString("idle_period_state",
                   IdleHelper::IdlePeriodStateToString(
                       idle_helper_.SchedulerIdlePeriodState()));
  state->SetBoolean("renderer_hidden", MainThreadOnly().renderer_hidden);
  state->SetBoolean("have_seen_a_begin_main_frame",
                    MainThreadOnly().have_seen_a_begin_main_frame);
  state->SetBoolean(
      "have_reported_blocking_intervention_in_current_policy",
      MainThreadOnly().have_reported_blocking_intervention_in_current_policy);
  state->SetBoolean(
      "have_reported_blocking_intervention_since_navigation",
      MainThreadOnly().have_reported_blocking_intervention_since_navigation);
  state->SetBoolean("renderer_backgrounded",
                    MainThreadOnly().renderer_backgrounded);
  state->SetBoolean("timer_queue_suspended_when_backgrounded",
                    MainThreadOnly().timer_queue_suspended_when_backgrounded);
  state->SetInteger("timer_queue_suspend_count",
                    MainThreadOnly().timer_queue_suspend_count);
  state->SetDouble("now", (optional_now - base::TimeTicks()).InMillisecondsF());
  state->SetDouble(
      "rails_loading_priority_deadline",
      (AnyThread().rails_loading_priority_deadline - base::TimeTicks())
          .InMillisecondsF());
  state->SetDouble(
      "fling_compositor_escalation_deadline",
      (AnyThread().fling_compositor_escalation_deadline - base::TimeTicks())
          .InMillisecondsF());
  state->SetInteger("navigation_task_expected_count",
                    MainThreadOnly().navigation_task_expected_count);
  state->SetDouble("last_idle_period_end_time",
                   (AnyThread().last_idle_period_end_time - base::TimeTicks())
                       .InMillisecondsF());
  state->SetBoolean("awaiting_touch_start_response",
                    AnyThread().awaiting_touch_start_response);
  state->SetBoolean("begin_main_frame_on_critical_path",
                    AnyThread().begin_main_frame_on_critical_path);
  state->SetBoolean("last_gesture_was_compositor_driven",
                    AnyThread().last_gesture_was_compositor_driven);
  state->SetBoolean("default_gesture_prevented",
                    AnyThread().default_gesture_prevented);
  state->SetDouble("expected_loading_task_duration",
                   MainThreadOnly()
                       .loading_task_cost_estimator.expected_task_duration()
                       .InMillisecondsF());
  state->SetDouble("expected_timer_task_duration",
                   MainThreadOnly()
                       .timer_task_cost_estimator.expected_task_duration()
                       .InMillisecondsF());
  state->SetBoolean("is_audio_playing", MainThreadOnly().is_audio_playing);

  // TODO(skyostil): Can we somehow trace how accurate these estimates were?
  state->SetDouble(
      "longest_jank_free_task_duration",
      MainThreadOnly().longest_jank_free_task_duration.InMillisecondsF());
  state->SetDouble(
      "compositor_frame_interval",
      MainThreadOnly().compositor_frame_interval.InMillisecondsF());
  state->SetDouble(
      "estimated_next_frame_begin",
      (MainThreadOnly().estimated_next_frame_begin - base::TimeTicks())
          .InMillisecondsF());
  state->SetBoolean("in_idle_period", AnyThread().in_idle_period);

  state->SetString(
      "expensive_task_policy",
      ExpensiveTaskPolicyToString(MainThreadOnly().expensive_task_policy));

  AnyThread().user_model.AsValueInto(state.get());
  render_widget_scheduler_signals_.AsValueInto(state.get());

  state->BeginDictionary("task_queue_throttler");
  task_queue_throttler_->AsValueInto(state.get(), optional_now);
  state->EndDictionary();

  return std::move(state);
}

void RendererSchedulerImpl::OnIdlePeriodStarted() {
  base::AutoLock lock(any_thread_lock_);
  AnyThread().in_idle_period = true;
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::OnIdlePeriodEnded() {
  base::AutoLock lock(any_thread_lock_);
  AnyThread().last_idle_period_end_time =
      helper_.scheduler_tqm_delegate()->NowTicks();
  AnyThread().in_idle_period = false;
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::AddPendingNavigation(
    WebScheduler::NavigatingFrameType type) {
  helper_.CheckOnValidThread();
  if (type == blink::WebScheduler::NavigatingFrameType::kMainFrame) {
    MainThreadOnly().navigation_task_expected_count++;
    UpdatePolicy();
  }
}

void RendererSchedulerImpl::RemovePendingNavigation(
    WebScheduler::NavigatingFrameType type) {
  helper_.CheckOnValidThread();
  DCHECK_GT(MainThreadOnly().navigation_task_expected_count, 0);
  if (type == blink::WebScheduler::NavigatingFrameType::kMainFrame &&
      MainThreadOnly().navigation_task_expected_count > 0) {
    MainThreadOnly().navigation_task_expected_count--;
    UpdatePolicy();
  }
}

void RendererSchedulerImpl::OnNavigationStarted() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "RendererSchedulerImpl::OnNavigationStarted");
  base::AutoLock lock(any_thread_lock_);
  AnyThread().rails_loading_priority_deadline =
      helper_.scheduler_tqm_delegate()->NowTicks() +
      base::TimeDelta::FromMilliseconds(
          kRailsInitialLoadingPrioritizationMillis);
  ResetForNavigationLocked();
}

void RendererSchedulerImpl::SuspendTimerQueueWhenBackgrounded() {
  DCHECK(MainThreadOnly().renderer_backgrounded);
  if (MainThreadOnly().timer_queue_suspended_when_backgrounded)
    return;

  MainThreadOnly().timer_queue_suspended_when_backgrounded = true;
  ForceUpdatePolicy();
}

void RendererSchedulerImpl::ResumeTimerQueueWhenForegroundedOrResumed() {
  DCHECK(!MainThreadOnly().renderer_backgrounded ||
         (MainThreadOnly().renderer_backgrounded &&
          !MainThreadOnly().renderer_suspended));
  if (!MainThreadOnly().timer_queue_suspended_when_backgrounded)
    return;

  MainThreadOnly().timer_queue_suspended_when_backgrounded = false;
  ForceUpdatePolicy();
}

void RendererSchedulerImpl::ResetForNavigationLocked() {
  helper_.CheckOnValidThread();
  any_thread_lock_.AssertAcquired();
  AnyThread().user_model.Reset(helper_.scheduler_tqm_delegate()->NowTicks());
  AnyThread().have_seen_touchstart = false;
  MainThreadOnly().loading_task_cost_estimator.Clear();
  MainThreadOnly().timer_task_cost_estimator.Clear();
  MainThreadOnly().idle_time_estimator.Clear();
  MainThreadOnly().have_seen_a_begin_main_frame = false;
  MainThreadOnly().have_reported_blocking_intervention_since_navigation = false;
  for (WebViewSchedulerImpl* web_view_scheduler :
       MainThreadOnly().web_view_schedulers) {
    web_view_scheduler->OnNavigation();
  }
  UpdatePolicyLocked(UpdateType::MAY_EARLY_OUT_IF_POLICY_UNCHANGED);
}

void RendererSchedulerImpl::SetTopLevelBlameContext(
    base::trace_event::BlameContext* blame_context) {
  // Any task that runs in the default task runners belongs to the context of
  // all frames (as opposed to a particular frame). Note that the task itself
  // may still enter a more specific blame context if necessary.
  //
  // Per-frame task runners (loading, timers, etc.) are configured with a more
  // specific blame context by WebFrameSchedulerImpl.
  control_task_runner_->SetBlameContext(blame_context);
  DefaultTaskRunner()->SetBlameContext(blame_context);
  default_loading_task_runner_->SetBlameContext(blame_context);
  default_timer_task_runner_->SetBlameContext(blame_context);
  compositor_task_runner_->SetBlameContext(blame_context);
  idle_helper_.IdleTaskRunner()->SetBlameContext(blame_context);
}

void RendererSchedulerImpl::SetRAILModeObserver(RAILModeObserver* observer) {
  MainThreadOnly().rail_mode_observer = observer;
}

void RendererSchedulerImpl::RegisterTimeDomain(TimeDomain* time_domain) {
  helper_.RegisterTimeDomain(time_domain);
}

void RendererSchedulerImpl::UnregisterTimeDomain(TimeDomain* time_domain) {
  helper_.UnregisterTimeDomain(time_domain);
}

base::TickClock* RendererSchedulerImpl::tick_clock() const {
  return helper_.scheduler_tqm_delegate().get();
}

void RendererSchedulerImpl::AddWebViewScheduler(
    WebViewSchedulerImpl* web_view_scheduler) {
  MainThreadOnly().web_view_schedulers.insert(web_view_scheduler);
}

void RendererSchedulerImpl::RemoveWebViewScheduler(
    WebViewSchedulerImpl* web_view_scheduler) {
  DCHECK(MainThreadOnly().web_view_schedulers.find(web_view_scheduler) !=
         MainThreadOnly().web_view_schedulers.end());
  MainThreadOnly().web_view_schedulers.erase(web_view_scheduler);
}

void RendererSchedulerImpl::BroadcastIntervention(const std::string& message) {
  helper_.CheckOnValidThread();
  for (auto* web_view_scheduler : MainThreadOnly().web_view_schedulers)
    web_view_scheduler->ReportIntervention(message);
}

void RendererSchedulerImpl::OnTriedToExecuteBlockedTask(
    const TaskQueue& queue,
    const base::PendingTask& task) {
  if (MainThreadOnly().current_use_case == UseCase::TOUCHSTART ||
      MainThreadOnly().longest_jank_free_task_duration <
          base::TimeDelta::FromMilliseconds(kRailsResponseTimeMillis) ||
      MainThreadOnly().timer_queue_suspend_count ||
      MainThreadOnly().timer_queue_suspended_when_backgrounded) {
    return;
  }
  if (!MainThreadOnly().timer_tasks_seem_expensive &&
      !MainThreadOnly().loading_tasks_seem_expensive) {
    return;
  }
  if (!MainThreadOnly().have_reported_blocking_intervention_in_current_policy) {
    MainThreadOnly().have_reported_blocking_intervention_in_current_policy =
        true;
    TRACE_EVENT_INSTANT0("renderer.scheduler",
                         "RendererSchedulerImpl::TaskBlocked",
                         TRACE_EVENT_SCOPE_THREAD);
  }

  if (!MainThreadOnly().have_reported_blocking_intervention_since_navigation) {
    {
      base::AutoLock lock(any_thread_lock_);
      if (!AnyThread().have_seen_touchstart)
        return;
    }
    MainThreadOnly().have_reported_blocking_intervention_since_navigation =
        true;
    BroadcastIntervention(
        "Blink deferred a task in order to make scrolling smoother. "
        "Your timer and network tasks should take less than 50ms to run "
        "to avoid this. Please see "
        "https://developers.google.com/web/tools/chrome-devtools/profile/evaluate-performance/rail"
        " and https://crbug.com/574343#c40 for more information.");
  }
}

void RendererSchedulerImpl::ReportTaskTime(TaskQueue* task_queue,
                                           double start_time,
                                           double end_time) {
  // TODO(scheduler-dev): Remove conversions when Blink starts using
  // base::TimeTicks instead of doubles for time.
  base::TimeTicks start_time_ticks =
      MonotonicTimeInSecondsToTimeTicks(start_time);
  base::TimeTicks end_time_ticks = MonotonicTimeInSecondsToTimeTicks(end_time);

  MainThreadOnly().queueing_time_estimator.OnToplevelTaskCompleted(
      start_time_ticks, end_time_ticks);

  task_queue_throttler()->OnTaskRunTimeReported(task_queue, start_time_ticks,
                                                end_time_ticks);

  // We want to measure thread time here, but for efficiency reasons
  // we stick with wall time.
  MainThreadOnly().foreground_main_thread_load_tracker.RecordTaskTime(
      start_time_ticks, end_time_ticks);
  MainThreadOnly().background_main_thread_load_tracker.RecordTaskTime(
      start_time_ticks, end_time_ticks);
  // TODO(altimin): Per-page metrics should also be considered.
  UMA_HISTOGRAM_CUSTOM_COUNTS("RendererScheduler.TaskTime",
                              (end_time_ticks - start_time_ticks).InMicroseconds(), 1,
                              1000000, 50);
  UMA_HISTOGRAM_ENUMERATION("RendererScheduler.NumberOfTasksPerQueueType",
                            static_cast<int>(task_queue->GetQueueType()),
                            static_cast<int>(TaskQueue::QueueType::COUNT));
}

void RendererSchedulerImpl::AddTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  helper_.AddTaskTimeObserver(task_time_observer);
}

void RendererSchedulerImpl::RemoveTaskTimeObserver(
    TaskTimeObserver* task_time_observer) {
  helper_.RemoveTaskTimeObserver(task_time_observer);
}

void RendererSchedulerImpl::OnQueueingTimeForWindowEstimated(
    base::TimeDelta queueing_time) {
  UMA_HISTOGRAM_TIMES("RendererScheduler.ExpectedTaskQueueingDuration",
                      queueing_time);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "estimated_queueing_time_for_window",
                 queueing_time.InMillisecondsF());
}

AutoAdvancingVirtualTimeDomain* RendererSchedulerImpl::GetVirtualTimeDomain() {
  if (!virtual_time_domain_) {
    virtual_time_domain_.reset(
        new AutoAdvancingVirtualTimeDomain(tick_clock()->NowTicks()));
    RegisterTimeDomain(virtual_time_domain_.get());
  }
  return virtual_time_domain_.get();
}

void RendererSchedulerImpl::EnableVirtualTime() {
  MainThreadOnly().use_virtual_time = true;

  // The |unthrottled_task_runners_| are not actively managed by UpdatePolicy().
  AutoAdvancingVirtualTimeDomain* time_domain = GetVirtualTimeDomain();
  for (const scoped_refptr<TaskQueue>& task_queue : unthrottled_task_runners_)
    task_queue->SetTimeDomain(time_domain);

  ForceUpdatePolicy();
}

bool RendererSchedulerImpl::ShouldDisableThrottlingBecauseOfAudio(
    base::TimeTicks now) {
  if (!MainThreadOnly().last_audio_state_change)
    return false;

  if (MainThreadOnly().is_audio_playing)
    return true;

  return MainThreadOnly().last_audio_state_change.value() +
             kThrottlingDelayAfterAudioIsPlayed >
         now;
}

TimeDomain* RendererSchedulerImpl::GetActiveTimeDomain() {
  if (MainThreadOnly().use_virtual_time) {
    return GetVirtualTimeDomain();
  } else {
    return real_time_domain();
  }
}

// static
const char* RendererSchedulerImpl::UseCaseToString(UseCase use_case) {
  switch (use_case) {
    case UseCase::NONE:
      return "none";
    case UseCase::COMPOSITOR_GESTURE:
      return "compositor_gesture";
    case UseCase::MAIN_THREAD_CUSTOM_INPUT_HANDLING:
      return "main_thread_custom_input_handling";
    case UseCase::SYNCHRONIZED_GESTURE:
      return "synchronized_gesture";
    case UseCase::TOUCHSTART:
      return "touchstart";
    case UseCase::LOADING:
      return "loading";
    case UseCase::MAIN_THREAD_GESTURE:
      return "main_thread_gesture";
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
const char* RendererSchedulerImpl::RAILModeToString(v8::RAILMode rail_mode) {
  switch (rail_mode) {
    case v8::PERFORMANCE_RESPONSE:
      return "response";
    case v8::PERFORMANCE_ANIMATION:
      return "animation";
    case v8::PERFORMANCE_IDLE:
      return "idle";
    case v8::PERFORMANCE_LOAD:
      return "load";
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace scheduler
}  // namespace blink
