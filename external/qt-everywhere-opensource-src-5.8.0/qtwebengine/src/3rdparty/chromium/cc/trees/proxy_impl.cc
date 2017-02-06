// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_impl.h"

#include <algorithm>
#include <string>

#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/trace_event/trace_event_synthetic_delay.h"
#include "cc/animation/animation_events.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/input/top_controls_manager.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/scheduler/compositor_timing_history.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/task_runner_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace cc {

namespace {

// Measured in seconds.
const double kSmoothnessTakesPriorityExpirationDelay = 0.25;

unsigned int nextBeginFrameId = 0;

}  // namespace

std::unique_ptr<ProxyImpl> ProxyImpl::Create(
    ChannelImpl* channel_impl,
    LayerTreeHost* layer_tree_host,
    TaskRunnerProvider* task_runner_provider,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  return base::WrapUnique(
      new ProxyImpl(channel_impl, layer_tree_host, task_runner_provider,
                    std::move(external_begin_frame_source)));
}

ProxyImpl::ProxyImpl(
    ChannelImpl* channel_impl,
    LayerTreeHost* layer_tree_host,
    TaskRunnerProvider* task_runner_provider,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source)
    : layer_tree_host_id_(layer_tree_host->id()),
      commit_completion_waits_for_activation_(false),
      commit_completion_event_(nullptr),
      activation_completion_event_(nullptr),
      next_frame_is_newly_committed_frame_(false),
      inside_draw_(false),
      input_throttled_until_commit_(false),
      task_runner_provider_(task_runner_provider),
      smoothness_priority_expiration_notifier_(
          task_runner_provider->ImplThreadTaskRunner(),
          base::Bind(&ProxyImpl::RenewTreePriority, base::Unretained(this)),
          base::TimeDelta::FromSecondsD(
              kSmoothnessTakesPriorityExpirationDelay)),
      external_begin_frame_source_(std::move(external_begin_frame_source)),
      rendering_stats_instrumentation_(
          layer_tree_host->rendering_stats_instrumentation()),
      channel_impl_(channel_impl) {
  TRACE_EVENT0("cc", "ProxyImpl::ProxyImpl");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());

  layer_tree_host_impl_ = layer_tree_host->CreateLayerTreeHostImpl(this);

  SchedulerSettings scheduler_settings(
      layer_tree_host->settings().ToSchedulerSettings());

  std::unique_ptr<CompositorTimingHistory> compositor_timing_history(
      new CompositorTimingHistory(
          scheduler_settings.using_synchronous_renderer_compositor,
          CompositorTimingHistory::RENDERER_UMA,
          rendering_stats_instrumentation_));

  BeginFrameSource* frame_source = external_begin_frame_source_.get();
  if (!scheduler_settings.throttle_frame_production) {
    // Unthrottled source takes precedence over external sources.
    unthrottled_begin_frame_source_.reset(
        new BackToBackBeginFrameSource(base::MakeUnique<DelayBasedTimeSource>(
            task_runner_provider_->ImplThreadTaskRunner())));
    frame_source = unthrottled_begin_frame_source_.get();
  }
  if (!frame_source) {
    synthetic_begin_frame_source_.reset(
        new DelayBasedBeginFrameSource(base::MakeUnique<DelayBasedTimeSource>(
            task_runner_provider_->ImplThreadTaskRunner())));
    frame_source = synthetic_begin_frame_source_.get();
  }
  scheduler_ =
      Scheduler::Create(this, scheduler_settings, layer_tree_host_id_,
                        task_runner_provider_->ImplThreadTaskRunner(),
                        frame_source, std::move(compositor_timing_history));

  DCHECK_EQ(scheduler_->visible(), layer_tree_host_impl_->visible());
}

ProxyImpl::BlockedMainCommitOnly::BlockedMainCommitOnly()
    : layer_tree_host(nullptr) {}

ProxyImpl::BlockedMainCommitOnly::~BlockedMainCommitOnly() {}

