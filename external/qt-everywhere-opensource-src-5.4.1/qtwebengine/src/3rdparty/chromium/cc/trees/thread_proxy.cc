// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/thread_proxy.h"

#include <algorithm>
#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/debug/trace_event_synthetic_delay.h"
#include "base/metrics/histogram.h"
#include "cc/base/swap_promise.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/input/input_handler.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/quads/draw_quad.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/frame_time.h"

namespace {

// Measured in seconds.
const double kSmoothnessTakesPriorityExpirationDelay = 0.25;

class SwapPromiseChecker {
 public:
  explicit SwapPromiseChecker(cc::LayerTreeHost* layer_tree_host)
      : layer_tree_host_(layer_tree_host) {}

  ~SwapPromiseChecker() {
    layer_tree_host_->BreakSwapPromises(cc::SwapPromise::COMMIT_FAILS);
  }

 private:
  cc::LayerTreeHost* layer_tree_host_;
};

}  // namespace

namespace cc {

struct ThreadProxy::CommitPendingRequest {
  CompletionEvent completion;
  bool commit_pending;
};

struct ThreadProxy::SchedulerStateRequest {
  CompletionEvent completion;
  scoped_ptr<base::Value> state;
};

scoped_ptr<Proxy> ThreadProxy::Create(
    LayerTreeHost* layer_tree_host,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  return make_scoped_ptr(new ThreadProxy(layer_tree_host, impl_task_runner))
      .PassAs<Proxy>();
}

ThreadProxy::ThreadProxy(
    LayerTreeHost* layer_tree_host,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner)
    : Proxy(impl_task_runner),
      main_thread_only_vars_unsafe_(this, layer_tree_host->id()),
      main_thread_or_blocked_vars_unsafe_(layer_tree_host),
      compositor_thread_vars_unsafe_(this, layer_tree_host->id()) {
  TRACE_EVENT0("cc", "ThreadProxy::ThreadProxy");
  DCHECK(IsMainThread());
  DCHECK(this->layer_tree_host());
}

ThreadProxy::MainThreadOnly::MainThreadOnly(ThreadProxy* proxy,
                                            int layer_tree_host_id)
    : layer_tree_host_id(layer_tree_host_id),
      animate_requested(false),
      commit_requested(false),
      commit_request_sent_to_impl_thread(false),
      started(false),
      manage_tiles_pending(false),
      can_cancel_commit(true),
      defer_commits(false),
      weak_factory(proxy) {}

ThreadProxy::MainThreadOnly::~MainThreadOnly() {}

ThreadProxy::MainThreadOrBlockedMainThread::MainThreadOrBlockedMainThread(
    LayerTreeHost* host)
    : layer_tree_host(host),
      commit_waits_for_activation(false),
      main_thread_inside_commit(false) {}

ThreadProxy::MainThreadOrBlockedMainThread::~MainThreadOrBlockedMainThread() {}

PrioritizedResourceManager*
ThreadProxy::MainThreadOrBlockedMainThread::contents_texture_manager() {
  return layer_tree_host->contents_texture_manager();
}

ThreadProxy::CompositorThreadOnly::CompositorThreadOnly(ThreadProxy* proxy,
                                                        int layer_tree_host_id)
    : layer_tree_host_id(layer_tree_host_id),
      contents_texture_manager(NULL),
      commit_completion_event(NULL),
      completion_event_for_commit_held_on_tree_activation(NULL),
      next_frame_is_newly_committed_frame(false),
      inside_draw(false),
      input_throttled_until_commit(false),
      animations_frozen_until_next_draw(false),
      did_commit_after_animating(false),
      smoothness_priority_expiration_notifier(
          proxy->ImplThreadTaskRunner(),
          base::Bind(&ThreadProxy::RenewTreePriority, base::Unretained(proxy)),
          base::TimeDelta::FromMilliseconds(
              kSmoothnessTakesPriorityExpirationDelay * 1000)),
      weak_factory(proxy) {
}

ThreadProxy::CompositorThreadOnly::~CompositorThreadOnly() {}

ThreadProxy::~ThreadProxy() {
  TRACE_EVENT0("cc", "ThreadProxy::~ThreadProxy");
  DCHECK(IsMainThread());
  DCHECK(!main().started);
}

void ThreadProxy::FinishAllRendering() {
  DCHECK(Proxy::IsMainThread());
  DCHECK(!main().defer_commits);

  // Make sure all GL drawing is finished on the impl thread.
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::FinishAllRenderingOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion));
  completion.Wait();
}

bool ThreadProxy::IsStarted() const {
  DCHECK(Proxy::IsMainThread());
  return main().started;
}

void ThreadProxy::SetLayerTreeHostClientReady() {
  TRACE_EVENT0("cc", "ThreadProxy::SetLayerTreeHostClientReady");
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetLayerTreeHostClientReadyOnImplThread,
                 impl_thread_weak_ptr_));
}

void ThreadProxy::SetLayerTreeHostClientReadyOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::SetLayerTreeHostClientReadyOnImplThread");
  impl().scheduler->SetCanStart();
}

void ThreadProxy::SetVisible(bool visible) {
  TRACE_EVENT0("cc", "ThreadProxy::SetVisible");
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);

  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetVisibleOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion,
                 visible));
  completion.Wait();
}

void ThreadProxy::SetVisibleOnImplThread(CompletionEvent* completion,
                                         bool visible) {
  TRACE_EVENT0("cc", "ThreadProxy::SetVisibleOnImplThread");
  impl().layer_tree_host_impl->SetVisible(visible);
  impl().scheduler->SetVisible(visible);
  UpdateBackgroundAnimateTicking();
  completion->Signal();
}

void ThreadProxy::UpdateBackgroundAnimateTicking() {
  bool should_background_tick =
      !impl().scheduler->WillDrawIfNeeded() &&
      impl().layer_tree_host_impl->active_tree()->root_layer();
  impl().layer_tree_host_impl->UpdateBackgroundAnimateTicking(
      should_background_tick);
  if (should_background_tick)
    impl().animations_frozen_until_next_draw = false;
}

