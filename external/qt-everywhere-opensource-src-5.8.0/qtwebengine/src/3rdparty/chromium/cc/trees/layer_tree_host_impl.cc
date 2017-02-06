// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/small_map.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/base/histograms.h"
#include "cc/base/math_util.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/debug/debug_rect_history.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_rate_counter.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/scroll_elasticity_helper.h"
#include "cc/input/scroll_state.h"
#include "cc/input/scrollbar_animation_controller.h"
#include "cc/input/top_controls_manager.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/viewport.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/delegating_renderer.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/software_renderer.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/tiles/eviction_tile_priority_queue.h"
#include "cc/tiles/gpu_image_decode_controller.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/tiles/raster_tile_priority_queue.h"
#include "cc/tiles/software_image_decode_controller.h"
#include "cc/tiles/tile_task_manager.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/latency_info_swap_promise_monitor.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {
namespace {

// Small helper class that saves the current viewport location as the user sees
// it and resets to the same location.
class ViewportAnchor {
 public:
  ViewportAnchor(LayerImpl* inner_scroll, LayerImpl* outer_scroll)
  : inner_(inner_scroll),
    outer_(outer_scroll) {
    viewport_in_content_coordinates_ = inner_->CurrentScrollOffset();

    if (outer_)
      viewport_in_content_coordinates_ += outer_->CurrentScrollOffset();
  }

  void ResetViewportToAnchoredPosition() {
    DCHECK(outer_);

    inner_->ClampScrollToMaxScrollOffset();
    outer_->ClampScrollToMaxScrollOffset();

    gfx::ScrollOffset viewport_location =
        inner_->CurrentScrollOffset() + outer_->CurrentScrollOffset();

    gfx::Vector2dF delta =
        viewport_in_content_coordinates_.DeltaFrom(viewport_location);

    delta = inner_->ScrollBy(delta);
    outer_->ScrollBy(delta);
  }

 private:
  LayerImpl* inner_;
  LayerImpl* outer_;
  gfx::ScrollOffset viewport_in_content_coordinates_;
};

void DidVisibilityChange(LayerTreeHostImpl* id, bool visible) {
  if (visible) {
    TRACE_EVENT_ASYNC_BEGIN1("cc", "LayerTreeHostImpl::SetVisible", id,
                             "LayerTreeHostImpl", id);
    return;
  }

  TRACE_EVENT_ASYNC_END0("cc", "LayerTreeHostImpl::SetVisible", id);
}

bool IsWheelBasedScroll(InputHandler::ScrollInputType type) {
  return type == InputHandler::WHEEL;
}

enum ScrollThread { MAIN_THREAD, CC_THREAD };

void RecordCompositorSlowScrollMetric(InputHandler::ScrollInputType type,
                                      ScrollThread scroll_thread) {
  bool scroll_on_main_thread = (scroll_thread == MAIN_THREAD);
  if (IsWheelBasedScroll(type)) {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.CompositorWheelScrollUpdateThread",
                          scroll_on_main_thread);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.CompositorTouchScrollUpdateThread",
                          scroll_on_main_thread);
  }
}

}  // namespace

DEFINE_SCOPED_UMA_HISTOGRAM_TIMER(PendingTreeDurationHistogramTimer,
                                  "Scheduling.%s.PendingTreeDuration");

LayerTreeHostImpl::FrameData::FrameData()
    : render_surface_layer_list(nullptr), has_no_damage(false) {}

LayerTreeHostImpl::FrameData::~FrameData() {}

std::unique_ptr<LayerTreeHostImpl> LayerTreeHostImpl::Create(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    TaskRunnerProvider* task_runner_provider,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    TaskGraphRunner* task_graph_runner,
    std::unique_ptr<AnimationHost> animation_host,
    int id) {
  return base::WrapUnique(new LayerTreeHostImpl(
      settings, client, task_runner_provider, rendering_stats_instrumentation,
      shared_bitmap_manager, gpu_memory_buffer_manager, task_graph_runner,
      std::move(animation_host), id));
}

LayerTreeHostImpl::LayerTreeHostImpl(
    const LayerTreeSettings& settings,
    LayerTreeHostImplClient* client,
    TaskRunnerProvider* task_runner_provider,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    TaskGraphRunner* task_graph_runner,
    std::unique_ptr<AnimationHost> animation_host,
    int id)
    : client_(client),
      task_runner_provider_(task_runner_provider),
      current_begin_frame_tracker_(BEGINFRAMETRACKER_FROM_HERE),
      output_surface_(nullptr),
      content_is_suitable_for_gpu_rasterization_(true),
      has_gpu_rasterization_trigger_(false),
      use_gpu_rasterization_(false),
      use_msaa_(false),
      gpu_rasterization_status_(GpuRasterizationStatus::OFF_DEVICE),
      tree_resources_for_gpu_rasterization_dirty_(false),
      input_handler_client_(NULL),
      did_lock_scrolling_layer_(false),
      wheel_scrolling_(false),
      scroll_affects_scroll_handler_(false),
      scroll_layer_id_when_mouse_over_scrollbar_(Layer::INVALID_ID),
      tile_priorities_dirty_(false),
      settings_(settings),
      visible_(false),
      cached_managed_memory_policy_(settings.memory_policy_),
      is_synchronous_single_threaded_(!task_runner_provider->HasImplThread() &&
                                      !settings.single_thread_proxy_scheduler),
      // Must be initialized after is_synchronous_single_threaded_ and
      // task_runner_provider_.
      tile_manager_(this,
                    GetTaskRunner(),
                    is_synchronous_single_threaded_
                        ? std::numeric_limits<size_t>::max()
                        : settings.scheduled_raster_task_limit,
                    settings.use_partial_raster,
                    settings.max_preraster_distance_in_screen_pixels),
      pinch_gesture_active_(false),
      pinch_gesture_end_should_clear_scrolling_layer_(false),
      fps_counter_(
          FrameRateCounter::Create(task_runner_provider_->HasImplThread())),
      memory_history_(MemoryHistory::Create()),
      debug_rect_history_(DebugRectHistory::Create()),
      texture_mailbox_deleter_(new TextureMailboxDeleter(GetTaskRunner())),
      max_memory_needed_bytes_(0),
      resourceless_software_draw_(false),
      animation_host_(std::move(animation_host)),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      micro_benchmark_controller_(this),
      shared_bitmap_manager_(shared_bitmap_manager),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      task_graph_runner_(task_graph_runner),
      id_(id),
      requires_high_res_to_draw_(false),
      is_likely_to_require_a_draw_(false),
      mutator_(nullptr) {
  DCHECK(animation_host_);
  animation_host_->SetMutatorHostClient(this);

  DCHECK(task_runner_provider_->IsImplThread());
  DidVisibilityChange(this, visible_);

  SetDebugState(settings.initial_debug_state);

  // LTHI always has an active tree.
  active_tree_ =
      LayerTreeImpl::create(this, new SyncedProperty<ScaleGroup>(),
                            new SyncedTopControls, new SyncedElasticOverscroll);
  active_tree_->property_trees()->is_active = true;

  viewport_ = Viewport::Create(this);

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::LayerTreeHostImpl", id_);

  top_controls_manager_ =
      TopControlsManager::Create(this,
                                 settings.top_controls_show_threshold,
                                 settings.top_controls_hide_threshold);
}

LayerTreeHostImpl::~LayerTreeHostImpl() {
  DCHECK(task_runner_provider_->IsImplThread());
  TRACE_EVENT0("cc", "LayerTreeHostImpl::~LayerTreeHostImpl()");
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::LayerTreeHostImpl", id_);

  if (input_handler_client_) {
    input_handler_client_->WillShutdown();
    input_handler_client_ = NULL;
  }
  if (scroll_elasticity_helper_)
    scroll_elasticity_helper_.reset();

  // The layer trees must be destroyed before the layer tree host. We've
  // made a contract with our animation controllers that the animation_host
  // will outlive them, and we must make good.
  if (recycle_tree_)
    recycle_tree_->Shutdown();
  if (pending_tree_)
    pending_tree_->Shutdown();
  active_tree_->Shutdown();
  recycle_tree_ = nullptr;
  pending_tree_ = nullptr;
  active_tree_ = nullptr;

  animation_host_->ClearTimelines();
  animation_host_->SetMutatorHostClient(nullptr);

  CleanUpTileManagerAndUIResources();
  renderer_ = nullptr;
  resource_provider_ = nullptr;

  if (output_surface_) {
    output_surface_->DetachFromClient();
    output_surface_ = nullptr;
  }
}

void LayerTreeHostImpl::BeginMainFrameAborted(CommitEarlyOutReason reason) {
  // If the begin frame data was handled, then scroll and scale set was applied
  // by the main thread, so the active tree needs to be updated as if these sent
  // values were applied and committed.
  if (CommitEarlyOutHandledCommit(reason))
    active_tree_->ApplySentScrollAndScaleDeltasFromAbortedCommit();
}

void LayerTreeHostImpl::BeginCommit() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::BeginCommit");

  // Ensure all textures are returned so partial texture updates can happen
  // during the commit.
  // TODO(ericrk): We should not need to ForceReclaimResources when using
  // Impl-side-painting as it doesn't upload during commits. However,
  // Display::Draw currently relies on resource being reclaimed to block drawing
  // between BeginCommit / Swap. See crbug.com/489515.
  if (output_surface_)
    output_surface_->ForceReclaimResources();

  if (!CommitToActiveTree())
    CreatePendingTree();
}

void LayerTreeHostImpl::CommitComplete() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::CommitComplete");

  if (CommitToActiveTree()) {
    // We have to activate animations here or "IsActive()" is true on the layers
    // but the animations aren't activated yet so they get ignored by
    // UpdateDrawProperties.
    ActivateAnimations();
  }

  // Start animations before UpdateDrawProperties and PrepareTiles, as they can
  // change the results. When doing commit to the active tree, this must happen
  // after ActivateAnimations() in order for this ticking to be propogated to
  // layers on the active tree.
  if (CommitToActiveTree())
    Animate();
  else
    AnimatePendingTreeAfterCommit();

  // LayerTreeHost may have changed the GPU rasterization flags state, which
  // may require an update of the tree resources.
  UpdateTreeResourcesForGpuRasterizationIfNeeded();
  sync_tree()->set_needs_update_draw_properties();

  // We need an update immediately post-commit to have the opportunity to create
  // tilings.  Because invalidations may be coming from the main thread, it's
  // safe to do an update for lcd text at this point and see if lcd text needs
  // to be disabled on any layers.
  bool update_lcd_text = true;
  sync_tree()->UpdateDrawProperties(update_lcd_text);
  // Start working on newly created tiles immediately if needed.
  // TODO(vmpstr): Investigate always having PrepareTiles issue
  // NotifyReadyToActivate, instead of handling it here.
  bool did_prepare_tiles = PrepareTiles();
  if (!did_prepare_tiles) {
    NotifyReadyToActivate();

    // Ensure we get ReadyToDraw signal even when PrepareTiles not run. This
    // is important for SingleThreadProxy and impl-side painting case. For
    // STP, we commit to active tree and RequiresHighResToDraw, and set
    // Scheduler to wait for ReadyToDraw signal to avoid Checkerboard.
    if (CommitToActiveTree())
      NotifyReadyToDraw();
  }

  micro_benchmark_controller_.DidCompleteCommit();
}