ProxyImpl::~ProxyImpl() {
  TRACE_EVENT0("cc", "ProxyImpl::~ProxyImpl");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());

  // Take away the OutputSurface before destroying things so it doesn't try
  // to call into its client mid-shutdown.
  scheduler_->DidLoseOutputSurface();
  layer_tree_host_impl_->ReleaseOutputSurface();

  scheduler_ = nullptr;
  external_begin_frame_source_ = nullptr;
  unthrottled_begin_frame_source_ = nullptr;
  synthetic_begin_frame_source_ = nullptr;
  layer_tree_host_impl_ = nullptr;
  // We need to explicitly shutdown the notifier to destroy any weakptrs it is
  // holding while still on the compositor thread. This also ensures any
  // callbacks holding a ProxyImpl pointer are cancelled.
  smoothness_priority_expiration_notifier_.Shutdown();
}

void ProxyImpl::InitializeMutatorOnImpl(
    std::unique_ptr<LayerTreeMutator> mutator) {
  TRACE_EVENT0("cc,compositor-worker", "ProxyImpl::InitializeMutatorOnImpl");
  DCHECK(IsImplThread());
  layer_tree_host_impl_->SetLayerTreeMutator(std::move(mutator));
}

void ProxyImpl::UpdateTopControlsStateOnImpl(TopControlsState constraints,
                                             TopControlsState current,
                                             bool animate) {
  DCHECK(IsImplThread());
  layer_tree_host_impl_->top_controls_manager()->UpdateTopControlsState(
      constraints, current, animate);
}

void ProxyImpl::InitializeOutputSurfaceOnImpl(OutputSurface* output_surface) {
  TRACE_EVENT0("cc", "ProxyImpl::InitializeOutputSurfaceOnImplThread");
  DCHECK(IsImplThread());

  LayerTreeHostImpl* host_impl = layer_tree_host_impl_.get();
  bool success = host_impl->InitializeRenderer(output_surface);
  RendererCapabilities capabilities;
  if (success) {
    capabilities =
        host_impl->GetRendererCapabilities().MainThreadCapabilities();
  }

  channel_impl_->DidInitializeOutputSurface(success, capabilities);

  if (success)
    scheduler_->DidCreateAndInitializeOutputSurface();
}

void ProxyImpl::MainThreadHasStoppedFlingingOnImpl() {
  DCHECK(IsImplThread());
  layer_tree_host_impl_->MainThreadHasStoppedFlinging();
}

void ProxyImpl::SetInputThrottledUntilCommitOnImpl(bool is_throttled) {
  DCHECK(IsImplThread());
  if (is_throttled == input_throttled_until_commit_)
    return;
  input_throttled_until_commit_ = is_throttled;
  RenewTreePriority();
}

void ProxyImpl::SetDeferCommitsOnImpl(bool defer_commits) const {
  DCHECK(IsImplThread());
  scheduler_->SetDeferCommits(defer_commits);
}

void ProxyImpl::SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) {
  DCHECK(IsImplThread());
  SetNeedsRedrawRectOnImplThread(damage_rect);
}

void ProxyImpl::SetNeedsCommitOnImpl() {
  DCHECK(IsImplThread());
  SetNeedsCommitOnImplThread();
}

void ProxyImpl::BeginMainFrameAbortedOnImpl(
    CommitEarlyOutReason reason,
    base::TimeTicks main_thread_start_time) {
  TRACE_EVENT1("cc", "ProxyImpl::BeginMainFrameAbortedOnImplThread", "reason",
               CommitEarlyOutReasonToString(reason));
  DCHECK(IsImplThread());
  DCHECK(scheduler_->CommitPending());

  if (CommitEarlyOutHandledCommit(reason)) {
    SetInputThrottledUntilCommitOnImpl(false);
  }
  layer_tree_host_impl_->BeginMainFrameAborted(reason);
  scheduler_->NotifyBeginMainFrameStarted(main_thread_start_time);
  scheduler_->BeginMainFrameAborted(reason);
}