void ThreadProxy::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "ThreadProxy::DidLoseOutputSurface");
  DCHECK(IsMainThread());
  layer_tree_host()->DidLoseOutputSurface();

  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    // Return lost resources to their owners immediately.
    BlockingTaskRunner::CapturePostTasks blocked;

    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::DeleteContentsTexturesOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion));
    completion.Wait();
  }
}

void ThreadProxy::CreateAndInitializeOutputSurface() {
  TRACE_EVENT0("cc", "ThreadProxy::DoCreateAndInitializeOutputSurface");
  DCHECK(IsMainThread());

  scoped_ptr<OutputSurface> output_surface =
      layer_tree_host()->CreateOutputSurface();

  if (output_surface) {
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::InitializeOutputSurfaceOnImplThread,
                   impl_thread_weak_ptr_,
                   base::Passed(&output_surface)));
    return;
  }

  DidInitializeOutputSurface(false, RendererCapabilities());
}

void ThreadProxy::DidInitializeOutputSurface(
    bool success,
    const RendererCapabilities& capabilities) {
  TRACE_EVENT0("cc", "ThreadProxy::DidInitializeOutputSurface");
  DCHECK(IsMainThread());
  main().renderer_capabilities_main_thread_copy = capabilities;
  layer_tree_host()->OnCreateAndInitializeOutputSurfaceAttempted(success);

  if (!success) {
    Proxy::MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::CreateAndInitializeOutputSurface,
                   main_thread_weak_ptr_));
  }
}

void ThreadProxy::SetRendererCapabilitiesMainThreadCopy(
    const RendererCapabilities& capabilities) {
  main().renderer_capabilities_main_thread_copy = capabilities;
}

void ThreadProxy::SendCommitRequestToImplThreadIfNeeded() {
  DCHECK(IsMainThread());
  if (main().commit_request_sent_to_impl_thread)
    return;
  main().commit_request_sent_to_impl_thread = true;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetNeedsCommitOnImplThread,
                 impl_thread_weak_ptr_));
}

const RendererCapabilities& ThreadProxy::GetRendererCapabilities() const {
  DCHECK(IsMainThread());
  DCHECK(!layer_tree_host()->output_surface_lost());
  return main().renderer_capabilities_main_thread_copy;
}

void ThreadProxy::SetNeedsAnimate() {
  DCHECK(IsMainThread());
  if (main().animate_requested)
    return;

  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsAnimate");
  main().animate_requested = true;
  SendCommitRequestToImplThreadIfNeeded();
}

void ThreadProxy::SetNeedsUpdateLayers() {
  DCHECK(IsMainThread());

  if (main().commit_request_sent_to_impl_thread)
    return;
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsUpdateLayers");

  SendCommitRequestToImplThreadIfNeeded();
}

void ThreadProxy::SetNeedsCommit() {
  DCHECK(IsMainThread());
  // Unconditionally set here to handle SetNeedsCommit calls during a commit.
  main().can_cancel_commit = false;

  if (main().commit_requested)
    return;
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsCommit");
  main().commit_requested = true;

  SendCommitRequestToImplThreadIfNeeded();
}

void ThreadProxy::UpdateRendererCapabilitiesOnImplThread() {
  DCHECK(IsImplThread());
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetRendererCapabilitiesMainThreadCopy,
                 main_thread_weak_ptr_,
                 impl()
                     .layer_tree_host_impl->GetRendererCapabilities()
                     .MainThreadCapabilities()));
}

void ThreadProxy::DidLoseOutputSurfaceOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::DidLoseOutputSurfaceOnImplThread");
  DCHECK(IsImplThread());
  CheckOutputSurfaceStatusOnImplThread();
}

void ThreadProxy::CheckOutputSurfaceStatusOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::CheckOutputSurfaceStatusOnImplThread");
  DCHECK(IsImplThread());
  if (!impl().layer_tree_host_impl->IsContextLost())
    return;
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::DidLoseOutputSurface, main_thread_weak_ptr_));
  impl().scheduler->DidLoseOutputSurface();
}

void ThreadProxy::CommitVSyncParameters(base::TimeTicks timebase,
                                        base::TimeDelta interval) {
  impl().scheduler->CommitVSyncParameters(timebase, interval);
}

void ThreadProxy::SetEstimatedParentDrawTime(base::TimeDelta draw_time) {
  impl().scheduler->SetEstimatedParentDrawTime(draw_time);
}

void ThreadProxy::SetMaxSwapsPendingOnImplThread(int max) {
  impl().scheduler->SetMaxSwapsPending(max);
}

void ThreadProxy::DidSwapBuffersOnImplThread() {
  impl().scheduler->DidSwapBuffers();
}

void ThreadProxy::DidSwapBuffersCompleteOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::DidSwapBuffersCompleteOnImplThread");
  DCHECK(IsImplThread());
  impl().scheduler->DidSwapBuffersComplete();
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::DidCompleteSwapBuffers, main_thread_weak_ptr_));
}

void ThreadProxy::SetNeedsBeginFrame(bool enable) {
  TRACE_EVENT1("cc", "ThreadProxy::SetNeedsBeginFrame", "enable", enable);
  impl().layer_tree_host_impl->SetNeedsBeginFrame(enable);
  UpdateBackgroundAnimateTicking();
}

void ThreadProxy::BeginFrame(const BeginFrameArgs& args) {
  impl().scheduler->BeginFrame(args);
}

void ThreadProxy::WillBeginImplFrame(const BeginFrameArgs& args) {
  impl().layer_tree_host_impl->WillBeginImplFrame(args);
}

void ThreadProxy::OnCanDrawStateChanged(bool can_draw) {
  TRACE_EVENT1(
      "cc", "ThreadProxy::OnCanDrawStateChanged", "can_draw", can_draw);
  DCHECK(IsImplThread());
  impl().scheduler->SetCanDraw(can_draw);
  UpdateBackgroundAnimateTicking();
}

void ThreadProxy::NotifyReadyToActivate() {
  TRACE_EVENT0("cc", "ThreadProxy::NotifyReadyToActivate");
  impl().scheduler->NotifyReadyToActivate();
}

void ThreadProxy::SetNeedsCommitOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsCommitOnImplThread");
  DCHECK(IsImplThread());
  impl().scheduler->SetNeedsCommit();
}

