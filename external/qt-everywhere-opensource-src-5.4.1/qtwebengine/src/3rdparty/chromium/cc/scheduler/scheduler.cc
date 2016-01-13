// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include <algorithm>
#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "ui/gfx/frame_time.h"

namespace cc {

Scheduler::SyntheticBeginFrameSource::SyntheticBeginFrameSource(
    Scheduler* scheduler,
    base::SingleThreadTaskRunner* task_runner)
    : scheduler_(scheduler) {
  if (gfx::FrameTime::TimestampsAreHighRes()) {
    time_source_ = DelayBasedTimeSourceHighRes::Create(
        scheduler_->VSyncInterval(), task_runner);
  } else {
    time_source_ = DelayBasedTimeSource::Create(scheduler_->VSyncInterval(),
                                                task_runner);
  }
  time_source_->SetClient(this);
}

Scheduler::SyntheticBeginFrameSource::~SyntheticBeginFrameSource() {
}

void Scheduler::SyntheticBeginFrameSource::CommitVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  time_source_->SetTimebaseAndInterval(timebase, interval);
}

void Scheduler::SyntheticBeginFrameSource::SetNeedsBeginFrame(
    bool needs_begin_frame,
    std::deque<BeginFrameArgs>* begin_retro_frame_args) {
  DCHECK(begin_retro_frame_args);
  base::TimeTicks missed_tick_time =
      time_source_->SetActive(needs_begin_frame);
  if (!missed_tick_time.is_null()) {
    begin_retro_frame_args->push_back(
        CreateSyntheticBeginFrameArgs(missed_tick_time));
  }
}

bool Scheduler::SyntheticBeginFrameSource::IsActive() const {
  return time_source_->Active();
}

void Scheduler::SyntheticBeginFrameSource::OnTimerTick() {
  BeginFrameArgs begin_frame_args(
      CreateSyntheticBeginFrameArgs(time_source_->LastTickTime()));
  scheduler_->BeginFrame(begin_frame_args);
}

scoped_ptr<base::Value> Scheduler::SyntheticBeginFrameSource::AsValue() const {
  return time_source_->AsValue();
}

BeginFrameArgs
Scheduler::SyntheticBeginFrameSource::CreateSyntheticBeginFrameArgs(
    base::TimeTicks frame_time) {
  base::TimeTicks deadline = time_source_->NextTickTime();
  return BeginFrameArgs::Create(
      frame_time, deadline, scheduler_->VSyncInterval());
}

Scheduler::Scheduler(
    SchedulerClient* client,
    const SchedulerSettings& scheduler_settings,
    int layer_tree_host_id,
    const scoped_refptr<base::SingleThreadTaskRunner>& impl_task_runner)
    : settings_(scheduler_settings),
      client_(client),
      layer_tree_host_id_(layer_tree_host_id),
      impl_task_runner_(impl_task_runner),
      vsync_interval_(BeginFrameArgs::DefaultInterval()),
      last_set_needs_begin_frame_(false),
      begin_unthrottled_frame_posted_(false),
      begin_retro_frame_posted_(false),
      state_machine_(scheduler_settings),
      inside_process_scheduled_actions_(false),
      inside_action_(SchedulerStateMachine::ACTION_NONE),
      weak_factory_(this) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
               "Scheduler::Scheduler",
               "settings",
               ToTrace(settings_));
  DCHECK(client_);
  DCHECK(!state_machine_.BeginFrameNeeded());
  if (settings_.main_frame_before_activation_enabled) {
    DCHECK(settings_.main_frame_before_draw_enabled);
  }

  begin_retro_frame_closure_ =
      base::Bind(&Scheduler::BeginRetroFrame, weak_factory_.GetWeakPtr());
  begin_unthrottled_frame_closure_ =
      base::Bind(&Scheduler::BeginUnthrottledFrame, weak_factory_.GetWeakPtr());
  begin_impl_frame_deadline_closure_ = base::Bind(
      &Scheduler::OnBeginImplFrameDeadline, weak_factory_.GetWeakPtr());
  poll_for_draw_triggers_closure_ = base::Bind(
      &Scheduler::PollForAnticipatedDrawTriggers, weak_factory_.GetWeakPtr());
  advance_commit_state_closure_ = base::Bind(
      &Scheduler::PollToAdvanceCommitState, weak_factory_.GetWeakPtr());

  if (!settings_.begin_frame_scheduling_enabled) {
    SetupSyntheticBeginFrames();
  }
}