void ProxyImpl::FinishAllRenderingOnImpl(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ProxyImpl::FinishAllRenderingOnImplThread");
  DCHECK(IsImplThread());
  layer_tree_host_impl_->FinishAllRendering();
  completion->Signal();
}

void ProxyImpl::SetVisibleOnImpl(bool visible) {
  TRACE_EVENT1("cc", "ProxyImpl::SetVisibleOnImplThread", "visible", visible);
  DCHECK(IsImplThread());
  layer_tree_host_impl_->SetVisible(visible);
  scheduler_->SetVisible(visible);
}

void ProxyImpl::ReleaseOutputSurfaceOnImpl(CompletionEvent* completion) {
  DCHECK(IsImplThread());

  // Unlike DidLoseOutputSurfaceOnImplThread, we don't need to call
  // LayerTreeHost::DidLoseOutputSurface since it already knows.
  scheduler_->DidLoseOutputSurface();
  layer_tree_host_impl_->ReleaseOutputSurface();
  completion->Signal();
}

void ProxyImpl::FinishGLOnImpl(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ProxyImpl::FinishGLOnImplThread");
  DCHECK(IsImplThread());
  if (layer_tree_host_impl_->output_surface()) {
    ContextProvider* context_provider =
        layer_tree_host_impl_->output_surface()->context_provider();
    if (context_provider)
      context_provider->ContextGL()->Finish();
  }
  completion->Signal();
}

void ProxyImpl::MainFrameWillHappenOnImplForTesting(
    CompletionEvent* completion,
    bool* main_frame_will_happen) {
  DCHECK(IsImplThread());
  if (layer_tree_host_impl_->output_surface()) {
    *main_frame_will_happen = scheduler_->MainFrameForTestingWillHappen();
  } else {
    *main_frame_will_happen = false;
  }
  completion->Signal();
}

void ProxyImpl::BlockNotifyReadyToActivateForTesting(bool block) {
  DCHECK(IsImplThread());
  layer_tree_host_impl_->BlockNotifyReadyToActivateForTesting(block);
}

CompletionEvent* ProxyImpl::ActivationCompletionEventForTesting() {
  DCHECK(IsImplThread());
  return activation_completion_event_;
}

void ProxyImpl::StartCommitOnImpl(CompletionEvent* completion,
                                  LayerTreeHost* layer_tree_host,
                                  base::TimeTicks main_thread_start_time,
                                  bool hold_commit_for_activation) {
  TRACE_EVENT0("cc", "ProxyImpl::StartCommitOnImplThread");
  DCHECK(!commit_completion_event_);
  DCHECK(IsImplThread() && IsMainThreadBlocked());
  DCHECK(scheduler_);
  DCHECK(scheduler_->CommitPending());

  if (!layer_tree_host_impl_) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoLayerTree",
                         TRACE_EVENT_SCOPE_THREAD);
    completion->Signal();
    return;
  }

  // Ideally, we should inform to impl thread when BeginMainFrame is started.
  // But, we can avoid a PostTask in here.
  scheduler_->NotifyBeginMainFrameStarted(main_thread_start_time);

  commit_completion_event_ = completion;
  commit_completion_waits_for_activation_ = hold_commit_for_activation;

  DCHECK(!blocked_main_commit().layer_tree_host);
  blocked_main_commit().layer_tree_host = layer_tree_host;
  scheduler_->NotifyReadyToCommit();
}

void ProxyImpl::UpdateRendererCapabilitiesOnImplThread() {
  DCHECK(IsImplThread());
  channel_impl_->SetRendererCapabilitiesMainCopy(
      layer_tree_host_impl_->GetRendererCapabilities()
          .MainThreadCapabilities());
}

void ProxyImpl::DidLoseOutputSurfaceOnImplThread() {
  TRACE_EVENT0("cc", "ProxyImpl::DidLoseOutputSurfaceOnImplThread");
  DCHECK(IsImplThread());
  channel_impl_->DidLoseOutputSurface();
  scheduler_->DidLoseOutputSurface();
}