void ThreadProxy::PostAnimationEventsToMainThreadOnImplThread(
    scoped_ptr<AnimationEventsVector> events) {
  TRACE_EVENT0("cc",
               "ThreadProxy::PostAnimationEventsToMainThreadOnImplThread");
  DCHECK(IsImplThread());
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetAnimationEvents,
                 main_thread_weak_ptr_,
                 base::Passed(&events)));
}

bool ThreadProxy::ReduceContentsTextureMemoryOnImplThread(size_t limit_bytes,
                                                          int priority_cutoff) {
  DCHECK(IsImplThread());

  if (!impl().contents_texture_manager)
    return false;
  if (!impl().layer_tree_host_impl->resource_provider())
    return false;

  bool reduce_result =
      impl().contents_texture_manager->ReduceMemoryOnImplThread(
          limit_bytes,
          priority_cutoff,
          impl().layer_tree_host_impl->resource_provider());
  if (!reduce_result)
    return false;

  // The texture upload queue may reference textures that were just purged,
  // clear them from the queue.
  if (impl().current_resource_update_controller) {
    impl()
        .current_resource_update_controller->DiscardUploadsToEvictedResources();
  }
  return true;
}

bool ThreadProxy::IsInsideDraw() { return impl().inside_draw; }

void ThreadProxy::SetNeedsRedraw(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsRedraw");
  DCHECK(IsMainThread());
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetNeedsRedrawRectOnImplThread,
                 impl_thread_weak_ptr_,
                 damage_rect));
}

void ThreadProxy::SetNextCommitWaitsForActivation() {
  DCHECK(IsMainThread());
  DCHECK(!blocked_main().main_thread_inside_commit);
  blocked_main().commit_waits_for_activation = true;
}

void ThreadProxy::SetDeferCommits(bool defer_commits) {
  DCHECK(IsMainThread());
  DCHECK_NE(main().defer_commits, defer_commits);
  main().defer_commits = defer_commits;

  if (main().defer_commits)
    TRACE_EVENT_ASYNC_BEGIN0("cc", "ThreadProxy::SetDeferCommits", this);
  else
    TRACE_EVENT_ASYNC_END0("cc", "ThreadProxy::SetDeferCommits", this);

  if (!main().defer_commits && main().pending_deferred_commit)
    Proxy::MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::BeginMainFrame,
                   main_thread_weak_ptr_,
                   base::Passed(&main().pending_deferred_commit)));
}

bool ThreadProxy::CommitRequested() const {
  DCHECK(IsMainThread());
  return main().commit_requested;
}

bool ThreadProxy::BeginMainFrameRequested() const {
  DCHECK(IsMainThread());
  return main().commit_request_sent_to_impl_thread;
}

void ThreadProxy::SetNeedsRedrawOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsRedrawOnImplThread");
  DCHECK(IsImplThread());
  impl().scheduler->SetNeedsRedraw();
}

void ThreadProxy::SetNeedsAnimateOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::SetNeedsAnimateOnImplThread");
  DCHECK(IsImplThread());
  impl().scheduler->SetNeedsAnimate();
}

void ThreadProxy::SetNeedsManageTilesOnImplThread() {
  DCHECK(IsImplThread());
  impl().scheduler->SetNeedsManageTiles();
}

void ThreadProxy::SetNeedsRedrawRectOnImplThread(const gfx::Rect& damage_rect) {
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl->SetViewportDamage(damage_rect);
  SetNeedsRedrawOnImplThread();
}

void ThreadProxy::SetSwapUsedIncompleteTileOnImplThread(
    bool used_incomplete_tile) {
  DCHECK(IsImplThread());
  if (used_incomplete_tile) {
    TRACE_EVENT_INSTANT0("cc",
                         "ThreadProxy::SetSwapUsedIncompleteTileOnImplThread",
                         TRACE_EVENT_SCOPE_THREAD);
  }
  impl().scheduler->SetSwapUsedIncompleteTile(used_incomplete_tile);
}

void ThreadProxy::DidInitializeVisibleTileOnImplThread() {
  TRACE_EVENT0("cc", "ThreadProxy::DidInitializeVisibleTileOnImplThread");
  DCHECK(IsImplThread());
  impl().scheduler->SetNeedsRedraw();
}

void ThreadProxy::MainThreadHasStoppedFlinging() {
  DCHECK(IsMainThread());
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::MainThreadHasStoppedFlingingOnImplThread,
                 impl_thread_weak_ptr_));
}

void ThreadProxy::MainThreadHasStoppedFlingingOnImplThread() {
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl->MainThreadHasStoppedFlinging();
}

void ThreadProxy::NotifyInputThrottledUntilCommit() {
  DCHECK(IsMainThread());
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetInputThrottledUntilCommitOnImplThread,
                 impl_thread_weak_ptr_,
                 true));
}

void ThreadProxy::SetInputThrottledUntilCommitOnImplThread(bool is_throttled) {
  DCHECK(IsImplThread());
  if (is_throttled == impl().input_throttled_until_commit)
    return;
  impl().input_throttled_until_commit = is_throttled;
  RenewTreePriority();
}

LayerTreeHost* ThreadProxy::layer_tree_host() {
  return blocked_main().layer_tree_host;
}

const LayerTreeHost* ThreadProxy::layer_tree_host() const {
  return blocked_main().layer_tree_host;
}

ThreadProxy::MainThreadOnly& ThreadProxy::main() {
  DCHECK(IsMainThread());
  return main_thread_only_vars_unsafe_;
}
const ThreadProxy::MainThreadOnly& ThreadProxy::main() const {
  DCHECK(IsMainThread());
  return main_thread_only_vars_unsafe_;
}

ThreadProxy::MainThreadOrBlockedMainThread& ThreadProxy::blocked_main() {
  DCHECK(IsMainThread() || IsMainThreadBlocked());
  return main_thread_or_blocked_vars_unsafe_;
}

const ThreadProxy::MainThreadOrBlockedMainThread& ThreadProxy::blocked_main()
    const {
  DCHECK(IsMainThread() || IsMainThreadBlocked());
  return main_thread_or_blocked_vars_unsafe_;
}

ThreadProxy::CompositorThreadOnly& ThreadProxy::impl() {
  DCHECK(IsImplThread());
  return compositor_thread_vars_unsafe_;
}

