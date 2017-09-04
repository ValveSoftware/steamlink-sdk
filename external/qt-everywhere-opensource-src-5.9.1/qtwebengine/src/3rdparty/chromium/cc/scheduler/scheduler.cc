// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/scheduler/compositor_timing_history.h"
#include "cc/scheduler/delay_based_time_source.h"

namespace cc {

namespace {
// This is a fudge factor we subtract from the deadline to account
// for message latency and kernel scheduling variability.
const base::TimeDelta kDeadlineFudgeFactor =
    base::TimeDelta::FromMicroseconds(1000);
}

Scheduler::Scheduler(
    SchedulerClient* client,
    const SchedulerSettings& settings,
    int layer_tree_host_id,
    base::SingleThreadTaskRunner* task_runner,
    std::unique_ptr<CompositorTimingHistory> compositor_timing_history)
    : settings_(settings),
      client_(client),
      layer_tree_host_id_(layer_tree_host_id),
      task_runner_(task_runner),
      begin_frame_source_(nullptr),
      observing_begin_frame_source_(false),
      compositor_timing_history_(std::move(compositor_timing_history)),
      begin_impl_frame_deadline_mode_(
          SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_NONE),
      begin_impl_frame_tracker_(BEGINFRAMETRACKER_FROM_HERE),
      state_machine_(settings),
      inside_process_scheduled_actions_(false),
      inside_action_(SchedulerStateMachine::ACTION_NONE),
      stopped_(false),
      weak_factory_(this) {
  TRACE_EVENT1("cc", "Scheduler::Scheduler", "settings", settings_.AsValue());
  DCHECK(client_);
  DCHECK(!state_machine_.BeginFrameNeeded());

  begin_impl_frame_deadline_closure_ = base::Bind(
      &Scheduler::OnBeginImplFrameDeadline, weak_factory_.GetWeakPtr());

  ProcessScheduledActions();
}

Scheduler::~Scheduler() {
  SetBeginFrameSource(nullptr);
}

void Scheduler::Stop() {
  stopped_ = true;
}

base::TimeTicks Scheduler::Now() const {
  base::TimeTicks now = base::TimeTicks::Now();
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.now"),
               "Scheduler::Now", "now", now);
  return now;
}

void Scheduler::SetVisible(bool visible) {
  state_machine_.SetVisible(visible);
  UpdateCompositorTimingHistoryRecordingEnabled();
  ProcessScheduledActions();
}

