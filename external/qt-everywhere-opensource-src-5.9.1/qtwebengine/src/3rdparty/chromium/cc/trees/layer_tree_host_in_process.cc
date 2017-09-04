// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_in_process.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/blimp/client_picture_cache.h"
#include "cc/blimp/engine_picture_cache.h"
#include "cc/blimp/image_serialization_processor.h"
#include "cc/blimp/picture_data.h"
#include "cc/blimp/picture_data_conversions.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_iterator.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/swap_promise_manager.h"
#include "cc/trees/tree_synchronizer.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace {
static base::StaticAtomicSequenceNumber s_layer_tree_host_sequence_number;
}

namespace cc {

LayerTreeHostInProcess::InitParams::InitParams() {}

LayerTreeHostInProcess::InitParams::~InitParams() {}

std::unique_ptr<LayerTreeHostInProcess> LayerTreeHostInProcess::CreateThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    InitParams* params) {
  DCHECK(params->main_task_runner.get());
  DCHECK(impl_task_runner.get());
  DCHECK(params->settings);
  std::unique_ptr<LayerTreeHostInProcess> layer_tree_host(
      new LayerTreeHostInProcess(params, CompositorMode::THREADED));
  layer_tree_host->InitializeThreaded(params->main_task_runner,
                                      impl_task_runner);
  return layer_tree_host;
}

std::unique_ptr<LayerTreeHostInProcess>
LayerTreeHostInProcess::CreateSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    InitParams* params) {
  DCHECK(params->settings);
  std::unique_ptr<LayerTreeHostInProcess> layer_tree_host(
      new LayerTreeHostInProcess(params, CompositorMode::SINGLE_THREADED));
  layer_tree_host->InitializeSingleThreaded(single_thread_client,
                                            params->main_task_runner);
  return layer_tree_host;
}

LayerTreeHostInProcess::LayerTreeHostInProcess(InitParams* params,
                                               CompositorMode mode)
    : LayerTreeHostInProcess(
          params,
          mode,
          base::MakeUnique<LayerTree>(params->mutator_host, this)) {}

LayerTreeHostInProcess::LayerTreeHostInProcess(
    InitParams* params,
    CompositorMode mode,
    std::unique_ptr<LayerTree> layer_tree)
    : micro_benchmark_controller_(this),
      layer_tree_(std::move(layer_tree)),
      compositor_mode_(mode),
      ui_resource_manager_(base::MakeUnique<UIResourceManager>()),
      client_(params->client),
      source_frame_number_(0),
      rendering_stats_instrumentation_(RenderingStatsInstrumentation::Create()),
      settings_(*params->settings),
      debug_state_(settings_.initial_debug_state),
      visible_(false),
      has_gpu_rasterization_trigger_(false),
      content_is_suitable_for_gpu_rasterization_(true),
      gpu_rasterization_histogram_recorded_(false),
      did_complete_scale_animation_(false),
      id_(s_layer_tree_host_sequence_number.GetNext() + 1),
      task_graph_runner_(params->task_graph_runner),
      image_serialization_processor_(params->image_serialization_processor) {
  DCHECK(task_graph_runner_);
  DCHECK(layer_tree_);
  DCHECK_NE(compositor_mode_, CompositorMode::REMOTE);

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());
}

void LayerTreeHostInProcess::InitializeThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  task_runner_provider_ =
      TaskRunnerProvider::Create(main_task_runner, impl_task_runner);
  std::unique_ptr<ProxyMain> proxy_main =
      ProxyMain::CreateThreaded(this, task_runner_provider_.get());
  InitializeProxy(std::move(proxy_main));
}

void LayerTreeHostInProcess::InitializeSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  task_runner_provider_ = TaskRunnerProvider::Create(main_task_runner, nullptr);
  InitializeProxy(SingleThreadProxy::Create(this, single_thread_client,
                                            task_runner_provider_.get()));
}

void LayerTreeHostInProcess::InitializeForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider,
    std::unique_ptr<Proxy> proxy_for_testing) {
  task_runner_provider_ = std::move(task_runner_provider);

  InitializePictureCacheForTesting();

  InitializeProxy(std::move(proxy_for_testing));
}

