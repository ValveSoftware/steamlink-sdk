// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/proxy_main.h"

#include <algorithm>
#include <string>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/trace_event/trace_event_synthetic_delay.h"
#include "cc/animation/animation_events.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/output/output_surface.h"
#include "cc/output/swap_promise.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/remote_channel_main.h"
#include "cc/trees/scoped_abort_remaining_swap_promises.h"
#include "cc/trees/threaded_channel.h"

namespace cc {

std::unique_ptr<ProxyMain> ProxyMain::CreateThreaded(
    LayerTreeHost* layer_tree_host,
    TaskRunnerProvider* task_runner_provider) {
  std::unique_ptr<ProxyMain> proxy_main(
      new ProxyMain(layer_tree_host, task_runner_provider));
  proxy_main->SetChannel(
      ThreadedChannel::Create(proxy_main.get(), task_runner_provider));
  return proxy_main;
}

std::unique_ptr<ProxyMain> ProxyMain::CreateRemote(
    RemoteProtoChannel* remote_proto_channel,
    LayerTreeHost* layer_tree_host,
    TaskRunnerProvider* task_runner_provider) {
  std::unique_ptr<ProxyMain> proxy_main(
      new ProxyMain(layer_tree_host, task_runner_provider));
  proxy_main->SetChannel(RemoteChannelMain::Create(
      remote_proto_channel, proxy_main.get(), task_runner_provider));
  return proxy_main;
}

ProxyMain::ProxyMain(LayerTreeHost* layer_tree_host,
                     TaskRunnerProvider* task_runner_provider)
    : layer_tree_host_(layer_tree_host),
      task_runner_provider_(task_runner_provider),
      layer_tree_host_id_(layer_tree_host->id()),
      max_requested_pipeline_stage_(NO_PIPELINE_STAGE),
      current_pipeline_stage_(NO_PIPELINE_STAGE),
      final_pipeline_stage_(NO_PIPELINE_STAGE),
      commit_waits_for_activation_(false),
      started_(false),
      defer_commits_(false) {
  TRACE_EVENT0("cc", "ProxyMain::ProxyMain");
  DCHECK(task_runner_provider_);
  DCHECK(IsMainThread());
}

ProxyMain::~ProxyMain() {
  TRACE_EVENT0("cc", "ProxyMain::~ProxyMain");
  DCHECK(IsMainThread());
  DCHECK(!started_);
}

void ProxyMain::SetChannel(std::unique_ptr<ChannelMain> channel_main) {
  DCHECK(!channel_main_);
  channel_main_ = std::move(channel_main);
}

void ProxyMain::DidCompleteSwapBuffers() {
  DCHECK(IsMainThread());
  layer_tree_host_->DidCompleteSwapBuffers();
}

void ProxyMain::SetRendererCapabilities(
    const RendererCapabilities& capabilities) {
  DCHECK(IsMainThread());
  renderer_capabilities_ = capabilities;
}

void ProxyMain::BeginMainFrameNotExpectedSoon() {
  TRACE_EVENT0("cc", "ProxyMain::BeginMainFrameNotExpectedSoon");
  DCHECK(IsMainThread());
  layer_tree_host_->BeginMainFrameNotExpectedSoon();
}

void ProxyMain::DidCommitAndDrawFrame() {
  DCHECK(IsMainThread());
  layer_tree_host_->DidCommitAndDrawFrame();
}

void ProxyMain::SetAnimationEvents(std::unique_ptr<AnimationEvents> events) {
  TRACE_EVENT0("cc", "ProxyMain::SetAnimationEvents");
  DCHECK(IsMainThread());
  layer_tree_host_->SetAnimationEvents(std::move(events));
}

void ProxyMain::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "ProxyMain::DidLoseOutputSurface");
  DCHECK(IsMainThread());
  layer_tree_host_->DidLoseOutputSurface();
}

void ProxyMain::RequestNewOutputSurface() {
  TRACE_EVENT0("cc", "ProxyMain::RequestNewOutputSurface");
  DCHECK(IsMainThread());
  layer_tree_host_->RequestNewOutputSurface();
}

void ProxyMain::DidInitializeOutputSurface(
    bool success,
    const RendererCapabilities& capabilities) {
  TRACE_EVENT0("cc", "ProxyMain::DidInitializeOutputSurface");
  DCHECK(IsMainThread());

  if (!success) {
    layer_tree_host_->DidFailToInitializeOutputSurface();
    return;
  }
  renderer_capabilities_ = capabilities;
  layer_tree_host_->DidInitializeOutputSurface();
}