void ProxyImpl::CommitVSyncParameters(base::TimeTicks timebase,
                                      base::TimeDelta interval) {
  DCHECK(IsImplThread());
  if (synthetic_begin_frame_source_)
    synthetic_begin_frame_source_->OnUpdateVSyncParameters(timebase, interval);
}

void ProxyImpl::SetBeginFrameSource(BeginFrameSource* source) {
  // During shutdown, destroying the OutputSurface may unset the
  // BeginFrameSource.
  if (scheduler_) {
    // TODO(enne): this overrides any preexisting begin frame source.  Those
    // other sources will eventually be removed and this will be the only path.
    scheduler_->SetBeginFrameSource(source);
  }
}

void ProxyImpl::SetEstimatedParentDrawTime(base::TimeDelta draw_time) {
  DCHECK(IsImplThread());
  scheduler_->SetEstimatedParentDrawTime(draw_time);
}

void ProxyImpl::DidSwapBuffersOnImplThread() {
  DCHECK(IsImplThread());
  scheduler_->DidSwapBuffers();
}

void ProxyImpl::DidSwapBuffersCompleteOnImplThread() {
  TRACE_EVENT0("cc,benchmark", "ProxyImpl::DidSwapBuffersCompleteOnImplThread");
  DCHECK(IsImplThread());
  scheduler_->DidSwapBuffersComplete();
  channel_impl_->DidCompleteSwapBuffers();
}

void ProxyImpl::OnCanDrawStateChanged(bool can_draw) {
  TRACE_EVENT1("cc", "ProxyImpl::OnCanDrawStateChanged", "can_draw", can_draw);
  DCHECK(IsImplThread());
  scheduler_->SetCanDraw(can_draw);
}

void ProxyImpl::NotifyReadyToActivate() {
  TRACE_EVENT0("cc", "ProxyImpl::NotifyReadyToActivate");
  DCHECK(IsImplThread());
  scheduler_->NotifyReadyToActivate();
}

void ProxyImpl::NotifyReadyToDraw() {
  TRACE_EVENT0("cc", "ProxyImpl::NotifyReadyToDraw");
  DCHECK(IsImplThread());
  scheduler_->NotifyReadyToDraw();
}

void ProxyImpl::SetNeedsRedrawOnImplThread() {
  TRACE_EVENT0("cc", "ProxyImpl::SetNeedsRedrawOnImplThread");
  DCHECK(IsImplThread());
  scheduler_->SetNeedsRedraw();
}

void ProxyImpl::SetNeedsRedrawRectOnImplThread(const gfx::Rect& damage_rect) {
  DCHECK(IsImplThread());
  layer_tree_host_impl_->SetViewportDamage(damage_rect);
  SetNeedsRedrawOnImplThread();
}

void ProxyImpl::SetNeedsOneBeginImplFrameOnImplThread() {
  TRACE_EVENT0("cc", "ProxyImpl::SetNeedsOneBeginImplFrameOnImplThread");
  DCHECK(IsImplThread());
  scheduler_->SetNeedsOneBeginImplFrame();
}

void ProxyImpl::SetNeedsPrepareTilesOnImplThread() {
  DCHECK(IsImplThread());
  scheduler_->SetNeedsPrepareTiles();
}

void ProxyImpl::SetNeedsCommitOnImplThread() {
  TRACE_EVENT0("cc", "ProxyImpl::SetNeedsCommitOnImplThread");
  DCHECK(IsImplThread());
  scheduler_->SetNeedsBeginMainFrame();
}

void ProxyImpl::SetVideoNeedsBeginFrames(bool needs_begin_frames) {
  TRACE_EVENT1("cc", "ProxyImpl::SetVideoNeedsBeginFrames",
               "needs_begin_frames", needs_begin_frames);
  DCHECK(IsImplThread());
  // In tests the layer tree is destroyed after the scheduler is.
  if (scheduler_)
    scheduler_->SetVideoNeedsBeginFrames(needs_begin_frames);
}

