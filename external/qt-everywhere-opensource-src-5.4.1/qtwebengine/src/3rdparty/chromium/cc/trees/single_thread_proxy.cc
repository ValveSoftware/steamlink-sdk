// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/single_thread_proxy.h"

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/quads/draw_quad.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_update_controller.h"
#include "cc/trees/blocking_task_runner.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/frame_time.h"

namespace cc {

scoped_ptr<Proxy> SingleThreadProxy::Create(
    LayerTreeHost* layer_tree_host,
    LayerTreeHostSingleThreadClient* client) {
  return make_scoped_ptr(
      new SingleThreadProxy(layer_tree_host, client)).PassAs<Proxy>();
}

SingleThreadProxy::SingleThreadProxy(LayerTreeHost* layer_tree_host,
                                     LayerTreeHostSingleThreadClient* client)
    : Proxy(NULL),
      layer_tree_host_(layer_tree_host),
      client_(client),
      next_frame_is_newly_committed_frame_(false),
      inside_draw_(false) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SingleThreadProxy");
  DCHECK(Proxy::IsMainThread());
  DCHECK(layer_tree_host);

  // Impl-side painting not supported without threaded compositing.
  CHECK(!layer_tree_host->settings().impl_side_painting)
      << "Threaded compositing must be enabled to use impl-side painting.";
}

void SingleThreadProxy::Start() {
  DebugScopedSetImplThread impl(this);
  layer_tree_host_impl_ = layer_tree_host_->CreateLayerTreeHostImpl(this);
}

SingleThreadProxy::~SingleThreadProxy() {
  TRACE_EVENT0("cc", "SingleThreadProxy::~SingleThreadProxy");
  DCHECK(Proxy::IsMainThread());
  // Make sure Stop() got called or never Started.
  DCHECK(!layer_tree_host_impl_);
}

void SingleThreadProxy::FinishAllRendering() {
  TRACE_EVENT0("cc", "SingleThreadProxy::FinishAllRendering");
  DCHECK(Proxy::IsMainThread());
  {
    DebugScopedSetImplThread impl(this);
    layer_tree_host_impl_->FinishAllRendering();
  }
}

bool SingleThreadProxy::IsStarted() const {
  DCHECK(Proxy::IsMainThread());
  return layer_tree_host_impl_;
}

void SingleThreadProxy::SetLayerTreeHostClientReady() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetLayerTreeHostClientReady");
  // Scheduling is controlled by the embedder in the single thread case, so
  // nothing to do.
}

void SingleThreadProxy::SetVisible(bool visible) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetVisible");
  DebugScopedSetImplThread impl(this);
  layer_tree_host_impl_->SetVisible(visible);

  // Changing visibility could change ShouldComposite().
  UpdateBackgroundAnimateTicking();
}

void SingleThreadProxy::CreateAndInitializeOutputSurface() {
  TRACE_EVENT0(
      "cc", "SingleThreadProxy::CreateAndInitializeOutputSurface");
  DCHECK(Proxy::IsMainThread());
  DCHECK(layer_tree_host_->output_surface_lost());

  scoped_ptr<OutputSurface> output_surface =
      layer_tree_host_->CreateOutputSurface();

  renderer_capabilities_for_main_thread_ = RendererCapabilities();

  bool success = !!output_surface;
  if (success) {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    DebugScopedSetImplThread impl(this);
    layer_tree_host_->DeleteContentsTexturesOnImplThread(
        layer_tree_host_impl_->resource_provider());
    success = layer_tree_host_impl_->InitializeRenderer(output_surface.Pass());
  }

  layer_tree_host_->OnCreateAndInitializeOutputSurfaceAttempted(success);

  if (!success) {
    // Force another recreation attempt to happen by requesting another commit.
    SetNeedsCommit();
  }
}

const RendererCapabilities& SingleThreadProxy::GetRendererCapabilities() const {
  DCHECK(Proxy::IsMainThread());
  DCHECK(!layer_tree_host_->output_surface_lost());
  return renderer_capabilities_for_main_thread_;
}

void SingleThreadProxy::SetNeedsAnimate() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsAnimate");
  DCHECK(Proxy::IsMainThread());
  client_->ScheduleAnimation();
}