const ThreadProxy::CompositorThreadOnly& ThreadProxy::impl() const {
  DCHECK(IsImplThread());
  return compositor_thread_vars_unsafe_;
}

void ThreadProxy::Start() {
  DCHECK(IsMainThread());
  DCHECK(Proxy::HasImplThread());

  // Create LayerTreeHostImpl.
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::InitializeImplOnImplThread,
                 base::Unretained(this),
                 &completion));
  completion.Wait();

  main_thread_weak_ptr_ = main().weak_factory.GetWeakPtr();

  main().started = true;
}

void ThreadProxy::Stop() {
  TRACE_EVENT0("cc", "ThreadProxy::Stop");
  DCHECK(IsMainThread());
  DCHECK(main().started);

  // Synchronously finishes pending GL operations and deletes the impl.
  // The two steps are done as separate post tasks, so that tasks posted
  // by the GL implementation due to the Finish can be executed by the
  // renderer before shutting it down.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::FinishGLOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion));
    completion.Wait();
  }
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::LayerTreeHostClosedOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion));
    completion.Wait();
  }

  main().weak_factory.InvalidateWeakPtrs();
  blocked_main().layer_tree_host = NULL;
  main().started = false;
}

void ThreadProxy::ForceSerializeOnSwapBuffers() {
  DebugScopedSetMainThreadBlocked main_thread_blocked(this);
  CompletionEvent completion;
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::ForceSerializeOnSwapBuffersOnImplThread,
                 impl_thread_weak_ptr_,
                 &completion));
  completion.Wait();
}

void ThreadProxy::ForceSerializeOnSwapBuffersOnImplThread(
    CompletionEvent* completion) {
  if (impl().layer_tree_host_impl->renderer())
    impl().layer_tree_host_impl->renderer()->DoNoOp();
  completion->Signal();
}

void ThreadProxy::SetDebugState(const LayerTreeDebugState& debug_state) {
  Proxy::ImplThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::SetDebugStateOnImplThread,
                 impl_thread_weak_ptr_,
                 debug_state));
}

void ThreadProxy::SetDebugStateOnImplThread(
    const LayerTreeDebugState& debug_state) {
  DCHECK(IsImplThread());
  impl().scheduler->SetContinuousPainting(debug_state.continuous_painting);
}

void ThreadProxy::FinishAllRenderingOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::FinishAllRenderingOnImplThread");
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl->FinishAllRendering();
  completion->Signal();
}

void ThreadProxy::ScheduledActionSendBeginMainFrame() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionSendBeginMainFrame");
  scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state(
      new BeginMainFrameAndCommitState);
  begin_main_frame_state->monotonic_frame_begin_time =
      impl().layer_tree_host_impl->CurrentFrameTimeTicks();
  begin_main_frame_state->scroll_info =
      impl().layer_tree_host_impl->ProcessScrollDeltas();

  if (!impl().layer_tree_host_impl->settings().impl_side_painting) {
    DCHECK_GT(impl().layer_tree_host_impl->memory_allocation_limit_bytes(), 0u);
  }
  begin_main_frame_state->memory_allocation_limit_bytes =
      impl().layer_tree_host_impl->memory_allocation_limit_bytes();
  begin_main_frame_state->memory_allocation_priority_cutoff =
      impl().layer_tree_host_impl->memory_allocation_priority_cutoff();
  begin_main_frame_state->evicted_ui_resources =
      impl().layer_tree_host_impl->EvictedUIResourcesExist();
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::BeginMainFrame,
                 main_thread_weak_ptr_,
                 base::Passed(&begin_main_frame_state)));
  devtools_instrumentation::DidRequestMainThreadFrame(
      impl().layer_tree_host_id);
  impl().timing_history.DidBeginMainFrame();
}