void ProxyMain::DidCompletePageScaleAnimation() {
  DCHECK(IsMainThread());
  layer_tree_host_->DidCompletePageScaleAnimation();
}

void ProxyMain::BeginMainFrame(
    std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {
  benchmark_instrumentation::ScopedBeginFrameTask begin_frame_task(
      benchmark_instrumentation::kDoBeginFrame,
      begin_main_frame_state->begin_frame_id);

  base::TimeTicks begin_main_frame_start_time = base::TimeTicks::Now();

  TRACE_EVENT_SYNTHETIC_DELAY_BEGIN("cc.BeginMainFrame");
  DCHECK(IsMainThread());
  DCHECK_EQ(NO_PIPELINE_STAGE, current_pipeline_stage_);

  if (defer_commits_) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_DeferCommit",
                         TRACE_EVENT_SCOPE_THREAD);
    channel_main_->BeginMainFrameAbortedOnImpl(
        CommitEarlyOutReason::ABORTED_DEFERRED_COMMIT,
        begin_main_frame_start_time);
    return;
  }

  // If the commit finishes, LayerTreeHost will transfer its swap promises to
  // LayerTreeImpl. The destructor of ScopedSwapPromiseChecker aborts the
  // remaining swap promises.
  ScopedAbortRemainingSwapPromises swap_promise_checker(layer_tree_host_);

  final_pipeline_stage_ = max_requested_pipeline_stage_;
  max_requested_pipeline_stage_ = NO_PIPELINE_STAGE;

  if (!layer_tree_host_->visible()) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NotVisible", TRACE_EVENT_SCOPE_THREAD);
    channel_main_->BeginMainFrameAbortedOnImpl(
        CommitEarlyOutReason::ABORTED_NOT_VISIBLE, begin_main_frame_start_time);
    return;
  }

  if (layer_tree_host_->output_surface_lost()) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_OutputSurfaceLost",
                         TRACE_EVENT_SCOPE_THREAD);
    channel_main_->BeginMainFrameAbortedOnImpl(
        CommitEarlyOutReason::ABORTED_OUTPUT_SURFACE_LOST,
        begin_main_frame_start_time);
    return;
  }

  current_pipeline_stage_ = ANIMATE_PIPELINE_STAGE;

  layer_tree_host_->ApplyScrollAndScale(
      begin_main_frame_state->scroll_info.get());

  if (begin_main_frame_state->begin_frame_callbacks) {
    for (auto& callback : *begin_main_frame_state->begin_frame_callbacks)
      callback.Run();
  }

  layer_tree_host_->WillBeginMainFrame();

  layer_tree_host_->BeginMainFrame(begin_main_frame_state->begin_frame_args);
  layer_tree_host_->AnimateLayers(
      begin_main_frame_state->begin_frame_args.frame_time);

  // Recreate all UI resources if there were evicted UI resources when the impl
  // thread initiated the commit.
  if (begin_main_frame_state->evicted_ui_resources)
    layer_tree_host_->RecreateUIResources();

  layer_tree_host_->RequestMainFrameUpdate();
  TRACE_EVENT_SYNTHETIC_DELAY_END("cc.BeginMainFrame");

  bool can_cancel_this_commit = final_pipeline_stage_ < COMMIT_PIPELINE_STAGE &&
                                !begin_main_frame_state->evicted_ui_resources;

  current_pipeline_stage_ = UPDATE_LAYERS_PIPELINE_STAGE;
  bool should_update_layers =
      final_pipeline_stage_ >= UPDATE_LAYERS_PIPELINE_STAGE;
  bool updated = should_update_layers && layer_tree_host_->UpdateLayers();

  layer_tree_host_->WillCommit();
  devtools_instrumentation::ScopedCommitTrace commit_task(
      layer_tree_host_->id());

  current_pipeline_stage_ = COMMIT_PIPELINE_STAGE;
  if (!updated && can_cancel_this_commit) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoUpdates", TRACE_EVENT_SCOPE_THREAD);
    channel_main_->BeginMainFrameAbortedOnImpl(
        CommitEarlyOutReason::FINISHED_NO_UPDATES, begin_main_frame_start_time);

    // Although the commit is internally aborted, this is because it has been
    // detected to be a no-op.  From the perspective of an embedder, this commit
    // went through, and input should no longer be throttled, etc.
    current_pipeline_stage_ = NO_PIPELINE_STAGE;
    layer_tree_host_->CommitComplete();
    layer_tree_host_->DidBeginMainFrame();
    layer_tree_host_->BreakSwapPromises(SwapPromise::COMMIT_NO_UPDATE);
    return;
  }

  // Notify the impl thread that the main thread is ready to commit. This will
  // begin the commit process, which is blocking from the main thread's
  // point of view, but asynchronously performed on the impl thread,
  // coordinated by the Scheduler.
  {
    TRACE_EVENT0("cc", "ProxyMain::BeginMainFrame::commit");

    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);

    // This CapturePostTasks should be destroyed before CommitComplete() is
    // called since that goes out to the embedder, and we want the embedder
    // to receive its callbacks before that.
    BlockingTaskRunner::CapturePostTasks blocked(
        task_runner_provider_->blocking_main_thread_task_runner());

    bool hold_commit_for_activation = commit_waits_for_activation_;
    commit_waits_for_activation_ = false;
    CompletionEvent completion;
    channel_main_->StartCommitOnImpl(&completion, layer_tree_host_,
                                     begin_main_frame_start_time,
                                     hold_commit_for_activation);
    completion.Wait();
  }

  current_pipeline_stage_ = NO_PIPELINE_STAGE;
  layer_tree_host_->CommitComplete();
  layer_tree_host_->DidBeginMainFrame();
}