void SingleThreadProxy::SetNeedsUpdateLayers() {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsUpdateLayers");
  DCHECK(Proxy::IsMainThread());
  client_->ScheduleComposite();
}

void SingleThreadProxy::DoCommit(scoped_ptr<ResourceUpdateQueue> queue) {
  TRACE_EVENT0("cc", "SingleThreadProxy::DoCommit");
  DCHECK(Proxy::IsMainThread());
  // Commit immediately.
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    DebugScopedSetImplThread impl(this);

    // This CapturePostTasks should be destroyed before CommitComplete() is
    // called since that goes out to the embedder, and we want the embedder
    // to receive its callbacks before that.
    BlockingTaskRunner::CapturePostTasks blocked;

    layer_tree_host_impl_->BeginCommit();

    if (PrioritizedResourceManager* contents_texture_manager =
        layer_tree_host_->contents_texture_manager()) {
      contents_texture_manager->PushTexturePrioritiesToBackings();
    }
    layer_tree_host_->BeginCommitOnImplThread(layer_tree_host_impl_.get());

    scoped_ptr<ResourceUpdateController> update_controller =
        ResourceUpdateController::Create(
            NULL,
            Proxy::MainThreadTaskRunner(),
            queue.Pass(),
            layer_tree_host_impl_->resource_provider());
    update_controller->Finalize();

    if (layer_tree_host_impl_->EvictedUIResourcesExist())
      layer_tree_host_->RecreateUIResources();

    layer_tree_host_->FinishCommitOnImplThread(layer_tree_host_impl_.get());

    layer_tree_host_impl_->CommitComplete();

#if DCHECK_IS_ON
    // In the single-threaded case, the scale and scroll deltas should never be
    // touched on the impl layer tree.
    scoped_ptr<ScrollAndScaleSet> scroll_info =
        layer_tree_host_impl_->ProcessScrollDeltas();
    DCHECK(!scroll_info->scrolls.size());
    DCHECK_EQ(1.f, scroll_info->page_scale_delta);
#endif

    RenderingStatsInstrumentation* stats_instrumentation =
        layer_tree_host_->rendering_stats_instrumentation();
    BenchmarkInstrumentation::IssueMainThreadRenderingStatsEvent(
        stats_instrumentation->main_thread_rendering_stats());
    stats_instrumentation->AccumulateAndClearMainThreadStats();
  }
  layer_tree_host_->CommitComplete();
  next_frame_is_newly_committed_frame_ = true;
}

void SingleThreadProxy::SetNeedsCommit() {
  DCHECK(Proxy::IsMainThread());
  client_->ScheduleComposite();
}

void SingleThreadProxy::SetNeedsRedraw(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc", "SingleThreadProxy::SetNeedsRedraw");
  SetNeedsRedrawRectOnImplThread(damage_rect);
  client_->ScheduleComposite();
}

void SingleThreadProxy::SetNextCommitWaitsForActivation() {
  // There is no activation here other than commit. So do nothing.
}

void SingleThreadProxy::SetDeferCommits(bool defer_commits) {
  // Thread-only feature.
  NOTREACHED();
}

bool SingleThreadProxy::CommitRequested() const { return false; }

bool SingleThreadProxy::BeginMainFrameRequested() const { return false; }

size_t SingleThreadProxy::MaxPartialTextureUpdates() const {
  return std::numeric_limits<size_t>::max();
}

void SingleThreadProxy::Stop() {
  TRACE_EVENT0("cc", "SingleThreadProxy::stop");
  DCHECK(Proxy::IsMainThread());
  {
    DebugScopedSetMainThreadBlocked main_thread_blocked(this);
    DebugScopedSetImplThread impl(this);

    BlockingTaskRunner::CapturePostTasks blocked;
    layer_tree_host_->DeleteContentsTexturesOnImplThread(
        layer_tree_host_impl_->resource_provider());
    layer_tree_host_impl_.reset();
  }
  layer_tree_host_ = NULL;
}

void SingleThreadProxy::OnCanDrawStateChanged(bool can_draw) {
  TRACE_EVENT1(
      "cc", "SingleThreadProxy::OnCanDrawStateChanged", "can_draw", can_draw);
  DCHECK(Proxy::IsImplThread());
  UpdateBackgroundAnimateTicking();
}

void SingleThreadProxy::NotifyReadyToActivate() {
  // Thread-only feature.
  NOTREACHED();
}