void LayerTreeHostInProcess::InitializePictureCacheForTesting() {
  if (!image_serialization_processor_)
    return;

  // Initialize both engine and client cache to ensure serialization tests
  // with a single LayerTreeHostInProcess can work correctly.
  engine_picture_cache_ =
      image_serialization_processor_->CreateEnginePictureCache();
  layer_tree_->set_engine_picture_cache(engine_picture_cache_.get());
  client_picture_cache_ =
      image_serialization_processor_->CreateClientPictureCache();
  layer_tree_->set_client_picture_cache(client_picture_cache_.get());
}

void LayerTreeHostInProcess::SetTaskRunnerProviderForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider) {
  DCHECK(!task_runner_provider_);
  task_runner_provider_ = std::move(task_runner_provider);
}

void LayerTreeHostInProcess::SetUIResourceManagerForTesting(
    std::unique_ptr<UIResourceManager> ui_resource_manager) {
  ui_resource_manager_ = std::move(ui_resource_manager);
}

void LayerTreeHostInProcess::InitializeProxy(std::unique_ptr<Proxy> proxy) {
  TRACE_EVENT0("cc", "LayerTreeHostInProcess::InitializeForReal");
  DCHECK(task_runner_provider_);

  proxy_ = std::move(proxy);
  proxy_->Start();

  layer_tree_->mutator_host()->SetSupportsScrollAnimations(
      proxy_->SupportsImplScrolling());
}

LayerTreeHostInProcess::~LayerTreeHostInProcess() {
  TRACE_EVENT0("cc", "LayerTreeHostInProcess::~LayerTreeHostInProcess");

  // Clear any references into the LayerTreeHostInProcess.
  layer_tree_.reset();

  if (proxy_) {
    DCHECK(task_runner_provider_->IsMainThread());
    proxy_->Stop();

    // Proxy must be destroyed before the Task Runner Provider.
    proxy_ = nullptr;
  }
}

int LayerTreeHostInProcess::GetId() const {
  return id_;
}

int LayerTreeHostInProcess::SourceFrameNumber() const {
  return source_frame_number_;
}

LayerTree* LayerTreeHostInProcess::GetLayerTree() {
  return layer_tree_.get();
}

const LayerTree* LayerTreeHostInProcess::GetLayerTree() const {
  return layer_tree_.get();
}

UIResourceManager* LayerTreeHostInProcess::GetUIResourceManager() const {
  return ui_resource_manager_.get();
}

TaskRunnerProvider* LayerTreeHostInProcess::GetTaskRunnerProvider() const {
  return task_runner_provider_.get();
}

SwapPromiseManager* LayerTreeHostInProcess::GetSwapPromiseManager() {
  return &swap_promise_manager_;
}

const LayerTreeSettings& LayerTreeHostInProcess::GetSettings() const {
  return settings_;
}

void LayerTreeHostInProcess::SetFrameSinkId(const FrameSinkId& frame_sink_id) {
  surface_sequence_generator_.set_frame_sink_id(frame_sink_id);
}

void LayerTreeHostInProcess::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  swap_promise_manager_.QueueSwapPromise(std::move(swap_promise));
}

SurfaceSequenceGenerator*
LayerTreeHostInProcess::GetSurfaceSequenceGenerator() {
  return &surface_sequence_generator_;
}

void LayerTreeHostInProcess::WillBeginMainFrame() {
  devtools_instrumentation::WillBeginMainThreadFrame(GetId(),
                                                     SourceFrameNumber());
  client_->WillBeginMainFrame();
}

void LayerTreeHostInProcess::DidBeginMainFrame() {
  client_->DidBeginMainFrame();
}

void LayerTreeHostInProcess::BeginMainFrameNotExpectedSoon() {
  client_->BeginMainFrameNotExpectedSoon();
}

void LayerTreeHostInProcess::BeginMainFrame(const BeginFrameArgs& args) {
  client_->BeginMainFrame(args);
}

void LayerTreeHostInProcess::DidStopFlinging() {
  proxy_->MainThreadHasStoppedFlinging();
}

const LayerTreeDebugState& LayerTreeHostInProcess::GetDebugState() const {
  return debug_state_;
}

void LayerTreeHostInProcess::RequestMainFrameUpdate() {
  client_->UpdateLayerTreeHost();
}