void ProxyImpl::PostAnimationEventsToMainThreadOnImplThread(
    std::unique_ptr<AnimationEvents> events) {
  TRACE_EVENT0("cc", "ProxyImpl::PostAnimationEventsToMainThreadOnImplThread");
  DCHECK(IsImplThread());
  channel_impl_->SetAnimationEvents(std::move(events));
}

bool ProxyImpl::IsInsideDraw() {
  return inside_draw_;
}

void ProxyImpl::RenewTreePriority() {
  DCHECK(IsImplThread());
  bool smoothness_takes_priority =
      layer_tree_host_impl_->pinch_gesture_active() ||
      layer_tree_host_impl_->page_scale_animation_active() ||
      layer_tree_host_impl_->IsActivelyScrolling();

  // Schedule expiration if smoothness currently takes priority.
  if (smoothness_takes_priority)
    smoothness_priority_expiration_notifier_.Schedule();

  // We use the same priority for both trees by default.
  TreePriority tree_priority = SAME_PRIORITY_FOR_BOTH_TREES;

  // Smoothness takes priority if we have an expiration for it scheduled.
  if (smoothness_priority_expiration_notifier_.HasPendingNotification())
    tree_priority = SMOOTHNESS_TAKES_PRIORITY;

  // New content always takes priority when there is an invalid viewport size or
  // ui resources have been evicted.
  if (layer_tree_host_impl_->active_tree()->ViewportSizeInvalid() ||
      layer_tree_host_impl_->EvictedUIResourcesExist() ||
      input_throttled_until_commit_) {
    // Once we enter NEW_CONTENTS_TAKES_PRIORITY mode, visible tiles on active
    // tree might be freed. We need to set RequiresHighResToDraw to ensure that
    // high res tiles will be required to activate pending tree.
    layer_tree_host_impl_->SetRequiresHighResToDraw();
    tree_priority = NEW_CONTENT_TAKES_PRIORITY;
  }

  layer_tree_host_impl_->SetTreePriority(tree_priority);

  // Only put the scheduler in impl latency prioritization mode if we don't
  // have a scroll listener. This gives the scroll listener a better chance of
  // handling scroll updates within the same frame. The tree itself is still
  // kept in prefer smoothness mode to allow checkerboarding.
  ScrollHandlerState scroll_handler_state =
      layer_tree_host_impl_->scroll_affects_scroll_handler()
          ? ScrollHandlerState::SCROLL_AFFECTS_SCROLL_HANDLER
          : ScrollHandlerState::SCROLL_DOES_NOT_AFFECT_SCROLL_HANDLER;
  scheduler_->SetTreePrioritiesAndScrollState(tree_priority,
                                              scroll_handler_state);
}

void ProxyImpl::PostDelayedAnimationTaskOnImplThread(const base::Closure& task,
                                                     base::TimeDelta delay) {
  DCHECK(IsImplThread());
  task_runner_provider_->ImplThreadTaskRunner()->PostDelayedTask(FROM_HERE,
                                                                 task, delay);
}

void ProxyImpl::DidActivateSyncTree() {
  TRACE_EVENT0("cc", "ProxyImpl::DidActivateSyncTreeOnImplThread");
  DCHECK(IsImplThread());

  if (activation_completion_event_) {
    TRACE_EVENT_INSTANT0("cc", "ReleaseCommitbyActivation",
                         TRACE_EVENT_SCOPE_THREAD);
    activation_completion_event_->Signal();
    activation_completion_event_ = nullptr;
  }
}

void ProxyImpl::WillPrepareTiles() {
  DCHECK(IsImplThread());
  scheduler_->WillPrepareTiles();
}

void ProxyImpl::DidPrepareTiles() {
  DCHECK(IsImplThread());
  scheduler_->DidPrepareTiles();
}

void ProxyImpl::DidCompletePageScaleAnimationOnImplThread() {
  DCHECK(IsImplThread());
  channel_impl_->DidCompletePageScaleAnimation();
}