void SingleThreadProxy::SetNeedsRedrawOnImplThread() {
  client_->ScheduleComposite();
}

void SingleThreadProxy::SetNeedsAnimateOnImplThread() {
  SetNeedsRedrawOnImplThread();
}

void SingleThreadProxy::SetNeedsManageTilesOnImplThread() {
  // Thread-only/Impl-side-painting-only feature.
  NOTREACHED();
}

void SingleThreadProxy::SetNeedsRedrawRectOnImplThread(
    const gfx::Rect& damage_rect) {
  // TODO(brianderson): Once we move render_widget scheduling into this class,
  // we can treat redraw requests more efficiently than CommitAndRedraw
  // requests.
  layer_tree_host_impl_->SetViewportDamage(damage_rect);
  SetNeedsCommit();
}

void SingleThreadProxy::DidInitializeVisibleTileOnImplThread() {
  // Impl-side painting only.
  NOTREACHED();
}

void SingleThreadProxy::SetNeedsCommitOnImplThread() {
  client_->ScheduleComposite();
}

void SingleThreadProxy::PostAnimationEventsToMainThreadOnImplThread(
    scoped_ptr<AnimationEventsVector> events) {
  TRACE_EVENT0(
      "cc", "SingleThreadProxy::PostAnimationEventsToMainThreadOnImplThread");
  DCHECK(Proxy::IsImplThread());
  DebugScopedSetMainThread main(this);
  layer_tree_host_->SetAnimationEvents(events.Pass());
}

bool SingleThreadProxy::ReduceContentsTextureMemoryOnImplThread(
    size_t limit_bytes,
    int priority_cutoff) {
  DCHECK(IsImplThread());
  PrioritizedResourceManager* contents_texture_manager =
      layer_tree_host_->contents_texture_manager();

  ResourceProvider* resource_provider =
      layer_tree_host_impl_->resource_provider();

  if (!contents_texture_manager || !resource_provider)
    return false;

  return contents_texture_manager->ReduceMemoryOnImplThread(
      limit_bytes, priority_cutoff, resource_provider);
}

bool SingleThreadProxy::IsInsideDraw() { return inside_draw_; }

void SingleThreadProxy::UpdateRendererCapabilitiesOnImplThread() {
  DCHECK(IsImplThread());
  renderer_capabilities_for_main_thread_ =
      layer_tree_host_impl_->GetRendererCapabilities().MainThreadCapabilities();
}

void SingleThreadProxy::DidLoseOutputSurfaceOnImplThread() {
  TRACE_EVENT0("cc", "SingleThreadProxy::DidLoseOutputSurfaceOnImplThread");
  // Cause a commit so we can notice the lost context.
  SetNeedsCommitOnImplThread();
  client_->DidAbortSwapBuffers();
}

void SingleThreadProxy::DidSwapBuffersOnImplThread() {
  client_->DidPostSwapBuffers();
}

void SingleThreadProxy::DidSwapBuffersCompleteOnImplThread() {
  TRACE_EVENT0("cc", "SingleThreadProxy::DidSwapBuffersCompleteOnImplThread");
  client_->DidCompleteSwapBuffers();
}

// Called by the legacy scheduling path (e.g. where render_widget does the
// scheduling)
void SingleThreadProxy::CompositeImmediately(base::TimeTicks frame_begin_time) {
  TRACE_EVENT0("cc", "SingleThreadProxy::CompositeImmediately");
  DCHECK(Proxy::IsMainThread());
  DCHECK(!layer_tree_host_->output_surface_lost());

  layer_tree_host_->AnimateLayers(frame_begin_time);

  if (PrioritizedResourceManager* contents_texture_manager =
          layer_tree_host_->contents_texture_manager()) {
    contents_texture_manager->UnlinkAndClearEvictedBackings();
    contents_texture_manager->SetMaxMemoryLimitBytes(
        layer_tree_host_impl_->memory_allocation_limit_bytes());
    contents_texture_manager->SetExternalPriorityCutoff(
        layer_tree_host_impl_->memory_allocation_priority_cutoff());
  }

  scoped_ptr<ResourceUpdateQueue> queue =
      make_scoped_ptr(new ResourceUpdateQueue);
  layer_tree_host_->UpdateLayers(queue.get());
  layer_tree_host_->WillCommit();
  DoCommit(queue.Pass());
  layer_tree_host_->DidBeginMainFrame();

  LayerTreeHostImpl::FrameData frame;
  if (DoComposite(frame_begin_time, &frame)) {
    {
      DebugScopedSetMainThreadBlocked main_thread_blocked(this);
      DebugScopedSetImplThread impl(this);

      // This CapturePostTasks should be destroyed before
      // DidCommitAndDrawFrame() is called since that goes out to the embedder,
      // and we want the embedder to receive its callbacks before that.
      // NOTE: This maintains consistent ordering with the ThreadProxy since
      // the DidCommitAndDrawFrame() must be post-tasked from the impl thread
      // there as the main thread is not blocked, so any posted tasks inside
      // the swap buffers will execute first.
      BlockingTaskRunner::CapturePostTasks blocked;

      layer_tree_host_impl_->SwapBuffers(frame);
    }
    DidSwapFrame();
  }
}