// This function commits the LayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a Layer,
// should be delayed until the LayerTreeHostInProcess::CommitComplete, which
// will run after the commit, but on the main thread.
void LayerTreeHostInProcess::FinishCommitOnImplThread(
    LayerTreeHostImpl* host_impl) {
  DCHECK(task_runner_provider_->IsImplThread());

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (is_new_trace &&
      frame_viewer_instrumentation::IsTracingLayerTreeSnapshots() &&
      layer_tree_->root_layer()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        layer_tree_.get(), [](Layer* layer) { layer->DidBeginTracing(); });
  }

  LayerTreeImpl* sync_tree = host_impl->sync_tree();

  if (next_commit_forces_redraw_) {
    sync_tree->ForceRedrawNextActivation();
    next_commit_forces_redraw_ = false;
  }
  if (next_commit_forces_recalculate_raster_scales_) {
    sync_tree->ForceRecalculateRasterScales();
    next_commit_forces_recalculate_raster_scales_ = false;
  }

  sync_tree->set_source_frame_number(SourceFrameNumber());

  if (layer_tree_->needs_full_tree_sync())
    TreeSynchronizer::SynchronizeTrees(layer_tree_->root_layer(), sync_tree);

  float page_scale_delta = 1.f;
  if (reflected_main_frame_state_)
    page_scale_delta = reflected_main_frame_state_->page_scale_delta;
  layer_tree_->PushPropertiesTo(sync_tree, page_scale_delta);

  sync_tree->PassSwapPromises(swap_promise_manager_.TakeSwapPromises());

  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentIsSuitableForGpuRasterization(
      content_is_suitable_for_gpu_rasterization_);
  RecordGpuRasterizationHistogram();

  host_impl->SetViewportSize(layer_tree_->device_viewport_size());
  sync_tree->SetDeviceScaleFactor(layer_tree_->device_scale_factor());
  host_impl->SetDebugState(debug_state_);

  sync_tree->set_ui_resource_request_queue(
      ui_resource_manager_->TakeUIResourcesRequests());

  {
    TRACE_EVENT0("cc", "LayerTreeHostInProcess::PushProperties");

    TreeSynchronizer::PushLayerProperties(layer_tree_.get(), sync_tree);

    if (reflected_main_frame_state_) {
      for (const auto& scroll_update : reflected_main_frame_state_->scrolls) {
        int layer_id = scroll_update.layer_id;
        gfx::Vector2dF scroll_delta = scroll_update.scroll_delta;

        PropertyTrees* property_trees = layer_tree_->property_trees();
        property_trees->scroll_tree.SetScrollOffset(
            layer_id, gfx::ScrollOffsetWithDelta(
                          layer_tree_->LayerById(layer_id)->scroll_offset(),
                          scroll_delta));
      }
    }

    // This must happen after synchronizing property trees and after pushing
    // properties, which updates the clobber_active_value flag.
    sync_tree->UpdatePropertyTreeScrollOffset(layer_tree_->property_trees());

    // This must happen after synchronizing property trees and after push
    // properties, which updates property tree indices, but before animation
    // host pushes properties as animation host push properties can change
    // Animation::InEffect and we want the old InEffect value for updating
    // property tree scrolling and animation.
    sync_tree->UpdatePropertyTreeScrollingAndAnimationFromMainThread();

    TRACE_EVENT0("cc", "LayerTreeHostInProcess::AnimationHost::PushProperties");
    DCHECK(host_impl->mutator_host());
    layer_tree_->mutator_host()->PushPropertiesTo(host_impl->mutator_host());
  }

  micro_benchmark_controller_.ScheduleImplBenchmarks(host_impl);
  layer_tree_->property_trees()->ResetAllChangeTracking();
  reflected_main_frame_state_ = nullptr;
}

void LayerTreeHostInProcess::WillCommit() {
  swap_promise_manager_.WillCommit();
  client_->WillCommit();
}

void LayerTreeHostInProcess::UpdateHudLayer() {}

void LayerTreeHostInProcess::CommitComplete() {
  source_frame_number_++;
  client_->DidCommit();
  if (did_complete_scale_animation_) {
    client_->DidCompletePageScaleAnimation();
    did_complete_scale_animation_ = false;
  }
}