void ProxyImpl::OnDrawForOutputSurface(bool resourceless_software_draw) {
  DCHECK(IsImplThread());
  scheduler_->OnDrawForOutputSurface(resourceless_software_draw);
}

void ProxyImpl::WillBeginImplFrame(const BeginFrameArgs& args) {
  DCHECK(IsImplThread());
  layer_tree_host_impl_->WillBeginImplFrame(args);
}

void ProxyImpl::DidFinishImplFrame() {
  DCHECK(IsImplThread());
  layer_tree_host_impl_->DidFinishImplFrame();
}

void ProxyImpl::ScheduledActionSendBeginMainFrame(const BeginFrameArgs& args) {
  DCHECK(IsImplThread());
  unsigned int begin_frame_id = nextBeginFrameId++;
  benchmark_instrumentation::ScopedBeginFrameTask begin_frame_task(
      benchmark_instrumentation::kSendBeginFrame, begin_frame_id);
  std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state(
      new BeginMainFrameAndCommitState);
  begin_main_frame_state->begin_frame_id = begin_frame_id;
  begin_main_frame_state->begin_frame_args = args;
  begin_main_frame_state->begin_frame_callbacks =
      layer_tree_host_impl_->ProcessLayerTreeMutations();
  begin_main_frame_state->scroll_info =
      layer_tree_host_impl_->ProcessScrollDeltas();
  begin_main_frame_state->memory_allocation_limit_bytes =
      layer_tree_host_impl_->memory_allocation_limit_bytes();
  begin_main_frame_state->evicted_ui_resources =
      layer_tree_host_impl_->EvictedUIResourcesExist();
  channel_impl_->BeginMainFrame(std::move(begin_main_frame_state));
  devtools_instrumentation::DidRequestMainThreadFrame(layer_tree_host_id_);
}

DrawResult ProxyImpl::ScheduledActionDrawAndSwapIfPossible() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionDrawAndSwap");
  DCHECK(IsImplThread());

  // SchedulerStateMachine::DidDrawIfPossibleCompleted isn't set up to
  // handle DRAW_ABORTED_CANT_DRAW.  Moreover, the scheduler should
  // never generate this call when it can't draw.
  DCHECK(layer_tree_host_impl_->CanDraw());

  bool forced_draw = false;
  return DrawAndSwapInternal(forced_draw);
}

DrawResult ProxyImpl::ScheduledActionDrawAndSwapForced() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionDrawAndSwapForced");
  DCHECK(IsImplThread());
  bool forced_draw = true;
  return DrawAndSwapInternal(forced_draw);
}

void ProxyImpl::ScheduledActionCommit() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionCommit");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());
  DCHECK(commit_completion_event_);

  layer_tree_host_impl_->BeginCommit();
  blocked_main_commit().layer_tree_host->FinishCommitOnImplThread(
      layer_tree_host_impl_.get());

  // Remove the LayerTreeHost reference before the completion event is signaled
  // and cleared. This is necessary since blocked_main_commit() allows access
  // only while we have the completion event to ensure the main thread is
  // blocked for a commit.
  blocked_main_commit().layer_tree_host = nullptr;

  if (commit_completion_waits_for_activation_) {
    // For some layer types in impl-side painting, the commit is held until the
    // sync tree is activated.  It's also possible that the sync tree has
    // already activated if there was no work to be done.
    TRACE_EVENT_INSTANT0("cc", "HoldCommit", TRACE_EVENT_SCOPE_THREAD);
    commit_completion_waits_for_activation_ = false;
    activation_completion_event_ = commit_completion_event_;
  } else {
    commit_completion_event_->Signal();
  }
  commit_completion_event_ = nullptr;

  scheduler_->DidCommit();

  // Delay this step until afer the main thread has been released as it's
  // often a good bit of work to update the tree and prepare the new frame.
  layer_tree_host_impl_->CommitComplete();

  SetInputThrottledUntilCommitOnImpl(false);

  next_frame_is_newly_committed_frame_ = true;
}