void Scheduler::SetCanDraw(bool can_draw) {
  state_machine_.SetCanDraw(can_draw);
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToActivate() {
  compositor_timing_history_->ReadyToActivate();
  state_machine_.NotifyReadyToActivate();
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToDraw() {
  // Future work might still needed for crbug.com/352894.
  state_machine_.NotifyReadyToDraw();
  ProcessScheduledActions();
}

void Scheduler::SetBeginFrameSource(BeginFrameSource* source) {
  if (source == begin_frame_source_)
    return;
  if (begin_frame_source_ && observing_begin_frame_source_)
    begin_frame_source_->RemoveObserver(this);
  begin_frame_source_ = source;
  if (!begin_frame_source_)
    return;
  if (observing_begin_frame_source_)
    begin_frame_source_->AddObserver(this);
}

void Scheduler::SetNeedsBeginMainFrame() {
  state_machine_.SetNeedsBeginMainFrame();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsOneBeginImplFrame() {
  state_machine_.SetNeedsOneBeginImplFrame();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsRedraw() {
  state_machine_.SetNeedsRedraw();
  ProcessScheduledActions();
}

void Scheduler::SetNeedsPrepareTiles() {
  DCHECK(!IsInsideAction(SchedulerStateMachine::ACTION_PREPARE_TILES));
  state_machine_.SetNeedsPrepareTiles();
  ProcessScheduledActions();
}

void Scheduler::DidSubmitCompositorFrame() {
  compositor_timing_history_->DidSubmitCompositorFrame();
  state_machine_.DidSubmitCompositorFrame();

  // There is no need to call ProcessScheduledActions here because
  // submitting a CompositorFrame should not trigger any new actions.
  if (!inside_process_scheduled_actions_) {
    DCHECK_EQ(state_machine_.NextAction(), SchedulerStateMachine::ACTION_NONE);
  }
}

void Scheduler::DidReceiveCompositorFrameAck() {
  DCHECK_GT(state_machine_.pending_submit_frames(), 0) << AsValue()->ToString();
  compositor_timing_history_->DidReceiveCompositorFrameAck();
  state_machine_.DidReceiveCompositorFrameAck();
  ProcessScheduledActions();
}

void Scheduler::SetTreePrioritiesAndScrollState(
    TreePriority tree_priority,
    ScrollHandlerState scroll_handler_state) {
  state_machine_.SetTreePrioritiesAndScrollState(tree_priority,
                                                 scroll_handler_state);
  ProcessScheduledActions();
}

void Scheduler::NotifyReadyToCommit() {
  TRACE_EVENT0("cc", "Scheduler::NotifyReadyToCommit");
  state_machine_.NotifyReadyToCommit();
  ProcessScheduledActions();
}

void Scheduler::DidCommit() {
  compositor_timing_history_->DidCommit();
}

void Scheduler::BeginMainFrameAborted(CommitEarlyOutReason reason) {
  TRACE_EVENT1("cc", "Scheduler::BeginMainFrameAborted", "reason",
               CommitEarlyOutReasonToString(reason));
  compositor_timing_history_->BeginMainFrameAborted();
  state_machine_.BeginMainFrameAborted(reason);
  ProcessScheduledActions();
}

void Scheduler::WillPrepareTiles() {
  compositor_timing_history_->WillPrepareTiles();
}

void Scheduler::DidPrepareTiles() {
  compositor_timing_history_->DidPrepareTiles();
  state_machine_.DidPrepareTiles();
}

void Scheduler::DidLoseCompositorFrameSink() {
  TRACE_EVENT0("cc", "Scheduler::DidLoseCompositorFrameSink");
  state_machine_.DidLoseCompositorFrameSink();
  UpdateCompositorTimingHistoryRecordingEnabled();
  ProcessScheduledActions();
}

void Scheduler::DidCreateAndInitializeCompositorFrameSink() {
  TRACE_EVENT0("cc", "Scheduler::DidCreateAndInitializeCompositorFrameSink");
  DCHECK(!observing_begin_frame_source_);
  DCHECK(begin_impl_frame_deadline_task_.IsCancelled());
  state_machine_.DidCreateAndInitializeCompositorFrameSink();
  compositor_timing_history_->DidCreateAndInitializeCompositorFrameSink();
  UpdateCompositorTimingHistoryRecordingEnabled();
  ProcessScheduledActions();
}

void Scheduler::NotifyBeginMainFrameStarted(
    base::TimeTicks main_thread_start_time) {
  TRACE_EVENT0("cc", "Scheduler::NotifyBeginMainFrameStarted");
  state_machine_.NotifyBeginMainFrameStarted();
  compositor_timing_history_->BeginMainFrameStarted(main_thread_start_time);
}

base::TimeTicks Scheduler::LastBeginImplFrameTime() {
  return begin_impl_frame_tracker_.Current().frame_time;
}

void Scheduler::BeginImplFrameNotExpectedSoon() {
  compositor_timing_history_->BeginImplFrameNotExpectedSoon();

  // Tying this to SendBeginMainFrameNotExpectedSoon will have some
  // false negatives, but we want to avoid running long idle tasks when
  // we are actually active.
  client_->SendBeginMainFrameNotExpectedSoon();
}

void Scheduler::SetupNextBeginFrameIfNeeded() {
  if (state_machine_.begin_impl_frame_state() !=
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE) {
    return;
  }

  bool needs_begin_frames = state_machine_.BeginFrameNeeded();
  if (needs_begin_frames && !observing_begin_frame_source_) {
    observing_begin_frame_source_ = true;
    if (begin_frame_source_)
      begin_frame_source_->AddObserver(this);
    devtools_instrumentation::NeedsBeginFrameChanged(layer_tree_host_id_, true);
  } else if (!needs_begin_frames && observing_begin_frame_source_) {
    observing_begin_frame_source_ = false;
    if (begin_frame_source_)
      begin_frame_source_->RemoveObserver(this);
    missed_begin_frame_task_.Cancel();
    BeginImplFrameNotExpectedSoon();
    devtools_instrumentation::NeedsBeginFrameChanged(layer_tree_host_id_,
                                                     false);
  }
}

void Scheduler::OnBeginFrameSourcePausedChanged(bool paused) {
  if (state_machine_.begin_frame_source_paused() == paused)
    return;
  TRACE_EVENT_INSTANT1("cc", "Scheduler::SetBeginFrameSourcePaused",
                       TRACE_EVENT_SCOPE_THREAD, "paused", paused);
  state_machine_.SetBeginFrameSourcePaused(paused);
  ProcessScheduledActions();
}

// BeginFrame is the mechanism that tells us that now is a good time to start
// making a frame. Usually this means that user input for the frame is complete.
// If the scheduler is busy, we queue the BeginFrame to be handled later as
// a BeginRetroFrame.
bool Scheduler::OnBeginFrameDerivedImpl(const BeginFrameArgs& args) {
  TRACE_EVENT1("cc,benchmark", "Scheduler::BeginFrame", "args", args.AsValue());

  if (!state_machine_.BeginFrameNeeded()) {
    TRACE_EVENT_INSTANT0("cc", "Scheduler::BeginFrameDropped",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // Trace this begin frame time through the Chrome stack
  TRACE_EVENT_FLOW_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"), "BeginFrameArgs",
      args.frame_time.ToInternalValue());

  if (settings_.using_synchronous_renderer_compositor) {
    BeginImplFrameSynchronous(args);
    return true;
  }

  if (inside_process_scheduled_actions_) {
    // The BFS can send a missed begin frame inside AddObserver. We can't handle
    // a begin frame inside ProcessScheduledActions so post a task.
    DCHECK_EQ(args.type, BeginFrameArgs::MISSED);
    DCHECK(missed_begin_frame_task_.IsCancelled());
    missed_begin_frame_task_.Reset(base::Bind(
        &Scheduler::BeginImplFrameWithDeadline, base::Unretained(this), args));
    task_runner_->PostTask(FROM_HERE, missed_begin_frame_task_.callback());
    return true;
  }

  BeginImplFrameWithDeadline(args);
  return true;
}

void Scheduler::SetVideoNeedsBeginFrames(bool video_needs_begin_frames) {
  state_machine_.SetVideoNeedsBeginFrames(video_needs_begin_frames);
  ProcessScheduledActions();
}

void Scheduler::OnDrawForCompositorFrameSink(bool resourceless_software_draw) {
  DCHECK(settings_.using_synchronous_renderer_compositor);
  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  DCHECK(begin_impl_frame_deadline_task_.IsCancelled());

  state_machine_.SetResourcelessSoftwareDraw(resourceless_software_draw);
  state_machine_.OnBeginImplFrameDeadline();
  ProcessScheduledActions();

  state_machine_.OnBeginImplFrameIdle();
  ProcessScheduledActions();
  state_machine_.SetResourcelessSoftwareDraw(false);
}

void Scheduler::BeginImplFrameWithDeadline(const BeginFrameArgs& args) {
  // The storage for |args| is owned by the missed begin frame task. Therefore
  // save |args| before cancelling the task either here or in the deadline.
  BeginFrameArgs adjusted_args = args;
  // Cancel the missed begin frame task in case the BFS sends a begin frame
  // before the missed frame task runs.
  missed_begin_frame_task_.Cancel();

  base::TimeTicks now = Now();

  // Discard missed begin frames if they are too late.
  if (adjusted_args.type == BeginFrameArgs::MISSED &&
      now > adjusted_args.deadline) {
    begin_frame_source_->DidFinishFrame(this, 0);
    return;
  }

  // Run the previous deadline if any.
  if (state_machine_.begin_impl_frame_state() ==
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME) {
    OnBeginImplFrameDeadline();
    // We may not need begin frames any longer.
    if (!observing_begin_frame_source_) {
      begin_frame_source_->DidFinishFrame(this, 0);
      return;
    }
  }
  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);

  bool main_thread_is_in_high_latency_mode =
      state_machine_.main_thread_missed_last_deadline();
  TRACE_EVENT2("cc,benchmark", "Scheduler::BeginImplFrame", "args",
               adjusted_args.AsValue(), "main_thread_missed_last_deadline",
               main_thread_is_in_high_latency_mode);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "MainThreadLatency", main_thread_is_in_high_latency_mode);

  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);

  adjusted_args.deadline -= compositor_timing_history_->DrawDurationEstimate();
  adjusted_args.deadline -= kDeadlineFudgeFactor;

  base::TimeDelta bmf_start_to_activate =
      compositor_timing_history_
          ->BeginMainFrameStartToCommitDurationEstimate() +
      compositor_timing_history_->CommitToReadyToActivateDurationEstimate() +
      compositor_timing_history_->ActivateDurationEstimate();

  base::TimeDelta bmf_to_activate_estimate_critical =
      bmf_start_to_activate +
      compositor_timing_history_->BeginMainFrameQueueDurationCriticalEstimate();

  state_machine_.SetCriticalBeginMainFrameToActivateIsFast(
      bmf_to_activate_estimate_critical < adjusted_args.interval);

  // Update the BeginMainFrame args now that we know whether the main
  // thread will be on the critical path or not.
  begin_main_frame_args_ = adjusted_args;
  begin_main_frame_args_.on_critical_path = !ImplLatencyTakesPriority();

  base::TimeDelta bmf_to_activate_estimate = bmf_to_activate_estimate_critical;
  if (!begin_main_frame_args_.on_critical_path) {
    bmf_to_activate_estimate =
        bmf_start_to_activate +
        compositor_timing_history_
            ->BeginMainFrameQueueDurationNotCriticalEstimate();
  }

  bool can_activate_before_deadline =
      CanBeginMainFrameAndActivateBeforeDeadline(adjusted_args,
                                                 bmf_to_activate_estimate, now);

  if (ShouldRecoverMainLatency(adjusted_args, can_activate_before_deadline)) {
    TRACE_EVENT_INSTANT0("cc", "SkipBeginMainFrameToReduceLatency",
                         TRACE_EVENT_SCOPE_THREAD);
    state_machine_.SetSkipNextBeginMainFrameToReduceLatency();
  } else if (ShouldRecoverImplLatency(adjusted_args,
                                      can_activate_before_deadline)) {
    TRACE_EVENT_INSTANT0("cc", "SkipBeginImplFrameToReduceLatency",
                         TRACE_EVENT_SCOPE_THREAD);
    if (begin_frame_source_)
      begin_frame_source_->DidFinishFrame(this, 0);
    return;
  }

  BeginImplFrame(adjusted_args);
}

void Scheduler::BeginImplFrameSynchronous(const BeginFrameArgs& args) {
  TRACE_EVENT1("cc,benchmark", "Scheduler::BeginImplFrame", "args",
               args.AsValue());

  // The main thread currently can't commit before we draw with the
  // synchronous compositor, so never consider the BeginMainFrame fast.
  state_machine_.SetCriticalBeginMainFrameToActivateIsFast(false);
  begin_main_frame_args_ = args;
  begin_main_frame_args_.on_critical_path = !ImplLatencyTakesPriority();

  BeginImplFrame(args);
  compositor_timing_history_->WillFinishImplFrame(
      state_machine_.needs_redraw());
  FinishImplFrame();
}

void Scheduler::FinishImplFrame() {
  state_machine_.OnBeginImplFrameIdle();
  ProcessScheduledActions();

  client_->DidFinishImplFrame();
  if (begin_frame_source_)
    begin_frame_source_->DidFinishFrame(this, 0);
  begin_impl_frame_tracker_.Finish();
}

// BeginImplFrame starts a compositor frame that will wait up until a deadline
// for a BeginMainFrame+activation to complete before it times out and draws
// any asynchronous animation and scroll/pinch updates.
void Scheduler::BeginImplFrame(const BeginFrameArgs& args) {
  DCHECK_EQ(state_machine_.begin_impl_frame_state(),
            SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_IDLE);
  DCHECK(begin_impl_frame_deadline_task_.IsCancelled());
  DCHECK(state_machine_.HasInitializedCompositorFrameSink());

  begin_impl_frame_tracker_.Start(args);
  state_machine_.OnBeginImplFrame();
  devtools_instrumentation::DidBeginFrame(layer_tree_host_id_);
  compositor_timing_history_->WillBeginImplFrame(
      state_machine_.NewActiveTreeLikely());
  client_->WillBeginImplFrame(begin_impl_frame_tracker_.Current());

  ProcessScheduledActions();
}

void Scheduler::ScheduleBeginImplFrameDeadline() {
  // The synchronous compositor does not post a deadline task.
  DCHECK(!settings_.using_synchronous_renderer_compositor);

  begin_impl_frame_deadline_task_.Cancel();
  begin_impl_frame_deadline_task_.Reset(begin_impl_frame_deadline_closure_);

  begin_impl_frame_deadline_mode_ =
      state_machine_.CurrentBeginImplFrameDeadlineMode();
  base::TimeTicks deadline;
  switch (begin_impl_frame_deadline_mode_) {
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_NONE:
      // No deadline.
      return;
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_IMMEDIATE:
      // We are ready to draw a new active tree immediately.
      // We don't use Now() here because it's somewhat expensive to call.
      deadline = base::TimeTicks();
      break;
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_REGULAR:
      // We are animating on the impl thread but we can wait for some time.
      deadline = begin_impl_frame_tracker_.Current().deadline;
      break;
    case SchedulerStateMachine::BEGIN_IMPL_FRAME_DEADLINE_MODE_LATE:
      // We are blocked for one reason or another and we should wait.
      // TODO(brianderson): Handle long deadlines (that are past the next
      // frame's frame time) properly instead of using this hack.
      deadline = begin_impl_frame_tracker_.Current().frame_time +
                 begin_impl_frame_tracker_.Current().interval;
      break;
    case SchedulerStateMachine::
        BEGIN_IMPL_FRAME_DEADLINE_MODE_BLOCKED_ON_READY_TO_DRAW:
      // We are blocked because we are waiting for ReadyToDraw signal. We would
      // post deadline after we received ReadyToDraw singal.
      TRACE_EVENT1("cc", "Scheduler::ScheduleBeginImplFrameDeadline",
                   "deadline_mode", "blocked_on_ready_to_draw");
      return;
  }

  TRACE_EVENT2("cc", "Scheduler::ScheduleBeginImplFrameDeadline", "mode",
               SchedulerStateMachine::BeginImplFrameDeadlineModeToString(
                   begin_impl_frame_deadline_mode_),
               "deadline", deadline);

  base::TimeDelta delta = std::max(deadline - Now(), base::TimeDelta());
  task_runner_->PostDelayedTask(
      FROM_HERE, begin_impl_frame_deadline_task_.callback(), delta);
}

void Scheduler::ScheduleBeginImplFrameDeadlineIfNeeded() {
  if (settings_.using_synchronous_renderer_compositor)
    return;

  if (state_machine_.begin_impl_frame_state() !=
      SchedulerStateMachine::BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME)
    return;

  if (begin_impl_frame_deadline_mode_ ==
          state_machine_.CurrentBeginImplFrameDeadlineMode() &&
      !begin_impl_frame_deadline_task_.IsCancelled()) {
    return;
  }

  ScheduleBeginImplFrameDeadline();
}

void Scheduler::OnBeginImplFrameDeadline() {
  TRACE_EVENT0("cc,benchmark", "Scheduler::OnBeginImplFrameDeadline");
  begin_impl_frame_deadline_task_.Cancel();
  // We split the deadline actions up into two phases so the state machine
  // has a chance to trigger actions that should occur durring and after
  // the deadline separately. For example:
  // * Sending the BeginMainFrame will not occur after the deadline in
  //     order to wait for more user-input before starting the next commit.
  // * Creating a new OuputSurface will not occur during the deadline in
  //     order to allow the state machine to "settle" first.
  compositor_timing_history_->WillFinishImplFrame(
      state_machine_.needs_redraw());
  state_machine_.OnBeginImplFrameDeadline();
  ProcessScheduledActions();
  FinishImplFrame();
}

void Scheduler::DrawIfPossible() {
  bool drawing_with_new_active_tree =
      state_machine_.active_tree_needs_first_draw();
  bool main_thread_missed_last_deadline =
      state_machine_.main_thread_missed_last_deadline();
  compositor_timing_history_->WillDraw();
  state_machine_.WillDraw();
  DrawResult result = client_->ScheduledActionDrawIfPossible();
  state_machine_.DidDraw(result);
  compositor_timing_history_->DidDraw(
      drawing_with_new_active_tree, main_thread_missed_last_deadline,
      begin_impl_frame_tracker_.DangerousMethodCurrentOrLast().frame_time);
}

void Scheduler::DrawForced() {
  bool drawing_with_new_active_tree =
      state_machine_.active_tree_needs_first_draw();
  bool main_thread_missed_last_deadline =
      state_machine_.main_thread_missed_last_deadline();
  compositor_timing_history_->WillDraw();
  state_machine_.WillDraw();
  DrawResult result = client_->ScheduledActionDrawForced();
  state_machine_.DidDraw(result);
  compositor_timing_history_->DidDraw(
      drawing_with_new_active_tree, main_thread_missed_last_deadline,
      begin_impl_frame_tracker_.DangerousMethodCurrentOrLast().frame_time);
}

void Scheduler::SetDeferCommits(bool defer_commits) {
  TRACE_EVENT1("cc", "Scheduler::SetDeferCommits", "defer_commits",
               defer_commits);
  state_machine_.SetDeferCommits(defer_commits);
  ProcessScheduledActions();
}

void Scheduler::ProcessScheduledActions() {
  // Do not perform actions during compositor shutdown.
  if (stopped_)
    return;

  // We do not allow ProcessScheduledActions to be recursive.
  // The top-level call will iteratively execute the next action for us anyway.
  if (inside_process_scheduled_actions_)
    return;

  base::AutoReset<bool> mark_inside(&inside_process_scheduled_actions_, true);

  SchedulerStateMachine::Action action;
  do {
    action = state_machine_.NextAction();
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"),
                 "SchedulerStateMachine", "state", AsValue());
    base::AutoReset<SchedulerStateMachine::Action> mark_inside_action(
        &inside_action_, action);
    switch (action) {
      case SchedulerStateMachine::ACTION_NONE:
        break;
      case SchedulerStateMachine::ACTION_SEND_BEGIN_MAIN_FRAME:
        compositor_timing_history_->WillBeginMainFrame(
            begin_main_frame_args_.on_critical_path,
            begin_main_frame_args_.frame_time);
        state_machine_.WillSendBeginMainFrame();
        // TODO(brianderson): Pass begin_main_frame_args_ directly to client.
        client_->ScheduledActionSendBeginMainFrame(begin_main_frame_args_);
        break;
      case SchedulerStateMachine::ACTION_COMMIT: {
        bool commit_has_no_updates = false;
        state_machine_.WillCommit(commit_has_no_updates);
        client_->ScheduledActionCommit();
        break;
      }
      case SchedulerStateMachine::ACTION_ACTIVATE_SYNC_TREE:
        compositor_timing_history_->WillActivate();
        state_machine_.WillActivate();
        client_->ScheduledActionActivateSyncTree();
        compositor_timing_history_->DidActivate();
        break;
      case SchedulerStateMachine::ACTION_DRAW_IF_POSSIBLE:
        DrawIfPossible();
        break;
      case SchedulerStateMachine::ACTION_DRAW_FORCED:
        DrawForced();
        break;
      case SchedulerStateMachine::ACTION_DRAW_ABORT: {
        // No action is actually performed, but this allows the state machine to
        // drain the pipeline without actually drawing.
        state_machine_.AbortDraw();
        compositor_timing_history_->DrawAborted();
        break;
      }
      case SchedulerStateMachine::ACTION_BEGIN_COMPOSITOR_FRAME_SINK_CREATION:
        state_machine_.WillBeginCompositorFrameSinkCreation();
        client_->ScheduledActionBeginCompositorFrameSinkCreation();
        break;
      case SchedulerStateMachine::ACTION_PREPARE_TILES:
        state_machine_.WillPrepareTiles();
        client_->ScheduledActionPrepareTiles();
        break;
      case SchedulerStateMachine::ACTION_INVALIDATE_COMPOSITOR_FRAME_SINK: {
        state_machine_.WillInvalidateCompositorFrameSink();
        client_->ScheduledActionInvalidateCompositorFrameSink();
        break;
      }
    }
  } while (action != SchedulerStateMachine::ACTION_NONE);

  ScheduleBeginImplFrameDeadlineIfNeeded();
  SetupNextBeginFrameIfNeeded();
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
Scheduler::AsValue() const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  base::TimeTicks now = Now();

  state->BeginDictionary("state_machine");
  state_machine_.AsValueInto(state.get());
  state->EndDictionary();

  state->BeginDictionary("scheduler_state");
  state->SetBoolean("observing_begin_frame_source",
                    observing_begin_frame_source_);
  state->SetBoolean("begin_impl_frame_deadline_task",
                    !begin_impl_frame_deadline_task_.IsCancelled());
  state->SetBoolean("missed_begin_frame_task",
                    !missed_begin_frame_task_.IsCancelled());
  state->SetString("inside_action",
                   SchedulerStateMachine::ActionToString(inside_action_));

  state->BeginDictionary("begin_impl_frame_args");
  begin_impl_frame_tracker_.AsValueInto(now, state.get());
  state->EndDictionary();

  state->SetString("begin_impl_frame_deadline_mode_",
                   SchedulerStateMachine::BeginImplFrameDeadlineModeToString(
                       begin_impl_frame_deadline_mode_));
  state->EndDictionary();

  state->BeginDictionary("compositor_timing_history");
  compositor_timing_history_->AsValueInto(state.get());
  state->EndDictionary();

  return std::move(state);
}

void Scheduler::UpdateCompositorTimingHistoryRecordingEnabled() {
  compositor_timing_history_->SetRecordingEnabled(
      state_machine_.HasInitializedCompositorFrameSink() &&
      state_machine_.visible());
}

bool Scheduler::ShouldRecoverMainLatency(
    const BeginFrameArgs& args,
    bool can_activate_before_deadline) const {
  DCHECK(!settings_.using_synchronous_renderer_compositor);

  if (!settings_.enable_latency_recovery)
    return false;

  // The main thread is in a low latency mode and there's no need to recover.
  if (!state_machine_.main_thread_missed_last_deadline())
    return false;

  // When prioritizing impl thread latency, we currently put the
  // main thread in a high latency mode. Don't try to fight it.
  if (state_machine_.ImplLatencyTakesPriority())
    return false;

  return can_activate_before_deadline;
}

bool Scheduler::ShouldRecoverImplLatency(
    const BeginFrameArgs& args,
    bool can_activate_before_deadline) const {
  DCHECK(!settings_.using_synchronous_renderer_compositor);

  if (!settings_.enable_latency_recovery)
    return false;

  // Disable impl thread latency recovery when using the unthrottled
  // begin frame source since we will always get a BeginFrame before
  // the swap ack and our heuristics below will not work.
  if (begin_frame_source_ && !begin_frame_source_->IsThrottled())
    return false;

  // If we are swap throttled at the BeginFrame, that means the impl thread is
  // very likely in a high latency mode.
  bool impl_thread_is_likely_high_latency = state_machine_.IsDrawThrottled();
  if (!impl_thread_is_likely_high_latency)
    return false;

  // The deadline may be in the past if our draw time is too long.
  bool can_draw_before_deadline = args.frame_time < args.deadline;

  // When prioritizing impl thread latency, the deadline doesn't wait
  // for the main thread.
  if (state_machine_.ImplLatencyTakesPriority())
    return can_draw_before_deadline;

  // If we only have impl-side updates, the deadline doesn't wait for
  // the main thread.
  if (state_machine_.OnlyImplSideUpdatesExpected())
    return can_draw_before_deadline;

  // If we get here, we know the main thread is in a low-latency mode relative
  // to the impl thread. In this case, only try to also recover impl thread
  // latency if both the main and impl threads can run serially before the
  // deadline.
  return can_activate_before_deadline;
}

bool Scheduler::CanBeginMainFrameAndActivateBeforeDeadline(
    const BeginFrameArgs& args,
    base::TimeDelta bmf_to_activate_estimate,
    base::TimeTicks now) const {
  // Check if the main thread computation and commit can be finished before the
  // impl thread's deadline.
  base::TimeTicks estimated_draw_time = now + bmf_to_activate_estimate;

  return estimated_draw_time < args.deadline;
}

bool Scheduler::IsBeginMainFrameSentOrStarted() const {
  return (state_machine_.begin_main_frame_state() ==
              SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_SENT ||
          state_machine_.begin_main_frame_state() ==
              SchedulerStateMachine::BEGIN_MAIN_FRAME_STATE_STARTED);
}

}  // namespace cc