void LayerTreeHostInProcess::SetCompositorFrameSink(
    std::unique_ptr<CompositorFrameSink> surface) {
  TRACE_EVENT0("cc", "LayerTreeHostInProcess::SetCompositorFrameSink");
  DCHECK(surface);

  DCHECK(!new_compositor_frame_sink_);
  new_compositor_frame_sink_ = std::move(surface);
  proxy_->SetCompositorFrameSink(new_compositor_frame_sink_.get());
}

std::unique_ptr<CompositorFrameSink>
LayerTreeHostInProcess::ReleaseCompositorFrameSink() {
  DCHECK(!visible_);

  DidLoseCompositorFrameSink();
  proxy_->ReleaseCompositorFrameSink();
  return std::move(current_compositor_frame_sink_);
}

void LayerTreeHostInProcess::RequestNewCompositorFrameSink() {
  client_->RequestNewCompositorFrameSink();
}

void LayerTreeHostInProcess::DidInitializeCompositorFrameSink() {
  DCHECK(new_compositor_frame_sink_);
  current_compositor_frame_sink_ = std::move(new_compositor_frame_sink_);
  client_->DidInitializeCompositorFrameSink();
}

void LayerTreeHostInProcess::DidFailToInitializeCompositorFrameSink() {
  DCHECK(new_compositor_frame_sink_);
  // Note: It is safe to drop all output surface references here as
  // LayerTreeHostImpl will not keep a pointer to either the old or
  // new CompositorFrameSink after failing to initialize the new one.
  current_compositor_frame_sink_ = nullptr;
  new_compositor_frame_sink_ = nullptr;
  client_->DidFailToInitializeCompositorFrameSink();
}

std::unique_ptr<LayerTreeHostImpl>
LayerTreeHostInProcess::CreateLayerTreeHostImpl(
    LayerTreeHostImplClient* client) {
  DCHECK(task_runner_provider_->IsImplThread());

  const bool supports_impl_scrolling = task_runner_provider_->HasImplThread();
  std::unique_ptr<MutatorHost> mutator_host_impl =
      layer_tree_->mutator_host()->CreateImplInstance(supports_impl_scrolling);

  std::unique_ptr<LayerTreeHostImpl> host_impl = LayerTreeHostImpl::Create(
      settings_, client, task_runner_provider_.get(),
      rendering_stats_instrumentation_.get(), task_graph_runner_,
      std::move(mutator_host_impl), id_);
  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentIsSuitableForGpuRasterization(
      content_is_suitable_for_gpu_rasterization_);
  task_graph_runner_ = NULL;
  input_handler_weak_ptr_ = host_impl->AsWeakPtr();
  return host_impl;
}

void LayerTreeHostInProcess::DidLoseCompositorFrameSink() {
  TRACE_EVENT0("cc", "LayerTreeHostInProcess::DidLoseCompositorFrameSink");
  DCHECK(task_runner_provider_->IsMainThread());
  SetNeedsCommit();
}

void LayerTreeHostInProcess::SetDeferCommits(bool defer_commits) {
  proxy_->SetDeferCommits(defer_commits);
}