bool LayerTreeHostImpl::CanDraw() const {
  // Note: If you are changing this function or any other function that might
  // affect the result of CanDraw, make sure to call
  // client_->OnCanDrawStateChanged in the proper places and update the
  // NotifyIfCanDrawChanged test.

  if (!renderer_) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw no renderer",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // Must have an OutputSurface if |renderer_| is not NULL.
  DCHECK(output_surface_);

  // TODO(boliu): Make draws without layers work and move this below
  // |resourceless_software_draw_| check. Tracked in crbug.com/264967.
  if (active_tree_->LayerListIsEmpty()) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw no root layer",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (resourceless_software_draw_)
    return true;

  if (DrawViewportSize().IsEmpty()) {
    TRACE_EVENT_INSTANT0("cc", "LayerTreeHostImpl::CanDraw empty viewport",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (active_tree_->ViewportSizeInvalid()) {
    TRACE_EVENT_INSTANT0(
        "cc", "LayerTreeHostImpl::CanDraw viewport size recently changed",
        TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  if (EvictedUIResourcesExist()) {
    TRACE_EVENT_INSTANT0(
        "cc", "LayerTreeHostImpl::CanDraw UI resources evicted not recreated",
        TRACE_EVENT_SCOPE_THREAD);
    return false;
  }
  return true;
}

void LayerTreeHostImpl::AnimatePendingTreeAfterCommit() {
  AnimateInternal(false);
}

void LayerTreeHostImpl::Animate() {
  AnimateInternal(true);
}

void LayerTreeHostImpl::AnimateInternal(bool active_tree) {
  DCHECK(task_runner_provider_->IsImplThread());
  base::TimeTicks monotonic_time = CurrentBeginFrameArgs().frame_time;

  // mithro(TODO): Enable these checks.
  // DCHECK(!current_begin_frame_tracker_.HasFinished());
  // DCHECK(monotonic_time == current_begin_frame_tracker_.Current().frame_time)
  //  << "Called animate with unknown frame time!?";

  bool did_animate = false;

  if (input_handler_client_) {
    // This animates fling scrolls. But on Android WebView root flings are
    // controlled by the application, so the compositor does not animate them.
    bool ignore_fling = settings_.ignore_root_layer_flings &&
                        IsCurrentlyScrollingInnerViewport();
    if (!ignore_fling) {
      // This does not set did_animate, because if the InputHandlerClient
      // changes anything it will be through the InputHandler interface which
      // does SetNeedsRedraw.
      input_handler_client_->Animate(monotonic_time);
    }
  }

  did_animate |= AnimatePageScale(monotonic_time);
  did_animate |= AnimateLayers(monotonic_time);
  did_animate |= AnimateScrollbars(monotonic_time);
  did_animate |= AnimateTopControls(monotonic_time);

  if (active_tree) {
    did_animate |= Mutate(monotonic_time);

    // Animating stuff can change the root scroll offset, so inform the
    // synchronous input handler.
    UpdateRootLayerStateForSynchronousInputHandler();
    if (did_animate) {
      // If the tree changed, then we want to draw at the end of the current
      // frame.
      SetNeedsRedraw();
    }
  }
}

bool LayerTreeHostImpl::Mutate(base::TimeTicks monotonic_time) {
  if (!mutator_)
    return false;
  TRACE_EVENT0("compositor-worker", "LayerTreeHostImpl::Mutate");
  if (mutator_->Mutate(monotonic_time, active_tree()))
    client_->SetNeedsOneBeginImplFrameOnImplThread();
  return true;
}

void LayerTreeHostImpl::SetNeedsMutate() {
  TRACE_EVENT0("compositor-worker", "LayerTreeHostImpl::SetNeedsMutate");
  client_->SetNeedsOneBeginImplFrameOnImplThread();
}

bool LayerTreeHostImpl::PrepareTiles() {
  if (!tile_priorities_dirty_)
    return false;

  client_->WillPrepareTiles();
  bool did_prepare_tiles = tile_manager_.PrepareTiles(global_tile_state_);
  if (did_prepare_tiles)
    tile_priorities_dirty_ = false;
  client_->DidPrepareTiles();
  return did_prepare_tiles;
}

void LayerTreeHostImpl::StartPageScaleAnimation(
    const gfx::Vector2d& target_offset,
    bool anchor_point,
    float page_scale,
    base::TimeDelta duration) {
  if (!InnerViewportScrollLayer())
    return;

  gfx::ScrollOffset scroll_total = active_tree_->TotalScrollOffset();
  gfx::SizeF scaled_scrollable_size = active_tree_->ScrollableSize();
  gfx::SizeF viewport_size =
      gfx::SizeF(active_tree_->InnerViewportContainerLayer()->bounds());

  // TODO(miletus) : Pass in ScrollOffset.
  page_scale_animation_ =
      PageScaleAnimation::Create(ScrollOffsetToVector2dF(scroll_total),
                                 active_tree_->current_page_scale_factor(),
                                 viewport_size, scaled_scrollable_size);

  if (anchor_point) {
    gfx::Vector2dF anchor(target_offset);
    page_scale_animation_->ZoomWithAnchor(anchor,
                                          page_scale,
                                          duration.InSecondsF());
  } else {
    gfx::Vector2dF scaled_target_offset = target_offset;
    page_scale_animation_->ZoomTo(scaled_target_offset,
                                  page_scale,
                                  duration.InSecondsF());
  }

  SetNeedsOneBeginImplFrame();
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::SetNeedsAnimateInput() {
  DCHECK(!IsCurrentlyScrollingInnerViewport() ||
         !settings_.ignore_root_layer_flings);
  SetNeedsOneBeginImplFrame();
}

bool LayerTreeHostImpl::IsCurrentlyScrollingInnerViewport() const {
  LayerImpl* scrolling_layer = CurrentlyScrollingLayer();
  if (!scrolling_layer)
    return false;
  return scrolling_layer == InnerViewportScrollLayer();
}

bool LayerTreeHostImpl::IsCurrentlyScrollingLayerAt(
    const gfx::Point& viewport_point,
    InputHandler::ScrollInputType type) const {
  LayerImpl* scrolling_layer_impl = CurrentlyScrollingLayer();
  if (!scrolling_layer_impl)
    return false;

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());

  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);

  bool scroll_on_main_thread = false;
  uint32_t main_thread_scrolling_reasons;
  LayerImpl* test_layer_impl = FindScrollLayerForDeviceViewportPoint(
      device_viewport_point, type, layer_impl, &scroll_on_main_thread,
      &main_thread_scrolling_reasons);

  if (!test_layer_impl)
    return false;

  if (scrolling_layer_impl == test_layer_impl)
    return true;

  // For active scrolling state treat the inner/outer viewports interchangeably.
  if ((scrolling_layer_impl == InnerViewportScrollLayer() &&
       test_layer_impl == OuterViewportScrollLayer()) ||
      (scrolling_layer_impl == OuterViewportScrollLayer() &&
       test_layer_impl == InnerViewportScrollLayer())) {
    return true;
  }

  return false;
}

EventListenerProperties LayerTreeHostImpl::GetEventListenerProperties(
    EventListenerClass event_class) const {
  return active_tree_->event_listener_properties(event_class);
}

bool LayerTreeHostImpl::DoTouchEventsBlockScrollAt(
    const gfx::Point& viewport_point) {
  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());

  // Now determine if there are actually any handlers at that point.
  // TODO(rbyers): Consider also honoring touch-action (crbug.com/347272).
  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPointInTouchHandlerRegion(
          device_viewport_point);
  return layer_impl != NULL;
}

std::unique_ptr<SwapPromiseMonitor>
LayerTreeHostImpl::CreateLatencyInfoSwapPromiseMonitor(
    ui::LatencyInfo* latency) {
  return base::WrapUnique(
      new LatencyInfoSwapPromiseMonitor(latency, NULL, this));
}

ScrollElasticityHelper* LayerTreeHostImpl::CreateScrollElasticityHelper() {
  DCHECK(!scroll_elasticity_helper_);
  if (settings_.enable_elastic_overscroll) {
    scroll_elasticity_helper_.reset(
        ScrollElasticityHelper::CreateForLayerTreeHostImpl(this));
  }
  return scroll_elasticity_helper_.get();
}

void LayerTreeHostImpl::QueueSwapPromiseForMainThreadScrollUpdate(
    std::unique_ptr<SwapPromise> swap_promise) {
  swap_promises_for_main_thread_scroll_update_.push_back(
      std::move(swap_promise));
}

void LayerTreeHostImpl::TrackDamageForAllSurfaces(
    const LayerImplList& render_surface_layer_list) {
  // For now, we use damage tracking to compute a global scissor. To do this, we
  // must compute all damage tracking before drawing anything, so that we know
  // the root damage rect. The root damage rect is then used to scissor each
  // surface.
  size_t render_surface_layer_list_size = render_surface_layer_list.size();
  for (size_t i = 0; i < render_surface_layer_list_size; ++i) {
    size_t surface_index = render_surface_layer_list_size - 1 - i;
    LayerImpl* render_surface_layer = render_surface_layer_list[surface_index];
    RenderSurfaceImpl* render_surface = render_surface_layer->render_surface();
    DCHECK(render_surface);
    render_surface->damage_tracker()->UpdateDamageTrackingState(
        render_surface->layer_list(), render_surface,
        render_surface->SurfacePropertyChangedOnlyFromDescendant(),
        render_surface->content_rect(), render_surface->MaskLayer(),
        render_surface_layer->filters());
  }
}

void LayerTreeHostImpl::FrameData::AsValueInto(
    base::trace_event::TracedValue* value) const {
  value->SetBoolean("has_no_damage", has_no_damage);

  // Quad data can be quite large, so only dump render passes if we select
  // cc.debug.quads.
  bool quads_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.quads"), &quads_enabled);
  if (quads_enabled) {
    value->BeginArray("render_passes");
    for (size_t i = 0; i < render_passes.size(); ++i) {
      value->BeginDictionary();
      render_passes[i]->AsValueInto(value);
      value->EndDictionary();
    }
    value->EndArray();
  }
}

void LayerTreeHostImpl::FrameData::AppendRenderPass(
    std::unique_ptr<RenderPass> render_pass) {
  render_passes.push_back(std::move(render_pass));
}

DrawMode LayerTreeHostImpl::GetDrawMode() const {
  if (resourceless_software_draw_) {
    return DRAW_MODE_RESOURCELESS_SOFTWARE;
  } else if (output_surface_->context_provider()) {
    return DRAW_MODE_HARDWARE;
  } else {
    return DRAW_MODE_SOFTWARE;
  }
}

static void AppendQuadsForRenderSurfaceLayer(
    RenderPass* target_render_pass,
    LayerImpl* layer,
    const RenderPass* contributing_render_pass,
    AppendQuadsData* append_quads_data) {
  RenderSurfaceImpl* surface = layer->render_surface();
  const gfx::Transform& draw_transform = surface->draw_transform();
  const Occlusion& occlusion = surface->occlusion_in_content_space();
  SkColor debug_border_color = surface->GetDebugBorderColor();
  float debug_border_width = surface->GetDebugBorderWidth();
  LayerImpl* mask_layer = surface->MaskLayer();

  surface->AppendQuads(target_render_pass, draw_transform, occlusion,
                       debug_border_color, debug_border_width, mask_layer,
                       append_quads_data, contributing_render_pass->id);

  // Add replica after the surface so that it appears below the surface.
  if (surface->HasReplica()) {
    const gfx::Transform& replica_draw_transform =
        surface->replica_draw_transform();
    Occlusion replica_occlusion = occlusion.GetOcclusionWithGivenDrawTransform(
        surface->replica_draw_transform());
    SkColor replica_debug_border_color = surface->GetReplicaDebugBorderColor();
    float replica_debug_border_width = surface->GetReplicaDebugBorderWidth();
    // TODO(danakj): By using the same RenderSurfaceImpl for both the
    // content and its reflection, it's currently not possible to apply a
    // separate mask to the reflection layer or correctly handle opacity in
    // reflections (opacity must be applied after drawing both the layer and its
    // reflection). The solution is to introduce yet another RenderSurfaceImpl
    // to draw the layer and its reflection in. For now we only apply a separate
    // reflection mask if the contents don't have a mask of their own.
    LayerImpl* replica_mask_layer =
        surface->HasMask() ? surface->MaskLayer() : surface->ReplicaMaskLayer();

    surface->AppendQuads(target_render_pass, replica_draw_transform,
                         replica_occlusion, replica_debug_border_color,
                         replica_debug_border_width, replica_mask_layer,
                         append_quads_data, contributing_render_pass->id);
  }
}

static void AppendQuadsToFillScreen(const gfx::Rect& root_scroll_layer_rect,
                                    RenderPass* target_render_pass,
                                    RenderSurfaceImpl* root_render_surface,
                                    SkColor screen_background_color,
                                    const Region& fill_region) {
  if (!root_render_surface || !SkColorGetA(screen_background_color))
    return;
  if (fill_region.IsEmpty())
    return;

  // Manually create the quad state for the gutter quads, as the root layer
  // doesn't have any bounds and so can't generate this itself.
  // TODO(danakj): Make the gutter quads generated by the solid color layer
  // (make it smarter about generating quads to fill unoccluded areas).

  gfx::Rect root_target_rect = root_render_surface->content_rect();
  float opacity = 1.f;
  int sorting_context_id = 0;
  SharedQuadState* shared_quad_state =
      target_render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(gfx::Transform(),
                            root_target_rect.size(),
                            root_target_rect,
                            root_target_rect,
                            false,
                            opacity,
                            SkXfermode::kSrcOver_Mode,
                            sorting_context_id);

  for (Region::Iterator fill_rects(fill_region); fill_rects.has_rect();
       fill_rects.next()) {
    gfx::Rect screen_space_rect = fill_rects.rect();
    gfx::Rect visible_screen_space_rect = screen_space_rect;
    // Skip the quad culler and just append the quads directly to avoid
    // occlusion checks.
    SolidColorDrawQuad* quad =
        target_render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state,
                 screen_space_rect,
                 visible_screen_space_rect,
                 screen_background_color,
                 false);
  }
}

static RenderPass* FindRenderPassById(const RenderPassList& list,
                                      RenderPassId id) {
  auto it = std::find_if(
      list.begin(), list.end(),
      [id](const std::unique_ptr<RenderPass>& p) { return p->id == id; });
  return it == list.end() ? nullptr : it->get();
}

DrawResult LayerTreeHostImpl::CalculateRenderPasses(
    FrameData* frame) {
  DCHECK(frame->render_passes.empty());
  DCHECK(CanDraw());
  DCHECK(!active_tree_->LayerListIsEmpty());

  TrackDamageForAllSurfaces(*frame->render_surface_layer_list);

  // If the root render surface has no visible damage, then don't generate a
  // frame at all.
  RenderSurfaceImpl* root_surface = active_tree_->RootRenderSurface();
  bool root_surface_has_no_visible_damage =
      !root_surface->damage_tracker()->current_damage_rect().Intersects(
          root_surface->content_rect());
  bool root_surface_has_contributing_layers =
      !root_surface->layer_list().empty();
  bool hud_wants_to_draw_ = active_tree_->hud_layer() &&
                            active_tree_->hud_layer()->IsAnimatingHUDContents();
  if (root_surface_has_contributing_layers &&
      root_surface_has_no_visible_damage &&
      !active_tree_->property_trees()->effect_tree.HasCopyRequests() &&
      !output_surface_->capabilities().can_force_reclaim_resources &&
      !hud_wants_to_draw_) {
    TRACE_EVENT0("cc",
                 "LayerTreeHostImpl::CalculateRenderPasses::EmptyDamageRect");
    frame->has_no_damage = true;
    DCHECK(!resourceless_software_draw_);
    return DRAW_SUCCESS;
  }

  TRACE_EVENT_BEGIN2(
      "cc", "LayerTreeHostImpl::CalculateRenderPasses",
      "render_surface_layer_list.size()",
      static_cast<uint64_t>(frame->render_surface_layer_list->size()),
      "RequiresHighResToDraw", RequiresHighResToDraw());

  // Create the render passes in dependency order.
  size_t render_surface_layer_list_size =
      frame->render_surface_layer_list->size();
  for (size_t i = 0; i < render_surface_layer_list_size; ++i) {
    size_t surface_index = render_surface_layer_list_size - 1 - i;
    LayerImpl* render_surface_layer =
        (*frame->render_surface_layer_list)[surface_index];
    RenderSurfaceImpl* render_surface = render_surface_layer->render_surface();

    bool should_draw_into_render_pass =
        active_tree_->IsRootLayer(render_surface_layer) ||
        render_surface->contributes_to_drawn_surface() ||
        render_surface->HasCopyRequest();
    if (should_draw_into_render_pass)
      render_surface->AppendRenderPasses(frame);
  }

  // Damage rects for non-root passes aren't meaningful, so set them to be
  // equal to the output rect.
  for (size_t i = 0; i + 1 < frame->render_passes.size(); ++i) {
    RenderPass* pass = frame->render_passes[i].get();
    pass->damage_rect = pass->output_rect;
  }

  // When we are displaying the HUD, change the root damage rect to cover the
  // entire root surface. This will disable partial-swap/scissor optimizations
  // that would prevent the HUD from updating, since the HUD does not cause
  // damage itself, to prevent it from messing with damage visualizations. Since
  // damage visualizations are done off the LayerImpls and RenderSurfaceImpls,
  // changing the RenderPass does not affect them.
  if (active_tree_->hud_layer()) {
    RenderPass* root_pass = frame->render_passes.back().get();
    root_pass->damage_rect = root_pass->output_rect;
  }

  // Grab this region here before iterating layers. Taking copy requests from
  // the layers while constructing the render passes will dirty the render
  // surface layer list and this unoccluded region, flipping the dirty bit to
  // true, and making us able to query for it without doing
  // UpdateDrawProperties again. The value inside the Region is not actually
  // changed until UpdateDrawProperties happens, so a reference to it is safe.
  const Region& unoccluded_screen_space_region =
      active_tree_->UnoccludedScreenSpaceRegion();

  // Typically when we are missing a texture and use a checkerboard quad, we
  // still draw the frame. However when the layer being checkerboarded is moving
  // due to an impl-animation, we drop the frame to avoid flashing due to the
  // texture suddenly appearing in the future.
  DrawResult draw_result = DRAW_SUCCESS;

  int layers_drawn = 0;

  const DrawMode draw_mode = GetDrawMode();

  int num_missing_tiles = 0;
  int num_incomplete_tiles = 0;
  int64_t checkerboarded_no_recording_content_area = 0;
  int64_t checkerboarded_needs_raster_content_area = 0;
  bool have_copy_request =
      active_tree()->property_trees()->effect_tree.HasCopyRequests();
  bool have_missing_animated_tiles = false;

  LayerIterator end = LayerIterator::End(frame->render_surface_layer_list);
  for (LayerIterator it =
           LayerIterator::Begin(frame->render_surface_layer_list);
       it != end; ++it) {
    RenderPassId target_render_pass_id =
        it.target_render_surface_layer()->render_surface()->GetRenderPassId();
    RenderPass* target_render_pass =
        FindRenderPassById(frame->render_passes, target_render_pass_id);

    AppendQuadsData append_quads_data;

    if (it.represents_target_render_surface()) {
      if (it->render_surface()->HasCopyRequest()) {
        active_tree()
            ->property_trees()
            ->effect_tree.TakeCopyRequestsAndTransformToSurface(
                it->render_surface()->EffectTreeIndex(),
                &target_render_pass->copy_requests);
      }
    } else if (it.represents_contributing_render_surface() &&
               it->render_surface()->contributes_to_drawn_surface()) {
      RenderPassId contributing_render_pass_id =
          it->render_surface()->GetRenderPassId();
      RenderPass* contributing_render_pass =
          FindRenderPassById(frame->render_passes, contributing_render_pass_id);
      AppendQuadsForRenderSurfaceLayer(target_render_pass,
                                       *it,
                                       contributing_render_pass,
                                       &append_quads_data);
    } else if (it.represents_itself() && !it->visible_layer_rect().IsEmpty()) {
      bool occluded =
          it->draw_properties().occlusion_in_content_space.IsOccluded(
              it->visible_layer_rect());
      if (!occluded && it->WillDraw(draw_mode, resource_provider_.get())) {
        DCHECK_EQ(active_tree_.get(), it->layer_tree_impl());

        frame->will_draw_layers.push_back(*it);

        it->AppendQuads(target_render_pass, &append_quads_data);
      }

      ++layers_drawn;
    }

    rendering_stats_instrumentation_->AddVisibleContentArea(
        append_quads_data.visible_layer_area);
    rendering_stats_instrumentation_->AddApproximatedVisibleContentArea(
        append_quads_data.approximated_visible_content_area);
    rendering_stats_instrumentation_->AddCheckerboardedVisibleContentArea(
        append_quads_data.checkerboarded_visible_content_area);
    rendering_stats_instrumentation_->AddCheckerboardedNoRecordingContentArea(
        append_quads_data.checkerboarded_no_recording_content_area);
    rendering_stats_instrumentation_->AddCheckerboardedNeedsRasterContentArea(
        append_quads_data.checkerboarded_needs_raster_content_area);

    num_missing_tiles += append_quads_data.num_missing_tiles;
    num_incomplete_tiles += append_quads_data.num_incomplete_tiles;
    checkerboarded_no_recording_content_area +=
        append_quads_data.checkerboarded_no_recording_content_area;
    checkerboarded_needs_raster_content_area +=
        append_quads_data.checkerboarded_needs_raster_content_area;
    if (append_quads_data.num_missing_tiles > 0) {
      have_missing_animated_tiles |=
          !it->was_ever_ready_since_last_transform_animation() ||
          it->screen_space_transform_is_animating();
    } else {
      it->set_was_ever_ready_since_last_transform_animation(true);
    }
  }

  // If CommitToActiveTree() is true, then we wait to draw until
  // NotifyReadyToDraw. That means we're in as good shape as is possible now,
  // so there's no reason to stop the draw now (and this is not supported by
  // SingleThreadProxy).
  if (have_missing_animated_tiles && !CommitToActiveTree())
    draw_result = DRAW_ABORTED_CHECKERBOARD_ANIMATIONS;

  // When we require high res to draw, abort the draw (almost) always. This does
  // not cause the scheduler to do a main frame, instead it will continue to try
  // drawing until we finally complete, so the copy request will not be lost.
  // TODO(weiliangc): Remove RequiresHighResToDraw. crbug.com/469175
  if (num_incomplete_tiles || num_missing_tiles) {
    if (RequiresHighResToDraw())
      draw_result = DRAW_ABORTED_MISSING_HIGH_RES_CONTENT;
  }

  // When doing a resourceless software draw, we don't have control over the
  // surface the compositor draws to, so even though the frame may not be
  // complete, the previous frame has already been potentially lost, so an
  // incomplete frame is better than nothing, so this takes highest precidence.
  if (resourceless_software_draw_)
    draw_result = DRAW_SUCCESS;

#if DCHECK_IS_ON()
  for (const auto& render_pass : frame->render_passes) {
    for (const auto& quad : render_pass->quad_list)
      DCHECK(quad->shared_quad_state);
  }
  DCHECK(frame->render_passes.back()->output_rect.origin().IsOrigin());
#endif

  if (!active_tree_->has_transparent_background()) {
    frame->render_passes.back()->has_transparent_background = false;
    AppendQuadsToFillScreen(
        active_tree_->RootScrollLayerDeviceViewportBounds(),
        frame->render_passes.back().get(), active_tree_->RootRenderSurface(),
        active_tree_->background_color(), unoccluded_screen_space_region);
  }

  RemoveRenderPasses(frame);
  renderer_->DecideRenderPassAllocationsForFrame(frame->render_passes);

  if (have_copy_request) {
    // Any copy requests left in the tree are not going to get serviced, and
    // should be aborted.
    active_tree()->property_trees()->effect_tree.ClearCopyRequests();

    // Draw properties depend on copy requests.
    active_tree()->set_needs_update_draw_properties();
  }

  // If we're making a frame to draw, it better have at least one render pass.
  DCHECK(!frame->render_passes.empty());

  if (active_tree_->has_ever_been_drawn()) {
    UMA_HISTOGRAM_COUNTS_100(
        "Compositing.RenderPass.AppendQuadData.NumMissingTiles",
        num_missing_tiles);
    UMA_HISTOGRAM_COUNTS_100(
        "Compositing.RenderPass.AppendQuadData.NumIncompleteTiles",
        num_incomplete_tiles);
    UMA_HISTOGRAM_COUNTS(
        "Compositing.RenderPass.AppendQuadData."
        "CheckerboardedNoRecordingContentArea",
        checkerboarded_no_recording_content_area);
    UMA_HISTOGRAM_COUNTS(
        "Compositing.RenderPass.AppendQuadData."
        "CheckerboardedNeedRasterContentArea",
        checkerboarded_needs_raster_content_area);
  }

  // Should only have one render pass in resourceless software mode.
  DCHECK(draw_mode != DRAW_MODE_RESOURCELESS_SOFTWARE ||
         frame->render_passes.size() == 1u)
      << frame->render_passes.size();

  TRACE_EVENT_END2("cc", "LayerTreeHostImpl::CalculateRenderPasses",
                   "draw_result", draw_result, "missing tiles",
                   num_missing_tiles);

  // Draw has to be successful to not drop the copy request layer.
  // When we have a copy request for a layer, we need to draw even if there
  // would be animating checkerboards, because failing under those conditions
  // triggers a new main frame, which may cause the copy request layer to be
  // destroyed.
  // TODO(weiliangc): Test copy request w/ output surface recreation. Would
  // trigger this DCHECK.
  DCHECK(!have_copy_request || draw_result == DRAW_SUCCESS);

  // TODO(crbug.com/564832): This workaround to prevent creating unnecessarily
  // persistent render passes. When a copy request is made, it may force a
  // separate render pass for the layer, which will persist until a new commit
  // removes it. Force a commit after copy requests, to remove extra render
  // passes.
  if (have_copy_request)
    client_->SetNeedsCommitOnImplThread();

  return draw_result;
}

void LayerTreeHostImpl::MainThreadHasStoppedFlinging() {
  top_controls_manager_->MainThreadHasStoppedFlinging();
  if (input_handler_client_)
    input_handler_client_->MainThreadHasStoppedFlinging();
}

void LayerTreeHostImpl::DidAnimateScrollOffset() {
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
}

void LayerTreeHostImpl::SetViewportDamage(const gfx::Rect& damage_rect) {
  viewport_damage_rect_.Union(damage_rect);
}

DrawResult LayerTreeHostImpl::PrepareToDraw(FrameData* frame) {
  TRACE_EVENT1("cc",
               "LayerTreeHostImpl::PrepareToDraw",
               "SourceFrameNumber",
               active_tree_->source_frame_number());
  if (input_handler_client_)
    input_handler_client_->ReconcileElasticOverscrollAndRootScroll();

  if (const char* client_name = GetClientNameForMetrics()) {
    size_t total_picture_memory = 0;
    for (const PictureLayerImpl* layer : active_tree()->picture_layers())
      total_picture_memory += layer->GetRasterSource()->GetPictureMemoryUsage();
    if (total_picture_memory != 0) {
      // GetClientNameForMetrics only returns one non-null value over the
      // lifetime of the process, so this histogram name is runtime constant.
      UMA_HISTOGRAM_COUNTS(
          base::StringPrintf("Compositing.%s.PictureMemoryUsageKb",
                             client_name),
          base::saturated_cast<int>(total_picture_memory / 1024));
    }
    // GetClientNameForMetrics only returns one non-null value over the lifetime
    // of the process, so this histogram name is runtime constant.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        base::StringPrintf("Compositing.%s.NumActiveLayers", client_name),
        base::saturated_cast<int>(active_tree_->NumLayers()), 1, 400, 20);
  }

  bool update_lcd_text = false;
  bool ok = active_tree_->UpdateDrawProperties(update_lcd_text);
  DCHECK(ok) << "UpdateDrawProperties failed during draw";

  // This will cause NotifyTileStateChanged() to be called for any tiles that
  // completed, which will add damage for visible tiles to the frame for them so
  // they appear as part of the current frame being drawn.
  tile_manager_.Flush();

  frame->render_surface_layer_list = &active_tree_->RenderSurfaceLayerList();
  frame->render_passes.clear();
  frame->will_draw_layers.clear();
  frame->has_no_damage = false;

  if (active_tree_->RootRenderSurface()) {
    gfx::Rect device_viewport_damage_rect = viewport_damage_rect_;
    viewport_damage_rect_ = gfx::Rect();

    active_tree_->RootRenderSurface()->damage_tracker()->AddDamageNextUpdate(
        device_viewport_damage_rect);
  }

  DrawResult draw_result = CalculateRenderPasses(frame);
  if (draw_result != DRAW_SUCCESS) {
    DCHECK(!resourceless_software_draw_);
    return draw_result;
  }

  // If we return DRAW_SUCCESS, then we expect DrawLayers() to be called before
  // this function is called again.
  return draw_result;
}