void ThreadProxy::BeginMainFrame(
    scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) {
  TRACE_EVENT0("cc", "ThreadProxy::BeginMainFrame");
  TRACE_EVENT_SYNTHETIC_DELAY_BEGIN("cc.BeginMainFrame");
  DCHECK(IsMainThread());

  if (main().defer_commits) {
    main().pending_deferred_commit = begin_main_frame_state.Pass();
    layer_tree_host()->DidDeferCommit();
    TRACE_EVENT_INSTANT0(
        "cc", "EarlyOut_DeferCommits", TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  // If the commit finishes, LayerTreeHost will transfer its swap promises to
  // LayerTreeImpl. The destructor of SwapPromiseChecker checks LayerTressHost's
  // swap promises.
  SwapPromiseChecker swap_promise_checker(layer_tree_host());

  main().commit_requested = false;
  main().commit_request_sent_to_impl_thread = false;
  main().animate_requested = false;

  if (!layer_tree_host()->visible()) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NotVisible", TRACE_EVENT_SCOPE_THREAD);
    bool did_handle = false;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::BeginMainFrameAbortedOnImplThread,
                   impl_thread_weak_ptr_,
                   did_handle));
    return;
  }

  if (layer_tree_host()->output_surface_lost()) {
    TRACE_EVENT_INSTANT0(
        "cc", "EarlyOut_OutputSurfaceLost", TRACE_EVENT_SCOPE_THREAD);
    bool did_handle = false;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::BeginMainFrameAbortedOnImplThread,
                   impl_thread_weak_ptr_,
                   did_handle));
    return;
  }

  // Do not notify the impl thread of commit requests that occur during
  // the apply/animate/layout part of the BeginMainFrameAndCommit process since
  // those commit requests will get painted immediately. Once we have done
  // the paint, main().commit_requested will be set to false to allow new commit
  // requests to be scheduled.
  // On the other hand, the animate_requested flag should remain cleared
  // here so that any animation requests generated by the apply or animate
  // callbacks will trigger another frame.
  main().commit_requested = true;
  main().commit_request_sent_to_impl_thread = true;

  layer_tree_host()->ApplyScrollAndScale(*begin_main_frame_state->scroll_info);

  layer_tree_host()->WillBeginMainFrame();

  layer_tree_host()->UpdateClientAnimations(
      begin_main_frame_state->monotonic_frame_begin_time);
  layer_tree_host()->AnimateLayers(
      begin_main_frame_state->monotonic_frame_begin_time);
  blocked_main().last_monotonic_frame_begin_time =
      begin_main_frame_state->monotonic_frame_begin_time;

  // Unlink any backings that the impl thread has evicted, so that we know to
  // re-paint them in UpdateLayers.
  if (blocked_main().contents_texture_manager()) {
    blocked_main().contents_texture_manager()->UnlinkAndClearEvictedBackings();

    blocked_main().contents_texture_manager()->SetMaxMemoryLimitBytes(
        begin_main_frame_state->memory_allocation_limit_bytes);
    blocked_main().contents_texture_manager()->SetExternalPriorityCutoff(
        begin_main_frame_state->memory_allocation_priority_cutoff);
  }

  // Recreate all UI resources if there were evicted UI resources when the impl
  // thread initiated the commit.
  if (begin_main_frame_state->evicted_ui_resources)
    layer_tree_host()->RecreateUIResources();

  layer_tree_host()->Layout();
  TRACE_EVENT_SYNTHETIC_DELAY_END("cc.BeginMainFrame");

  // Clear the commit flag after updating animations and layout here --- objects
  // that only layout when painted will trigger another SetNeedsCommit inside
  // UpdateLayers.
  main().commit_requested = false;
  main().commit_request_sent_to_impl_thread = false;
  bool can_cancel_this_commit =
      main().can_cancel_commit && !begin_main_frame_state->evicted_ui_resources;
  main().can_cancel_commit = true;

  scoped_ptr<ResourceUpdateQueue> queue =
      make_scoped_ptr(new ResourceUpdateQueue);

  bool updated = layer_tree_host()->UpdateLayers(queue.get());

  layer_tree_host()->WillCommit();

  // Before calling animate, we set main().animate_requested to false. If it is
  // true now, it means SetNeedAnimate was called again, but during a state when
  // main().commit_request_sent_to_impl_thread = true. We need to force that
  // call to happen again now so that the commit request is sent to the impl
  // thread.
  if (main().animate_requested) {
    // Forces SetNeedsAnimate to consider posting a commit task.
    main().animate_requested = false;
    SetNeedsAnimate();
  }

  if (!updated && can_cancel_this_commit) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoUpdates", TRACE_EVENT_SCOPE_THREAD);
    bool did_handle = true;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::BeginMainFrameAbortedOnImplThread,
                   impl_thread_weak_ptr_,
                   did_handle));

    // Although the commit is internally aborted, this is because it has been
    // detected to be a no-op.  From the perspective of an embedder, this commit
    // went through, and input should no longer be throttled, etc.
    layer_tree_host()->CommitComplete();
    layer_tree_host()->DidBeginMainFrame();
    return;
  }

  // Notify the impl thread that the main thread is ready to commit. This will
  // begin the commit process, which is blocking from the main thread's
  // point of view, but asynchronously performed on the impl thread,
  // coordinated by the Scheduler.
  {
    TRACE_EVENT0("cc", "ThreadProxy::BeginMainFrame::commit");

    DebugScopedSetMainThreadBlocked main_thread_blocked(this);

    // This CapturePostTasks should be destroyed before CommitComplete() is
    // called since that goes out to the embedder, and we want the embedder
    // to receive its callbacks before that.
    BlockingTaskRunner::CapturePostTasks blocked;

    CompletionEvent completion;
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::StartCommitOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion,
                   queue.release()));
    completion.Wait();

    RenderingStatsInstrumentation* stats_instrumentation =
        layer_tree_host()->rendering_stats_instrumentation();
    BenchmarkInstrumentation::IssueMainThreadRenderingStatsEvent(
        stats_instrumentation->main_thread_rendering_stats());
    stats_instrumentation->AccumulateAndClearMainThreadStats();
  }

  layer_tree_host()->CommitComplete();
  layer_tree_host()->DidBeginMainFrame();
}

void ThreadProxy::StartCommitOnImplThread(CompletionEvent* completion,
                                          ResourceUpdateQueue* raw_queue) {
  TRACE_EVENT0("cc", "ThreadProxy::StartCommitOnImplThread");
  DCHECK(!impl().commit_completion_event);
  DCHECK(IsImplThread() && IsMainThreadBlocked());
  DCHECK(impl().scheduler);
  DCHECK(impl().scheduler->CommitPending());

  if (!impl().layer_tree_host_impl) {
    TRACE_EVENT_INSTANT0(
        "cc", "EarlyOut_NoLayerTree", TRACE_EVENT_SCOPE_THREAD);
    completion->Signal();
    return;
  }

  // Ideally, we should inform to impl thread when BeginMainFrame is started.
  // But, we can avoid a PostTask in here.
  impl().scheduler->NotifyBeginMainFrameStarted();

  scoped_ptr<ResourceUpdateQueue> queue(raw_queue);

  if (impl().contents_texture_manager) {
    DCHECK_EQ(impl().contents_texture_manager,
              blocked_main().contents_texture_manager());
  } else {
    // Cache this pointer that was created on the main thread side to avoid a
    // data race between creating it and using it on the compositor thread.
    impl().contents_texture_manager = blocked_main().contents_texture_manager();
  }

  if (impl().contents_texture_manager) {
    if (impl().contents_texture_manager->LinkedEvictedBackingsExist()) {
      // Clear any uploads we were making to textures linked to evicted
      // resources
      queue->ClearUploadsToEvictedResources();
      // Some textures in the layer tree are invalid. Kick off another commit
      // to fill them again.
      SetNeedsCommitOnImplThread();
    }

    impl().contents_texture_manager->PushTexturePrioritiesToBackings();
  }

  impl().commit_completion_event = completion;
  impl().current_resource_update_controller = ResourceUpdateController::Create(
      this,
      Proxy::ImplThreadTaskRunner(),
      queue.Pass(),
      impl().layer_tree_host_impl->resource_provider());
  impl().current_resource_update_controller->PerformMoreUpdates(
      impl().scheduler->AnticipatedDrawTime());
}