DISABLE_CFI_PERF
void LayerTreeHostInProcess::SetNeedsAnimate() {
  proxy_->SetNeedsAnimate();
  swap_promise_manager_.NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

DISABLE_CFI_PERF
void LayerTreeHostInProcess::SetNeedsUpdateLayers() {
  proxy_->SetNeedsUpdateLayers();
  swap_promise_manager_.NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHostInProcess::SetNeedsCommit() {
  proxy_->SetNeedsCommit();
  swap_promise_manager_.NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHostInProcess::SetNeedsRecalculateRasterScales() {
  next_commit_forces_recalculate_raster_scales_ = true;
  proxy_->SetNeedsCommit();
}

void LayerTreeHostInProcess::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  proxy_->SetNeedsRedraw(damage_rect);
}

bool LayerTreeHostInProcess::CommitRequested() const {
  return proxy_->CommitRequested();
}

bool LayerTreeHostInProcess::BeginMainFrameRequested() const {
  return proxy_->BeginMainFrameRequested();
}

void LayerTreeHostInProcess::SetNextCommitWaitsForActivation() {
  proxy_->SetNextCommitWaitsForActivation();
}

void LayerTreeHostInProcess::SetNextCommitForcesRedraw() {
  next_commit_forces_redraw_ = true;
  proxy_->SetNeedsUpdateLayers();
}

void LayerTreeHostInProcess::SetAnimationEvents(
    std::unique_ptr<MutatorEvents> events) {
  DCHECK(task_runner_provider_->IsMainThread());
  layer_tree_->mutator_host()->SetAnimationEvents(std::move(events));
}

void LayerTreeHostInProcess::SetDebugState(
    const LayerTreeDebugState& debug_state) {
  LayerTreeDebugState new_debug_state =
      LayerTreeDebugState::Unite(settings_.initial_debug_state, debug_state);

  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;

  debug_state_ = new_debug_state;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());

  SetNeedsCommit();
}

void LayerTreeHostInProcess::ResetGpuRasterizationTracking() {
  content_is_suitable_for_gpu_rasterization_ = true;
  gpu_rasterization_histogram_recorded_ = false;
}

void LayerTreeHostInProcess::SetHasGpuRasterizationTrigger(bool has_trigger) {
  if (has_trigger == has_gpu_rasterization_trigger_)
    return;

  has_gpu_rasterization_trigger_ = has_trigger;
  TRACE_EVENT_INSTANT1(
      "cc", "LayerTreeHostInProcess::SetHasGpuRasterizationTrigger",
      TRACE_EVENT_SCOPE_THREAD, "has_trigger", has_gpu_rasterization_trigger_);
}

void LayerTreeHostInProcess::ApplyPageScaleDeltaFromImplSide(
    float page_scale_delta) {
  DCHECK(CommitRequested());
  if (page_scale_delta == 1.f)
    return;
  float page_scale = layer_tree_->page_scale_factor() * page_scale_delta;
  layer_tree_->SetPageScaleFromImplSide(page_scale);
}

void LayerTreeHostInProcess::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  proxy_->SetVisible(visible);
}

bool LayerTreeHostInProcess::IsVisible() const {
  return visible_;
}

void LayerTreeHostInProcess::NotifyInputThrottledUntilCommit() {
  proxy_->NotifyInputThrottledUntilCommit();
}

void LayerTreeHostInProcess::LayoutAndUpdateLayers() {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  RequestMainFrameUpdate();
  UpdateLayers();
}

void LayerTreeHostInProcess::Composite(base::TimeTicks frame_begin_time) {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  SingleThreadProxy* proxy = static_cast<SingleThreadProxy*>(proxy_.get());

  proxy->CompositeImmediately(frame_begin_time);
}

bool LayerTreeHostInProcess::UpdateLayers() {
  if (!layer_tree_->root_layer())
    return false;
  DCHECK(!layer_tree_->root_layer()->parent());
  bool result = DoUpdateLayers(layer_tree_->root_layer());
  micro_benchmark_controller_.DidUpdateLayers();
  return result || next_commit_forces_redraw_;
}

void LayerTreeHostInProcess::DidCompletePageScaleAnimation() {
  did_complete_scale_animation_ = true;
}

void LayerTreeHostInProcess::RecordGpuRasterizationHistogram() {
  // Gpu rasterization is only supported for Renderer compositors.
  // Checking for IsSingleThreaded() to exclude Browser compositors.
  if (gpu_rasterization_histogram_recorded_ || IsSingleThreaded())
    return;

  // Record how widely gpu rasterization is enabled.
  // This number takes device/gpu whitelisting/backlisting into account.
  // Note that we do not consider the forced gpu rasterization mode, which is
  // mostly used for debugging purposes.
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationEnabled",
                        settings_.gpu_rasterization_enabled);
  if (settings_.gpu_rasterization_enabled) {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationTriggered",
                          has_gpu_rasterization_trigger_);
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationSuitableContent",
                          content_is_suitable_for_gpu_rasterization_);
    // Record how many pages actually get gpu rasterization when enabled.
    UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationUsed",
                          (has_gpu_rasterization_trigger_ &&
                           content_is_suitable_for_gpu_rasterization_));
  }

  gpu_rasterization_histogram_recorded_ = true;
}