void LayerTreeHostImpl::RemoveRenderPasses(FrameData* frame) {
  // There is always at least a root RenderPass.
  DCHECK_GE(frame->render_passes.size(), 1u);

  // A set of RenderPasses that we have seen.
  std::set<RenderPassId> pass_exists;
  // A set of RenderPassDrawQuads that we have seen (stored by the RenderPasses
  // they refer to).
  base::SmallMap<std::unordered_map<RenderPassId, int, RenderPassIdHash>>
      pass_references;

  // Iterate RenderPasses in draw order, removing empty render passes (except
  // the root RenderPass).
  for (size_t i = 0; i < frame->render_passes.size(); ++i) {
    RenderPass* pass = frame->render_passes[i].get();

    // Remove orphan RenderPassDrawQuads.
    for (auto it = pass->quad_list.begin(); it != pass->quad_list.end();) {
      if (it->material != DrawQuad::RENDER_PASS) {
        ++it;
        continue;
      }
      const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(*it);
      // If the RenderPass doesn't exist, we can remove the quad.
      if (pass_exists.count(quad->render_pass_id)) {
        // Otherwise, save a reference to the RenderPass so we know there's a
        // quad using it.
        pass_references[quad->render_pass_id]++;
        ++it;
      } else {
        it = pass->quad_list.EraseAndInvalidateAllPointers(it);
      }
    }

    if (i == frame->render_passes.size() - 1) {
      // Don't remove the root RenderPass.
      break;
    }

    if (pass->quad_list.empty() && pass->copy_requests.empty()) {
      // Remove the pass and decrement |i| to counter the for loop's increment,
      // so we don't skip the next pass in the loop.
      frame->render_passes.erase(frame->render_passes.begin() + i);
      --i;
      continue;
    }

    pass_exists.insert(pass->id);
  }

  // Remove RenderPasses that are not referenced by any draw quads or copy
  // requests (except the root RenderPass).
  for (size_t i = 0; i < frame->render_passes.size() - 1; ++i) {
    // Iterating from the back of the list to the front, skipping over the
    // back-most (root) pass, in order to remove each qualified RenderPass, and
    // drop references to earlier RenderPasses allowing them to be removed to.
    RenderPass* pass =
        frame->render_passes[frame->render_passes.size() - 2 - i].get();
    if (!pass->copy_requests.empty())
      continue;
    if (pass_references[pass->id])
      continue;

    for (auto it = pass->quad_list.begin(); it != pass->quad_list.end(); ++it) {
      if (it->material != DrawQuad::RENDER_PASS)
        continue;
      const RenderPassDrawQuad* quad = RenderPassDrawQuad::MaterialCast(*it);
      pass_references[quad->render_pass_id]--;
    }

    frame->render_passes.erase(frame->render_passes.end() - 2 - i);
    --i;
  }
}

void LayerTreeHostImpl::EvictTexturesForTesting() {
  UpdateTileManagerMemoryPolicy(ManagedMemoryPolicy(0));
}

void LayerTreeHostImpl::BlockNotifyReadyToActivateForTesting(bool block) {
  NOTREACHED();
}

void LayerTreeHostImpl::ResetTreesForTesting() {
  if (active_tree_)
    active_tree_->DetachLayers();
  active_tree_ =
      LayerTreeImpl::create(this, active_tree()->page_scale_factor(),
                            active_tree()->top_controls_shown_ratio(),
                            active_tree()->elastic_overscroll());
  active_tree_->property_trees()->is_active = true;
  if (pending_tree_)
    pending_tree_->DetachLayers();
  pending_tree_ = nullptr;
  pending_tree_duration_timer_ = nullptr;
  if (recycle_tree_)
    recycle_tree_->DetachLayers();
  recycle_tree_ = nullptr;
}

size_t LayerTreeHostImpl::SourceAnimationFrameNumberForTesting() const {
  return fps_counter_->current_frame_number();
}

void LayerTreeHostImpl::UpdateTileManagerMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  if (!resource_pool_)
    return;

  global_tile_state_.hard_memory_limit_in_bytes = 0;
  global_tile_state_.soft_memory_limit_in_bytes = 0;
  if (visible_ && policy.bytes_limit_when_visible > 0) {
    global_tile_state_.hard_memory_limit_in_bytes =
        policy.bytes_limit_when_visible;
    global_tile_state_.soft_memory_limit_in_bytes =
        (static_cast<int64_t>(global_tile_state_.hard_memory_limit_in_bytes) *
         settings_.max_memory_for_prepaint_percentage) /
        100;
  }
  global_tile_state_.memory_limit_policy =
      ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(
          visible_ ?
          policy.priority_cutoff_when_visible :
          gpu::MemoryAllocation::CUTOFF_ALLOW_NOTHING);
  global_tile_state_.num_resources_limit = policy.num_resources_limit;

  if (global_tile_state_.hard_memory_limit_in_bytes > 0) {
    // If |global_tile_state_.hard_memory_limit_in_bytes| is greater than 0, we
    // allow the worker context and image decode controller to retain allocated
    // resources. Notify them here. If the memory policy has become zero, we'll
    // handle the notification in NotifyAllTileTasksCompleted, after
    // in-progress work finishes.
    if (output_surface_) {
      output_surface_->SetWorkerContextShouldAggressivelyFreeResources(
          false /* aggressively_free_resources */);
    }

    if (image_decode_controller_) {
      image_decode_controller_->SetShouldAggressivelyFreeResources(
          false /* aggressively_free_resources */);
    }
  }

  DCHECK(resource_pool_);
  resource_pool_->CheckBusyResources();
  // Soft limit is used for resource pool such that memory returns to soft
  // limit after going over.
  resource_pool_->SetResourceUsageLimits(
      global_tile_state_.soft_memory_limit_in_bytes,
      global_tile_state_.num_resources_limit);

  DidModifyTilePriorities();
}

void LayerTreeHostImpl::DidModifyTilePriorities() {
  // Mark priorities as dirty and schedule a PrepareTiles().
  tile_priorities_dirty_ = true;
  client_->SetNeedsPrepareTilesOnImplThread();
}

std::unique_ptr<RasterTilePriorityQueue> LayerTreeHostImpl::BuildRasterQueue(
    TreePriority tree_priority,
    RasterTilePriorityQueue::Type type) {
  TRACE_EVENT0("disabled-by-default-cc.debug",
               "LayerTreeHostImpl::BuildRasterQueue");

  return RasterTilePriorityQueue::Create(active_tree_->picture_layers(),
                                         pending_tree_
                                             ? pending_tree_->picture_layers()
                                             : std::vector<PictureLayerImpl*>(),
                                         tree_priority, type);
}

std::unique_ptr<EvictionTilePriorityQueue>
LayerTreeHostImpl::BuildEvictionQueue(TreePriority tree_priority) {
  TRACE_EVENT0("disabled-by-default-cc.debug",
               "LayerTreeHostImpl::BuildEvictionQueue");

  std::unique_ptr<EvictionTilePriorityQueue> queue(
      new EvictionTilePriorityQueue);
  queue->Build(active_tree_->picture_layers(),
               pending_tree_ ? pending_tree_->picture_layers()
                             : std::vector<PictureLayerImpl*>(),
               tree_priority);
  return queue;
}

void LayerTreeHostImpl::SetIsLikelyToRequireADraw(
    bool is_likely_to_require_a_draw) {
  // Proactively tell the scheduler that we expect to draw within each vsync
  // until we get all the tiles ready to draw. If we happen to miss a required
  // for draw tile here, then we will miss telling the scheduler each frame that
  // we intend to draw so it may make worse scheduling decisions.
  is_likely_to_require_a_draw_ = is_likely_to_require_a_draw;
}

void LayerTreeHostImpl::NotifyReadyToActivate() {
  client_->NotifyReadyToActivate();
}

void LayerTreeHostImpl::NotifyReadyToDraw() {
  // Tiles that are ready will cause NotifyTileStateChanged() to be called so we
  // don't need to schedule a draw here. Just stop WillBeginImplFrame() from
  // causing optimistic requests to draw a frame.
  is_likely_to_require_a_draw_ = false;

  client_->NotifyReadyToDraw();
}

void LayerTreeHostImpl::NotifyAllTileTasksCompleted() {
  // The tile tasks started by the most recent call to PrepareTiles have
  // completed. Now is a good time to free resources if necessary.
  if (global_tile_state_.hard_memory_limit_in_bytes == 0) {
    // Free image decode controller resources before worker context resources.
    // This ensures that the imaged decode controller has released all Skia refs
    // at the time Skia's cleanup executes (within worker context's cleanup).
    if (image_decode_controller_) {
      image_decode_controller_->SetShouldAggressivelyFreeResources(
          true /* aggressively_free_resources */);
    }
    if (output_surface_) {
      output_surface_->SetWorkerContextShouldAggressivelyFreeResources(
          true /* aggressively_free_resources */);
    }
  }
}

void LayerTreeHostImpl::NotifyTileStateChanged(const Tile* tile) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::NotifyTileStateChanged");

  if (active_tree_) {
    LayerImpl* layer_impl =
        active_tree_->FindActiveTreeLayerById(tile->layer_id());
    if (layer_impl)
      layer_impl->NotifyTileStateChanged(tile);
  }

  if (pending_tree_) {
    LayerImpl* layer_impl =
        pending_tree_->FindPendingTreeLayerById(tile->layer_id());
    if (layer_impl)
      layer_impl->NotifyTileStateChanged(tile);
  }

  // Check for a non-null active tree to avoid doing this during shutdown.
  if (active_tree_ && !client_->IsInsideDraw() && tile->required_for_draw()) {
    // The LayerImpl::NotifyTileStateChanged() should damage the layer, so this
    // redraw will make those tiles be displayed.
    SetNeedsRedraw();
  }
}

void LayerTreeHostImpl::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  SetManagedMemoryPolicy(policy);

  // This is short term solution to synchronously drop tile resources when
  // using synchronous compositing to avoid memory usage regression.
  // TODO(boliu): crbug.com/499004 to track removing this.
  if (!policy.bytes_limit_when_visible && resource_pool_ &&
      settings_.using_synchronous_renderer_compositor) {
    ReleaseTreeResources();
    CleanUpTileManagerAndUIResources();

    // Force a call to NotifyAllTileTasks completed - otherwise this logic may
    // be skipped if no work was enqueued at the time the tile manager was
    // destroyed.
    NotifyAllTileTasksCompleted();

    CreateTileManagerResources();
    RecreateTreeResources();
  }
}

void LayerTreeHostImpl::SetTreeActivationCallback(
    const base::Closure& callback) {
  DCHECK(task_runner_provider_->IsImplThread());
  tree_activation_callback_ = callback;
}

void LayerTreeHostImpl::SetManagedMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  if (cached_managed_memory_policy_ == policy)
    return;

  ManagedMemoryPolicy old_policy = ActualManagedMemoryPolicy();

  cached_managed_memory_policy_ = policy;
  ManagedMemoryPolicy actual_policy = ActualManagedMemoryPolicy();

  if (old_policy == actual_policy)
    return;

  if (!task_runner_provider_->HasImplThread()) {
    // In single-thread mode, this can be called on the main thread by
    // GLRenderer::OnMemoryAllocationChanged.
    DebugScopedSetImplThread impl_thread(task_runner_provider_);
    UpdateTileManagerMemoryPolicy(actual_policy);
  } else {
    DCHECK(task_runner_provider_->IsImplThread());
    UpdateTileManagerMemoryPolicy(actual_policy);
  }

  // If there is already enough memory to draw everything imaginable and the
  // new memory limit does not change this, then do not re-commit. Don't bother
  // skipping commits if this is not visible (commits don't happen when not
  // visible, there will almost always be a commit when this becomes visible).
  bool needs_commit = true;
  if (visible() &&
      actual_policy.bytes_limit_when_visible >= max_memory_needed_bytes_ &&
      old_policy.bytes_limit_when_visible >= max_memory_needed_bytes_ &&
      actual_policy.priority_cutoff_when_visible ==
          old_policy.priority_cutoff_when_visible) {
    needs_commit = false;
  }

  if (needs_commit)
    client_->SetNeedsCommitOnImplThread();
}

void LayerTreeHostImpl::SetExternalTilePriorityConstraints(
    const gfx::Rect& viewport_rect,
    const gfx::Transform& transform) {
  gfx::Rect viewport_rect_for_tile_priority_in_view_space;
  gfx::Transform screen_to_view(gfx::Transform::kSkipInitialization);
  if (transform.GetInverse(&screen_to_view)) {
    // Convert from screen space to view space.
    viewport_rect_for_tile_priority_in_view_space =
        MathUtil::ProjectEnclosingClippedRect(screen_to_view, viewport_rect);
  }

  const bool tile_priority_params_changed =
      viewport_rect_for_tile_priority_ !=
      viewport_rect_for_tile_priority_in_view_space;

  viewport_rect_for_tile_priority_ =
      viewport_rect_for_tile_priority_in_view_space;

  if (tile_priority_params_changed) {
    active_tree_->set_needs_update_draw_properties();
    if (pending_tree_)
      pending_tree_->set_needs_update_draw_properties();

    // Compositor, not OutputSurface, is responsible for setting damage and
    // triggering redraw for constraint changes.
    SetFullRootLayerDamage();
    SetNeedsRedraw();
  }
}

void LayerTreeHostImpl::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  if (damage_rect.IsEmpty())
    return;
  NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  client_->SetNeedsRedrawRectOnImplThread(damage_rect);
}

void LayerTreeHostImpl::DidSwapBuffers() {
  client_->DidSwapBuffersOnImplThread();
}

void LayerTreeHostImpl::DidSwapBuffersComplete() {
  client_->DidSwapBuffersCompleteOnImplThread();
}

void LayerTreeHostImpl::DidReceiveTextureInUseResponses(
    const gpu::TextureInUseResponses& responses) {
  NOTREACHED();
}

void LayerTreeHostImpl::ReclaimResources(const CompositorFrameAck* ack) {
  // TODO(piman): We may need to do some validation on this ack before
  // processing it.
  if (renderer_)
    renderer_->ReceiveSwapBuffersAck(*ack);

  // In OOM, we now might be able to release more resources that were held
  // because they were exported.
  if (resource_pool_) {
    if (resource_pool_->memory_usage_bytes()) {
      const size_t kMegabyte = 1024 * 1024;

      // This is a good time to log memory usage. A chunk of work has just
      // completed but none of the memory used for that work has likely been
      // freed.
      UMA_HISTOGRAM_MEMORY_MB(
          "Renderer4.ResourcePoolMemoryUsage",
          static_cast<int>(resource_pool_->memory_usage_bytes() / kMegabyte));
    }

    resource_pool_->CheckBusyResources();
    resource_pool_->ReduceResourceUsage();
  }

  // If we're not visible, we likely released resources, so we want to
  // aggressively flush here to make sure those DeleteTextures make it to the
  // GPU process to free up the memory.
  if (output_surface_->context_provider() && !visible_) {
    output_surface_->context_provider()->ContextGL()->ShallowFlushCHROMIUM();
  }
}

void LayerTreeHostImpl::OnDraw(const gfx::Transform& transform,
                               const gfx::Rect& viewport,
                               const gfx::Rect& clip,
                               bool resourceless_software_draw) {
  DCHECK(!resourceless_software_draw_);
  const bool transform_changed = external_transform_ != transform;
  const bool viewport_changed = external_viewport_ != viewport;
  const bool clip_changed = external_clip_ != clip;

  external_transform_ = transform;
  external_viewport_ = viewport;
  external_clip_ = clip;

  {
    base::AutoReset<bool> resourceless_software_draw_reset(
        &resourceless_software_draw_, resourceless_software_draw);

    // For resourceless software draw, always set full damage to ensure they
    // always swap. Otherwise, need to set redraw for any changes to draw
    // parameters.
    const bool draw_params_changed =
        transform_changed || viewport_changed || clip_changed;
    if (resourceless_software_draw_ || draw_params_changed) {
      SetFullRootLayerDamage();
      SetNeedsRedraw();
    }

    // UpdateDrawProperties does not depend on clip.
    if (transform_changed || viewport_changed || resourceless_software_draw_) {
      active_tree_->set_needs_update_draw_properties();
    }

    if (resourceless_software_draw) {
      client_->OnCanDrawStateChanged(CanDraw());
    }

    client_->OnDrawForOutputSurface(resourceless_software_draw_);
  }

  if (resourceless_software_draw) {
    active_tree_->set_needs_update_draw_properties();
    client_->OnCanDrawStateChanged(CanDraw());
    // This draw may have reset all damage, which would lead to subsequent
    // incorrect hardware draw, so explicitly set damage for next hardware
    // draw as well.
    SetFullRootLayerDamage();
  }
}