scoped_ptr<base::Value> SingleThreadProxy::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  {
    // The following line casts away const modifiers because it is just
    // setting debug state. We still want the AsValue() function and its
    // call chain to be const throughout.
    DebugScopedSetImplThread impl(const_cast<SingleThreadProxy*>(this));

    state->Set("layer_tree_host_impl",
               layer_tree_host_impl_->AsValue().release());
  }
  return state.PassAs<base::Value>();
}

void SingleThreadProxy::ForceSerializeOnSwapBuffers() {
  {
    DebugScopedSetImplThread impl(this);
    if (layer_tree_host_impl_->renderer()) {
      DCHECK(!layer_tree_host_->output_surface_lost());
      layer_tree_host_impl_->renderer()->DoNoOp();
    }
  }
}

bool SingleThreadProxy::ShouldComposite() const {
  DCHECK(Proxy::IsImplThread());
  return layer_tree_host_impl_->visible() &&
         layer_tree_host_impl_->CanDraw();
}

void SingleThreadProxy::UpdateBackgroundAnimateTicking() {
  DCHECK(Proxy::IsImplThread());
  layer_tree_host_impl_->UpdateBackgroundAnimateTicking(
      !ShouldComposite() && layer_tree_host_impl_->active_tree()->root_layer());
}

bool SingleThreadProxy::DoComposite(
    base::TimeTicks frame_begin_time,
    LayerTreeHostImpl::FrameData* frame) {
  TRACE_EVENT0("cc", "SingleThreadProxy::DoComposite");
  DCHECK(!layer_tree_host_->output_surface_lost());

  bool lost_output_surface = false;
  {
    DebugScopedSetImplThread impl(this);
    base::AutoReset<bool> mark_inside(&inside_draw_, true);

    // We guard PrepareToDraw() with CanDraw() because it always returns a valid
    // frame, so can only be used when such a frame is possible. Since
    // DrawLayers() depends on the result of PrepareToDraw(), it is guarded on
    // CanDraw() as well.
    if (!ShouldComposite()) {
      UpdateBackgroundAnimateTicking();
      return false;
    }

    layer_tree_host_impl_->Animate(
        layer_tree_host_impl_->CurrentFrameTimeTicks());
    UpdateBackgroundAnimateTicking();

    if (!layer_tree_host_impl_->IsContextLost()) {
      layer_tree_host_impl_->PrepareToDraw(frame);
      layer_tree_host_impl_->DrawLayers(frame, frame_begin_time);
      layer_tree_host_impl_->DidDrawAllLayers(*frame);
    }
    lost_output_surface = layer_tree_host_impl_->IsContextLost();

    bool start_ready_animations = true;
    layer_tree_host_impl_->UpdateAnimationState(start_ready_animations);

    layer_tree_host_impl_->ResetCurrentFrameTimeForNextFrame();
  }

  if (lost_output_surface) {
    layer_tree_host_->DidLoseOutputSurface();
    return false;
  }

  return true;
}

void SingleThreadProxy::DidSwapFrame() {
  if (next_frame_is_newly_committed_frame_) {
    next_frame_is_newly_committed_frame_ = false;
    layer_tree_host_->DidCommitAndDrawFrame();
  }
}

bool SingleThreadProxy::CommitPendingForTesting() { return false; }

}  // namespace cc