bool LayerTreeHostInProcess::DoUpdateLayers(Layer* root_layer) {
  TRACE_EVENT1("cc", "LayerTreeHostInProcess::DoUpdateLayers",
               "source_frame_number", SourceFrameNumber());

  layer_tree_->UpdateHudLayer(debug_state_.ShowHudInfo());
  UpdateHudLayer();

  Layer* root_scroll =
      PropertyTreeBuilder::FindFirstScrollableLayer(root_layer);
  Layer* page_scale_layer = layer_tree_->page_scale_layer();
  if (!page_scale_layer && root_scroll)
    page_scale_layer = root_scroll->parent();

  if (layer_tree_->hud_layer()) {
    layer_tree_->hud_layer()->PrepareForCalculateDrawProperties(
        layer_tree_->device_viewport_size(),
        layer_tree_->device_scale_factor());
  }

  gfx::Transform identity_transform;
  LayerList update_layer_list;

  {
    TRACE_EVENT0("cc",
                 "LayerTreeHostInProcess::UpdateLayers::BuildPropertyTrees");
    TRACE_EVENT0(
        TRACE_DISABLED_BY_DEFAULT("cc.debug.cdp-perf"),
        "LayerTreeHostInProcessCommon::ComputeVisibleRectsWithPropertyTrees");
    PropertyTreeBuilder::PreCalculateMetaInformation(root_layer);
    bool can_render_to_separate_surface = true;
    PropertyTrees* property_trees = layer_tree_->property_trees();
    if (!settings_.use_layer_lists) {
      // If use_layer_lists is set, then the property trees should have been
      // built by the client already.
      PropertyTreeBuilder::BuildPropertyTrees(
          root_layer, page_scale_layer,
          layer_tree_->inner_viewport_scroll_layer(),
          layer_tree_->outer_viewport_scroll_layer(),
          layer_tree_->overscroll_elasticity_layer(),
          layer_tree_->elastic_overscroll(), layer_tree_->page_scale_factor(),
          layer_tree_->device_scale_factor(),
          gfx::Rect(layer_tree_->device_viewport_size()), identity_transform,
          property_trees);
      TRACE_EVENT_INSTANT1(
          "cc", "LayerTreeHostInProcess::UpdateLayers_BuiltPropertyTrees",
          TRACE_EVENT_SCOPE_THREAD, "property_trees",
          property_trees->AsTracedValue());
    } else {
      TRACE_EVENT_INSTANT1(
          "cc", "LayerTreeHostInProcess::UpdateLayers_ReceivedPropertyTrees",
          TRACE_EVENT_SCOPE_THREAD, "property_trees",
          property_trees->AsTracedValue());
    }
    draw_property_utils::UpdatePropertyTrees(property_trees,
                                             can_render_to_separate_surface);
    draw_property_utils::FindLayersThatNeedUpdates(
        layer_tree_.get(), property_trees, &update_layer_list);
  }

  for (const auto& layer : update_layer_list)
    layer->SavePaintProperties();

  bool content_is_suitable_for_gpu = true;
  bool did_paint_content = layer_tree_->UpdateLayers(
      update_layer_list, &content_is_suitable_for_gpu);

  if (content_is_suitable_for_gpu) {
    ++num_consecutive_frames_suitable_for_gpu_;
    if (num_consecutive_frames_suitable_for_gpu_ >=
        kNumFramesToConsiderBeforeGpuRasterization) {
      content_is_suitable_for_gpu_rasterization_ = true;
    }
  } else {
    num_consecutive_frames_suitable_for_gpu_ = 0;
    content_is_suitable_for_gpu_rasterization_ = false;
  }
  return did_paint_content;
}

void LayerTreeHostInProcess::ApplyViewportDeltas(ScrollAndScaleSet* info) {
  gfx::Vector2dF inner_viewport_scroll_delta;
  if (info->inner_viewport_scroll.layer_id != Layer::INVALID_ID)
    inner_viewport_scroll_delta = info->inner_viewport_scroll.scroll_delta;

  if (inner_viewport_scroll_delta.IsZero() && info->page_scale_delta == 1.f &&
      info->elastic_overscroll_delta.IsZero() && !info->top_controls_delta)
    return;

  // Preemptively apply the scroll offset and scale delta here before sending
  // it to the client.  If the client comes back and sets it to the same
  // value, then the layer can early out without needing a full commit.
  if (layer_tree_->inner_viewport_scroll_layer()) {
    layer_tree_->inner_viewport_scroll_layer()->SetScrollOffsetFromImplSide(
        gfx::ScrollOffsetWithDelta(
            layer_tree_->inner_viewport_scroll_layer()->scroll_offset(),
            inner_viewport_scroll_delta));
  }

  ApplyPageScaleDeltaFromImplSide(info->page_scale_delta);
  layer_tree_->SetElasticOverscrollFromImplSide(
      layer_tree_->elastic_overscroll() + info->elastic_overscroll_delta);
  // TODO(ccameron): pass the elastic overscroll here so that input events
  // may be translated appropriately.
  client_->ApplyViewportDeltas(inner_viewport_scroll_delta, gfx::Vector2dF(),
                               info->elastic_overscroll_delta,
                               info->page_scale_delta,
                               info->top_controls_delta);
  SetNeedsUpdateLayers();
}