void LayerTreeHostImpl::OnCanDrawStateChangedForTree() {
  client_->OnCanDrawStateChanged(CanDraw());
}

CompositorFrameMetadata LayerTreeHostImpl::MakeCompositorFrameMetadata() const {
  CompositorFrameMetadata metadata;
  metadata.device_scale_factor = active_tree_->painted_device_scale_factor() *
                                 active_tree_->device_scale_factor();

  metadata.page_scale_factor = active_tree_->current_page_scale_factor();
  metadata.scrollable_viewport_size = active_tree_->ScrollableViewportSize();
  metadata.root_layer_size = active_tree_->ScrollableSize();
  metadata.min_page_scale_factor = active_tree_->min_page_scale_factor();
  metadata.max_page_scale_factor = active_tree_->max_page_scale_factor();
  metadata.location_bar_offset =
      gfx::Vector2dF(0.f, top_controls_manager_->ControlsTopOffset());
  metadata.location_bar_content_translation =
      gfx::Vector2dF(0.f, top_controls_manager_->ContentTopOffset());
  metadata.root_background_color = active_tree_->background_color();

  active_tree_->GetViewportSelection(&metadata.selection);

  if (OuterViewportScrollLayer()) {
    metadata.root_overflow_x_hidden =
        !OuterViewportScrollLayer()->user_scrollable_horizontal();
    metadata.root_overflow_y_hidden =
        !OuterViewportScrollLayer()->user_scrollable_vertical();
  }

  for (LayerImpl* surface_layer : active_tree_->SurfaceLayers()) {
    metadata.referenced_surfaces.push_back(
        static_cast<SurfaceLayerImpl*>(surface_layer)->surface_id());
  }
  if (!InnerViewportScrollLayer())
    return metadata;

  metadata.root_overflow_x_hidden |=
      !InnerViewportScrollLayer()->user_scrollable_horizontal();
  metadata.root_overflow_y_hidden |=
      !InnerViewportScrollLayer()->user_scrollable_vertical();

  // TODO(miletus) : Change the metadata to hold ScrollOffset.
  metadata.root_scroll_offset = gfx::ScrollOffsetToVector2dF(
      active_tree_->TotalScrollOffset());

  return metadata;
}

void LayerTreeHostImpl::DrawLayers(FrameData* frame) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::DrawLayers");

  base::TimeTicks frame_begin_time = CurrentBeginFrameArgs().frame_time;
  DCHECK(CanDraw());

  if (frame->has_no_damage) {
    TRACE_EVENT_INSTANT0("cc", "EarlyOut_NoDamage", TRACE_EVENT_SCOPE_THREAD);
    DCHECK(!resourceless_software_draw_);
    return;
  }

  DCHECK(!frame->render_passes.empty());

  fps_counter_->SaveTimeStamp(frame_begin_time,
                              !output_surface_->context_provider());
  rendering_stats_instrumentation_->IncrementFrameCount(1);

  memory_history_->SaveEntry(tile_manager_.memory_stats_from_last_assign());

  if (debug_state_.ShowHudRects()) {
    debug_rect_history_->SaveDebugRectsForCurrentFrame(
        active_tree(), active_tree_->hud_layer(),
        *frame->render_surface_layer_list, debug_state_);
  }

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (is_new_trace) {
    if (pending_tree_) {
      LayerTreeHostCommon::CallFunctionForEveryLayer(
          pending_tree(), [](LayerImpl* layer) { layer->DidBeginTracing(); });
    }
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        active_tree(), [](LayerImpl* layer) { layer->DidBeginTracing(); });
  }

  {
    TRACE_EVENT0("cc", "DrawLayers.FrameViewerTracing");
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        frame_viewer_instrumentation::kCategoryLayerTree,
        "cc::LayerTreeHostImpl", id_, AsValueWithFrame(frame));
  }

  const DrawMode draw_mode = GetDrawMode();

  // Because the contents of the HUD depend on everything else in the frame, the
  // contents of its texture are updated as the last thing before the frame is
  // drawn.
  if (active_tree_->hud_layer()) {
    TRACE_EVENT0("cc", "DrawLayers.UpdateHudTexture");
    active_tree_->hud_layer()->UpdateHudTexture(draw_mode,
                                                resource_provider_.get());
  }

  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE) {
    bool disable_picture_quad_image_filtering =
        IsActivelyScrolling() || animation_host_->NeedsAnimateLayers();

    // We must disable the image hijack canvas when using GPU rasterization but
    // performing a resourceless software draw. Otherwise, we will attempt to
    // use the GPU ImageDecodeController during software raster.
    bool use_image_hijack_canvas = !use_gpu_rasterization_;

    std::unique_ptr<SoftwareRenderer> temp_software_renderer =
        SoftwareRenderer::Create(this, &settings_.renderer_settings,
                                 output_surface_, nullptr,
                                 use_image_hijack_canvas);
    temp_software_renderer->DrawFrame(
        &frame->render_passes, active_tree_->device_scale_factor(),
        gfx::ColorSpace(), DeviceViewport(), DeviceClip(),
        disable_picture_quad_image_filtering);
  } else {
    renderer_->DrawFrame(&frame->render_passes,
                         active_tree_->device_scale_factor(), gfx::ColorSpace(),
                         DeviceViewport(), DeviceClip(), false);
  }
  // The render passes should be consumed by the renderer.
  DCHECK(frame->render_passes.empty());

  // The next frame should start by assuming nothing has changed, and changes
  // are noted as they occur.
  // TODO(boliu): If we did a temporary software renderer frame, propogate the
  // damage forward to the next frame.
  for (size_t i = 0; i < frame->render_surface_layer_list->size(); i++) {
    (*frame->render_surface_layer_list)[i]->render_surface()->damage_tracker()->
        DidDrawDamagedArea();
  }
  active_tree_->ResetAllChangeTracking();

  active_tree_->set_has_ever_been_drawn(true);
  devtools_instrumentation::DidDrawFrame(id_);
  benchmark_instrumentation::IssueImplThreadRenderingStatsEvent(
      rendering_stats_instrumentation_->impl_thread_rendering_stats());
  rendering_stats_instrumentation_->AccumulateAndClearImplThreadStats();
}

void LayerTreeHostImpl::DidDrawAllLayers(const FrameData& frame) {
  for (size_t i = 0; i < frame.will_draw_layers.size(); ++i)
    frame.will_draw_layers[i]->DidDraw(resource_provider_.get());

  for (auto& it : video_frame_controllers_)
    it->DidDrawFrame();
}

void LayerTreeHostImpl::FinishAllRendering() {
  if (renderer_)
    renderer_->Finish();
}

int LayerTreeHostImpl::RequestedMSAASampleCount() const {
  if (settings_.gpu_rasterization_msaa_sample_count == -1) {
    // Use the most up-to-date version of device_scale_factor that we have.
    float device_scale_factor = pending_tree_
                                    ? pending_tree_->device_scale_factor()
                                    : active_tree_->device_scale_factor();
    return device_scale_factor >= 2.0f ? 4 : 8;
  }

  return settings_.gpu_rasterization_msaa_sample_count;
}

bool LayerTreeHostImpl::CanUseGpuRasterization() {
  if (!(output_surface_ && output_surface_->context_provider() &&
        output_surface_->worker_context_provider()))
    return false;

  ContextProvider* context_provider =
      output_surface_->worker_context_provider();
  ContextProvider::ScopedContextLock scoped_context(context_provider);
  if (!context_provider->GrContext())
    return false;

  return true;
}

void LayerTreeHostImpl::UpdateGpuRasterizationStatus() {
  bool use_gpu = false;
  bool use_msaa = false;
  bool using_msaa_for_complex_content =
      renderer() && RequestedMSAASampleCount() > 0 &&
      GetRendererCapabilities().max_msaa_samples >= RequestedMSAASampleCount();
  if (settings_.gpu_rasterization_forced) {
    use_gpu = true;
    gpu_rasterization_status_ = GpuRasterizationStatus::ON_FORCED;
    use_msaa = !content_is_suitable_for_gpu_rasterization_ &&
               using_msaa_for_complex_content;
    if (use_msaa) {
      gpu_rasterization_status_ = GpuRasterizationStatus::MSAA_CONTENT;
    }
  } else if (!settings_.gpu_rasterization_enabled) {
    gpu_rasterization_status_ = GpuRasterizationStatus::OFF_DEVICE;
  } else if (!has_gpu_rasterization_trigger_) {
    gpu_rasterization_status_ = GpuRasterizationStatus::OFF_VIEWPORT;
  } else if (content_is_suitable_for_gpu_rasterization_) {
    use_gpu = true;
    gpu_rasterization_status_ = GpuRasterizationStatus::ON;
  } else if (using_msaa_for_complex_content) {
    use_gpu = use_msaa = true;
    gpu_rasterization_status_ = GpuRasterizationStatus::MSAA_CONTENT;
  } else {
    gpu_rasterization_status_ = GpuRasterizationStatus::OFF_CONTENT;
  }

  if (use_gpu && !use_gpu_rasterization_) {
    if (!CanUseGpuRasterization()) {
      // If GPU rasterization is unusable, e.g. if GlContext could not
      // be created due to losing the GL context, force use of software
      // raster.
      use_gpu = false;
      use_msaa = false;
      gpu_rasterization_status_ = GpuRasterizationStatus::OFF_DEVICE;
    }
  }

  if (use_gpu == use_gpu_rasterization_ && use_msaa == use_msaa_)
    return;

  // Note that this must happen first, in case the rest of the calls want to
  // query the new state of |use_gpu_rasterization_|.
  use_gpu_rasterization_ = use_gpu;
  use_msaa_ = use_msaa;

  tree_resources_for_gpu_rasterization_dirty_ = true;
}

void LayerTreeHostImpl::UpdateTreeResourcesForGpuRasterizationIfNeeded() {
  if (!tree_resources_for_gpu_rasterization_dirty_)
    return;

  // Clean up and replace existing tile manager with another one that uses
  // appropriate rasterizer. Only do this however if we already have a
  // resource pool, since otherwise we might not be able to create a new
  // one.
  ReleaseTreeResources();
  if (resource_pool_) {
    CleanUpTileManagerAndUIResources();
    CreateTileManagerResources();
  }
  RecreateTreeResources();

  // We have released tilings for both active and pending tree.
  // We would not have any content to draw until the pending tree is activated.
  // Prevent the active tree from drawing until activation.
  // TODO(crbug.com/469175): Replace with RequiresHighResToDraw.
  SetRequiresHighResToDraw();

  tree_resources_for_gpu_rasterization_dirty_ = false;
}

const RendererCapabilitiesImpl&
LayerTreeHostImpl::GetRendererCapabilities() const {
  CHECK(renderer_);
  return renderer_->Capabilities();
}

bool LayerTreeHostImpl::SwapBuffers(const LayerTreeHostImpl::FrameData& frame) {
  ResetRequiresHighResToDraw();
  if (frame.has_no_damage) {
    active_tree()->BreakSwapPromises(SwapPromise::SWAP_FAILS);
    return false;
  }
  CompositorFrameMetadata metadata = MakeCompositorFrameMetadata();
  active_tree()->FinishSwapPromises(&metadata);
  for (auto& latency : metadata.latency_info) {
    TRACE_EVENT_WITH_FLOW1("input,benchmark",
                           "LatencyInfo.Flow",
                           TRACE_ID_DONT_MANGLE(latency.trace_id()),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", "SwapBuffers");
    // Only add the latency component once for renderer swap, not the browser
    // swap.
    if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT,
                             0, nullptr)) {
      latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT,
                               0, 0);
    }
  }
  renderer_->SwapBuffers(std::move(metadata));
  return true;
}

void LayerTreeHostImpl::WillBeginImplFrame(const BeginFrameArgs& args) {
  current_begin_frame_tracker_.Start(args);

  if (is_likely_to_require_a_draw_) {
    // Optimistically schedule a draw. This will let us expect the tile manager
    // to complete its work so that we can draw new tiles within the impl frame
    // we are beginning now.
    SetNeedsRedraw();
  }

  Animate();

  for (auto& it : video_frame_controllers_)
    it->OnBeginFrame(args);
}

void LayerTreeHostImpl::DidFinishImplFrame() {
  current_begin_frame_tracker_.Finish();
}

void LayerTreeHostImpl::UpdateViewportContainerSizes() {
  LayerImpl* inner_container = active_tree_->InnerViewportContainerLayer();
  LayerImpl* outer_container = active_tree_->OuterViewportContainerLayer();

  if (!inner_container)
    return;

  ViewportAnchor anchor(InnerViewportScrollLayer(),
                        OuterViewportScrollLayer());

  float top_controls_layout_height =
      active_tree_->top_controls_shrink_blink_size()
          ? active_tree_->top_controls_height()
          : 0.f;
  float delta_from_top_controls =
      top_controls_layout_height - top_controls_manager_->ContentTopOffset();

  // Adjust the viewport layers by shrinking/expanding the container to account
  // for changes in the size (e.g. top controls) since the last resize from
  // Blink.
  gfx::Vector2dF amount_to_expand(
      0.f,
      delta_from_top_controls);
  inner_container->SetBoundsDelta(amount_to_expand);

  if (outer_container && !outer_container->BoundsForScrolling().IsEmpty()) {
    // Adjust the outer viewport container as well, since adjusting only the
    // inner may cause its bounds to exceed those of the outer, causing scroll
    // clamping.
    gfx::Vector2dF amount_to_expand_scaled = gfx::ScaleVector2d(
        amount_to_expand, 1.f / active_tree_->min_page_scale_factor());
    outer_container->SetBoundsDelta(amount_to_expand_scaled);
    active_tree_->InnerViewportScrollLayer()->SetBoundsDelta(
        amount_to_expand_scaled);

    anchor.ResetViewportToAnchoredPosition();
  }
}

void LayerTreeHostImpl::SynchronouslyInitializeAllTiles() {
  // Only valid for the single-threaded non-scheduled/synchronous case
  // using the zero copy raster worker pool.
  single_thread_synchronous_task_graph_runner_->RunUntilIdle();
}

void LayerTreeHostImpl::DidLoseOutputSurface() {
  if (resource_provider_)
    resource_provider_->DidLoseOutputSurface();
  client_->DidLoseOutputSurfaceOnImplThread();
}

bool LayerTreeHostImpl::HaveRootScrollLayer() const {
  return !!InnerViewportScrollLayer();
}

LayerImpl* LayerTreeHostImpl::InnerViewportScrollLayer() const {
  return active_tree_->InnerViewportScrollLayer();
}

LayerImpl* LayerTreeHostImpl::OuterViewportScrollLayer() const {
  return active_tree_->OuterViewportScrollLayer();
}

LayerImpl* LayerTreeHostImpl::CurrentlyScrollingLayer() const {
  return active_tree_->CurrentlyScrollingLayer();
}

bool LayerTreeHostImpl::IsActivelyScrolling() const {
  if (!CurrentlyScrollingLayer())
    return false;
  // On Android WebView root flings are controlled by the application,
  // so the compositor does not animate them and can't tell if they
  // are actually animating. So assume there are none.
  if (settings_.ignore_root_layer_flings && IsCurrentlyScrollingInnerViewport())
    return false;
  return did_lock_scrolling_layer_;
}

void LayerTreeHostImpl::CreatePendingTree() {
  CHECK(!pending_tree_);
  if (recycle_tree_)
    recycle_tree_.swap(pending_tree_);
  else
    pending_tree_ =
        LayerTreeImpl::create(this, active_tree()->page_scale_factor(),
                              active_tree()->top_controls_shown_ratio(),
                              active_tree()->elastic_overscroll());

  client_->OnCanDrawStateChanged(CanDraw());
  TRACE_EVENT_ASYNC_BEGIN0("cc", "PendingTree:waiting", pending_tree_.get());

  DCHECK(!pending_tree_duration_timer_);
  pending_tree_duration_timer_.reset(new PendingTreeDurationHistogramTimer());
}

void LayerTreeHostImpl::ActivateSyncTree() {
  if (pending_tree_) {
    TRACE_EVENT_ASYNC_END0("cc", "PendingTree:waiting", pending_tree_.get());

    DCHECK(pending_tree_duration_timer_);
    // Reset will call the destructor and log the timer histogram.
    pending_tree_duration_timer_.reset();

    // Process any requests in the UI resource queue.  The request queue is
    // given in LayerTreeHost::FinishCommitOnImplThread.  This must take place
    // before the swap.
    pending_tree_->ProcessUIResourceRequestQueue();

    if (pending_tree_->needs_full_tree_sync()) {
      TreeSynchronizer::SynchronizeTrees(pending_tree_.get(),
                                         active_tree_.get());
    }

    // Property trees may store damage status. We preserve the active tree
    // damage status by pushing the damage status from active tree property
    // trees to pending tree property trees or by moving it onto the layers.
    if (active_tree_->property_trees()->changed) {
      if (pending_tree_->property_trees()->sequence_number ==
          active_tree_->property_trees()->sequence_number)
        active_tree_->property_trees()->PushChangeTrackingTo(
            pending_tree_->property_trees());
      else
        active_tree_->MoveChangeTrackingToLayers();
    }
    active_tree_->property_trees()->PushOpacityIfNeeded(
        pending_tree_->property_trees());

    TreeSynchronizer::PushLayerProperties(pending_tree(), active_tree());
    pending_tree_->PushPropertiesTo(active_tree_.get());
    if (!pending_tree_->LayerListIsEmpty())
      pending_tree_->property_trees()->ResetAllChangeTracking();

    // Now that we've synced everything from the pending tree to the active
    // tree, rename the pending tree the recycle tree so we can reuse it on the
    // next sync.
    DCHECK(!recycle_tree_);
    pending_tree_.swap(recycle_tree_);

    // If we commit to the active tree directly, this is already done during
    // commit.
    ActivateAnimations();

    // Compositor worker operates on the active tree so we have to run again
    // after activation.
    Mutate(CurrentBeginFrameArgs().frame_time);
  } else {
    active_tree_->ProcessUIResourceRequestQueue();
  }

  // bounds_delta isn't a pushed property, so the newly-pushed property tree
  // won't already account for current bounds_delta values. This needs to
  // happen before calling UpdateViewportContainerSizes().
  active_tree_->UpdatePropertyTreesForBoundsDelta();
  UpdateViewportContainerSizes();

  active_tree_->DidBecomeActive();
  client_->RenewTreePriority();
  // If we have any picture layers, then by activating we also modified tile
  // priorities.
  if (!active_tree_->picture_layers().empty())
    DidModifyTilePriorities();

  client_->OnCanDrawStateChanged(CanDraw());
  client_->DidActivateSyncTree();
  if (!tree_activation_callback_.is_null())
    tree_activation_callback_.Run();

  std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation =
      active_tree_->TakePendingPageScaleAnimation();
  if (pending_page_scale_animation) {
    StartPageScaleAnimation(
        pending_page_scale_animation->target_offset,
        pending_page_scale_animation->use_anchor,
        pending_page_scale_animation->scale,
        pending_page_scale_animation->duration);
  }
  // Activation can change the root scroll offset, so inform the synchronous
  // input handler.
  UpdateRootLayerStateForSynchronousInputHandler();
}