Scheduler::~Scheduler() {
  if (synthetic_begin_frame_source_) {
    synthetic_begin_frame_source_->SetNeedsBeginFrame(false,
                                                      &begin_retro_frame_args_);
  }
}

void Scheduler::SetupSyntheticBeginFrames() {
  DCHECK(!synthetic_begin_frame_source_);
  synthetic_begin_frame_source_.reset(
      new SyntheticBeginFrameSource(this, impl_task_runner_.get()));
}

void Scheduler::CommitVSyncParameters(base::TimeTicks timebase,
                                      base::TimeDelta interval) {
  // TODO(brianderson): We should not be receiving 0 intervals.
  if (interval == base::TimeDelta())
    interval = BeginFrameArgs::DefaultInterval();
  vsync_interval_ = interval;
  if (!settings_.begin_frame_scheduling_enabled)
    synthetic_begin_frame_source_->CommitVSyncParameters(timebase, interval);
}

void Scheduler::SetEstimatedParentDrawTime(base::TimeDelta draw_time) {
  DCHECK_GE(draw_time.ToInternalValue(), 0);
  estimated_parent_draw_time_ = draw_time;
}

void Scheduler::SetCanStart() {
  state_machine_.SetCanStart();
  ProcessScheduledActions();
}

void Scheduler::SetVisible(bool visible) {
  state_machine_.SetVisible(visible);
  ProcessScheduledActions();
}