void ThreadProxy::BeginMainFrameAbortedOnImplThread(bool did_handle) {
  TRACE_EVENT0("cc", "ThreadProxy::BeginMainFrameAbortedOnImplThread");
  DCHECK(IsImplThread());
  DCHECK(impl().scheduler);
  DCHECK(impl().scheduler->CommitPending());
  DCHECK(!impl().layer_tree_host_impl->pending_tree());

  if (did_handle)
    SetInputThrottledUntilCommitOnImplThread(false);
  impl().layer_tree_host_impl->BeginMainFrameAborted(did_handle);
  impl().scheduler->BeginMainFrameAborted(did_handle);
}

void ThreadProxy::ScheduledActionAnimate() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionAnimate");
  DCHECK(IsImplThread());

  if (!impl().animations_frozen_until_next_draw) {
    impl().animation_time =
        impl().layer_tree_host_impl->CurrentFrameTimeTicks();
  }
  impl().layer_tree_host_impl->Animate(impl().animation_time);
  impl().did_commit_after_animating = false;
}

void ThreadProxy::ScheduledActionCommit() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionCommit");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());
  DCHECK(impl().commit_completion_event);
  DCHECK(impl().current_resource_update_controller);

  // Complete all remaining texture updates.
  impl().current_resource_update_controller->Finalize();
  impl().current_resource_update_controller.reset();

  if (impl().animations_frozen_until_next_draw) {
    impl().animation_time = std::max(
        impl().animation_time, blocked_main().last_monotonic_frame_begin_time);
  }
  impl().did_commit_after_animating = true;

  blocked_main().main_thread_inside_commit = true;
  impl().layer_tree_host_impl->BeginCommit();
  layer_tree_host()->BeginCommitOnImplThread(impl().layer_tree_host_impl.get());
  layer_tree_host()->FinishCommitOnImplThread(
      impl().layer_tree_host_impl.get());
  blocked_main().main_thread_inside_commit = false;

  bool hold_commit = layer_tree_host()->settings().impl_side_painting &&
                     blocked_main().commit_waits_for_activation;
  blocked_main().commit_waits_for_activation = false;

  if (hold_commit) {
    // For some layer types in impl-side painting, the commit is held until
    // the pending tree is activated.  It's also possible that the
    // pending tree has already activated if there was no work to be done.
    TRACE_EVENT_INSTANT0("cc", "HoldCommit", TRACE_EVENT_SCOPE_THREAD);
    impl().completion_event_for_commit_held_on_tree_activation =
        impl().commit_completion_event;
    impl().commit_completion_event = NULL;
  } else {
    impl().commit_completion_event->Signal();
    impl().commit_completion_event = NULL;
  }

  // Delay this step until afer the main thread has been released as it's
  // often a good bit of work to update the tree and prepare the new frame.
  impl().layer_tree_host_impl->CommitComplete();

  SetInputThrottledUntilCommitOnImplThread(false);

  UpdateBackgroundAnimateTicking();

  impl().next_frame_is_newly_committed_frame = true;

  impl().timing_history.DidCommit();
}

void ThreadProxy::ScheduledActionUpdateVisibleTiles() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionUpdateVisibleTiles");
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl->UpdateVisibleTiles();
}

void ThreadProxy::ScheduledActionActivatePendingTree() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionActivatePendingTree");
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl->ActivatePendingTree();
}

void ThreadProxy::ScheduledActionBeginOutputSurfaceCreation() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionBeginOutputSurfaceCreation");
  DCHECK(IsImplThread());
  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::CreateAndInitializeOutputSurface,
                 main_thread_weak_ptr_));
}

DrawResult ThreadProxy::DrawSwapInternal(bool forced_draw) {
  TRACE_EVENT_SYNTHETIC_DELAY("cc.DrawAndSwap");
  DrawResult result;

  DCHECK(IsImplThread());
  DCHECK(impl().layer_tree_host_impl.get());

  impl().timing_history.DidStartDrawing();
  base::TimeDelta draw_duration_estimate = DrawDurationEstimate();
  base::AutoReset<bool> mark_inside(&impl().inside_draw, true);

  if (impl().did_commit_after_animating) {
    impl().layer_tree_host_impl->Animate(impl().animation_time);
    impl().did_commit_after_animating = false;
  }

  if (impl().layer_tree_host_impl->pending_tree())
    impl().layer_tree_host_impl->pending_tree()->UpdateDrawProperties();

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

  if (impl().layer_tree_host_impl->CanDraw()) {
    result = impl().layer_tree_host_impl->PrepareToDraw(&frame);
    draw_frame = forced_draw || result == DRAW_SUCCESS;
  } else {
    result = DRAW_ABORTED_CANT_DRAW;
  }

  if (draw_frame) {
    impl().layer_tree_host_impl->DrawLayers(
        &frame, impl().scheduler->LastBeginImplFrameTime());
    result = DRAW_SUCCESS;
    impl().animations_frozen_until_next_draw = false;
  } else if (result == DRAW_ABORTED_CHECKERBOARD_ANIMATIONS &&
             !impl().layer_tree_host_impl->settings().impl_side_painting) {
    // Without impl-side painting, the animated layer that is checkerboarding
    // will continue to checkerboard until the next commit. If this layer
    // continues to move during the commit, it may continue to checkerboard
    // after the commit since the region rasterized during the commit will not
    // match the region that is currently visible; eventually this
    // checkerboarding will be displayed when we force a draw. To avoid this,
    // we freeze animations until we successfully draw.
    impl().animations_frozen_until_next_draw = true;
  } else {
    DCHECK_NE(DRAW_SUCCESS, result);
  }
  impl().layer_tree_host_impl->DidDrawAllLayers(frame);

  bool start_ready_animations = draw_frame;
  impl().layer_tree_host_impl->UpdateAnimationState(start_ready_animations);

  if (draw_frame) {
    bool did_request_swap = impl().layer_tree_host_impl->SwapBuffers(frame);

    // We don't know if we have incomplete tiles if we didn't actually swap.
    if (did_request_swap) {
      DCHECK(!frame.has_no_damage);
      SetSwapUsedIncompleteTileOnImplThread(frame.contains_incomplete_tile);
    }
  }

  // Tell the main thread that the the newly-commited frame was drawn.
  if (impl().next_frame_is_newly_committed_frame) {
    impl().next_frame_is_newly_committed_frame = false;
    Proxy::MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::DidCommitAndDrawFrame, main_thread_weak_ptr_));
  }

  if (draw_frame)
    CheckOutputSurfaceStatusOnImplThread();

  if (result == DRAW_SUCCESS) {
    base::TimeDelta draw_duration = impl().timing_history.DidFinishDrawing();

    base::TimeDelta draw_duration_overestimate;
    base::TimeDelta draw_duration_underestimate;
    if (draw_duration > draw_duration_estimate)
      draw_duration_underestimate = draw_duration - draw_duration_estimate;
    else
      draw_duration_overestimate = draw_duration_estimate - draw_duration;
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDuration",
                               draw_duration,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDurationUnderestimate",
                               draw_duration_underestimate,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer.DrawDurationOverestimate",
                               draw_duration_overestimate,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(100),
                               50);
  }

  DCHECK_NE(INVALID_RESULT, result);
  return result;
}