void LayerTreeHostImpl::SetVisible(bool visible) {
  DCHECK(task_runner_provider_->IsImplThread());

  if (visible_ == visible)
    return;
  visible_ = visible;
  DidVisibilityChange(this, visible_);
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());

  // If we just became visible, we have to ensure that we draw high res tiles,
  // to prevent checkerboard/low res flashes.
  if (visible_) {
    // TODO(crbug.com/469175): Replace with RequiresHighResToDraw.
    SetRequiresHighResToDraw();
  } else {
    EvictAllUIResources();
  }

  // Call PrepareTiles to evict tiles when we become invisible.
  if (!visible)
    PrepareTiles();

  if (!renderer_)
    return;

  renderer_->SetVisible(visible);
}

void LayerTreeHostImpl::SetNeedsOneBeginImplFrame() {
  // TODO(miletus): This is just the compositor-thread-side call to the
  // SwapPromiseMonitor to say something happened that may cause a swap in the
  // future. The name should not refer to SetNeedsRedraw but it does for now.
  NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  client_->SetNeedsOneBeginImplFrameOnImplThread();
}

void LayerTreeHostImpl::SetNeedsRedraw() {
  NotifySwapPromiseMonitorsOfSetNeedsRedraw();
  client_->SetNeedsRedrawOnImplThread();
}

ManagedMemoryPolicy LayerTreeHostImpl::ActualManagedMemoryPolicy() const {
  ManagedMemoryPolicy actual = cached_managed_memory_policy_;
  if (debug_state_.rasterize_only_visible_content) {
    actual.priority_cutoff_when_visible =
        gpu::MemoryAllocation::CUTOFF_ALLOW_REQUIRED_ONLY;
  } else if (use_gpu_rasterization()) {
    actual.priority_cutoff_when_visible =
        gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
  }
  return actual;
}

size_t LayerTreeHostImpl::memory_allocation_limit_bytes() const {
  return ActualManagedMemoryPolicy().bytes_limit_when_visible;
}

void LayerTreeHostImpl::ReleaseTreeResources() {
  active_tree_->ReleaseResources();
  if (pending_tree_)
    pending_tree_->ReleaseResources();
  if (recycle_tree_)
    recycle_tree_->ReleaseResources();

  EvictAllUIResources();
}

void LayerTreeHostImpl::RecreateTreeResources() {
  active_tree_->RecreateResources();
  if (pending_tree_)
    pending_tree_->RecreateResources();
  if (recycle_tree_)
    recycle_tree_->RecreateResources();
}

void LayerTreeHostImpl::CreateAndSetRenderer() {
  DCHECK(!renderer_);
  DCHECK(output_surface_);
  DCHECK(resource_provider_);

  if (output_surface_->capabilities().delegated_rendering) {
    renderer_ =
        DelegatingRenderer::Create(this, &settings_.renderer_settings,
                                   output_surface_, resource_provider_.get());
  } else if (output_surface_->context_provider()) {
    renderer_ = GLRenderer::Create(
        this, &settings_.renderer_settings, output_surface_,
        resource_provider_.get(), texture_mailbox_deleter_.get(),
        settings_.renderer_settings.highp_threshold_min);
  } else if (output_surface_->software_device()) {
    renderer_ = SoftwareRenderer::Create(
        this, &settings_.renderer_settings, output_surface_,
        resource_provider_.get(), true /* use_image_hijack_canvas */);
  }
  DCHECK(renderer_);

  renderer_->SetVisible(visible_);
  SetFullRootLayerDamage();

  // See note in LayerTreeImpl::UpdateDrawProperties.  Renderer needs to be
  // initialized to get max texture size.  Also, after releasing resources,
  // trees need another update to generate new ones.
  active_tree_->set_needs_update_draw_properties();
  if (pending_tree_)
    pending_tree_->set_needs_update_draw_properties();
  client_->UpdateRendererCapabilitiesOnImplThread();
}

void LayerTreeHostImpl::CreateTileManagerResources() {
  CreateResourceAndRasterBufferProvider(&raster_buffer_provider_,
                                        &resource_pool_);

  if (use_gpu_rasterization_) {
    image_decode_controller_ = base::WrapUnique(new GpuImageDecodeController(
        output_surface_->worker_context_provider(),
        settings_.renderer_settings.preferred_tile_format,
        settings_.gpu_decoded_image_budget_bytes));
  } else {
    image_decode_controller_ =
        base::WrapUnique(new SoftwareImageDecodeController(
            settings_.renderer_settings.preferred_tile_format,
            settings_.software_decoded_image_budget_bytes));
  }

  // Pass the single-threaded synchronous task graph runner to the worker pool
  // if we're in synchronous single-threaded mode.
  TaskGraphRunner* task_graph_runner = task_graph_runner_;
  if (is_synchronous_single_threaded_) {
    DCHECK(!single_thread_synchronous_task_graph_runner_);
    single_thread_synchronous_task_graph_runner_.reset(
        new SynchronousTaskGraphRunner);
    task_graph_runner = single_thread_synchronous_task_graph_runner_.get();
  }

  tile_task_manager_ = TileTaskManagerImpl::Create(task_graph_runner);

  // TODO(vmpstr): Initialize tile task limit at ctor time.
  tile_manager_.SetResources(
      resource_pool_.get(), image_decode_controller_.get(),
      tile_task_manager_.get(), raster_buffer_provider_.get(),
      is_synchronous_single_threaded_ ? std::numeric_limits<size_t>::max()
                                      : settings_.scheduled_raster_task_limit,
      use_gpu_rasterization_);
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
}

void LayerTreeHostImpl::CreateResourceAndRasterBufferProvider(
    std::unique_ptr<RasterBufferProvider>* raster_buffer_provider,
    std::unique_ptr<ResourcePool>* resource_pool) {
  DCHECK(GetTaskRunner());
  // TODO(vmpstr): Make this a DCHECK (or remove) when crbug.com/419086 is
  // resolved.
  CHECK(resource_provider_);

  ContextProvider* compositor_context_provider =
      output_surface_->context_provider();
  if (!compositor_context_provider) {
    *resource_pool =
        ResourcePool::Create(resource_provider_.get(), GetTaskRunner());

    *raster_buffer_provider =
        BitmapRasterBufferProvider::Create(resource_provider_.get());
    return;
  }

  ContextProvider* worker_context_provider =
      output_surface_->worker_context_provider();
  if (use_gpu_rasterization_) {
    DCHECK(worker_context_provider);

    *resource_pool =
        ResourcePool::Create(resource_provider_.get(), GetTaskRunner());

    int msaa_sample_count = use_msaa_ ? RequestedMSAASampleCount() : 0;

    *raster_buffer_provider = base::MakeUnique<GpuRasterBufferProvider>(
        compositor_context_provider, worker_context_provider,
        resource_provider_.get(), settings_.use_distance_field_text,
        msaa_sample_count, settings_.async_worker_context_enabled);
    return;
  }

  DCHECK(GetRendererCapabilities().using_image);

  bool use_zero_copy = settings_.use_zero_copy;
  // TODO(reveman): Remove this when mojo supports worker contexts.
  // crbug.com/522440
  if (!use_zero_copy && !worker_context_provider) {
    LOG(ERROR)
        << "Forcing zero-copy tile initialization as worker context is missing";
    use_zero_copy = true;
  }

  if (use_zero_copy) {
    *resource_pool = ResourcePool::CreateForGpuMemoryBufferResources(
        resource_provider_.get(), GetTaskRunner());

    *raster_buffer_provider = ZeroCopyRasterBufferProvider::Create(
        resource_provider_.get(),
        settings_.renderer_settings.preferred_tile_format);
    return;
  }

  *resource_pool =
      ResourcePool::Create(resource_provider_.get(), GetTaskRunner());

  const int max_copy_texture_chromium_size =
      compositor_context_provider->ContextCapabilities()
          .max_copy_texture_chromium_size;

  *raster_buffer_provider = base::MakeUnique<OneCopyRasterBufferProvider>(
      GetTaskRunner(), compositor_context_provider, worker_context_provider,
      resource_provider_.get(), max_copy_texture_chromium_size,
      settings_.use_partial_raster, settings_.max_staging_buffer_usage_in_bytes,
      settings_.renderer_settings.preferred_tile_format,
      settings_.async_worker_context_enabled);
}

void LayerTreeHostImpl::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  if (mutator == mutator_)
    return;
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "LayerTreeHostImpl::SetLayerTreeMutator");
  mutator_ = std::move(mutator);
  mutator_->SetClient(this);
}

void LayerTreeHostImpl::CleanUpTileManagerAndUIResources() {
  ClearUIResources();
  tile_manager_.FinishTasksAndCleanUp();
  resource_pool_ = nullptr;
  tile_task_manager_ = nullptr;
  single_thread_synchronous_task_graph_runner_ = nullptr;
  image_decode_controller_ = nullptr;
}

void LayerTreeHostImpl::ReleaseOutputSurface() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ReleaseOutputSurface");

  // Since we will create a new resource provider, we cannot continue to use
  // the old resources (i.e. render_surfaces and texture IDs). Clear them
  // before we destroy the old resource provider.
  ReleaseTreeResources();

  // Note: order is important here.
  renderer_ = nullptr;
  CleanUpTileManagerAndUIResources();
  resource_provider_ = nullptr;

  // Detach from the old output surface and reset |output_surface_| pointer
  // as this surface is going to be destroyed independent of if binding the
  // new output surface succeeds or not.
  if (output_surface_) {
    output_surface_->DetachFromClient();
    output_surface_ = nullptr;
  }

  // We don't know if the next OutputSurface will support GPU rasterization.
  // Make sure to clear the flag so that we force a re-computation.
  use_gpu_rasterization_ = false;
}

bool LayerTreeHostImpl::InitializeRenderer(OutputSurface* output_surface) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::InitializeRenderer");

  ReleaseOutputSurface();
  if (!output_surface->BindToClient(this)) {
    // Avoid recreating tree resources because we might not have enough
    // information to do this yet (eg. we don't have a TileManager at this
    // point).
    return false;
  }

  output_surface_ = output_surface;
  resource_provider_ = base::MakeUnique<ResourceProvider>(
      output_surface_->context_provider(), shared_bitmap_manager_,
      gpu_memory_buffer_manager_,
      task_runner_provider_->blocking_main_thread_task_runner(),
      settings_.renderer_settings.highp_threshold_min,
      settings_.renderer_settings.texture_id_allocation_chunk_size,
      output_surface_->capabilities().delegated_sync_points_required,
      settings_.renderer_settings.use_gpu_memory_buffer_resources,
      settings_.use_image_texture_targets);

  CreateAndSetRenderer();

  // Since the new renderer may be capable of MSAA, update status here.
  UpdateGpuRasterizationStatus();

  CreateTileManagerResources();
  RecreateTreeResources();

  // Initialize vsync parameters to sane values.
  const base::TimeDelta display_refresh_interval =
      base::TimeDelta::FromMicroseconds(
          base::Time::kMicrosecondsPerSecond /
          settings_.renderer_settings.refresh_rate);
  CommitVSyncParameters(base::TimeTicks(), display_refresh_interval);

  // TODO(brianderson): Don't use a hard-coded parent draw time.
  base::TimeDelta parent_draw_time =
      (!settings_.use_external_begin_frame_source &&
       output_surface_->capabilities().adjust_deadline_for_parent)
          ? BeginFrameArgs::DefaultEstimatedParentDrawTime()
          : base::TimeDelta();
  client_->SetEstimatedParentDrawTime(parent_draw_time);

  DCHECK_EQ(1, output_surface_->capabilities().max_frames_pending);
  client_->OnCanDrawStateChanged(CanDraw());

  // There will not be anything to draw here, so set high res
  // to avoid checkerboards, typically when we are recovering
  // from lost context.
  // TODO(crbug.com/469175): Replace with RequiresHighResToDraw.
  SetRequiresHighResToDraw();

  return true;
}

void LayerTreeHostImpl::CommitVSyncParameters(base::TimeTicks timebase,
                                              base::TimeDelta interval) {
  client_->CommitVSyncParameters(timebase, interval);
}

void LayerTreeHostImpl::SetBeginFrameSource(BeginFrameSource* source) {
  client_->SetBeginFrameSource(source);
}

void LayerTreeHostImpl::SetViewportSize(const gfx::Size& device_viewport_size) {
  if (device_viewport_size == device_viewport_size_)
    return;
  TRACE_EVENT_INSTANT2("cc", "LayerTreeHostImpl::SetViewportSize",
                       TRACE_EVENT_SCOPE_THREAD, "width",
                       device_viewport_size.width(), "height",
                       device_viewport_size.height());

  if (pending_tree_)
    active_tree_->SetViewportSizeInvalid();

  device_viewport_size_ = device_viewport_size;

  UpdateViewportContainerSizes();
  client_->OnCanDrawStateChanged(CanDraw());
  SetFullRootLayerDamage();
  active_tree_->set_needs_update_draw_properties();
}

const gfx::Rect LayerTreeHostImpl::ViewportRectForTilePriority() const {
  if (viewport_rect_for_tile_priority_.IsEmpty())
    return DeviceViewport();

  return viewport_rect_for_tile_priority_;
}

gfx::Size LayerTreeHostImpl::DrawViewportSize() const {
  return DeviceViewport().size();
}

gfx::Rect LayerTreeHostImpl::DeviceViewport() const {
  if (external_viewport_.IsEmpty())
    return gfx::Rect(device_viewport_size_);

  return external_viewport_;
}

gfx::Rect LayerTreeHostImpl::DeviceClip() const {
  if (external_clip_.IsEmpty())
    return DeviceViewport();

  return external_clip_;
}

const gfx::Transform& LayerTreeHostImpl::DrawTransform() const {
  return external_transform_;
}

void LayerTreeHostImpl::DidChangeTopControlsPosition() {
  UpdateViewportContainerSizes();
  SetNeedsRedraw();
  SetNeedsOneBeginImplFrame();
  active_tree_->set_needs_update_draw_properties();
  SetFullRootLayerDamage();
}

float LayerTreeHostImpl::TopControlsHeight() const {
  return active_tree_->top_controls_height();
}

void LayerTreeHostImpl::SetCurrentTopControlsShownRatio(float ratio) {
  if (active_tree_->SetCurrentTopControlsShownRatio(ratio))
    DidChangeTopControlsPosition();
}

float LayerTreeHostImpl::CurrentTopControlsShownRatio() const {
  return active_tree_->CurrentTopControlsShownRatio();
}

void LayerTreeHostImpl::BindToClient(InputHandlerClient* client) {
  DCHECK(input_handler_client_ == NULL);
  input_handler_client_ = client;
}

InputHandler::ScrollStatus LayerTreeHostImpl::TryScroll(
    const gfx::PointF& screen_space_point,
    InputHandler::ScrollInputType type,
    const ScrollTree& scroll_tree,
    ScrollNode* scroll_node) const {
  InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  if (!!scroll_node->data.main_thread_scrolling_reasons) {
    TRACE_EVENT0("cc", "LayerImpl::TryScroll: Failed ShouldScrollOnMainThread");
    scroll_status.thread = InputHandler::SCROLL_ON_MAIN_THREAD;
    scroll_status.main_thread_scrolling_reasons =
        scroll_node->data.main_thread_scrolling_reasons;
    return scroll_status;
  }

  gfx::Transform screen_space_transform =
      scroll_tree.ScreenSpaceTransform(scroll_node->id);
  if (!screen_space_transform.IsInvertible()) {
    TRACE_EVENT0("cc", "LayerImpl::TryScroll: Ignored NonInvertibleTransform");
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNonInvertibleTransform;
    return scroll_status;
  }

  if (scroll_node->data.contains_non_fast_scrollable_region) {
    bool clipped = false;
    gfx::Transform inverse_screen_space_transform(
        gfx::Transform::kSkipInitialization);
    if (!screen_space_transform.GetInverse(&inverse_screen_space_transform)) {
      // TODO(shawnsingh): We shouldn't be applying a projection if screen space
      // transform is uninvertible here. Perhaps we should be returning
      // SCROLL_ON_MAIN_THREAD in this case?
    }

    gfx::PointF hit_test_point_in_layer_space = MathUtil::ProjectPoint(
        inverse_screen_space_transform, screen_space_point, &clipped);
    if (!clipped &&
        active_tree()
            ->LayerById(scroll_node->owner_id)
            ->non_fast_scrollable_region()
            .Contains(gfx::ToRoundedPoint(hit_test_point_in_layer_space))) {
      TRACE_EVENT0("cc",
                   "LayerImpl::tryScroll: Failed NonFastScrollableRegion");
      scroll_status.thread = InputHandler::SCROLL_ON_MAIN_THREAD;
      scroll_status.main_thread_scrolling_reasons =
          MainThreadScrollingReason::kNonFastScrollableRegion;
      return scroll_status;
    }
  }

  if (!scroll_node->data.scrollable) {
    TRACE_EVENT0("cc", "LayerImpl::tryScroll: Ignored not scrollable");
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNotScrollable;
    return scroll_status;
  }

  gfx::ScrollOffset max_scroll_offset =
      scroll_tree.MaxScrollOffset(scroll_node->id);
  if (max_scroll_offset.x() <= 0 && max_scroll_offset.y() <= 0) {
    TRACE_EVENT0("cc",
                 "LayerImpl::tryScroll: Ignored. Technically scrollable,"
                 " but has no affordance in either direction.");
    scroll_status.thread = InputHandler::SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNotScrollable;
    return scroll_status;
  }

  scroll_status.thread = InputHandler::SCROLL_ON_IMPL_THREAD;
  return scroll_status;
}