void ProxyMain::FinishAllRendering() {
  DCHECK(IsMainThread());
  DCHECK(!defer_commits_);

  // Make sure all GL drawing is finished on the impl thread.
  DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
  CompletionEvent completion;
  channel_main_->FinishAllRenderingOnImpl(&completion);
  completion.Wait();
}

bool ProxyMain::IsStarted() const {
  DCHECK(IsMainThread());
  return started_;
}

bool ProxyMain::CommitToActiveTree() const {
  // With ProxyMain, we use a pending tree and activate it once it's ready to
  // draw to allow input to modify the active tree and draw during raster.
  return false;
}

void ProxyMain::SetOutputSurface(OutputSurface* output_surface) {
  channel_main_->InitializeOutputSurfaceOnImpl(output_surface);
}

void ProxyMain::SetVisible(bool visible) {
  TRACE_EVENT1("cc", "ProxyMain::SetVisible", "visible", visible);
  channel_main_->SetVisibleOnImpl(visible);
}

const RendererCapabilities& ProxyMain::GetRendererCapabilities() const {
  DCHECK(IsMainThread());
  DCHECK(!layer_tree_host_->output_surface_lost());
  return renderer_capabilities_;
}

void ProxyMain::SetNeedsAnimate() {
  DCHECK(IsMainThread());
  if (SendCommitRequestToImplThreadIfNeeded(ANIMATE_PIPELINE_STAGE)) {
    TRACE_EVENT_INSTANT0("cc", "ProxyMain::SetNeedsAnimate",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

void ProxyMain::SetNeedsUpdateLayers() {
  DCHECK(IsMainThread());
  // If we are currently animating, make sure we also update the layers.
  if (current_pipeline_stage_ == ANIMATE_PIPELINE_STAGE) {
    final_pipeline_stage_ =
        std::max(final_pipeline_stage_, UPDATE_LAYERS_PIPELINE_STAGE);
    return;
  }
  if (SendCommitRequestToImplThreadIfNeeded(UPDATE_LAYERS_PIPELINE_STAGE)) {
    TRACE_EVENT_INSTANT0("cc", "ProxyMain::SetNeedsUpdateLayers",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

void ProxyMain::SetNeedsCommit() {
  DCHECK(IsMainThread());
  // If we are currently animating, make sure we don't skip the commit. Note
  // that requesting a commit during the layer update stage means we need to
  // schedule another full commit.
  if (current_pipeline_stage_ == ANIMATE_PIPELINE_STAGE) {
    final_pipeline_stage_ =
        std::max(final_pipeline_stage_, COMMIT_PIPELINE_STAGE);
    return;
  }
  if (SendCommitRequestToImplThreadIfNeeded(COMMIT_PIPELINE_STAGE)) {
    TRACE_EVENT_INSTANT0("cc", "ProxyMain::SetNeedsCommit",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

void ProxyMain::SetNeedsRedraw(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "ProxyMain::SetNeedsRedraw");
  DCHECK(IsMainThread());
  channel_main_->SetNeedsRedrawOnImpl(damage_rect);
}

void ProxyMain::SetNextCommitWaitsForActivation() {
  DCHECK(IsMainThread());
  commit_waits_for_activation_ = true;
}

void ProxyMain::NotifyInputThrottledUntilCommit() {
  DCHECK(IsMainThread());
  channel_main_->SetInputThrottledUntilCommitOnImpl(true);
}

void ProxyMain::SetDeferCommits(bool defer_commits) {
  DCHECK(IsMainThread());
  if (defer_commits_ == defer_commits)
    return;

  defer_commits_ = defer_commits;
  if (defer_commits_)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ProxyMain::SetDeferCommits", this);
  else
    TRACE_EVENT_ASYNC_END0("cc", "ProxyMain::SetDeferCommits", this);

  channel_main_->SetDeferCommitsOnImpl(defer_commits);
}

bool ProxyMain::CommitRequested() const {
  DCHECK(IsMainThread());
  // TODO(skyostil): Split this into something like CommitRequested() and
  // CommitInProgress().
  return current_pipeline_stage_ != NO_PIPELINE_STAGE ||
         max_requested_pipeline_stage_ >= COMMIT_PIPELINE_STAGE;
}

bool ProxyMain::BeginMainFrameRequested() const {
  DCHECK(IsMainThread());
  return max_requested_pipeline_stage_ != NO_PIPELINE_STAGE;
}

void ProxyMain::MainThreadHasStoppedFlinging() {
  DCHECK(IsMainThread());
  channel_main_->MainThreadHasStoppedFlingingOnImpl();
}

void ProxyMain::Start(
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  DCHECK(IsMainThread());
  DCHECK(layer_tree_host_->IsThreaded() || layer_tree_host_->IsRemoteServer());
  DCHECK(channel_main_);
  DCHECK(!layer_tree_host_->settings().use_external_begin_frame_source ||
         external_begin_frame_source);

  // Create LayerTreeHostImpl.
  channel_main_->SynchronouslyInitializeImpl(
      layer_tree_host_, std::move(external_begin_frame_source));

  started_ = true;
}

void ProxyMain::Stop() {
  TRACE_EVENT0("cc", "ProxyMain::Stop");
  DCHECK(IsMainThread());
  DCHECK(started_);

  channel_main_->SynchronouslyCloseImpl();

  layer_tree_host_ = nullptr;
  started_ = false;
}

void ProxyMain::SetMutator(std::unique_ptr<LayerTreeMutator> mutator) {
  TRACE_EVENT0("compositor-worker", "ThreadProxy::SetMutator");
  channel_main_->InitializeMutatorOnImpl(std::move(mutator));
}

bool ProxyMain::SupportsImplScrolling() const {
  return true;
}

bool ProxyMain::MainFrameWillHappenForTesting() {
  DCHECK(IsMainThread());
  bool main_frame_will_happen = false;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
    CompletionEvent completion;
    channel_main_->MainFrameWillHappenOnImplForTesting(&completion,
                                                       &main_frame_will_happen);
    completion.Wait();
  }
  return main_frame_will_happen;
}

void ProxyMain::ReleaseOutputSurface() {
  DCHECK(IsMainThread());
  DCHECK(layer_tree_host_->output_surface_lost());

  DebugScopedSetMainThreadBlocked main_thread_blocked(task_runner_provider_);
  CompletionEvent completion;
  channel_main_->ReleaseOutputSurfaceOnImpl(&completion);
  completion.Wait();
}

void ProxyMain::UpdateTopControlsState(TopControlsState constraints,
                                       TopControlsState current,
                                       bool animate) {
  DCHECK(IsMainThread());
  channel_main_->UpdateTopControlsStateOnImpl(constraints, current, animate);
}

bool ProxyMain::SendCommitRequestToImplThreadIfNeeded(
    CommitPipelineStage required_stage) {
  DCHECK(IsMainThread());
  DCHECK_NE(NO_PIPELINE_STAGE, required_stage);
  bool already_posted = max_requested_pipeline_stage_ != NO_PIPELINE_STAGE;
  max_requested_pipeline_stage_ =
      std::max(max_requested_pipeline_stage_, required_stage);
  if (already_posted)
    return false;
  channel_main_->SetNeedsCommitOnImpl();
  return true;
}

bool ProxyMain::IsMainThread() const {
  return task_runner_provider_->IsMainThread();
}

}  // namespace cc