void ThreadProxy::ScheduledActionManageTiles() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionManageTiles");
  DCHECK(impl().layer_tree_host_impl->settings().impl_side_painting);
  impl().layer_tree_host_impl->ManageTiles();
}

DrawResult ThreadProxy::ScheduledActionDrawAndSwapIfPossible() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionDrawAndSwap");

  // SchedulerStateMachine::DidDrawIfPossibleCompleted isn't set up to
  // handle DRAW_ABORTED_CANT_DRAW.  Moreover, the scheduler should
  // never generate this call when it can't draw.
  DCHECK(impl().layer_tree_host_impl->CanDraw());

  bool forced_draw = false;
  return DrawSwapInternal(forced_draw);
}

DrawResult ThreadProxy::ScheduledActionDrawAndSwapForced() {
  TRACE_EVENT0("cc", "ThreadProxy::ScheduledActionDrawAndSwapForced");
  bool forced_draw = true;
  return DrawSwapInternal(forced_draw);
}

void ThreadProxy::DidAnticipatedDrawTimeChange(base::TimeTicks time) {
  if (impl().current_resource_update_controller)
    impl().current_resource_update_controller->PerformMoreUpdates(time);
}

base::TimeDelta ThreadProxy::DrawDurationEstimate() {
  return impl().timing_history.DrawDurationEstimate();
}

base::TimeDelta ThreadProxy::BeginMainFrameToCommitDurationEstimate() {
  return impl().timing_history.BeginMainFrameToCommitDurationEstimate();
}

base::TimeDelta ThreadProxy::CommitToActivateDurationEstimate() {
  return impl().timing_history.CommitToActivateDurationEstimate();
}

void ThreadProxy::DidBeginImplFrameDeadline() {
  impl().layer_tree_host_impl->ResetCurrentFrameTimeForNextFrame();
}

void ThreadProxy::ReadyToFinalizeTextureUpdates() {
  DCHECK(IsImplThread());
  impl().scheduler->NotifyReadyToCommit();
}

void ThreadProxy::DidCommitAndDrawFrame() {
  DCHECK(IsMainThread());
  layer_tree_host()->DidCommitAndDrawFrame();
}

void ThreadProxy::DidCompleteSwapBuffers() {
  DCHECK(IsMainThread());
  layer_tree_host()->DidCompleteSwapBuffers();
}

void ThreadProxy::SetAnimationEvents(scoped_ptr<AnimationEventsVector> events) {
  TRACE_EVENT0("cc", "ThreadProxy::SetAnimationEvents");
  DCHECK(IsMainThread());
  layer_tree_host()->SetAnimationEvents(events.Pass());
}

void ThreadProxy::InitializeImplOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeImplOnImplThread");
  DCHECK(IsImplThread());
  impl().layer_tree_host_impl =
      layer_tree_host()->CreateLayerTreeHostImpl(this);
  SchedulerSettings scheduler_settings(layer_tree_host()->settings());
  impl().scheduler = Scheduler::Create(this,
                                       scheduler_settings,
                                       impl().layer_tree_host_id,
                                       ImplThreadTaskRunner());
  impl().scheduler->SetVisible(impl().layer_tree_host_impl->visible());

  impl_thread_weak_ptr_ = impl().weak_factory.GetWeakPtr();
  completion->Signal();
}

void ThreadProxy::DeleteContentsTexturesOnImplThread(
    CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::DeleteContentsTexturesOnImplThread");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());
  layer_tree_host()->DeleteContentsTexturesOnImplThread(
      impl().layer_tree_host_impl->resource_provider());
  completion->Signal();
}

void ThreadProxy::InitializeOutputSurfaceOnImplThread(
    scoped_ptr<OutputSurface> output_surface) {
  TRACE_EVENT0("cc", "ThreadProxy::InitializeOutputSurfaceOnImplThread");
  DCHECK(IsImplThread());

  LayerTreeHostImpl* host_impl = impl().layer_tree_host_impl.get();
  bool success = host_impl->InitializeRenderer(output_surface.Pass());
  RendererCapabilities capabilities;
  if (success) {
    capabilities =
        host_impl->GetRendererCapabilities().MainThreadCapabilities();
  }

  Proxy::MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadProxy::DidInitializeOutputSurface,
                 main_thread_weak_ptr_,
                 success,
                 capabilities));

  if (success)
    impl().scheduler->DidCreateAndInitializeOutputSurface();
}

void ThreadProxy::FinishGLOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::FinishGLOnImplThread");
  DCHECK(IsImplThread());
  if (impl().layer_tree_host_impl->resource_provider())
    impl().layer_tree_host_impl->resource_provider()->Finish();
  completion->Signal();
}

void ThreadProxy::LayerTreeHostClosedOnImplThread(CompletionEvent* completion) {
  TRACE_EVENT0("cc", "ThreadProxy::LayerTreeHostClosedOnImplThread");
  DCHECK(IsImplThread());
  DCHECK(IsMainThreadBlocked());
  layer_tree_host()->DeleteContentsTexturesOnImplThread(
      impl().layer_tree_host_impl->resource_provider());
  impl().current_resource_update_controller.reset();
  impl().layer_tree_host_impl->SetNeedsBeginFrame(false);
  impl().scheduler.reset();
  impl().layer_tree_host_impl.reset();
  impl().weak_factory.InvalidateWeakPtrs();
  impl().contents_texture_manager = NULL;
  completion->Signal();
}