LayerImpl* LayerTreeHostImpl::FindScrollLayerForDeviceViewportPoint(
    const gfx::PointF& device_viewport_point,
    InputHandler::ScrollInputType type,
    LayerImpl* layer_impl,
    bool* scroll_on_main_thread,
    uint32_t* main_thread_scrolling_reasons) const {
  DCHECK(scroll_on_main_thread);
  DCHECK(main_thread_scrolling_reasons);
  *main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;

  // Walk up the hierarchy and look for a scrollable layer.
  LayerImpl* potentially_scrolling_layer_impl = NULL;
  if (layer_impl) {
    ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
    ScrollNode* scroll_node = scroll_tree.Node(layer_impl->scroll_tree_index());
    for (; scroll_tree.parent(scroll_node);
         scroll_node = scroll_tree.parent(scroll_node)) {
      // The content layer can also block attempts to scroll outside the main
      // thread.
      ScrollStatus status =
          TryScroll(device_viewport_point, type, scroll_tree, scroll_node);
      if (status.thread == SCROLL_ON_MAIN_THREAD) {
        if (!!scroll_node->data.main_thread_scrolling_reasons) {
          DCHECK(MainThreadScrollingReason::MainThreadCanSetScrollReasons(
              status.main_thread_scrolling_reasons));
        } else {
          DCHECK(MainThreadScrollingReason::CompositorCanSetScrollReasons(
              status.main_thread_scrolling_reasons));
        }

        *scroll_on_main_thread = true;
        *main_thread_scrolling_reasons = status.main_thread_scrolling_reasons;
        return NULL;
      }

      if (status.thread == InputHandler::SCROLL_ON_IMPL_THREAD &&
          !potentially_scrolling_layer_impl) {
        potentially_scrolling_layer_impl =
            active_tree_->LayerById(scroll_node->owner_id);
      }
    }
  }
  // Falling back to the root scroll layer ensures generation of root overscroll
  // notifications while preventing scroll updates from being unintentionally
  // forwarded to the main thread. The inner viewport layer represents the
  // viewport during scrolling.
  if (!potentially_scrolling_layer_impl)
    potentially_scrolling_layer_impl = InnerViewportScrollLayer();

  // The inner viewport layer represents the viewport.
  if (potentially_scrolling_layer_impl == OuterViewportScrollLayer())
    potentially_scrolling_layer_impl = InnerViewportScrollLayer();

  return potentially_scrolling_layer_impl;
}

// Similar to LayerImpl::HasAncestor, but walks up the scroll parents.
static bool HasScrollAncestor(LayerImpl* child, LayerImpl* scroll_ancestor) {
  DCHECK(scroll_ancestor);
  if (!child)
    return false;
  ScrollTree& scroll_tree =
      child->layer_tree_impl()->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.Node(child->scroll_tree_index());
  for (; scroll_tree.parent(scroll_node);
       scroll_node = scroll_tree.parent(scroll_node)) {
    if (scroll_node->data.scrollable)
      return scroll_node->owner_id == scroll_ancestor->id();
  }
  return false;
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollBeginImpl(
    ScrollState* scroll_state,
    LayerImpl* scrolling_layer_impl,
    InputHandler::ScrollInputType type) {
  DCHECK(scroll_state);
  DCHECK(scroll_state->delta_x() == 0 && scroll_state->delta_y() == 0);

  InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  if (!scrolling_layer_impl) {
    scroll_status.thread = SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNoScrollingLayer;
    return scroll_status;
  }
  scroll_status.thread = SCROLL_ON_IMPL_THREAD;
  ScrollAnimationAbort(scrolling_layer_impl);

  top_controls_manager_->ScrollBegin();

  active_tree_->SetCurrentlyScrollingLayer(scrolling_layer_impl);
  // TODO(majidvp): get rid of wheel_scrolling_ and set is_direct_manipulation
  // in input_handler_proxy instead.
  wheel_scrolling_ = IsWheelBasedScroll(type);
  scroll_state->set_is_direct_manipulation(!wheel_scrolling_);
  // Invoke |DistributeScrollDelta| even with zero delta and velocity to ensure
  // scroll customization callbacks are invoked.
  DistributeScrollDelta(scroll_state);

  client_->RenewTreePriority();
  RecordCompositorSlowScrollMetric(type, CC_THREAD);

  return scroll_status;
}

InputHandler::ScrollStatus LayerTreeHostImpl::RootScrollBegin(
    ScrollState* scroll_state,
    InputHandler::ScrollInputType type) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::RootScrollBegin");

  ClearCurrentlyScrollingLayer();

  return ScrollBeginImpl(scroll_state, InnerViewportScrollLayer(), type);
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollBegin(
    ScrollState* scroll_state,
    InputHandler::ScrollInputType type) {
  ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollBegin");

  ClearCurrentlyScrollingLayer();

  gfx::Point viewport_point(scroll_state->position_x(),
                            scroll_state->position_y());

  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());
  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);

  if (layer_impl) {
    LayerImpl* scroll_layer_impl =
        active_tree_->FindFirstScrollingLayerOrScrollbarLayerThatIsHitByPoint(
            device_viewport_point);
    if (scroll_layer_impl &&
        !HasScrollAncestor(layer_impl, scroll_layer_impl)) {
      scroll_status.thread = SCROLL_UNKNOWN;
      scroll_status.main_thread_scrolling_reasons =
          MainThreadScrollingReason::kFailedHitTest;
      return scroll_status;
    }
  }

  bool scroll_on_main_thread = false;
  LayerImpl* scrolling_layer_impl = FindScrollLayerForDeviceViewportPoint(
      device_viewport_point, type, layer_impl, &scroll_on_main_thread,
      &scroll_status.main_thread_scrolling_reasons);

  if (scrolling_layer_impl)
    scroll_affects_scroll_handler_ =
        scrolling_layer_impl->layer_tree_impl()->have_scroll_event_handlers();

  if (scroll_on_main_thread) {
    RecordCompositorSlowScrollMetric(type, MAIN_THREAD);

    scroll_status.thread = SCROLL_ON_MAIN_THREAD;
    return scroll_status;
  }

  return ScrollBeginImpl(scroll_state, scrolling_layer_impl, type);
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollAnimatedBegin(
    const gfx::Point& viewport_point) {
  InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  if (scroll_node) {
    gfx::Vector2dF delta;

    if (ScrollAnimationUpdateTarget(scroll_node, delta)) {
      scroll_status.thread = SCROLL_ON_IMPL_THREAD;
    } else {
      scroll_status.thread = SCROLL_IGNORED;
      scroll_status.main_thread_scrolling_reasons =
          MainThreadScrollingReason::kNotScrollable;
    }
    return scroll_status;
  }
  ScrollStateData scroll_state_data;
  scroll_state_data.position_x = viewport_point.x();
  scroll_state_data.position_y = viewport_point.y();
  scroll_state_data.is_in_inertial_phase = true;
  ScrollState scroll_state(scroll_state_data);

  // ScrollAnimated is used for animated wheel scrolls. We find the first layer
  // that can scroll and set up an animation of its scroll offset. Note that
  // this does not currently go through the scroll customization machinery
  // that ScrollBy uses for non-animated wheel scrolls.
  scroll_status = ScrollBegin(&scroll_state, WHEEL);
  scroll_node = scroll_tree.CurrentlyScrollingNode();
  if (scroll_status.thread == SCROLL_ON_IMPL_THREAD) {
    ScrollStateData scroll_state_end_data;
    scroll_state_end_data.is_ending = true;
    ScrollState scroll_state_end(scroll_state_end_data);
    ScrollEnd(&scroll_state_end);
  }
  return scroll_status;
}

gfx::Vector2dF LayerTreeHostImpl::ComputeScrollDelta(
    ScrollNode* scroll_node,
    const gfx::Vector2dF& delta) {
  ScrollTree& scroll_tree = active_tree()->property_trees()->scroll_tree;
  float scale_factor = active_tree()->current_page_scale_factor();

  gfx::Vector2dF adjusted_scroll(delta);
  adjusted_scroll.Scale(1.f / scale_factor);
  if (!scroll_node->data.user_scrollable_horizontal)
    adjusted_scroll.set_x(0);
  if (!scroll_node->data.user_scrollable_vertical)
    adjusted_scroll.set_y(0);

  gfx::ScrollOffset old_offset =
      scroll_tree.current_scroll_offset(scroll_node->owner_id);
  gfx::ScrollOffset new_offset = scroll_tree.ClampScrollOffsetToLimits(
      old_offset + gfx::ScrollOffset(adjusted_scroll), scroll_node);

  gfx::ScrollOffset scrolled = new_offset - old_offset;
  return gfx::Vector2dF(scrolled.x(), scrolled.y());
}

bool LayerTreeHostImpl::ScrollAnimationCreate(ScrollNode* scroll_node,
                                              const gfx::Vector2dF& delta) {
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;

  const float kEpsilon = 0.1f;
  bool scroll_animated =
      (std::abs(delta.x()) > kEpsilon || std::abs(delta.y()) > kEpsilon);
  if (!scroll_animated) {
    scroll_tree.ScrollBy(scroll_node, delta, active_tree());
    return false;
  }

  scroll_tree.set_currently_scrolling_node(scroll_node->id);

  gfx::ScrollOffset current_offset =
      scroll_tree.current_scroll_offset(scroll_node->owner_id);
  gfx::ScrollOffset target_offset = scroll_tree.ClampScrollOffsetToLimits(
      current_offset + gfx::ScrollOffset(delta), scroll_node);
  DCHECK_EQ(
      ElementId(active_tree()->LayerById(scroll_node->owner_id)->element_id()),
      scroll_node->data.element_id);

  animation_host_->ImplOnlyScrollAnimationCreate(scroll_node->data.element_id,
                                                 target_offset, current_offset);

  SetNeedsOneBeginImplFrame();

  return true;
}

InputHandler::ScrollStatus LayerTreeHostImpl::ScrollAnimated(
    const gfx::Point& viewport_point,
    const gfx::Vector2dF& scroll_delta) {
  InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  if (scroll_node) {
    gfx::Vector2dF delta = scroll_delta;
    if (!scroll_node->data.user_scrollable_horizontal)
      delta.set_x(0);
    if (!scroll_node->data.user_scrollable_vertical)
      delta.set_y(0);

    if (ScrollAnimationUpdateTarget(scroll_node, delta)) {
      scroll_status.thread = SCROLL_ON_IMPL_THREAD;
    } else {
      scroll_status.thread = SCROLL_IGNORED;
      scroll_status.main_thread_scrolling_reasons =
          MainThreadScrollingReason::kNotScrollable;
    }
    return scroll_status;
  }

  ScrollStateData scroll_state_data;
  scroll_state_data.position_x = viewport_point.x();
  scroll_state_data.position_y = viewport_point.y();
  scroll_state_data.is_in_inertial_phase = true;
  ScrollState scroll_state(scroll_state_data);

  // ScrollAnimated is used for animated wheel scrolls. We find the first layer
  // that can scroll and set up an animation of its scroll offset. Note that
  // this does not currently go through the scroll customization machinery
  // that ScrollBy uses for non-animated wheel scrolls.
  scroll_status = ScrollBegin(&scroll_state, WHEEL);
  scroll_node = scroll_tree.CurrentlyScrollingNode();
  if (scroll_status.thread == SCROLL_ON_IMPL_THREAD) {
    gfx::Vector2dF pending_delta = scroll_delta;
    if (scroll_node) {
      for (; scroll_tree.parent(scroll_node);
           scroll_node = scroll_tree.parent(scroll_node)) {
        if (!scroll_node->data.scrollable ||
            scroll_node->data.is_outer_viewport_scroll_layer)
          continue;

        if (scroll_node->data.is_inner_viewport_scroll_layer) {
          gfx::Vector2dF scrolled = viewport()->ScrollAnimated(pending_delta);
          // Viewport::ScrollAnimated returns pending_delta as long as it
          // starts an animation.
          if (scrolled == pending_delta)
            return scroll_status;
          pending_delta -= scrolled;
          continue;
        }

        gfx::Vector2dF scroll_delta =
            ComputeScrollDelta(scroll_node, pending_delta);
        if (ScrollAnimationCreate(scroll_node, scroll_delta))
          return scroll_status;

        pending_delta -= scroll_delta;
      }
    }
  }
  scroll_state.set_is_ending(true);
  ScrollEnd(&scroll_state);
  return scroll_status;
}

gfx::Vector2dF LayerTreeHostImpl::ScrollNodeWithViewportSpaceDelta(
    ScrollNode* scroll_node,
    const gfx::PointF& viewport_point,
    const gfx::Vector2dF& viewport_delta,
    ScrollTree* scroll_tree) {
  // Layers with non-invertible screen space transforms should not have passed
  // the scroll hit test in the first place.
  const gfx::Transform screen_space_transform =
      scroll_tree->ScreenSpaceTransform(scroll_node->id);
  DCHECK(screen_space_transform.IsInvertible());
  gfx::Transform inverse_screen_space_transform(
      gfx::Transform::kSkipInitialization);
  bool did_invert =
      screen_space_transform.GetInverse(&inverse_screen_space_transform);
  // TODO(shawnsingh): With the advent of impl-side scrolling for non-root
  // layers, we may need to explicitly handle uninvertible transforms here.
  DCHECK(did_invert);

  float scale_from_viewport_to_screen_space =
      active_tree_->device_scale_factor();
  gfx::PointF screen_space_point =
      gfx::ScalePoint(viewport_point, scale_from_viewport_to_screen_space);

  gfx::Vector2dF screen_space_delta = viewport_delta;
  screen_space_delta.Scale(scale_from_viewport_to_screen_space);

  // First project the scroll start and end points to local layer space to find
  // the scroll delta in layer coordinates.
  bool start_clipped, end_clipped;
  gfx::PointF screen_space_end_point = screen_space_point + screen_space_delta;
  gfx::PointF local_start_point =
      MathUtil::ProjectPoint(inverse_screen_space_transform,
                             screen_space_point,
                             &start_clipped);
  gfx::PointF local_end_point =
      MathUtil::ProjectPoint(inverse_screen_space_transform,
                             screen_space_end_point,
                             &end_clipped);

  // In general scroll point coordinates should not get clipped.
  DCHECK(!start_clipped);
  DCHECK(!end_clipped);
  if (start_clipped || end_clipped)
    return gfx::Vector2dF();

  // Apply the scroll delta.
  gfx::ScrollOffset previous_offset =
      scroll_tree->current_scroll_offset(scroll_node->owner_id);
  scroll_tree->ScrollBy(scroll_node, local_end_point - local_start_point,
                        active_tree());
  gfx::ScrollOffset scrolled =
      scroll_tree->current_scroll_offset(scroll_node->owner_id) -
      previous_offset;

  // Get the end point in the layer's content space so we can apply its
  // ScreenSpaceTransform.
  gfx::PointF actual_local_end_point =
      local_start_point + gfx::Vector2dF(scrolled.x(), scrolled.y());

  // Calculate the applied scroll delta in viewport space coordinates.
  gfx::PointF actual_screen_space_end_point = MathUtil::MapPoint(
      screen_space_transform, actual_local_end_point, &end_clipped);
  DCHECK(!end_clipped);
  if (end_clipped)
    return gfx::Vector2dF();
  gfx::PointF actual_viewport_end_point =
      gfx::ScalePoint(actual_screen_space_end_point,
                      1.f / scale_from_viewport_to_screen_space);
  return actual_viewport_end_point - viewport_point;
}

static gfx::Vector2dF ScrollNodeWithLocalDelta(
    ScrollNode* scroll_node,
    const gfx::Vector2dF& local_delta,
    float page_scale_factor,
    LayerTreeImpl* layer_tree_impl) {
  ScrollTree& scroll_tree = layer_tree_impl->property_trees()->scroll_tree;
  gfx::ScrollOffset previous_offset =
      scroll_tree.current_scroll_offset(scroll_node->owner_id);
  gfx::Vector2dF delta = local_delta;
  delta.Scale(1.f / page_scale_factor);
  scroll_tree.ScrollBy(scroll_node, delta, layer_tree_impl);
  gfx::ScrollOffset scrolled =
      scroll_tree.current_scroll_offset(scroll_node->owner_id) -
      previous_offset;
  gfx::Vector2dF consumed_scroll(scrolled.x(), scrolled.y());
  consumed_scroll.Scale(page_scale_factor);

  return consumed_scroll;
}

// TODO(danakj): Make this into two functions, one with delta, one with
// viewport_point, no bool required.
gfx::Vector2dF LayerTreeHostImpl::ScrollSingleNode(
    ScrollNode* scroll_node,
    const gfx::Vector2dF& delta,
    const gfx::Point& viewport_point,
    bool is_direct_manipulation,
    ScrollTree* scroll_tree) {
  // Events representing direct manipulation of the screen (such as gesture
  // events) need to be transformed from viewport coordinates to local layer
  // coordinates so that the scrolling contents exactly follow the user's
  // finger. In contrast, events not representing direct manipulation of the
  // screen (such as wheel events) represent a fixed amount of scrolling so we
  // can just apply them directly, but the page scale factor is applied to the
  // scroll delta.
  if (is_direct_manipulation) {
    return ScrollNodeWithViewportSpaceDelta(
        scroll_node, gfx::PointF(viewport_point), delta, scroll_tree);
  }
  float scale_factor = active_tree()->current_page_scale_factor();
  return ScrollNodeWithLocalDelta(scroll_node, delta, scale_factor,
                                  active_tree());
}

void LayerTreeHostImpl::ApplyScroll(ScrollNode* scroll_node,
                                    ScrollState* scroll_state) {
  DCHECK(scroll_state);
  gfx::Point viewport_point(scroll_state->position_x(),
                            scroll_state->position_y());
  const gfx::Vector2dF delta(scroll_state->delta_x(), scroll_state->delta_y());
  gfx::Vector2dF applied_delta;
  // TODO(tdresser): Use a more rational epsilon. See crbug.com/510550 for
  // details.
  const float kEpsilon = 0.1f;

  if (scroll_node->data.is_inner_viewport_scroll_layer) {
    bool affect_top_controls = !wheel_scrolling_;
    Viewport::ScrollResult result = viewport()->ScrollBy(
        delta, viewport_point, scroll_state->is_direct_manipulation(),
        affect_top_controls);
    applied_delta = result.consumed_delta;
    scroll_state->set_caused_scroll(
        std::abs(result.content_scrolled_delta.x()) > kEpsilon,
        std::abs(result.content_scrolled_delta.y()) > kEpsilon);
    scroll_state->ConsumeDelta(applied_delta.x(), applied_delta.y());
  } else {
    applied_delta = ScrollSingleNode(
        scroll_node, delta, viewport_point,
        scroll_state->is_direct_manipulation(),
        &scroll_state->layer_tree_impl()->property_trees()->scroll_tree);
  }

  // If the layer wasn't able to move, try the next one in the hierarchy.
  bool scrolled = std::abs(applied_delta.x()) > kEpsilon;
  scrolled = scrolled || std::abs(applied_delta.y()) > kEpsilon;

  if (scrolled && !scroll_node->data.is_inner_viewport_scroll_layer) {
    // If the applied delta is within 45 degrees of the input
    // delta, bail out to make it easier to scroll just one layer
    // in one direction without affecting any of its parents.
    float angle_threshold = 45;
    if (MathUtil::SmallestAngleBetweenVectors(applied_delta, delta) <
        angle_threshold) {
      applied_delta = delta;
    } else {
      // Allow further movement only on an axis perpendicular to the direction
      // in which the layer moved.
      applied_delta = MathUtil::ProjectVector(delta, applied_delta);
    }
    scroll_state->set_caused_scroll(std::abs(applied_delta.x()) > kEpsilon,
                                    std::abs(applied_delta.y()) > kEpsilon);
    scroll_state->ConsumeDelta(applied_delta.x(), applied_delta.y());
  }

  if (!scrolled)
    return;

  scroll_state->set_current_native_scrolling_node(scroll_node);
}