void ProxyImpl::ScheduledActionActivateSyncTree() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionActivateSyncTree");
  DCHECK(IsImplThread());
  layer_tree_host_impl_->ActivateSyncTree();
}

void ProxyImpl::ScheduledActionBeginOutputSurfaceCreation() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionBeginOutputSurfaceCreation");
  DCHECK(IsImplThread());
  channel_impl_->RequestNewOutputSurface();
}

void ProxyImpl::ScheduledActionPrepareTiles() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionPrepareTiles");
  DCHECK(IsImplThread());
  layer_tree_host_impl_->PrepareTiles();
}

void ProxyImpl::ScheduledActionInvalidateOutputSurface() {
  TRACE_EVENT0("cc", "ProxyImpl::ScheduledActionInvalidateOutputSurface");
  DCHECK(IsImplThread());
  DCHECK(layer_tree_host_impl_->output_surface());
  layer_tree_host_impl_->output_surface()->Invalidate();
}

void ProxyImpl::SendBeginMainFrameNotExpectedSoon() {
  DCHECK(IsImplThread());
  channel_impl_->BeginMainFrameNotExpectedSoon();
}

DrawResult ProxyImpl::DrawAndSwapInternal(bool forced_draw) {
  TRACE_EVENT_SYNTHETIC_DELAY("cc.DrawAndSwap");

  DCHECK(IsImplThread());
  DCHECK(layer_tree_host_impl_.get());

  base::AutoReset<bool> mark_inside(&inside_draw_, true);

  if (layer_tree_host_impl_->pending_tree()) {
    bool update_lcd_text = false;
    layer_tree_host_impl_->pending_tree()->UpdateDrawProperties(
        update_lcd_text);
  }

  // This method is called on a forced draw, regardless of whether we are able
  // to produce a frame, as the calling site on main thread is blocked until its
  // request completes, and we signal completion here. If CanDraw() is false, we
  // will indicate success=false to the caller, but we must still signal
  // completion to avoid deadlock.

  // We guard PrepareToDraw() with CanDraw() because it always returns a valid
  // frame, so can only be used when such a frame is possible. Since
  // DrawLayers() depends on the result of PrepareToDraw(), it is guarded on
  // CanDraw() as well.

  LayerTreeHostImpl::FrameData frame;
  bool draw_frame = false;

  DrawResult result;
  if (layer_tree_host_impl_->CanDraw()) {
    result = layer_tree_host_impl_->PrepareToDraw(&frame);
    draw_frame = forced_draw || result == DRAW_SUCCESS;
  } else {
    result = DRAW_ABORTED_CANT_DRAW;
  }

  if (draw_frame) {
    layer_tree_host_impl_->DrawLayers(&frame);
    result = DRAW_SUCCESS;
  } else {
    DCHECK_NE(DRAW_SUCCESS, result);
  }
  layer_tree_host_impl_->DidDrawAllLayers(frame);

  bool start_ready_animations = draw_frame;
  layer_tree_host_impl_->UpdateAnimationState(start_ready_animations);

  if (draw_frame)
    layer_tree_host_impl_->SwapBuffers(frame);

  // Tell the main thread that the the newly-commited frame was drawn.
  if (next_frame_is_newly_committed_frame_) {
    next_frame_is_newly_committed_frame_ = false;
    channel_impl_->DidCommitAndDrawFrame();
  }

  DCHECK_NE(INVALID_RESULT, result);
  return result;
}

bool ProxyImpl::IsImplThread() const {
  return task_runner_provider_->IsImplThread();
}

bool ProxyImpl::IsMainThreadBlocked() const {
  return task_runner_provider_->IsMainThreadBlocked();
}

ProxyImpl::BlockedMainCommitOnly& ProxyImpl::blocked_main_commit() {
  DCHECK(IsMainThreadBlocked() && commit_completion_event_);
  return main_thread_blocked_commit_vars_unsafe_;
}

}  // namespace cc