void Scheduler::SetCanDraw(bool can_draw) {
  state_machine_.SetCanDraw(can_draw);
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToActivate() {
  state_machine_.NotifyReadyToActivate();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsCommit() {
  state_machine_.SetNeedsCommit();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsRedraw() {
  state_machine_.SetNeedsRedraw();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsAnimate() {
  state_machine_.SetNeedsAnimate();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsManageTiles() {
  DCHECK(!IsInsideAction(SchedulerStateMachine::ACTION_MANAGE_TILES));
  state_machine_.SetNeedsManageTiles();
  ProcessScheduledActions();
}

void Scheduler::SetMaxSwapsPending(int max) {
  state_machine_.SetMaxSwapsPending(max);
}

void Scheduler::DidSwapBuffers() {
  state_machine_.DidSwapBuffers();

  // There is no need to call ProcessScheduledActions here because
  // swapping should not trigger any new actions.
  if (!inside_process_scheduled_actions_) {
    DCHECK_EQ(state_machine_.NextAction(), SchedulerStateMachine::ACTION_NONE);
  }
}

void Scheduler::SetSwapUsedIncompleteTile(bool used_incomplete_tile) {
  state_machine_.SetSwapUsedIncompleteTile(used_incomplete_tile);
  ProcessScheduledActions();
}

void Scheduler::DidSwapBuffersComplete() {
  state_machine_.DidSwapBuffersComplete();
  ProcessScheduledActions();
}

void Scheduler::SetSmoothnessTakesPriority(bool smoothness_takes_priority) {
  state_machine_.SetSmoothnessTakesPriority(smoothness_takes_priority);
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToCommit() {
  TRACE_EVENT0("cc", "Scheduler::NotifyReadyToCommit");
  state_machine_.NotifyReadyToCommit();
  ProcessScheduledActions();
}

void Scheduler::BeginMainFrameAborted(bool did_handle) {
  TRACE_EVENT0("cc", "Scheduler::BeginMainFrameAborted");
  state_machine_.BeginMainFrameAborted(did_handle);
  ProcessScheduledActions();
}

void Scheduler::DidManageTiles() {
  state_machine_.DidManageTiles();
}

void Scheduler::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::DidLoseOutputSurface");
  state_machine_.DidLoseOutputSurface();
  last_set_needs_begin_frame_ = false;
  if (!settings_.begin_frame_scheduling_enabled) {
    synthetic_begin_frame_source_->SetNeedsBeginFrame(false,
                                                      &begin_retro_frame_args_);
  }
  begin_retro_frame_args_.clear();
  ProcessScheduledActions();
}

void Scheduler::DidCreateAndInitializeOutputSurface() {
  TRACE_EVENT0("cc", "Scheduler::DidCreateAndInitializeOutputSurface");
  DCHECK(!last_set_needs_begin_frame_);
  DCHECK(begin_impl_frame_deadline_task_.IsCancelled());
  state_machine_.DidCreateAndInitializeOutputSurface();
  ProcessScheduledActions();
}

void Scheduler::NotifyBeginMainFrameStarted() {
  TRACE_EVENT0("cc", "Scheduler::NotifyBeginMainFrameStarted");
  state_machine_.NotifyBeginMainFrameStarted();
}

base::TimeTicks Scheduler::AnticipatedDrawTime() const {
  if (!last_set_needs_begin_frame_ ||
      begin_impl_frame_args_.interval <= base::TimeDelta())
    return base::TimeTicks();

  base::TimeTicks now = gfx::FrameTime::Now();
  base::TimeTicks timebase = std::max(begin_impl_frame_args_.frame_time,
                                      begin_impl_frame_args_.deadline);
  int64 intervals = 1 + ((now - timebase) / begin_impl_frame_args_.interval);
  return timebase + (begin_impl_frame_args_.interval * intervals);
}

base::TimeTicks Scheduler::LastBeginImplFrameTime() {
  return begin_impl_frame_args_.frame_time;
}

void Scheduler::SetupNextBeginFrameIfNeeded() {
  bool needs_begin_frame = state_machine_.BeginFrameNeeded();

  if (settings_.throttle_frame_production) {
    SetupNextBeginFrameWhenVSyncThrottlingEnabled(needs_begin_frame);
  } else {
    SetupNextBeginFrameWhenVSyncThrottlingDisabled(needs_begin_frame);
  }
  SetupPollingMechanisms(needs_begin_frame);
}

// When we are throttling frame production, we request BeginFrames
// from the OutputSurface.
void Scheduler::SetupNextBeginFrameWhenVSyncThrottlingEnabled(
    bool needs_begin_frame) {
  bool at_end_of_deadline =
      state_machine_.begin_impl_frame_state() ==
          SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE;

  bool should_call_set_needs_begin_frame =
      // Always request the BeginFrame immediately if it wasn't needed before.
      (needs_begin_frame && !last_set_needs_begin_frame_) ||
      // Only stop requesting BeginFrames after a deadline.
      (!needs_begin_frame && last_set_needs_begin_frame_ && at_end_of_deadline);

  if (should_call_set_needs_begin_frame) {
    if (settings_.begin_frame_scheduling_enabled) {
      client_->SetNeedsBeginFrame(needs_begin_frame);
    } else {
      synthetic_begin_frame_source_->SetNeedsBeginFrame(
          needs_begin_frame, &begin_retro_frame_args_);
    }
    last_set_needs_begin_frame_ = needs_begin_frame;
  }

  PostBeginRetroFrameIfNeeded();
}

// When we aren't throttling frame production, we initiate a BeginFrame
// as soon as one is needed.
void Scheduler::SetupNextBeginFrameWhenVSyncThrottlingDisabled(
    bool needs_begin_frame) {
  last_set_needs_begin_frame_ = needs_begin_frame;

  if (!needs_begin_frame || begin_unthrottled_frame_posted_)
    return;

  if (state_machine_.begin_impl_frame_state() !=
          SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE &&
      state_machine_.begin_impl_frame_state() !=
          SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE) {
    return;
  }

  begin_unthrottled_frame_posted_ = true;
  impl_task_runner_->PostTask(FROM_HERE, begin_unthrottled_frame_closure_);
}

// BeginUnthrottledFrame is used when we aren't throttling frame production.
// This will usually be because VSync is disabled.
void Scheduler::BeginUnthrottledFrame() {
  DCHECK(!settings_.throttle_frame_production);
  DCHECK(begin_retro_frame_args_.empty());

  base::TimeTicks now = gfx::FrameTime::Now();
  base::TimeTicks deadline = now + vsync_interval_;

  BeginFrameArgs begin_frame_args =
      BeginFrameArgs::Create(now, deadline, vsync_interval_);
  BeginImplFrame(begin_frame_args);

  begin_unthrottled_frame_posted_ = false;
}

// We may need to poll when we can't rely on BeginFrame to advance certain
// state or to avoid deadlock.
void Scheduler::SetupPollingMechanisms(bool needs_begin_frame) {
  bool needs_advance_commit_state_timer = false;
  // Setup PollForAnticipatedDrawTriggers if we need to monitor state but
  // aren't expecting any more BeginFrames. This should only be needed by
  // the synchronous compositor when BeginFrameNeeded is false.
  if (state_machine_.ShouldPollForAnticipatedDrawTriggers()) {
    DCHECK(!state_machine_.SupportsProactiveBeginFrame());
    DCHECK(!needs_begin_frame);
    if (poll_for_draw_triggers_task_.IsCancelled()) {
      poll_for_draw_triggers_task_.Reset(poll_for_draw_triggers_closure_);
      base::TimeDelta delay = begin_impl_frame_args_.IsValid()
                                  ? begin_impl_frame_args_.interval
                                  : BeginFrameArgs::DefaultInterval();
      impl_task_runner_->PostDelayedTask(
          FROM_HERE, poll_for_draw_triggers_task_.callback(), delay);
    }
  } else {
    poll_for_draw_triggers_task_.Cancel();

    // At this point we'd prefer to advance through the commit flow by
    // drawing a frame, however it's possible that the frame rate controller
    // will not give us a BeginFrame until the commit completes.  See
    // crbug.com/317430 for an example of a swap ack being held on commit. Thus
    // we set a repeating timer to poll on ProcessScheduledActions until we
    // successfully reach BeginFrame. Synchronous compositor does not use
    // frame rate controller or have the circular wait in the bug.
    if (IsBeginMainFrameSentOrStarted() &&
        !settings_.using_synchronous_renderer_compositor) {
      needs_advance_commit_state_timer = true;
    }
  }

  if (needs_advance_commit_state_timer) {
    if (advance_commit_state_task_.IsCancelled() &&
        begin_impl_frame_args_.IsValid()) {
      // Since we'd rather get a BeginImplFrame by the normal mechanism, we
      // set the interval to twice the interval from the previous frame.
      advance_commit_state_task_.Reset(advance_commit_state_closure_);
      impl_task_runner_->PostDelayedTask(FROM_HERE,
                                         advance_commit_state_task_.callback(),
                                         begin_impl_frame_args_.interval * 2);
    }
  } else {
    advance_commit_state_task_.Cancel();
  }
}

// BeginFrame is the mechanism that tells us that now is a good time to start
// making a frame. Usually this means that user input for the frame is complete.
// If the scheduler is busy, we queue the BeginFrame to be handled later as
// a BeginRetroFrame.
void Scheduler::BeginFrame(const BeginFrameArgs& args) {
  TRACE_EVENT1("cc", "Scheduler::BeginFrame", "args", ToTrace(args));
  DCHECK(settings_.throttle_frame_production);

  BeginFrameArgs adjusted_args(args);
  adjusted_args.deadline -= EstimatedParentDrawTime();

  bool should_defer_begin_frame;
  if (settings_.using_synchronous_renderer_compositor) {
    should_defer_begin_frame = false;
  } else {
    should_defer_begin_frame =
        !begin_retro_frame_args_.empty() || begin_retro_frame_posted_ ||
        !last_set_needs_begin_frame_ ||
        (state_machine_.begin_impl_frame_state() !=
         SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  }

  if (should_defer_begin_frame) {
    begin_retro_frame_args_.push_back(adjusted_args);
    TRACE_EVENT_INSTANT0(
        "cc", "Scheduler::BeginFrame deferred", TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  BeginImplFrame(adjusted_args);
}

// BeginRetroFrame is called for BeginFrames that we've deferred because
// the scheduler was in the middle of processing a previous BeginFrame.
void Scheduler::BeginRetroFrame() {
  TRACE_EVENT0("cc", "Scheduler::BeginRetroFrame");
  DCHECK(!settings_.using_synchronous_renderer_compositor);
  DCHECK(begin_retro_frame_posted_);
  begin_retro_frame_posted_ = false;

  // If there aren't any retroactive BeginFrames, then we've lost the
  // OutputSurface and should abort.
  if (begin_retro_frame_args_.empty())
    return;

  // Discard expired BeginRetroFrames
  // Today, we should always end up with at most one un-expired BeginRetroFrame
  // because deadlines will not be greater than the next frame time. We don't
  // DCHECK though because some systems don't always have monotonic timestamps.
  // TODO(brianderson): In the future, long deadlines could result in us not
  // draining the queue if we don't catch up. If we consistently can't catch
  // up, our fallback should be to lower our frame rate.
  base::TimeTicks now = gfx::FrameTime::Now();
  base::TimeDelta draw_duration_estimate = client_->DrawDurationEstimate();
  while (!begin_retro_frame_args_.empty() &&
         now > AdjustedBeginImplFrameDeadline(begin_retro_frame_args_.front(),
                                              draw_duration_estimate)) {
    TRACE_EVENT1("cc",
                 "Scheduler::BeginRetroFrame discarding",
                 "frame_time",
                 begin_retro_frame_args_.front().frame_time);
    begin_retro_frame_args_.pop_front();
  }

  if (begin_retro_frame_args_.empty()) {
    DCHECK(settings_.throttle_frame_production);
    TRACE_EVENT_INSTANT0("cc",
                         "Scheduler::BeginRetroFrames all expired",
                         TRACE_EVENT_SCOPE_THREAD);
  } else {
    BeginImplFrame(begin_retro_frame_args_.front());
    begin_retro_frame_args_.pop_front();
  }
}

// There could be a race between the posted BeginRetroFrame and a new
// BeginFrame arriving via the normal mechanism. Scheduler::BeginFrame
// will check if there is a pending BeginRetroFrame to ensure we handle
// BeginFrames in FIFO order.
void Scheduler::PostBeginRetroFrameIfNeeded() {
  if (!last_set_needs_begin_frame_)
    return;

  if (begin_retro_frame_args_.empty() || begin_retro_frame_posted_)
    return;

  // begin_retro_frame_args_ should always be empty for the
  // synchronous compositor.
  DCHECK(!settings_.using_synchronous_renderer_compositor);

  if (state_machine_.begin_impl_frame_state() !=
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE)
    return;

  begin_retro_frame_posted_ = true;
  impl_task_runner_->PostTask(FROM_HERE, begin_retro_frame_closure_);
}

// BeginImplFrame starts a compositor frame that will wait up until a deadline
// for a BeginMainFrame+activation to complete before it times out and draws
// any asynchronous animation and scroll/pinch updates.
void Scheduler::BeginImplFrame(const BeginFrameArgs& args) {
  TRACE_EVENT1("cc", "Scheduler::BeginImplFrame", "args", ToTrace(args));
  DCHECK(state_machine_.begin_impl_frame_state() ==
         SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  DCHECK(state_machine_.HasInitializedOutputSurface());

  advance_commit_state_task_.Cancel();

  base::TimeDelta draw_duration_estimate = client_->DrawDurationEstimate();
  begin_impl_frame_args_ = args;
  begin_impl_frame_args_.deadline -= draw_duration_estimate;

  if (!state_machine_.smoothness_takes_priority() &&
      state_machine_.MainThreadIsInHighLatencyMode() &&
      CanCommitAndActivateBeforeDeadline()) {
    state_machine_.SetSkipNextBeginMainFrameToReduceLatency();
  }

  client_->WillBeginImplFrame(begin_impl_frame_args_);
  state_machine_.OnBeginImplFrame(begin_impl_frame_args_);
  devtools_instrumentation::DidBeginFrame(layer_tree_host_id_);

  ProcessScheduledActions();

  state_machine_.OnBeginImplFrameDeadlinePending();
  ScheduleBeginImplFrameDeadline(
      AdjustedBeginImplFrameDeadline(args, draw_duration_estimate));
}

base::TimeTicks Scheduler::AdjustedBeginImplFrameDeadline(
    const BeginFrameArgs& args,
    base::TimeDelta draw_duration_estimate) const {
  if (settings_.using_synchronous_renderer_compositor) {
    // The synchronous compositor needs to draw right away.
    return base::TimeTicks();
  } else if (state_machine_.ShouldTriggerBeginImplFrameDeadlineEarly()) {
    // We are ready to draw a new active tree immediately.
    return base::TimeTicks();
  } else if (state_machine_.needs_redraw()) {
    // We have an animation or fast input path on the impl thread that wants
    // to draw, so don't wait too long for a new active tree.
    return args.deadline - draw_duration_estimate;
  } else {
    // The impl thread doesn't have anything it wants to draw and we are just
    // waiting for a new active tree, so post the deadline for the next
    // expected BeginImplFrame start. This allows us to draw immediately when
    // there is a new active tree, instead of waiting for the next
    // BeginImplFrame.
    // TODO(brianderson): Handle long deadlines (that are past the next frame's
    // frame time) properly instead of using this hack.
    return args.frame_time + args.interval;
  }
}

void Scheduler::ScheduleBeginImplFrameDeadline(base::TimeTicks deadline) {
  if (settings_.using_synchronous_renderer_compositor) {
    // The synchronous renderer compositor has to make its GL calls
    // within this call.
    // TODO(brianderson): Have the OutputSurface initiate the deadline tasks
    // so the sychronous renderer compositor can take advantage of splitting
    // up the BeginImplFrame and deadline as well.
    OnBeginImplFrameDeadline();
    return;
  }
  begin_impl_frame_deadline_task_.Cancel();
  begin_impl_frame_deadline_task_.Reset(begin_impl_frame_deadline_closure_);

  base::TimeDelta delta = deadline - gfx::FrameTime::Now();
  if (delta <= base::TimeDelta())
    delta = base::TimeDelta();
  impl_task_runner_->PostDelayedTask(
      FROM_HERE, begin_impl_frame_deadline_task_.callback(), delta);
}

void Scheduler::OnBeginImplFrameDeadline() {
  TRACE_EVENT0("cc", "Scheduler::OnBeginImplFrameDeadline");
  begin_impl_frame_deadline_task_.Cancel();

  // We split the deadline actions up into two phases so the state machine
  // has a chance to trigger actions that should occur durring and after
  // the deadline separately. For example:
  // * Sending the BeginMainFrame will not occur after the deadline in
  //     order to wait for more user-input before starting the next commit.
  // * Creating a new OuputSurface will not occur during the deadline in
  //     order to allow the state machine to "settle" first.
  state_machine_.OnBeginImplFrameDeadline();
  ProcessScheduledActions();
  state_machine_.OnBeginImplFrameIdle();
  ProcessScheduledActions();

  client_->DidBeginImplFrameDeadline();
}

void Scheduler::PollForAnticipatedDrawTriggers() {
  TRACE_EVENT0("cc", "Scheduler::PollForAnticipatedDrawTriggers");
  poll_for_draw_triggers_task_.Cancel();
  state_machine_.DidEnterPollForAnticipatedDrawTriggers();
  ProcessScheduledActions();
  state_machine_.DidLeavePollForAnticipatedDrawTriggers();
}

void Scheduler::PollToAdvanceCommitState() {
  TRACE_EVENT0("cc", "Scheduler::PollToAdvanceCommitState");
  advance_commit_state_task_.Cancel();
  ProcessScheduledActions();
}

bool Scheduler::IsBeginMainFrameSent() const {
  return state_machine_.commit_state() ==
         SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT;
}

void Scheduler::DrawAndSwapIfPossible() {
  DrawResult result = client_->ScheduledActionDrawAndSwapIfPossible();
  state_machine_.DidDrawIfPossibleCompleted(result);
}

void Scheduler::ProcessScheduledActions() {
  // We do not allow ProcessScheduledActions to be recursive.
  // The top-level call will iteratively execute the next action for us anyway.
  if (inside_process_scheduled_actions_)
    return;

  base::AutoReset<bool> mark_inside(&inside_process_scheduled_actions_, true);

  SchedulerStateMachine::Action action;
  do {
    action = state_machine_.NextAction();
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "SchedulerStateMachine",
                 "state",
                 ToTrace(this));
    state_machine_.UpdateState(action);
    base::AutoReset<SchedulerStateMachine::Action>
        mark_inside_action(&inside_action_, action);
    switch (action) {
      case SchedulerStateMachine::ACTION_NONE:
        break;
      case SchedulerStateMachine::ACTION_ANIMATE:
        client_->ScheduledActionAnimate();
        break;
      case SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME:
        client_->ScheduledActionSendBeginMainFrame();
        break;
      case SchedulerStateMachine::ACTION_COMMIT:
        client_->ScheduledActionCommit();
        break;
      case SchedulerStateMachine::ACTION_UPDATE_VISIBLE_TILES:
        client_->ScheduledActionUpdateVisibleTiles();
        break;
      case SchedulerStateMachine::ACTION_ACTIVATE_PENDING_TREE:
        client_->ScheduledActionActivatePendingTree();
        break;
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_IF_POSSIBLE:
        DrawAndSwapIfPossible();
        break;
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_FORCED:
        client_->ScheduledActionDrawAndSwapForced();
        break;
      case SchedulerStateMachine::ACTION_DRAW_AND_SWAP_ABORT:
        // No action is actually performed, but this allows the state machine to
        // advance out of its waiting to draw state without actually drawing.
        break;
      case SchedulerStateMachine::ACTION_BEGIN_OUTPUT_SURFACE_CREATION:
        client_->ScheduledActionBeginOutputSurfaceCreation();
        break;
      case SchedulerStateMachine::ACTION_MANAGE_TILES:
        client_->ScheduledActionManageTiles();
        break;
    }
  } while (action != SchedulerStateMachine::ACTION_NONE);

  SetupNextBeginFrameIfNeeded();
  client_->DidAnticipatedDrawTimeChange(AnticipatedDrawTime());

  if (state_machine_.ShouldTriggerBeginImplFrameDeadlineEarly()) {
    DCHECK(!settings_.using_synchronous_renderer_compositor);
    ScheduleBeginImplFrameDeadline(base::TimeTicks());
  }
}

bool Scheduler::WillDrawIfNeeded() const {
  return !state_machine_.PendingDrawsShouldBeAborted();
}

scoped_ptr<base::Value> Scheduler::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);
  state->Set("state_machine", state_machine_.AsValue().release());
  if (synthetic_begin_frame_source_)
    state->Set("synthetic_begin_frame_source_",
               synthetic_begin_frame_source_->AsValue().release());

  scoped_ptr<base::DictionaryValue> scheduler_state(new base::DictionaryValue);
  scheduler_state->SetDouble(
      "time_until_anticipated_draw_time_ms",
      (AnticipatedDrawTime() - base::TimeTicks::Now()).InMillisecondsF());
  scheduler_state->SetDouble("vsync_interval_ms",
                             vsync_interval_.InMillisecondsF());
  scheduler_state->SetDouble("estimated_parent_draw_time_ms",
                             estimated_parent_draw_time_.InMillisecondsF());
  scheduler_state->SetBoolean("last_set_needs_begin_frame_",
                              last_set_needs_begin_frame_);
  scheduler_state->SetBoolean("begin_unthrottled_frame_posted_",
                              begin_unthrottled_frame_posted_);
  scheduler_state->SetBoolean("begin_retro_frame_posted_",
                              begin_retro_frame_posted_);
  scheduler_state->SetInteger("begin_retro_frame_args_",
                              begin_retro_frame_args_.size());
  scheduler_state->SetBoolean("begin_impl_frame_deadline_task_",
                              !begin_impl_frame_deadline_task_.IsCancelled());
  scheduler_state->SetBoolean("poll_for_draw_triggers_task_",
                              !poll_for_draw_triggers_task_.IsCancelled());
  scheduler_state->SetBoolean("advance_commit_state_task_",
                              !advance_commit_state_task_.IsCancelled());
  scheduler_state->Set("begin_impl_frame_args",
                       begin_impl_frame_args_.AsValue().release());

  state->Set("scheduler_state", scheduler_state.release());

  scoped_ptr<base::DictionaryValue> client_state(new base::DictionaryValue);
  client_state->SetDouble("draw_duration_estimate_ms",
                          client_->DrawDurationEstimate().InMillisecondsF());
  client_state->SetDouble(
      "begin_main_frame_to_commit_duration_estimate_ms",
      client_->BeginMainFrameToCommitDurationEstimate().InMillisecondsF());
  client_state->SetDouble(
      "commit_to_activate_duration_estimate_ms",
      client_->CommitToActivateDurationEstimate().InMillisecondsF());
  state->Set("client_state", client_state.release());
  return state.PassAs<base::Value>();
}

bool Scheduler::CanCommitAndActivateBeforeDeadline() const {
  // Check if the main thread computation and commit can be finished before the
  // impl thread's deadline.
  base::TimeTicks estimated_draw_time =
      begin_impl_frame_args_.frame_time +
      client_->BeginMainFrameToCommitDurationEstimate() +
      client_->CommitToActivateDurationEstimate();

  TRACE_EVENT2(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
      "CanCommitAndActivateBeforeDeadline",
      "time_left_after_drawing_ms",
      (begin_impl_frame_args_.deadline - estimated_draw_time).InMillisecondsF(),
      "state",
      ToTrace(this));

  return estimated_draw_time < begin_impl_frame_args_.deadline;
}

bool Scheduler::IsBeginMainFrameSentOrStarted() const {
  return (state_machine_.commit_state() ==
              SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_SENT ||
          state_machine_.commit_state() ==
              SchedulerStateMachine::COMMIT_STATE_BEGIN_MAIN_FRAME_STARTED);
}

}  // namespace cc