void LayerTreeHostImpl::DistributeScrollDelta(ScrollState* scroll_state) {
  // TODO(majidvp): in Blink we compute scroll chain only at scroll begin which
  // is not the case here. We eventually want to have the same behaviour on both
  // sides but it may become a non issue if we get rid of scroll chaining (see
  // crbug.com/526462)
  std::list<const ScrollNode*> current_scroll_chain;
  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  if (scroll_node) {
    for (; scroll_tree.parent(scroll_node);
         scroll_node = scroll_tree.parent(scroll_node)) {
      // Skip the outer viewport scroll layer so that we try to scroll the
      // viewport only once. i.e. The inner viewport layer represents the
      // viewport.
      if (!scroll_node->data.scrollable ||
          scroll_node->data.is_outer_viewport_scroll_layer)
        continue;
      current_scroll_chain.push_front(scroll_node);
    }
  }
  scroll_state->set_scroll_chain_and_layer_tree(current_scroll_chain,
                                                active_tree());
  scroll_state->DistributeToScrollChainDescendant();
}

InputHandlerScrollResult LayerTreeHostImpl::ScrollBy(
    ScrollState* scroll_state) {
  DCHECK(scroll_state);

  TRACE_EVENT0("cc", "LayerTreeHostImpl::ScrollBy");
  if (!CurrentlyScrollingLayer())
    return InputHandlerScrollResult();

  float initial_top_controls_offset =
      top_controls_manager_->ControlsTopOffset();

  scroll_state->set_delta_consumed_for_scroll_sequence(
      did_lock_scrolling_layer_);
  scroll_state->set_is_direct_manipulation(!wheel_scrolling_);
  scroll_state->set_current_native_scrolling_node(
      active_tree()->property_trees()->scroll_tree.CurrentlyScrollingNode());

  DistributeScrollDelta(scroll_state);

  active_tree_->SetCurrentlyScrollingLayer(active_tree_->LayerById(
      scroll_state->current_native_scrolling_node()->owner_id));
  did_lock_scrolling_layer_ =
      scroll_state->delta_consumed_for_scroll_sequence();

  bool did_scroll_x = scroll_state->caused_scroll_x();
  bool did_scroll_y = scroll_state->caused_scroll_y();
  bool did_scroll_content = did_scroll_x || did_scroll_y;
  if (did_scroll_content) {
    // If we are scrolling with an active scroll handler, forward latency
    // tracking information to the main thread so the delay introduced by the
    // handler is accounted for.
    if (scroll_affects_scroll_handler())
      NotifySwapPromiseMonitorsOfForwardingToMainThread();
    client_->SetNeedsCommitOnImplThread();
    SetNeedsRedraw();
    client_->RenewTreePriority();
  }

  // Scrolling along an axis resets accumulated root overscroll for that axis.
  if (did_scroll_x)
    accumulated_root_overscroll_.set_x(0);
  if (did_scroll_y)
    accumulated_root_overscroll_.set_y(0);
  gfx::Vector2dF unused_root_delta(scroll_state->delta_x(),
                                   scroll_state->delta_y());

  // When inner viewport is unscrollable, disable overscrolls.
  if (InnerViewportScrollLayer()) {
    if (!InnerViewportScrollLayer()->user_scrollable_horizontal())
      unused_root_delta.set_x(0);
    if (!InnerViewportScrollLayer()->user_scrollable_vertical())
      unused_root_delta.set_y(0);
  }

  accumulated_root_overscroll_ += unused_root_delta;

  bool did_scroll_top_controls =
      initial_top_controls_offset != top_controls_manager_->ControlsTopOffset();

  InputHandlerScrollResult scroll_result;
  scroll_result.did_scroll = did_scroll_content || did_scroll_top_controls;
  scroll_result.did_overscroll_root = !unused_root_delta.IsZero();
  scroll_result.accumulated_root_overscroll = accumulated_root_overscroll_;
  scroll_result.unused_scroll_delta = unused_root_delta;

  if (scroll_result.did_scroll) {
    // Scrolling can change the root scroll offset, so inform the synchronous
    // input handler.
    UpdateRootLayerStateForSynchronousInputHandler();
  }

  // Update compositor worker mutations which may respond to scrolling.
  Mutate(CurrentBeginFrameArgs().frame_time);

  return scroll_result;
}

// This implements scrolling by page as described here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms645601(v=vs.85).aspx#_win32_The_Mouse_Wheel
// for events with WHEEL_PAGESCROLL set.
bool LayerTreeHostImpl::ScrollVerticallyByPage(const gfx::Point& viewport_point,
                                               ScrollDirection direction) {
  DCHECK(wheel_scrolling_);

  ScrollTree& scroll_tree = active_tree_->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  if (scroll_node) {
    for (; scroll_tree.parent(scroll_node);
         scroll_node = scroll_tree.parent(scroll_node)) {
      // The inner viewport layer represents the viewport.
      if (!scroll_node->data.scrollable ||
          scroll_node->data.is_outer_viewport_scroll_layer)
        continue;

      float height =
          scroll_tree.scroll_clip_layer_bounds(scroll_node->id).height();

      // These magical values match WebKit and are designed to scroll nearly the
      // entire visible content height but leave a bit of overlap.
      float page = std::max(height * 0.875f, 1.f);
      if (direction == SCROLL_BACKWARD)
        page = -page;

      gfx::Vector2dF delta = gfx::Vector2dF(0.f, page);

      gfx::Vector2dF applied_delta =
          ScrollNodeWithLocalDelta(scroll_node, delta, 1.f, active_tree());

      if (!applied_delta.IsZero()) {
        client_->SetNeedsCommitOnImplThread();
        SetNeedsRedraw();
        client_->RenewTreePriority();
        return true;
      }

      scroll_tree.set_currently_scrolling_node(scroll_node->id);
    }
  }
  return false;
}

void LayerTreeHostImpl::RequestUpdateForSynchronousInputHandler() {
  UpdateRootLayerStateForSynchronousInputHandler();
}

void LayerTreeHostImpl::SetSynchronousInputHandlerRootScrollOffset(
    const gfx::ScrollOffset& root_offset) {
  bool changed = active_tree_->DistributeRootScrollOffset(root_offset);
  if (!changed)
    return;

  client_->SetNeedsCommitOnImplThread();
  // After applying the synchronous input handler's scroll offset, tell it what
  // we ended up with.
  UpdateRootLayerStateForSynchronousInputHandler();
  SetFullRootLayerDamage();
  SetNeedsRedraw();
}

void LayerTreeHostImpl::ClearCurrentlyScrollingLayer() {
  active_tree_->ClearCurrentlyScrollingLayer();
  did_lock_scrolling_layer_ = false;
  scroll_affects_scroll_handler_ = false;
  accumulated_root_overscroll_ = gfx::Vector2dF();
}

void LayerTreeHostImpl::ScrollEnd(ScrollState* scroll_state) {
  DCHECK(scroll_state);
  DCHECK(scroll_state->delta_x() == 0 && scroll_state->delta_y() == 0);

  DistributeScrollDelta(scroll_state);
  top_controls_manager_->ScrollEnd();
  ClearCurrentlyScrollingLayer();
}

InputHandler::ScrollStatus LayerTreeHostImpl::FlingScrollBegin() {
  InputHandler::ScrollStatus scroll_status;
  scroll_status.main_thread_scrolling_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;
  if (!CurrentlyScrollingLayer()) {
    scroll_status.thread = SCROLL_IGNORED;
    scroll_status.main_thread_scrolling_reasons =
        MainThreadScrollingReason::kNoScrollingLayer;
  } else {
    scroll_status.thread = SCROLL_ON_IMPL_THREAD;
  }
  return scroll_status;
}

float LayerTreeHostImpl::DeviceSpaceDistanceToLayer(
    const gfx::PointF& device_viewport_point,
    LayerImpl* layer_impl) {
  if (!layer_impl)
    return std::numeric_limits<float>::max();

  gfx::Rect layer_impl_bounds(layer_impl->bounds());

  gfx::RectF device_viewport_layer_impl_bounds = MathUtil::MapClippedRect(
      layer_impl->ScreenSpaceTransform(), gfx::RectF(layer_impl_bounds));

  return device_viewport_layer_impl_bounds.ManhattanDistanceToPoint(
      device_viewport_point);
}

void LayerTreeHostImpl::MouseMoveAt(const gfx::Point& viewport_point) {
  gfx::PointF device_viewport_point = gfx::ScalePoint(
      gfx::PointF(viewport_point), active_tree_->device_scale_factor());
  LayerImpl* layer_impl =
      active_tree_->FindLayerThatIsHitByPoint(device_viewport_point);
  HandleMouseOverScrollbar(layer_impl);
  if (scroll_layer_id_when_mouse_over_scrollbar_ != Layer::INVALID_ID)
    return;

  bool scroll_on_main_thread = false;
  uint32_t main_thread_scrolling_reasons;
  LayerImpl* scroll_layer_impl = FindScrollLayerForDeviceViewportPoint(
      device_viewport_point, InputHandler::TOUCHSCREEN, layer_impl,
      &scroll_on_main_thread, &main_thread_scrolling_reasons);
  if (scroll_layer_impl == InnerViewportScrollLayer())
    scroll_layer_impl = OuterViewportScrollLayer();
  if (scroll_on_main_thread || !scroll_layer_impl)
    return;

  ScrollbarAnimationController* animation_controller =
      ScrollbarAnimationControllerForId(scroll_layer_impl->id());
  if (!animation_controller)
    return;

  float distance_to_scrollbar = std::numeric_limits<float>::max();
  for (ScrollbarLayerImplBase* scrollbar :
       ScrollbarsFor(scroll_layer_impl->id()))
    distance_to_scrollbar =
        std::min(distance_to_scrollbar,
                 DeviceSpaceDistanceToLayer(device_viewport_point, scrollbar));

  animation_controller->DidMouseMoveNear(distance_to_scrollbar /
                                         active_tree_->device_scale_factor());
}

void LayerTreeHostImpl::HandleMouseOverScrollbar(LayerImpl* layer_impl) {
  int new_id = Layer::INVALID_ID;
  if (layer_impl && layer_impl->ToScrollbarLayer())
    new_id = layer_impl->ToScrollbarLayer()->ScrollLayerId();

  if (new_id == scroll_layer_id_when_mouse_over_scrollbar_)
    return;

  ScrollbarAnimationController* old_animation_controller =
      ScrollbarAnimationControllerForId(
          scroll_layer_id_when_mouse_over_scrollbar_);
  if (old_animation_controller)
    old_animation_controller->DidMouseMoveOffScrollbar();

  scroll_layer_id_when_mouse_over_scrollbar_ = new_id;

  ScrollbarAnimationController* new_animation_controller =
      ScrollbarAnimationControllerForId(
          scroll_layer_id_when_mouse_over_scrollbar_);
  if (new_animation_controller)
    new_animation_controller->DidMouseMoveNear(0);
}

void LayerTreeHostImpl::PinchGestureBegin() {
  pinch_gesture_active_ = true;
  client_->RenewTreePriority();
  pinch_gesture_end_should_clear_scrolling_layer_ = !CurrentlyScrollingLayer();
  active_tree_->SetCurrentlyScrollingLayer(
      active_tree_->InnerViewportScrollLayer());
  top_controls_manager_->PinchBegin();
}

void LayerTreeHostImpl::PinchGestureUpdate(float magnify_delta,
                                           const gfx::Point& anchor) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::PinchGestureUpdate");
  if (!InnerViewportScrollLayer())
    return;
  viewport()->PinchUpdate(magnify_delta, anchor);
  client_->SetNeedsCommitOnImplThread();
  SetNeedsRedraw();
  client_->RenewTreePriority();
  // Pinching can change the root scroll offset, so inform the synchronous input
  // handler.
  UpdateRootLayerStateForSynchronousInputHandler();
}

void LayerTreeHostImpl::PinchGestureEnd() {
  pinch_gesture_active_ = false;
  if (pinch_gesture_end_should_clear_scrolling_layer_) {
    pinch_gesture_end_should_clear_scrolling_layer_ = false;
    ClearCurrentlyScrollingLayer();
  }
  viewport()->PinchEnd();
  top_controls_manager_->PinchEnd();
  client_->SetNeedsCommitOnImplThread();
  // When a pinch ends, we may be displaying content cached at incorrect scales,
  // so updating draw properties and drawing will ensure we are using the right
  // scales that we want when we're not inside a pinch.
  active_tree_->set_needs_update_draw_properties();
  SetNeedsRedraw();
}

std::unique_ptr<BeginFrameCallbackList>
LayerTreeHostImpl::ProcessLayerTreeMutations() {
  std::unique_ptr<BeginFrameCallbackList> callbacks(new BeginFrameCallbackList);
  if (mutator_) {
    const base::Closure& callback = mutator_->TakeMutations();
    if (!callback.is_null())
      callbacks->push_back(callback);
  }
  return callbacks;
}

static void CollectScrollDeltas(ScrollAndScaleSet* scroll_info,
                                LayerTreeImpl* tree_impl) {
  if (tree_impl->LayerListIsEmpty())
    return;

  return tree_impl->property_trees()->scroll_tree.CollectScrollDeltas(
      scroll_info);
}

std::unique_ptr<ScrollAndScaleSet> LayerTreeHostImpl::ProcessScrollDeltas() {
  std::unique_ptr<ScrollAndScaleSet> scroll_info(new ScrollAndScaleSet());

  CollectScrollDeltas(scroll_info.get(), active_tree_.get());
  scroll_info->page_scale_delta =
      active_tree_->page_scale_factor()->PullDeltaForMainThread();
  scroll_info->top_controls_delta =
      active_tree()->top_controls_shown_ratio()->PullDeltaForMainThread();
  scroll_info->elastic_overscroll_delta =
      active_tree_->elastic_overscroll()->PullDeltaForMainThread();
  scroll_info->swap_promises.swap(swap_promises_for_main_thread_scroll_update_);

  return scroll_info;
}

void LayerTreeHostImpl::SetFullRootLayerDamage() {
  SetViewportDamage(gfx::Rect(DrawViewportSize()));
}

void LayerTreeHostImpl::ScrollViewportInnerFirst(gfx::Vector2dF scroll_delta) {
  DCHECK(InnerViewportScrollLayer());
  LayerImpl* scroll_layer = InnerViewportScrollLayer();

  gfx::Vector2dF unused_delta = scroll_layer->ScrollBy(scroll_delta);
  if (!unused_delta.IsZero() && OuterViewportScrollLayer())
    OuterViewportScrollLayer()->ScrollBy(unused_delta);
}

void LayerTreeHostImpl::ScrollViewportBy(gfx::Vector2dF scroll_delta) {
  DCHECK(InnerViewportScrollLayer());
  LayerImpl* scroll_layer = OuterViewportScrollLayer()
                                ? OuterViewportScrollLayer()
                                : InnerViewportScrollLayer();

  gfx::Vector2dF unused_delta = scroll_layer->ScrollBy(scroll_delta);

  if (!unused_delta.IsZero() && (scroll_layer == OuterViewportScrollLayer()))
    InnerViewportScrollLayer()->ScrollBy(unused_delta);
}

bool LayerTreeHostImpl::AnimatePageScale(base::TimeTicks monotonic_time) {
  if (!page_scale_animation_)
    return false;

  gfx::ScrollOffset scroll_total = active_tree_->TotalScrollOffset();

  if (!page_scale_animation_->IsAnimationStarted())
    page_scale_animation_->StartAnimation(monotonic_time);

  active_tree_->SetPageScaleOnActiveTree(
      page_scale_animation_->PageScaleFactorAtTime(monotonic_time));
  gfx::ScrollOffset next_scroll = gfx::ScrollOffset(
      page_scale_animation_->ScrollOffsetAtTime(monotonic_time));

  ScrollViewportInnerFirst(next_scroll.DeltaFrom(scroll_total));

  if (page_scale_animation_->IsAnimationCompleteAtTime(monotonic_time)) {
    page_scale_animation_ = nullptr;
    client_->SetNeedsCommitOnImplThread();
    client_->RenewTreePriority();
    client_->DidCompletePageScaleAnimationOnImplThread();
  } else {
    SetNeedsOneBeginImplFrame();
  }
  return true;
}

bool LayerTreeHostImpl::AnimateTopControls(base::TimeTicks time) {
  if (!top_controls_manager_->has_animation())
    return false;

  gfx::Vector2dF scroll = top_controls_manager_->Animate(time);

  if (top_controls_manager_->has_animation())
    SetNeedsOneBeginImplFrame();

  if (active_tree_->TotalScrollOffset().y() == 0.f)
    return false;

  if (scroll.IsZero())
    return false;

  ScrollViewportBy(gfx::ScaleVector2d(
      scroll, 1.f / active_tree_->current_page_scale_factor()));
  client_->SetNeedsCommitOnImplThread();
  client_->RenewTreePriority();
  return true;
}

bool LayerTreeHostImpl::AnimateScrollbars(base::TimeTicks monotonic_time) {
  bool animated = false;
  for (auto& pair : scrollbar_animation_controllers_)
    animated |= pair.second->Animate(monotonic_time);
  return animated;
}

bool LayerTreeHostImpl::AnimateLayers(base::TimeTicks monotonic_time) {
  const bool animated = animation_host_->AnimateLayers(monotonic_time);

  // TODO(crbug.com/551134): Only do this if the animations are on the active
  // tree, or if they are on the pending tree waiting for some future time to
  // start.
  // TODO(crbug.com/551138): We currently have a single signal from the
  // animation_host, so on the last frame of an animation we will
  // still request an extra SetNeedsAnimate here.
  if (animated)
    SetNeedsOneBeginImplFrame();
  // TODO(crbug.com/551138): We could return true only if the animations are on
  // the active tree. There's no need to cause a draw to take place from
  // animations starting/ticking on the pending tree.
  return animated;
}

void LayerTreeHostImpl::UpdateAnimationState(bool start_ready_animations) {
  std::unique_ptr<AnimationEvents> events = animation_host_->CreateEvents();

  const bool has_active_animations = animation_host_->UpdateAnimationState(
      start_ready_animations, events.get());

  if (!events->events_.empty())
    client_->PostAnimationEventsToMainThreadOnImplThread(std::move(events));

  if (has_active_animations)
    SetNeedsOneBeginImplFrame();
}

void LayerTreeHostImpl::ActivateAnimations() {
  const bool activated = animation_host_->ActivateAnimations();
  if (activated) {
    // Activating an animation changes layer draw properties, such as
    // screen_space_transform_is_animating. So when we see a new animation get
    // activated, we need to update the draw properties on the active tree.
    active_tree()->set_needs_update_draw_properties();
    // Request another frame to run the next tick of the animation.
    SetNeedsOneBeginImplFrame();
  }
}