size_t ThreadProxy::MaxPartialTextureUpdates() const {
  return ResourceUpdateController::MaxPartialTextureUpdates();
}

ThreadProxy::BeginMainFrameAndCommitState::BeginMainFrameAndCommitState()
    : memory_allocation_limit_bytes(0),
      memory_allocation_priority_cutoff(0),
      evicted_ui_resources(false) {}

ThreadProxy::BeginMainFrameAndCommitState::~BeginMainFrameAndCommitState() {}

scoped_ptr<base::Value> ThreadProxy::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());

  CompletionEvent completion;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(
        const_cast<ThreadProxy*>(this));
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::AsValueOnImplThread,
                   impl_thread_weak_ptr_,
                   &completion,
                   state.get()));
    completion.Wait();
  }
  return state.PassAs<base::Value>();
}

void ThreadProxy::AsValueOnImplThread(CompletionEvent* completion,
                                      base::DictionaryValue* state) const {
  state->Set("layer_tree_host_impl",
             impl().layer_tree_host_impl->AsValue().release());
  completion->Signal();
}

bool ThreadProxy::CommitPendingForTesting() {
  DCHECK(IsMainThread());
  CommitPendingRequest commit_pending_request;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::CommitPendingOnImplThreadForTesting,
                   impl_thread_weak_ptr_,
                   &commit_pending_request));
    commit_pending_request.completion.Wait();
  }
  return commit_pending_request.commit_pending;
}

void ThreadProxy::CommitPendingOnImplThreadForTesting(
    CommitPendingRequest* request) {
  DCHECK(IsImplThread());
  if (impl().layer_tree_host_impl->output_surface())
    request->commit_pending = impl().scheduler->CommitPending();
  else
    request->commit_pending = false;
  request->completion.Signal();
}

scoped_ptr<base::Value> ThreadProxy::SchedulerAsValueForTesting() {
  if (IsImplThread())
    return impl().scheduler->AsValue().Pass();

  SchedulerStateRequest scheduler_state_request;
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    Proxy::ImplThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ThreadProxy::SchedulerAsValueOnImplThreadForTesting,
                   impl_thread_weak_ptr_,
                   &scheduler_state_request));
    scheduler_state_request.completion.Wait();
  }
  return scheduler_state_request.state.Pass();
}

void ThreadProxy::SchedulerAsValueOnImplThreadForTesting(
    SchedulerStateRequest* request) {
  DCHECK(IsImplThread());
  request->state = impl().scheduler->AsValue();
  request->completion.Signal();
}

void ThreadProxy::RenewTreePriority() {
  DCHECK(IsImplThread());
  bool smoothness_takes_priority =
      impl().layer_tree_host_impl->pinch_gesture_active() ||
      impl().layer_tree_host_impl->page_scale_animation_active() ||
      (impl().layer_tree_host_impl->IsCurrentlyScrolling() &&
       !impl().layer_tree_host_impl->scroll_affects_scroll_handler());

  // Schedule expiration if smoothness currently takes priority.
  if (smoothness_takes_priority)
    impl().smoothness_priority_expiration_notifier.Schedule();

  // We use the same priority for both trees by default.
  TreePriority priority = SAME_PRIORITY_FOR_BOTH_TREES;

  // Smoothness takes priority if we have an expiration for it scheduled.
  if (impl().smoothness_priority_expiration_notifier.HasPendingNotification())
    priority = SMOOTHNESS_TAKES_PRIORITY;

  // New content always takes priority when the active tree has
  // evicted resources or there is an invalid viewport size.
  if (impl().layer_tree_host_impl->active_tree()->ContentsTexturesPurged() ||
      impl().layer_tree_host_impl->active_tree()->ViewportSizeInvalid() ||
      impl().layer_tree_host_impl->EvictedUIResourcesExist() ||
      impl().input_throttled_until_commit) {
    // Once we enter NEW_CONTENTS_TAKES_PRIORITY mode, visible tiles on active
    // tree might be freed. We need to set RequiresHighResToDraw to ensure that
    // high res tiles will be required to activate pending tree.
    impl().layer_tree_host_impl->active_tree()->SetRequiresHighResToDraw();
    priority = NEW_CONTENT_TAKES_PRIORITY;
  }

  impl().layer_tree_host_impl->SetTreePriority(priority);
  impl().scheduler->SetSmoothnessTakesPriority(priority ==
                                               SMOOTHNESS_TAKES_PRIORITY);

  // Notify the the client of this compositor via the output surface.
  // TODO(epenner): Route this to compositor-thread instead of output-surface
  // after GTFO refactor of compositor-thread (http://crbug/170828).
  if (impl().layer_tree_host_impl->output_surface()) {
    impl()
        .layer_tree_host_impl->output_surface()
        ->UpdateSmoothnessTakesPriority(priority == SMOOTHNESS_TAKES_PRIORITY);
  }
}

void ThreadProxy::PostDelayedScrollbarFadeOnImplThread(
    const base::Closure& start_fade,
    base::TimeDelta delay) {
  Proxy::ImplThreadTaskRunner()->PostDelayedTask(FROM_HERE, start_fade, delay);
}

void ThreadProxy::DidActivatePendingTree() {
  TRACE_EVENT0("cc", "ThreadProxy::DidActivatePendingTreeOnImplThread");
  DCHECK(IsImplThread());
  DCHECK(!impl().layer_tree_host_impl->pending_tree());

  if (impl().completion_event_for_commit_held_on_tree_activation) {
    TRACE_EVENT_INSTANT0(
        "cc", "ReleaseCommitbyActivation", TRACE_EVENT_SCOPE_THREAD);
    DCHECK(impl().layer_tree_host_impl->settings().impl_side_painting);
    impl().completion_event_for_commit_held_on_tree_activation->Signal();
    impl().completion_event_for_commit_held_on_tree_activation = NULL;
  }

  UpdateBackgroundAnimateTicking();

  impl().timing_history.DidActivatePendingTree();
}

void ThreadProxy::DidManageTiles() {
  DCHECK(IsImplThread());
  impl().scheduler->DidManageTiles();
}

}  // namespace cc