void LayerTreeHostInProcess::ApplyScrollAndScale(ScrollAndScaleSet* info) {
  for (auto& swap_promise : info->swap_promises) {
    TRACE_EVENT_WITH_FLOW1("input,benchmark", "LatencyInfo.Flow",
                           TRACE_ID_DONT_MANGLE(swap_promise->TraceId()),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", "Main thread scroll update");
    swap_promise_manager_.QueueSwapPromise(std::move(swap_promise));
  }

  if (layer_tree_->root_layer()) {
    for (size_t i = 0; i < info->scrolls.size(); ++i) {
      Layer* layer = layer_tree_->LayerById(info->scrolls[i].layer_id);
      if (!layer)
        continue;
      layer->SetScrollOffsetFromImplSide(gfx::ScrollOffsetWithDelta(
          layer->scroll_offset(), info->scrolls[i].scroll_delta));
      SetNeedsUpdateLayers();
    }
    for (size_t i = 0; i < info->scrollbars.size(); ++i) {
      Layer* layer = layer_tree_->LayerById(info->scrollbars[i].layer_id);
      if (!layer)
        continue;
      layer->SetScrollbarsHiddenFromImplSide(info->scrollbars[i].hidden);
    }
  }

  // This needs to happen after scroll deltas have been sent to prevent top
  // controls from clamping the layout viewport both on the compositor and
  // on the main thread.
  ApplyViewportDeltas(info);
}

void LayerTreeHostInProcess::SetReflectedMainFrameState(
    std::unique_ptr<ReflectedMainFrameState> reflected_main_frame_state) {
  DCHECK(IsThreaded());

  reflected_main_frame_state_ = std::move(reflected_main_frame_state);
  SetNeedsCommit();
}

const base::WeakPtr<InputHandler>& LayerTreeHostInProcess::GetInputHandler()
    const {
  return input_handler_weak_ptr_;
}

void LayerTreeHostInProcess::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  // Browser controls are only used in threaded mode.
  DCHECK(IsThreaded());
  proxy_->UpdateBrowserControlsState(constraints, current, animate);
}

void LayerTreeHostInProcess::AnimateLayers(base::TimeTicks monotonic_time) {
  MutatorHost* mutator_host = layer_tree_->mutator_host();
  std::unique_ptr<MutatorEvents> events = mutator_host->CreateEvents();

  if (mutator_host->AnimateLayers(monotonic_time))
    mutator_host->UpdateAnimationState(true, events.get());

  if (!events->IsEmpty())
    layer_tree_->property_trees()->needs_rebuild = true;
}

int LayerTreeHostInProcess::ScheduleMicroBenchmark(
    const std::string& benchmark_name,
    std::unique_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback) {
  return micro_benchmark_controller_.ScheduleRun(benchmark_name,
                                                 std::move(value), callback);
}

bool LayerTreeHostInProcess::SendMessageToMicroBenchmark(
    int id,
    std::unique_ptr<base::Value> value) {
  return micro_benchmark_controller_.SendMessage(id, std::move(value));
}

void LayerTreeHostInProcess::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  proxy_->SetMutator(std::move(mutator));
}

bool LayerTreeHostInProcess::IsSingleThreaded() const {
  DCHECK(compositor_mode_ != CompositorMode::SINGLE_THREADED ||
         !task_runner_provider_->HasImplThread());
  return compositor_mode_ == CompositorMode::SINGLE_THREADED;
}

bool LayerTreeHostInProcess::IsThreaded() const {
  DCHECK(compositor_mode_ != CompositorMode::THREADED ||
         task_runner_provider_->HasImplThread());
  return compositor_mode_ == CompositorMode::THREADED;
}

}  // namespace cc