std::string LayerTreeHostImpl::LayerTreeAsJson() const {
  std::string str;
  if (active_tree_->root_layer_for_testing()) {
    std::unique_ptr<base::Value> json(
        active_tree_->root_layer_for_testing()->LayerTreeAsJson());
    base::JSONWriter::WriteWithOptions(
        *json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &str);
  }
  return str;
}

void LayerTreeHostImpl::RegisterScrollbarAnimationController(
    int scroll_layer_id) {
  if (settings().scrollbar_animator == LayerTreeSettings::NO_ANIMATOR)
    return;
  if (ScrollbarAnimationControllerForId(scroll_layer_id))
    return;
  scrollbar_animation_controllers_[scroll_layer_id] =
      active_tree_->CreateScrollbarAnimationController(scroll_layer_id);
}

void LayerTreeHostImpl::UnregisterScrollbarAnimationController(
    int scroll_layer_id) {
  scrollbar_animation_controllers_.erase(scroll_layer_id);
}

ScrollbarAnimationController*
LayerTreeHostImpl::ScrollbarAnimationControllerForId(
    int scroll_layer_id) const {
  if (InnerViewportScrollLayer() && OuterViewportScrollLayer() &&
      scroll_layer_id == InnerViewportScrollLayer()->id())
    scroll_layer_id = OuterViewportScrollLayer()->id();
  auto i = scrollbar_animation_controllers_.find(scroll_layer_id);
  if (i == scrollbar_animation_controllers_.end())
    return nullptr;
  return i->second.get();
}

void LayerTreeHostImpl::PostDelayedScrollbarAnimationTask(
    const base::Closure& task,
    base::TimeDelta delay) {
  client_->PostDelayedAnimationTaskOnImplThread(task, delay);
}

// TODO(danakj): Make this a return value from the Animate() call instead of an
// interface on LTHI. (Also, crbug.com/551138.)
void LayerTreeHostImpl::SetNeedsAnimateForScrollbarAnimation() {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::SetNeedsAnimateForScrollbarAnimation");
  SetNeedsOneBeginImplFrame();
}

// TODO(danakj): Make this a return value from the Animate() call instead of an
// interface on LTHI. (Also, crbug.com/551138.)
void LayerTreeHostImpl::SetNeedsRedrawForScrollbarAnimation() {
  SetNeedsRedraw();
}

ScrollbarSet LayerTreeHostImpl::ScrollbarsFor(int scroll_layer_id) const {
  return active_tree_->ScrollbarsFor(scroll_layer_id);
}

void LayerTreeHostImpl::AddVideoFrameController(
    VideoFrameController* controller) {
  bool was_empty = video_frame_controllers_.empty();
  video_frame_controllers_.insert(controller);
  if (current_begin_frame_tracker_.DangerousMethodHasStarted() &&
      !current_begin_frame_tracker_.DangerousMethodHasFinished())
    controller->OnBeginFrame(current_begin_frame_tracker_.Current());
  if (was_empty)
    client_->SetVideoNeedsBeginFrames(true);
}

void LayerTreeHostImpl::RemoveVideoFrameController(
    VideoFrameController* controller) {
  video_frame_controllers_.erase(controller);
  if (video_frame_controllers_.empty())
    client_->SetVideoNeedsBeginFrames(false);
}

void LayerTreeHostImpl::SetTreePriority(TreePriority priority) {
  if (global_tile_state_.tree_priority == priority)
    return;
  global_tile_state_.tree_priority = priority;
  DidModifyTilePriorities();
}

TreePriority LayerTreeHostImpl::GetTreePriority() const {
  return global_tile_state_.tree_priority;
}

BeginFrameArgs LayerTreeHostImpl::CurrentBeginFrameArgs() const {
  // TODO(mithro): Replace call with current_begin_frame_tracker_.Current()
  // once all calls which happens outside impl frames are fixed.
  return current_begin_frame_tracker_.DangerousMethodCurrentOrLast();
}

base::TimeDelta LayerTreeHostImpl::CurrentBeginFrameInterval() const {
  return current_begin_frame_tracker_.Interval();
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
LayerTreeHostImpl::AsValueWithFrame(FrameData* frame) const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  AsValueWithFrameInto(frame, state.get());
  return std::move(state);
}

void LayerTreeHostImpl::AsValueWithFrameInto(
    FrameData* frame,
    base::trace_event::TracedValue* state) const {
  if (this->pending_tree_) {
    state->BeginDictionary("activation_state");
    ActivationStateAsValueInto(state);
    state->EndDictionary();
  }
  MathUtil::AddToTracedValue("device_viewport_size", device_viewport_size_,
                             state);

  std::vector<PrioritizedTile> prioritized_tiles;
  active_tree_->GetAllPrioritizedTilesForTracing(&prioritized_tiles);
  if (pending_tree_)
    pending_tree_->GetAllPrioritizedTilesForTracing(&prioritized_tiles);

  state->BeginArray("active_tiles");
  for (const auto& prioritized_tile : prioritized_tiles) {
    state->BeginDictionary();
    prioritized_tile.AsValueInto(state);
    state->EndDictionary();
  }
  state->EndArray();

  state->BeginDictionary("tile_manager_basic_state");
  tile_manager_.BasicStateAsValueInto(state);
  state->EndDictionary();

  state->BeginDictionary("active_tree");
  active_tree_->AsValueInto(state);
  state->EndDictionary();
  if (pending_tree_) {
    state->BeginDictionary("pending_tree");
    pending_tree_->AsValueInto(state);
    state->EndDictionary();
  }
  if (frame) {
    state->BeginDictionary("frame");
    frame->AsValueInto(state);
    state->EndDictionary();
  }
}

void LayerTreeHostImpl::ActivationStateAsValueInto(
    base::trace_event::TracedValue* state) const {
  TracedValue::SetIDRef(this, state, "lthi");
  state->BeginDictionary("tile_manager");
  tile_manager_.BasicStateAsValueInto(state);
  state->EndDictionary();
}

void LayerTreeHostImpl::SetDebugState(
    const LayerTreeDebugState& new_debug_state) {
  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;

  debug_state_ = new_debug_state;
  UpdateTileManagerMemoryPolicy(ActualManagedMemoryPolicy());
  SetFullRootLayerDamage();
}

void LayerTreeHostImpl::CreateUIResource(UIResourceId uid,
                                         const UIResourceBitmap& bitmap) {
  DCHECK_GT(uid, 0);

  // Allow for multiple creation requests with the same UIResourceId.  The
  // previous resource is simply deleted.
  ResourceId id = ResourceIdForUIResource(uid);
  if (id)
    DeleteUIResource(uid);

  ResourceFormat format = resource_provider_->best_texture_format();
  switch (bitmap.GetFormat()) {
    case UIResourceBitmap::RGBA8:
      break;
    case UIResourceBitmap::ALPHA_8:
      format = ALPHA_8;
      break;
    case UIResourceBitmap::ETC1:
      format = ETC1;
      break;
  }
  id = resource_provider_->CreateResource(
      bitmap.GetSize(), ResourceProvider::TEXTURE_HINT_IMMUTABLE, format);

  UIResourceData data;
  data.resource_id = id;
  data.size = bitmap.GetSize();
  data.opaque = bitmap.GetOpaque();

  ui_resource_map_[uid] = data;

  AutoLockUIResourceBitmap bitmap_lock(bitmap);
  resource_provider_->CopyToResource(id, bitmap_lock.GetPixels(),
                                     bitmap.GetSize());

  resource_provider_->GenerateSyncTokenForResource(id);
  MarkUIResourceNotEvicted(uid);
}

void LayerTreeHostImpl::DeleteUIResource(UIResourceId uid) {
  ResourceId id = ResourceIdForUIResource(uid);
  if (id) {
    resource_provider_->DeleteResource(id);
    ui_resource_map_.erase(uid);
  }
  MarkUIResourceNotEvicted(uid);
}

void LayerTreeHostImpl::ClearUIResources() {
  for (UIResourceMap::const_iterator iter = ui_resource_map_.begin();
      iter != ui_resource_map_.end();
      ++iter) {
    evicted_ui_resources_.insert(iter->first);
    resource_provider_->DeleteResource(iter->second.resource_id);
  }
  ui_resource_map_.clear();
}

void LayerTreeHostImpl::EvictAllUIResources() {
  if (ui_resource_map_.empty())
    return;
  ClearUIResources();

  client_->SetNeedsCommitOnImplThread();
  client_->OnCanDrawStateChanged(CanDraw());
  client_->RenewTreePriority();
}

ResourceId LayerTreeHostImpl::ResourceIdForUIResource(UIResourceId uid) const {
  UIResourceMap::const_iterator iter = ui_resource_map_.find(uid);
  if (iter != ui_resource_map_.end())
    return iter->second.resource_id;
  return 0;
}

bool LayerTreeHostImpl::IsUIResourceOpaque(UIResourceId uid) const {
  UIResourceMap::const_iterator iter = ui_resource_map_.find(uid);
  DCHECK(iter != ui_resource_map_.end());
  return iter->second.opaque;
}

bool LayerTreeHostImpl::EvictedUIResourcesExist() const {
  return !evicted_ui_resources_.empty();
}

void LayerTreeHostImpl::MarkUIResourceNotEvicted(UIResourceId uid) {
  std::set<UIResourceId>::iterator found_in_evicted =
      evicted_ui_resources_.find(uid);
  if (found_in_evicted == evicted_ui_resources_.end())
    return;
  evicted_ui_resources_.erase(found_in_evicted);
  if (evicted_ui_resources_.empty())
    client_->OnCanDrawStateChanged(CanDraw());
}

void LayerTreeHostImpl::ScheduleMicroBenchmark(
    std::unique_ptr<MicroBenchmarkImpl> benchmark) {
  micro_benchmark_controller_.ScheduleRun(std::move(benchmark));
}

void LayerTreeHostImpl::InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.insert(monitor);
}

void LayerTreeHostImpl::RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.erase(monitor);
}

void LayerTreeHostImpl::NotifySwapPromiseMonitorsOfSetNeedsRedraw() {
  std::set<SwapPromiseMonitor*>::iterator it = swap_promise_monitor_.begin();
  for (; it != swap_promise_monitor_.end(); it++)
    (*it)->OnSetNeedsRedrawOnImpl();
}

void LayerTreeHostImpl::NotifySwapPromiseMonitorsOfForwardingToMainThread() {
  std::set<SwapPromiseMonitor*>::iterator it = swap_promise_monitor_.begin();
  for (; it != swap_promise_monitor_.end(); it++)
    (*it)->OnForwardScrollUpdateToMainThreadOnImpl();
}

void LayerTreeHostImpl::UpdateRootLayerStateForSynchronousInputHandler() {
  if (!input_handler_client_)
    return;
  input_handler_client_->UpdateRootLayerStateForSynchronousInputHandler(
      active_tree_->TotalScrollOffset(), active_tree_->TotalMaxScrollOffset(),
      active_tree_->ScrollableSize(), active_tree_->current_page_scale_factor(),
      active_tree_->min_page_scale_factor(),
      active_tree_->max_page_scale_factor());
}

void LayerTreeHostImpl::ScrollAnimationAbort(LayerImpl* layer_impl) {
  return animation_host_->ScrollAnimationAbort(false /* needs_completion */);
}

bool LayerTreeHostImpl::ScrollAnimationUpdateTarget(
    ScrollNode* scroll_node,
    const gfx::Vector2dF& scroll_delta) {
  DCHECK_EQ(
      ElementId(active_tree()->LayerById(scroll_node->owner_id)->element_id()),
      scroll_node->data.element_id);

  return animation_host_->ImplOnlyScrollAnimationUpdateTarget(
      scroll_node->data.element_id, scroll_delta,
      active_tree_->property_trees()->scroll_tree.MaxScrollOffset(
          scroll_node->id),
      CurrentBeginFrameArgs().frame_time);
}

bool LayerTreeHostImpl::IsElementInList(ElementId element_id,
                                        ElementListType list_type) const {
  if (list_type == ElementListType::ACTIVE) {
    return active_tree()
               ? active_tree()->LayerByElementId(element_id) != nullptr
               : false;
  } else {
    if (pending_tree() && pending_tree()->LayerByElementId(element_id))
      return true;
    if (recycle_tree() && recycle_tree()->LayerByElementId(element_id))
      return true;

    return false;
  }
}

void LayerTreeHostImpl::SetMutatorsNeedCommit() {}

void LayerTreeHostImpl::SetMutatorsNeedRebuildPropertyTrees() {}

void LayerTreeHostImpl::SetTreeLayerFilterMutated(
    ElementId element_id,
    LayerTreeImpl* tree,
    const FilterOperations& filters) {
  if (!tree)
    return;

  LayerImpl* layer = tree->LayerByElementId(element_id);
  if (layer)
    layer->OnFilterAnimated(filters);
}

void LayerTreeHostImpl::SetTreeLayerOpacityMutated(ElementId element_id,
                                                   LayerTreeImpl* tree,
                                                   float opacity) {
  if (!tree)
    return;

  LayerImpl* layer = tree->LayerByElementId(element_id);
  if (layer)
    layer->OnOpacityAnimated(opacity);
}

void LayerTreeHostImpl::SetTreeLayerTransformMutated(
    ElementId element_id,
    LayerTreeImpl* tree,
    const gfx::Transform& transform) {
  if (!tree)
    return;

  LayerImpl* layer = tree->LayerByElementId(element_id);
  if (layer)
    layer->OnTransformAnimated(transform);
}

void LayerTreeHostImpl::SetTreeLayerScrollOffsetMutated(
    ElementId element_id,
    LayerTreeImpl* tree,
    const gfx::ScrollOffset& scroll_offset) {
  if (!tree)
    return;

  LayerImpl* layer = tree->LayerByElementId(element_id);
  if (layer) {
    layer->OnScrollOffsetAnimated(scroll_offset);
    // Run mutation callbacks to respond to updated scroll offset.
    Mutate(CurrentBeginFrameArgs().frame_time);
  }
}

bool LayerTreeHostImpl::AnimationsPreserveAxisAlignment(
    const LayerImpl* layer) const {
  return animation_host_->AnimationsPreserveAxisAlignment(layer->element_id());
}

void LayerTreeHostImpl::SetElementFilterMutated(
    ElementId element_id,
    ElementListType list_type,
    const FilterOperations& filters) {
  if (list_type == ElementListType::ACTIVE) {
    SetTreeLayerFilterMutated(element_id, active_tree(), filters);
  } else {
    SetTreeLayerFilterMutated(element_id, pending_tree(), filters);
    SetTreeLayerFilterMutated(element_id, recycle_tree(), filters);
  }
}

void LayerTreeHostImpl::SetElementOpacityMutated(ElementId element_id,
                                                 ElementListType list_type,
                                                 float opacity) {
  if (list_type == ElementListType::ACTIVE) {
    SetTreeLayerOpacityMutated(element_id, active_tree(), opacity);
  } else {
    SetTreeLayerOpacityMutated(element_id, pending_tree(), opacity);
    SetTreeLayerOpacityMutated(element_id, recycle_tree(), opacity);
  }
}

void LayerTreeHostImpl::SetElementTransformMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::Transform& transform) {
  if (list_type == ElementListType::ACTIVE) {
    SetTreeLayerTransformMutated(element_id, active_tree(), transform);
  } else {
    SetTreeLayerTransformMutated(element_id, pending_tree(), transform);
    SetTreeLayerTransformMutated(element_id, recycle_tree(), transform);
  }
}

void LayerTreeHostImpl::SetElementScrollOffsetMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  if (list_type == ElementListType::ACTIVE) {
    SetTreeLayerScrollOffsetMutated(element_id, active_tree(), scroll_offset);
  } else {
    SetTreeLayerScrollOffsetMutated(element_id, pending_tree(), scroll_offset);
    SetTreeLayerScrollOffsetMutated(element_id, recycle_tree(), scroll_offset);
  }
}

void LayerTreeHostImpl::ElementTransformIsAnimatingChanged(
    ElementId element_id,
    ElementListType list_type,
    AnimationChangeType change_type,
    bool is_animating) {
  LayerTreeImpl* tree =
      list_type == ElementListType::ACTIVE ? active_tree() : pending_tree();
  if (!tree)
    return;
  LayerImpl* layer = tree->LayerByElementId(element_id);
  if (layer) {
    switch (change_type) {
      case AnimationChangeType::POTENTIAL:
        layer->OnTransformIsPotentiallyAnimatingChanged(is_animating);
        break;
      case AnimationChangeType::RUNNING:
        layer->OnTransformIsCurrentlyAnimatingChanged(is_animating);
        break;
      case AnimationChangeType::BOTH:
        layer->OnTransformIsPotentiallyAnimatingChanged(is_animating);
        layer->OnTransformIsCurrentlyAnimatingChanged(is_animating);
        break;
    }
  }
}

void LayerTreeHostImpl::ElementOpacityIsAnimatingChanged(
    ElementId element_id,
    ElementListType list_type,
    AnimationChangeType change_type,
    bool is_animating) {
  LayerTreeImpl* tree =
      list_type == ElementListType::ACTIVE ? active_tree() : pending_tree();
  if (!tree)
    return;
  LayerImpl* layer = tree->LayerByElementId(element_id);
  if (layer) {
    switch (change_type) {
      case AnimationChangeType::POTENTIAL:
        layer->OnOpacityIsPotentiallyAnimatingChanged(is_animating);
        break;
      case AnimationChangeType::RUNNING:
        layer->OnOpacityIsCurrentlyAnimatingChanged(is_animating);
        break;
      case AnimationChangeType::BOTH:
        layer->OnOpacityIsPotentiallyAnimatingChanged(is_animating);
        layer->OnOpacityIsCurrentlyAnimatingChanged(is_animating);
        break;
    }
  }
}

void LayerTreeHostImpl::ScrollOffsetAnimationFinished() {
  // TODO(majidvp): We should pass in the original starting scroll position here
  ScrollStateData scroll_state_data;
  ScrollState scroll_state(scroll_state_data);
  ScrollEnd(&scroll_state);
}

gfx::ScrollOffset LayerTreeHostImpl::GetScrollOffsetForAnimation(
    ElementId element_id) const {
  if (active_tree()) {
    LayerImpl* layer = active_tree()->LayerByElementId(element_id);
    if (layer)
      return layer->ScrollOffsetForAnimation();
  }

  return gfx::ScrollOffset();
}

bool LayerTreeHostImpl::SupportsImplScrolling() const {
  // Supported in threaded mode.
  return task_runner_provider_->HasImplThread();
}

bool LayerTreeHostImpl::CommitToActiveTree() const {
  // In single threaded mode we skip the pending tree and commit directly to the
  // active tree.
  return !task_runner_provider_->HasImplThread();
}

}  // namespace cc
