// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>

#include "base/atomic_sequence_num.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_math.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_host.h"
#include "cc/base/math_util.h"
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
#include "cc/layers/layer_proto_converter.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_tree_host.pb.h"
#include "cc/resources/ui_resource_request.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/proxy_main.h"
#include "cc/trees/remote_channel_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/tree_synchronizer.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace {
static base::StaticAtomicSequenceNumber s_layer_tree_host_sequence_number;
}

namespace cc {
namespace {

Layer* UpdateAndGetLayer(Layer* current_layer,
                         int layer_id,
                         const std::unordered_map<int, Layer*>& layer_id_map) {
  if (layer_id == Layer::INVALID_ID) {
    if (current_layer)
      current_layer->SetLayerTreeHost(nullptr);

    return nullptr;
  }

  auto layer_it = layer_id_map.find(layer_id);
  DCHECK(layer_it != layer_id_map.end());
  if (current_layer && current_layer != layer_it->second)
    current_layer->SetLayerTreeHost(nullptr);

  return layer_it->second;
}

std::unique_ptr<base::trace_event::TracedValue>
ComputeLayerTreeHostProtoSizeSplitAsValue(proto::LayerTreeHost* proto) {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  base::CheckedNumeric<int> base_layer_properties_size = 0;
  base::CheckedNumeric<int> picture_layer_properties_size = 0;
  base::CheckedNumeric<int> display_item_list_size = 0;
  base::CheckedNumeric<int> drawing_display_items_size = 0;

  const proto::LayerUpdate& layer_update_proto = proto->layer_updates();
  for (int i = 0; i < layer_update_proto.layers_size(); ++i) {
    const proto::LayerProperties layer_properties_proto =
        layer_update_proto.layers(i);
    base_layer_properties_size += layer_properties_proto.base().ByteSize();

    if (layer_properties_proto.has_picture()) {
      const proto::PictureLayerProperties& picture_proto =
          layer_properties_proto.picture();
      picture_layer_properties_size += picture_proto.ByteSize();

      const proto::RecordingSource& recording_source_proto =
          picture_proto.recording_source();
      const proto::DisplayItemList& display_list_proto =
          recording_source_proto.display_list();
      display_item_list_size += display_list_proto.ByteSize();

      for (int j = 0; j < display_list_proto.items_size(); ++j) {
        const proto::DisplayItem& display_item = display_list_proto.items(j);
        if (display_item.type() == proto::DisplayItem::Type_Drawing)
          drawing_display_items_size += display_item.ByteSize();
      }
    }
  }

  value->SetInteger("TotalLayerTreeHostProtoSize", proto->ByteSize());
  value->SetInteger("LayerTreeHierarchySize", proto->root_layer().ByteSize());
  value->SetInteger("LayerUpdatesSize", proto->layer_updates().ByteSize());
  value->SetInteger("PropertyTreesSize", proto->property_trees().ByteSize());

  // LayerUpdate size breakdown.
  value->SetInteger("TotalBasePropertiesSize",
                    base_layer_properties_size.ValueOrDefault(-1));
  value->SetInteger("PictureLayerPropertiesSize",
                    picture_layer_properties_size.ValueOrDefault(-1));
  value->SetInteger("DisplayItemListSize",
                    display_item_list_size.ValueOrDefault(-1));
  value->SetInteger("DrawingDisplayItemsSize",
                    drawing_display_items_size.ValueOrDefault(-1));
  return value;
}

}  // namespace

LayerTreeHost::InitParams::InitParams() {
}

LayerTreeHost::InitParams::~InitParams() {
}

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    InitParams* params) {
  DCHECK(params->main_task_runner.get());
  DCHECK(impl_task_runner.get());
  DCHECK(params->settings);
  std::unique_ptr<LayerTreeHost> layer_tree_host(
      new LayerTreeHost(params, CompositorMode::THREADED));
  layer_tree_host->InitializeThreaded(
      params->main_task_runner, impl_task_runner,
      std::move(params->external_begin_frame_source));
  return layer_tree_host;
}

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    InitParams* params) {
  DCHECK(params->settings);
  std::unique_ptr<LayerTreeHost> layer_tree_host(
      new LayerTreeHost(params, CompositorMode::SINGLE_THREADED));
  layer_tree_host->InitializeSingleThreaded(
      single_thread_client, params->main_task_runner,
      std::move(params->external_begin_frame_source));
  return layer_tree_host;
}

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateRemoteServer(
    RemoteProtoChannel* remote_proto_channel,
    InitParams* params) {
  DCHECK(params->main_task_runner.get());
  DCHECK(params->settings);
  DCHECK(remote_proto_channel);
  TRACE_EVENT0("cc.remote", "LayerTreeHost::CreateRemoteServer");

  // Using an external begin frame source is not supported on the server in
  // remote mode.
  DCHECK(!params->settings->use_external_begin_frame_source);
  DCHECK(!params->external_begin_frame_source);
  DCHECK(params->image_serialization_processor);

  std::unique_ptr<LayerTreeHost> layer_tree_host(
      new LayerTreeHost(params, CompositorMode::REMOTE));
  layer_tree_host->InitializeRemoteServer(remote_proto_channel,
                                          params->main_task_runner);
  return layer_tree_host;
}

std::unique_ptr<LayerTreeHost> LayerTreeHost::CreateRemoteClient(
    RemoteProtoChannel* remote_proto_channel,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    InitParams* params) {
  DCHECK(params->main_task_runner.get());
  DCHECK(params->settings);
  DCHECK(remote_proto_channel);

  // Using an external begin frame source is not supported in remote mode.
  // TODO(khushalsagar): Add support for providing an external begin frame
  // source on the client LayerTreeHost. crbug/576962
  DCHECK(!params->settings->use_external_begin_frame_source);
  DCHECK(!params->external_begin_frame_source);
  DCHECK(params->image_serialization_processor);

  std::unique_ptr<LayerTreeHost> layer_tree_host(
      new LayerTreeHost(params, CompositorMode::REMOTE));
  layer_tree_host->InitializeRemoteClient(
      remote_proto_channel, params->main_task_runner, impl_task_runner);
  return layer_tree_host;
}

LayerTreeHost::LayerTreeHost(InitParams* params, CompositorMode mode)
    : micro_benchmark_controller_(this),
      next_ui_resource_id_(1),
      compositor_mode_(mode),
      needs_full_tree_sync_(true),
      needs_meta_info_recomputation_(true),
      client_(params->client),
      source_frame_number_(0),
      rendering_stats_instrumentation_(RenderingStatsInstrumentation::Create()),
      output_surface_lost_(true),
      settings_(*params->settings),
      debug_state_(settings_.initial_debug_state),
      top_controls_shrink_blink_size_(false),
      top_controls_height_(0.f),
      top_controls_shown_ratio_(0.f),
      device_scale_factor_(1.f),
      painted_device_scale_factor_(1.f),
      visible_(false),
      page_scale_factor_(1.f),
      min_page_scale_factor_(1.f),
      max_page_scale_factor_(1.f),
      has_gpu_rasterization_trigger_(false),
      content_is_suitable_for_gpu_rasterization_(true),
      gpu_rasterization_histogram_recorded_(false),
      background_color_(SK_ColorWHITE),
      has_transparent_background_(false),
      have_scroll_event_handlers_(false),
      event_listener_properties_(),
      animation_host_(std::move(params->animation_host)),
      did_complete_scale_animation_(false),
      in_paint_layer_contents_(false),
      id_(s_layer_tree_host_sequence_number.GetNext() + 1),
      next_commit_forces_redraw_(false),
      shared_bitmap_manager_(params->shared_bitmap_manager),
      gpu_memory_buffer_manager_(params->gpu_memory_buffer_manager),
      task_graph_runner_(params->task_graph_runner),
      image_serialization_processor_(params->image_serialization_processor),
      surface_id_namespace_(0u),
      next_surface_sequence_(1u) {
  DCHECK(task_graph_runner_);

  DCHECK(animation_host_);
  animation_host_->SetMutatorHostClient(this);

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());
}

void LayerTreeHost::InitializeThreaded(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  task_runner_provider_ =
      TaskRunnerProvider::Create(main_task_runner, impl_task_runner);
  std::unique_ptr<ProxyMain> proxy_main =
      ProxyMain::CreateThreaded(this, task_runner_provider_.get());
  InitializeProxy(std::move(proxy_main),
                  std::move(external_begin_frame_source));
}

void LayerTreeHost::InitializeSingleThreaded(
    LayerTreeHostSingleThreadClient* single_thread_client,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  task_runner_provider_ = TaskRunnerProvider::Create(main_task_runner, nullptr);
  InitializeProxy(SingleThreadProxy::Create(this, single_thread_client,
                                            task_runner_provider_.get()),
                  std::move(external_begin_frame_source));
}

void LayerTreeHost::InitializeRemoteServer(
    RemoteProtoChannel* remote_proto_channel,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  task_runner_provider_ = TaskRunnerProvider::Create(main_task_runner, nullptr);

  if (image_serialization_processor_) {
    engine_picture_cache_ =
        image_serialization_processor_->CreateEnginePictureCache();
  }

  // The LayerTreeHost on the server never requests the output surface since
  // it is only needed on the client. Since ProxyMain aborts commits if
  // output_surface_lost() is true, always assume we have the output surface
  // on the server.
  output_surface_lost_ = false;

  InitializeProxy(ProxyMain::CreateRemote(remote_proto_channel, this,
                                          task_runner_provider_.get()),
                  nullptr);
}

void LayerTreeHost::InitializeRemoteClient(
    RemoteProtoChannel* remote_proto_channel,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner) {
  task_runner_provider_ =
      TaskRunnerProvider::Create(main_task_runner, impl_task_runner);

  if (image_serialization_processor_) {
    client_picture_cache_ =
        image_serialization_processor_->CreateClientPictureCache();
  }

  // For the remote mode, the RemoteChannelImpl implements the Proxy, which is
  // owned by the LayerTreeHost. The RemoteChannelImpl pipes requests which need
  // to handled locally, for instance the Output Surface creation to the
  // LayerTreeHost on the client, while the other requests are sent to the
  // RemoteChannelMain on the server which directs them to ProxyMain and the
  // remote server LayerTreeHost.
  InitializeProxy(RemoteChannelImpl::Create(this, remote_proto_channel,
                                            task_runner_provider_.get()),
                  nullptr);
}

void LayerTreeHost::InitializeForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider,
    std::unique_ptr<Proxy> proxy_for_testing,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  task_runner_provider_ = std::move(task_runner_provider);

  InitializePictureCacheForTesting();

  InitializeProxy(std::move(proxy_for_testing),
                  std::move(external_begin_frame_source));
}

void LayerTreeHost::InitializePictureCacheForTesting() {
  if (!image_serialization_processor_)
    return;

  // Initialize both engine and client cache to ensure serialization tests
  // with a single LayerTreeHost can work correctly.
  engine_picture_cache_ =
      image_serialization_processor_->CreateEnginePictureCache();
  client_picture_cache_ =
      image_serialization_processor_->CreateClientPictureCache();
}

void LayerTreeHost::SetTaskRunnerProviderForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider) {
  DCHECK(!task_runner_provider_);
  task_runner_provider_ = std::move(task_runner_provider);
}

void LayerTreeHost::InitializeProxy(
    std::unique_ptr<Proxy> proxy,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  TRACE_EVENT0("cc", "LayerTreeHost::InitializeForReal");
  DCHECK(task_runner_provider_);

  proxy_ = std::move(proxy);
  proxy_->Start(std::move(external_begin_frame_source));

  animation_host_->SetSupportsScrollAnimations(proxy_->SupportsImplScrolling());
}

LayerTreeHost::~LayerTreeHost() {
  TRACE_EVENT0("cc", "LayerTreeHost::~LayerTreeHost");

  animation_host_->SetMutatorHostClient(nullptr);

  if (root_layer_.get())
    root_layer_->SetLayerTreeHost(NULL);

  DCHECK(swap_promise_monitor_.empty());

  BreakSwapPromises(SwapPromise::COMMIT_FAILS);

  if (proxy_) {
    DCHECK(task_runner_provider_->IsMainThread());
    proxy_->Stop();

    // Proxy must be destroyed before the Task Runner Provider.
    proxy_ = nullptr;
  }

  // We must clear any pointers into the layer tree prior to destroying it.
  RegisterViewportLayers(NULL, NULL, NULL, NULL);

  if (root_layer_.get()) {
    // The layer tree must be destroyed before the layer tree host. We've
    // made a contract with our animation controllers that the animation_host
    // will outlive them, and we must make good.
    root_layer_ = NULL;
  }
}

void LayerTreeHost::WillBeginMainFrame() {
  devtools_instrumentation::WillBeginMainThreadFrame(id(),
                                                     source_frame_number());
  client_->WillBeginMainFrame();
}

void LayerTreeHost::DidBeginMainFrame() {
  client_->DidBeginMainFrame();
}

void LayerTreeHost::BeginMainFrameNotExpectedSoon() {
  client_->BeginMainFrameNotExpectedSoon();
}

void LayerTreeHost::BeginMainFrame(const BeginFrameArgs& args) {
  client_->BeginMainFrame(args);
}

void LayerTreeHost::DidStopFlinging() {
  proxy_->MainThreadHasStoppedFlinging();
}

void LayerTreeHost::RequestMainFrameUpdate() {
  client_->UpdateLayerTreeHost();
}

// This function commits the LayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a Layer,
// should be delayed until the LayerTreeHost::CommitComplete, which will run
// after the commit, but on the main thread.
void LayerTreeHost::FinishCommitOnImplThread(LayerTreeHostImpl* host_impl) {
  DCHECK(!IsRemoteServer());
  DCHECK(task_runner_provider_->IsImplThread());

  bool is_new_trace;
  TRACE_EVENT_IS_NEW_TRACE(&is_new_trace);
  if (is_new_trace &&
      frame_viewer_instrumentation::IsTracingLayerTreeSnapshots() &&
      root_layer()) {
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        this, [](Layer* layer) { layer->DidBeginTracing(); });
  }

  LayerTreeImpl* sync_tree = host_impl->sync_tree();

  if (next_commit_forces_redraw_) {
    sync_tree->ForceRedrawNextActivation();
    next_commit_forces_redraw_ = false;
  }

  sync_tree->set_source_frame_number(source_frame_number());

  if (needs_full_tree_sync_)
    TreeSynchronizer::SynchronizeTrees(root_layer(), sync_tree);

  sync_tree->set_needs_full_tree_sync(needs_full_tree_sync_);
  needs_full_tree_sync_ = false;

  if (hud_layer_.get()) {
    LayerImpl* hud_impl = sync_tree->LayerById(hud_layer_->id());
    sync_tree->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    sync_tree->set_hud_layer(NULL);
  }

  sync_tree->set_background_color(background_color_);
  sync_tree->set_has_transparent_background(has_transparent_background_);
  sync_tree->set_have_scroll_event_handlers(have_scroll_event_handlers_);
  sync_tree->set_event_listener_properties(
      EventListenerClass::kTouchStartOrMove,
      event_listener_properties(EventListenerClass::kTouchStartOrMove));
  sync_tree->set_event_listener_properties(
      EventListenerClass::kMouseWheel,
      event_listener_properties(EventListenerClass::kMouseWheel));
  sync_tree->set_event_listener_properties(
      EventListenerClass::kTouchEndOrCancel,
      event_listener_properties(EventListenerClass::kTouchEndOrCancel));

  if (page_scale_layer_.get() && inner_viewport_scroll_layer_.get()) {
    sync_tree->SetViewportLayersFromIds(
        overscroll_elasticity_layer_.get() ? overscroll_elasticity_layer_->id()
                                           : Layer::INVALID_ID,
        page_scale_layer_->id(), inner_viewport_scroll_layer_->id(),
        outer_viewport_scroll_layer_.get() ? outer_viewport_scroll_layer_->id()
                                           : Layer::INVALID_ID);
    DCHECK(inner_viewport_scroll_layer_->IsContainerForFixedPositionLayers());
  } else {
    sync_tree->ClearViewportLayers();
  }

  sync_tree->RegisterSelection(selection_);

  bool property_trees_changed_on_active_tree =
      sync_tree->IsActiveTree() && sync_tree->property_trees()->changed;
  // Property trees may store damage status. We preserve the sync tree damage
  // status by pushing the damage status from sync tree property trees to main
  // thread property trees or by moving it onto the layers.
  if (root_layer_ && property_trees_changed_on_active_tree) {
    if (property_trees_.sequence_number ==
        sync_tree->property_trees()->sequence_number)
      sync_tree->property_trees()->PushChangeTrackingTo(&property_trees_);
    else
      sync_tree->MoveChangeTrackingToLayers();
  }
  // Setting property trees must happen before pushing the page scale.
  sync_tree->SetPropertyTrees(&property_trees_);

  sync_tree->PushPageScaleFromMainThread(
      page_scale_factor_, min_page_scale_factor_, max_page_scale_factor_);
  sync_tree->elastic_overscroll()->PushFromMainThread(elastic_overscroll_);
  if (sync_tree->IsActiveTree())
    sync_tree->elastic_overscroll()->PushPendingToActive();

  sync_tree->PassSwapPromises(&swap_promise_list_);

  sync_tree->set_top_controls_shrink_blink_size(
      top_controls_shrink_blink_size_);
  sync_tree->set_top_controls_height(top_controls_height_);
  sync_tree->PushTopControlsFromMainThread(top_controls_shown_ratio_);

  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentIsSuitableForGpuRasterization(
      content_is_suitable_for_gpu_rasterization_);
  RecordGpuRasterizationHistogram();

  host_impl->SetViewportSize(device_viewport_size_);
  // TODO(senorblanco): Move this up so that it happens before GPU rasterization
  // properties are set, since those trigger an update of GPU rasterization
  // status, which depends on the device scale factor. (crbug.com/535700)
  sync_tree->SetDeviceScaleFactor(device_scale_factor_);
  sync_tree->set_painted_device_scale_factor(painted_device_scale_factor_);
  host_impl->SetDebugState(debug_state_);
  if (pending_page_scale_animation_) {
    sync_tree->SetPendingPageScaleAnimation(
        std::move(pending_page_scale_animation_));
  }

  if (!ui_resource_request_queue_.empty()) {
    sync_tree->set_ui_resource_request_queue(ui_resource_request_queue_);
    ui_resource_request_queue_.clear();
  }

  DCHECK(!sync_tree->ViewportSizeInvalid());

  sync_tree->set_has_ever_been_drawn(false);

  {
    TRACE_EVENT0("cc", "LayerTreeHost::PushProperties");

    TreeSynchronizer::PushLayerProperties(this, sync_tree);

    // This must happen after synchronizing property trees and after push
    // properties, which updates property tree indices, but before animation
    // host pushes properties as animation host push properties can change
    // Animation::InEffect and we want the old InEffect value for updating
    // property tree scrolling and animation.
    sync_tree->UpdatePropertyTreeScrollingAndAnimationFromMainThread();

    TRACE_EVENT0("cc", "LayerTreeHost::AnimationHost::PushProperties");
    DCHECK(host_impl->animation_host());
    animation_host_->PushPropertiesTo(host_impl->animation_host());
  }

  // This must happen after synchronizing property trees and after pushing
  // properties, which updates the clobber_active_value flag.
  sync_tree->UpdatePropertyTreeScrollOffset(&property_trees_);

  micro_benchmark_controller_.ScheduleImplBenchmarks(host_impl);
  property_trees_.ResetAllChangeTracking();
}

void LayerTreeHost::WillCommit() {
  OnCommitForSwapPromises();
  client_->WillCommit();
}

void LayerTreeHost::UpdateHudLayer() {
  if (debug_state_.ShowHudInfo()) {
    if (!hud_layer_.get()) {
      hud_layer_ = HeadsUpDisplayLayer::Create();
    }

    if (root_layer_.get() && !hud_layer_->parent())
      root_layer_->AddChild(hud_layer_);
  } else if (hud_layer_.get()) {
    hud_layer_->RemoveFromParent();
    hud_layer_ = NULL;
  }
}

void LayerTreeHost::CommitComplete() {
  source_frame_number_++;
  client_->DidCommit();
  if (did_complete_scale_animation_) {
    client_->DidCompletePageScaleAnimation();
    did_complete_scale_animation_ = false;
  }
}

void LayerTreeHost::SetOutputSurface(std::unique_ptr<OutputSurface> surface) {
  TRACE_EVENT0("cc", "LayerTreeHost::SetOutputSurface");
  DCHECK(output_surface_lost_);
  DCHECK(surface);

  DCHECK(!new_output_surface_);
  new_output_surface_ = std::move(surface);
  proxy_->SetOutputSurface(new_output_surface_.get());
}

std::unique_ptr<OutputSurface> LayerTreeHost::ReleaseOutputSurface() {
  DCHECK(!visible_);
  DCHECK(!output_surface_lost_);

  DidLoseOutputSurface();
  proxy_->ReleaseOutputSurface();
  return std::move(current_output_surface_);
}

void LayerTreeHost::RequestNewOutputSurface() {
  client_->RequestNewOutputSurface();
}

void LayerTreeHost::DidInitializeOutputSurface() {
  DCHECK(new_output_surface_);
  output_surface_lost_ = false;
  current_output_surface_ = std::move(new_output_surface_);
  client_->DidInitializeOutputSurface();
}

void LayerTreeHost::DidFailToInitializeOutputSurface() {
  DCHECK(output_surface_lost_);
  DCHECK(new_output_surface_);
  // Note: It is safe to drop all output surface references here as
  // LayerTreeHostImpl will not keep a pointer to either the old or
  // new output surface after failing to initialize the new one.
  current_output_surface_ = nullptr;
  new_output_surface_ = nullptr;
  client_->DidFailToInitializeOutputSurface();
}

std::unique_ptr<LayerTreeHostImpl> LayerTreeHost::CreateLayerTreeHostImpl(
    LayerTreeHostImplClient* client) {
  DCHECK(!IsRemoteServer());
  DCHECK(task_runner_provider_->IsImplThread());

  const bool supports_impl_scrolling = task_runner_provider_->HasImplThread();
  std::unique_ptr<AnimationHost> animation_host_impl =
      animation_host_->CreateImplInstance(supports_impl_scrolling);

  std::unique_ptr<LayerTreeHostImpl> host_impl = LayerTreeHostImpl::Create(
      settings_, client, task_runner_provider_.get(),
      rendering_stats_instrumentation_.get(), shared_bitmap_manager_,
      gpu_memory_buffer_manager_, task_graph_runner_,
      std::move(animation_host_impl), id_);
  host_impl->SetHasGpuRasterizationTrigger(has_gpu_rasterization_trigger_);
  host_impl->SetContentIsSuitableForGpuRasterization(
      content_is_suitable_for_gpu_rasterization_);
  shared_bitmap_manager_ = NULL;
  gpu_memory_buffer_manager_ = NULL;
  task_graph_runner_ = NULL;
  input_handler_weak_ptr_ = host_impl->AsWeakPtr();
  return host_impl;
}

void LayerTreeHost::DidLoseOutputSurface() {
  TRACE_EVENT0("cc", "LayerTreeHost::DidLoseOutputSurface");
  DCHECK(task_runner_provider_->IsMainThread());

  if (output_surface_lost_)
    return;

  output_surface_lost_ = true;
  SetNeedsCommit();
}

void LayerTreeHost::FinishAllRendering() {
  proxy_->FinishAllRendering();
}

void LayerTreeHost::SetDeferCommits(bool defer_commits) {
  proxy_->SetDeferCommits(defer_commits);
}

void LayerTreeHost::SetNeedsDisplayOnAllLayers() {
  for (auto* layer : *this)
    layer->SetNeedsDisplay();
}

const RendererCapabilities& LayerTreeHost::GetRendererCapabilities() const {
  return proxy_->GetRendererCapabilities();
}

void LayerTreeHost::SetNeedsAnimate() {
  proxy_->SetNeedsAnimate();
  NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetNeedsUpdateLayers() {
  proxy_->SetNeedsUpdateLayers();
  NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetPropertyTreesNeedRebuild() {
  property_trees_.needs_rebuild = true;
  SetNeedsUpdateLayers();
}

void LayerTreeHost::SetNeedsCommit() {
  proxy_->SetNeedsCommit();
  NotifySwapPromiseMonitorsOfSetNeedsCommit();
}

void LayerTreeHost::SetNeedsFullTreeSync() {
  needs_full_tree_sync_ = true;
  needs_meta_info_recomputation_ = true;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTreeHost::SetNeedsMetaInfoRecomputation(bool needs_recomputation) {
  needs_meta_info_recomputation_ = needs_recomputation;
}

void LayerTreeHost::SetNeedsRedraw() {
  SetNeedsRedrawRect(gfx::Rect(device_viewport_size_));
}

void LayerTreeHost::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  proxy_->SetNeedsRedraw(damage_rect);
}

bool LayerTreeHost::CommitRequested() const {
  return proxy_->CommitRequested();
}

bool LayerTreeHost::BeginMainFrameRequested() const {
  return proxy_->BeginMainFrameRequested();
}

void LayerTreeHost::SetNextCommitWaitsForActivation() {
  proxy_->SetNextCommitWaitsForActivation();
}

void LayerTreeHost::SetNextCommitForcesRedraw() {
  next_commit_forces_redraw_ = true;
  proxy_->SetNeedsUpdateLayers();
}

void LayerTreeHost::SetAnimationEvents(
    std::unique_ptr<AnimationEvents> events) {
  DCHECK(task_runner_provider_->IsMainThread());
  animation_host_->SetAnimationEvents(std::move(events));
}

void LayerTreeHost::SetRootLayer(scoped_refptr<Layer> root_layer) {
  if (root_layer_.get() == root_layer.get())
    return;

  if (root_layer_.get())
    root_layer_->SetLayerTreeHost(NULL);
  root_layer_ = root_layer;
  if (root_layer_.get()) {
    DCHECK(!root_layer_->parent());
    root_layer_->SetLayerTreeHost(this);
  }

  if (hud_layer_.get())
    hud_layer_->RemoveFromParent();

  // Reset gpu rasterization flag.
  // This flag is sticky until a new tree comes along.
  content_is_suitable_for_gpu_rasterization_ = true;
  gpu_rasterization_histogram_recorded_ = false;

  SetNeedsFullTreeSync();
}

void LayerTreeHost::SetDebugState(const LayerTreeDebugState& debug_state) {
  LayerTreeDebugState new_debug_state =
      LayerTreeDebugState::Unite(settings_.initial_debug_state, debug_state);

  if (LayerTreeDebugState::Equal(debug_state_, new_debug_state))
    return;

  debug_state_ = new_debug_state;

  rendering_stats_instrumentation_->set_record_rendering_stats(
      debug_state_.RecordRenderingStats());

  SetNeedsCommit();
}

void LayerTreeHost::SetHasGpuRasterizationTrigger(bool has_trigger) {
  if (has_trigger == has_gpu_rasterization_trigger_)
    return;

  has_gpu_rasterization_trigger_ = has_trigger;
  TRACE_EVENT_INSTANT1("cc",
                       "LayerTreeHost::SetHasGpuRasterizationTrigger",
                       TRACE_EVENT_SCOPE_THREAD,
                       "has_trigger",
                       has_gpu_rasterization_trigger_);
}

void LayerTreeHost::SetViewportSize(const gfx::Size& device_viewport_size) {
  if (device_viewport_size == device_viewport_size_)
    return;

  device_viewport_size_ = device_viewport_size;

  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTreeHost::SetTopControlsHeight(float height, bool shrink) {
  if (top_controls_height_ == height &&
      top_controls_shrink_blink_size_ == shrink)
    return;

  top_controls_height_ = height;
  top_controls_shrink_blink_size_ = shrink;
  SetNeedsCommit();
}

void LayerTreeHost::SetTopControlsShownRatio(float ratio) {
  if (top_controls_shown_ratio_ == ratio)
    return;

  top_controls_shown_ratio_ = ratio;
  SetNeedsCommit();
}

void LayerTreeHost::ApplyPageScaleDeltaFromImplSide(float page_scale_delta) {
  DCHECK(CommitRequested());
  if (page_scale_delta == 1.f)
    return;
  page_scale_factor_ *= page_scale_delta;
  SetPropertyTreesNeedRebuild();
}

void LayerTreeHost::SetPageScaleFactorAndLimits(float page_scale_factor,
                                                float min_page_scale_factor,
                                                float max_page_scale_factor) {
  if (page_scale_factor == page_scale_factor_ &&
      min_page_scale_factor == min_page_scale_factor_ &&
      max_page_scale_factor == max_page_scale_factor_)
    return;

  page_scale_factor_ = page_scale_factor;
  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTreeHost::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  proxy_->SetVisible(visible);
}

void LayerTreeHost::StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                                            bool use_anchor,
                                            float scale,
                                            base::TimeDelta duration) {
  pending_page_scale_animation_.reset(
      new PendingPageScaleAnimation(
          target_offset,
          use_anchor,
          scale,
          duration));

  SetNeedsCommit();
}

bool LayerTreeHost::HasPendingPageScaleAnimation() const {
  return !!pending_page_scale_animation_.get();
}

void LayerTreeHost::NotifyInputThrottledUntilCommit() {
  proxy_->NotifyInputThrottledUntilCommit();
}

void LayerTreeHost::LayoutAndUpdateLayers() {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  SingleThreadProxy* proxy = static_cast<SingleThreadProxy*>(proxy_.get());

  if (output_surface_lost()) {
    proxy->RequestNewOutputSurface();
    // RequestNewOutputSurface could have synchronously created an output
    // surface, so check again before returning.
    if (output_surface_lost())
      return;
  }

  RequestMainFrameUpdate();
  UpdateLayers();
}

void LayerTreeHost::Composite(base::TimeTicks frame_begin_time) {
  DCHECK(IsSingleThreaded());
  // This function is only valid when not using the scheduler.
  DCHECK(!settings_.single_thread_proxy_scheduler);
  SingleThreadProxy* proxy = static_cast<SingleThreadProxy*>(proxy_.get());

  proxy->CompositeImmediately(frame_begin_time);
}

bool LayerTreeHost::UpdateLayers() {
  DCHECK(!output_surface_lost_);
  if (!root_layer())
    return false;
  DCHECK(!root_layer()->parent());
  bool result = DoUpdateLayers(root_layer());
  micro_benchmark_controller_.DidUpdateLayers();
  return result || next_commit_forces_redraw_;
}

LayerListIterator<Layer> LayerTreeHost::begin() const {
  return LayerListIterator<Layer>(root_layer_.get());
}

LayerListIterator<Layer> LayerTreeHost::end() const {
  return LayerListIterator<Layer>(nullptr);
}

LayerListReverseIterator<Layer> LayerTreeHost::rbegin() {
  return LayerListReverseIterator<Layer>(root_layer_.get());
}

LayerListReverseIterator<Layer> LayerTreeHost::rend() {
  return LayerListReverseIterator<Layer>(nullptr);
}

void LayerTreeHost::DidCompletePageScaleAnimation() {
  did_complete_scale_animation_ = true;
}

void LayerTreeHost::RecordGpuRasterizationHistogram() {
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

void LayerTreeHost::BuildPropertyTreesForTesting() {
  PropertyTreeBuilder::PreCalculateMetaInformation(root_layer_.get());
  gfx::Transform identity_transform;
  PropertyTreeBuilder::BuildPropertyTrees(
      root_layer_.get(), page_scale_layer_.get(),
      inner_viewport_scroll_layer_.get(), outer_viewport_scroll_layer_.get(),
      overscroll_elasticity_layer_.get(), elastic_overscroll_,
      page_scale_factor_, device_scale_factor_,
      gfx::Rect(device_viewport_size_), identity_transform, &property_trees_);
}

static void SetElementIdForTesting(Layer* layer) {
  layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
}

void LayerTreeHost::SetElementIdsForTesting() {
  LayerTreeHostCommon::CallFunctionForEveryLayer(this, SetElementIdForTesting);
}

bool LayerTreeHost::UsingSharedMemoryResources() {
  return GetRendererCapabilities().using_shared_memory_resources;
}

bool LayerTreeHost::DoUpdateLayers(Layer* root_layer) {
  TRACE_EVENT1("cc", "LayerTreeHost::DoUpdateLayers", "source_frame_number",
               source_frame_number());

  UpdateHudLayer();

  Layer* root_scroll =
      PropertyTreeBuilder::FindFirstScrollableLayer(root_layer);
  Layer* page_scale_layer = page_scale_layer_.get();
  if (!page_scale_layer && root_scroll)
    page_scale_layer = root_scroll->parent();

  if (hud_layer_.get()) {
    hud_layer_->PrepareForCalculateDrawProperties(device_viewport_size(),
                                                  device_scale_factor_);
  }

  gfx::Transform identity_transform;
  LayerList update_layer_list;

  {
    TRACE_EVENT0("cc", "LayerTreeHost::UpdateLayers::BuildPropertyTrees");
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.cdp-perf"),
                 "LayerTreeHostCommon::ComputeVisibleRectsWithPropertyTrees");
    PropertyTreeBuilder::PreCalculateMetaInformation(root_layer);
    bool can_render_to_separate_surface = true;
    if (!settings_.use_layer_lists) {
      // If use_layer_lists is set, then the property trees should have been
      // built by the client already.
      PropertyTreeBuilder::BuildPropertyTrees(
          root_layer, page_scale_layer, inner_viewport_scroll_layer_.get(),
          outer_viewport_scroll_layer_.get(),
          overscroll_elasticity_layer_.get(), elastic_overscroll_,
          page_scale_factor_, device_scale_factor_,
          gfx::Rect(device_viewport_size_), identity_transform,
          &property_trees_);
      TRACE_EVENT_INSTANT1("cc",
                           "LayerTreeHost::UpdateLayers_BuiltPropertyTrees",
                           TRACE_EVENT_SCOPE_THREAD, "property_trees",
                           property_trees_.AsTracedValue());
    } else {
      TRACE_EVENT_INSTANT1("cc",
                           "LayerTreeHost::UpdateLayers_ReceivedPropertyTrees",
                           TRACE_EVENT_SCOPE_THREAD, "property_trees",
                           property_trees_.AsTracedValue());
    }
    draw_property_utils::UpdatePropertyTrees(&property_trees_,
                                             can_render_to_separate_surface);
    draw_property_utils::FindLayersThatNeedUpdates(
        this, property_trees_.transform_tree, property_trees_.effect_tree,
        &update_layer_list);
  }

  for (const auto& layer : update_layer_list)
    layer->SavePaintProperties();

  base::AutoReset<bool> painting(&in_paint_layer_contents_, true);
  bool did_paint_content = false;
  for (const auto& layer : update_layer_list) {
    did_paint_content |= layer->Update();
    content_is_suitable_for_gpu_rasterization_ &=
        layer->IsSuitableForGpuRasterization();
  }
  return did_paint_content;
}

void LayerTreeHost::ApplyScrollAndScale(ScrollAndScaleSet* info) {
  for (auto& swap_promise : info->swap_promises) {
    TRACE_EVENT_WITH_FLOW1("input,benchmark",
                           "LatencyInfo.Flow",
                           TRACE_ID_DONT_MANGLE(swap_promise->TraceId()),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", "Main thread scroll update");
    QueueSwapPromise(std::move(swap_promise));
  }

  gfx::Vector2dF inner_viewport_scroll_delta;
  gfx::Vector2dF outer_viewport_scroll_delta;

  if (root_layer_.get()) {
    for (size_t i = 0; i < info->scrolls.size(); ++i) {
      Layer* layer = LayerById(info->scrolls[i].layer_id);
      if (!layer)
        continue;
      if (layer == outer_viewport_scroll_layer_.get()) {
        outer_viewport_scroll_delta += info->scrolls[i].scroll_delta;
      } else if (layer == inner_viewport_scroll_layer_.get()) {
        inner_viewport_scroll_delta += info->scrolls[i].scroll_delta;
      } else {
        layer->SetScrollOffsetFromImplSide(
            gfx::ScrollOffsetWithDelta(layer->scroll_offset(),
                                       info->scrolls[i].scroll_delta));
      }
      SetNeedsUpdateLayers();
    }
  }

  if (!inner_viewport_scroll_delta.IsZero() ||
      !outer_viewport_scroll_delta.IsZero() || info->page_scale_delta != 1.f ||
      !info->elastic_overscroll_delta.IsZero() || info->top_controls_delta) {
    // Preemptively apply the scroll offset and scale delta here before sending
    // it to the client.  If the client comes back and sets it to the same
    // value, then the layer can early out without needing a full commit.
    if (inner_viewport_scroll_layer_.get()) {
      inner_viewport_scroll_layer_->SetScrollOffsetFromImplSide(
          gfx::ScrollOffsetWithDelta(
              inner_viewport_scroll_layer_->scroll_offset(),
              inner_viewport_scroll_delta));
    }

    if (outer_viewport_scroll_layer_.get()) {
      outer_viewport_scroll_layer_->SetScrollOffsetFromImplSide(
          gfx::ScrollOffsetWithDelta(
              outer_viewport_scroll_layer_->scroll_offset(),
              outer_viewport_scroll_delta));
    }

    ApplyPageScaleDeltaFromImplSide(info->page_scale_delta);
    elastic_overscroll_ += info->elastic_overscroll_delta;
    // TODO(ccameron): pass the elastic overscroll here so that input events
    // may be translated appropriately.
    client_->ApplyViewportDeltas(
        inner_viewport_scroll_delta, outer_viewport_scroll_delta,
        info->elastic_overscroll_delta, info->page_scale_delta,
        info->top_controls_delta);
    SetNeedsUpdateLayers();
  }
}

void LayerTreeHost::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor == device_scale_factor_)
    return;
  device_scale_factor_ = device_scale_factor;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTreeHost::SetPaintedDeviceScaleFactor(
    float painted_device_scale_factor) {
  if (painted_device_scale_factor == painted_device_scale_factor_)
    return;
  painted_device_scale_factor_ = painted_device_scale_factor;

  SetNeedsCommit();
}

void LayerTreeHost::UpdateTopControlsState(TopControlsState constraints,
                                           TopControlsState current,
                                           bool animate) {
  // Top controls are only used in threaded or remote mode.
  DCHECK(IsThreaded() || IsRemoteServer());
  proxy_->UpdateTopControlsState(constraints, current, animate);
}

void LayerTreeHost::AnimateLayers(base::TimeTicks monotonic_time) {
  std::unique_ptr<AnimationEvents> events = animation_host_->CreateEvents();

  if (animation_host_->AnimateLayers(monotonic_time))
    animation_host_->UpdateAnimationState(true, events.get());

  if (!events->events_.empty())
    property_trees_.needs_rebuild = true;
}

UIResourceId LayerTreeHost::CreateUIResource(UIResourceClient* client) {
  DCHECK(client);

  UIResourceId next_id = next_ui_resource_id_++;
  DCHECK(ui_resource_client_map_.find(next_id) ==
         ui_resource_client_map_.end());

  bool resource_lost = false;
  UIResourceRequest request(UIResourceRequest::UI_RESOURCE_CREATE, next_id,
                            client->GetBitmap(next_id, resource_lost));
  ui_resource_request_queue_.push_back(request);

  UIResourceClientData data;
  data.client = client;
  data.size = request.GetBitmap().GetSize();

  ui_resource_client_map_[request.GetId()] = data;
  return request.GetId();
}

// Deletes a UI resource.  May safely be called more than once.
void LayerTreeHost::DeleteUIResource(UIResourceId uid) {
  UIResourceClientMap::iterator iter = ui_resource_client_map_.find(uid);
  if (iter == ui_resource_client_map_.end())
    return;

  UIResourceRequest request(UIResourceRequest::UI_RESOURCE_DELETE, uid);
  ui_resource_request_queue_.push_back(request);
  ui_resource_client_map_.erase(iter);
}

void LayerTreeHost::RecreateUIResources() {
  for (UIResourceClientMap::iterator iter = ui_resource_client_map_.begin();
       iter != ui_resource_client_map_.end();
       ++iter) {
    UIResourceId uid = iter->first;
    const UIResourceClientData& data = iter->second;
    bool resource_lost = true;
    UIResourceRequest request(UIResourceRequest::UI_RESOURCE_CREATE, uid,
                              data.client->GetBitmap(uid, resource_lost));
    ui_resource_request_queue_.push_back(request);
  }
}

// Returns the size of a resource given its id.
gfx::Size LayerTreeHost::GetUIResourceSize(UIResourceId uid) const {
  UIResourceClientMap::const_iterator iter = ui_resource_client_map_.find(uid);
  if (iter == ui_resource_client_map_.end())
    return gfx::Size();

  const UIResourceClientData& data = iter->second;
  return data.size;
}

void LayerTreeHost::RegisterViewportLayers(
    scoped_refptr<Layer> overscroll_elasticity_layer,
    scoped_refptr<Layer> page_scale_layer,
    scoped_refptr<Layer> inner_viewport_scroll_layer,
    scoped_refptr<Layer> outer_viewport_scroll_layer) {
  DCHECK(!inner_viewport_scroll_layer ||
         inner_viewport_scroll_layer != outer_viewport_scroll_layer);
  overscroll_elasticity_layer_ = overscroll_elasticity_layer;
  page_scale_layer_ = page_scale_layer;
  inner_viewport_scroll_layer_ = inner_viewport_scroll_layer;
  outer_viewport_scroll_layer_ = outer_viewport_scroll_layer;
}

void LayerTreeHost::RegisterSelection(const LayerSelection& selection) {
  if (selection_ == selection)
    return;

  selection_ = selection;
  SetNeedsCommit();
}

void LayerTreeHost::SetHaveScrollEventHandlers(bool have_event_handlers) {
  if (have_scroll_event_handlers_ == have_event_handlers)
    return;

  have_scroll_event_handlers_ = have_event_handlers;
  SetNeedsCommit();
}

void LayerTreeHost::SetEventListenerProperties(
    EventListenerClass event_class,
    EventListenerProperties properties) {
  const size_t index = static_cast<size_t>(event_class);
  if (event_listener_properties_[index] == properties)
    return;

  event_listener_properties_[index] = properties;
  SetNeedsCommit();
}

int LayerTreeHost::ScheduleMicroBenchmark(
    const std::string& benchmark_name,
    std::unique_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback) {
  return micro_benchmark_controller_.ScheduleRun(benchmark_name,
                                                 std::move(value), callback);
}

bool LayerTreeHost::SendMessageToMicroBenchmark(
    int id,
    std::unique_ptr<base::Value> value) {
  return micro_benchmark_controller_.SendMessage(id, std::move(value));
}

void LayerTreeHost::InsertSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.insert(monitor);
}

void LayerTreeHost::RemoveSwapPromiseMonitor(SwapPromiseMonitor* monitor) {
  swap_promise_monitor_.erase(monitor);
}

void LayerTreeHost::NotifySwapPromiseMonitorsOfSetNeedsCommit() {
  std::set<SwapPromiseMonitor*>::iterator it = swap_promise_monitor_.begin();
  for (; it != swap_promise_monitor_.end(); it++)
    (*it)->OnSetNeedsCommitOnMain();
}

void LayerTreeHost::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  DCHECK(swap_promise);
  swap_promise_list_.push_back(std::move(swap_promise));
}

void LayerTreeHost::BreakSwapPromises(SwapPromise::DidNotSwapReason reason) {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->DidNotSwap(reason);
  swap_promise_list_.clear();
}

void LayerTreeHost::OnCommitForSwapPromises() {
  for (const auto& swap_promise : swap_promise_list_)
    swap_promise->OnCommit();
}

void LayerTreeHost::set_surface_id_namespace(uint32_t id_namespace) {
  surface_id_namespace_ = id_namespace;
}

SurfaceSequence LayerTreeHost::CreateSurfaceSequence() {
  return SurfaceSequence(surface_id_namespace_, next_surface_sequence_++);
}

void LayerTreeHost::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  proxy_->SetMutator(std::move(mutator));
}

Layer* LayerTreeHost::LayerById(int id) const {
  LayerIdMap::const_iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

Layer* LayerTreeHost::LayerByElementId(ElementId element_id) const {
  ElementLayersMap::const_iterator iter = element_layers_map_.find(element_id);
  return iter != element_layers_map_.end() ? iter->second : nullptr;
}

void LayerTreeHost::AddToElementMap(Layer* layer) {
  if (!layer->element_id())
    return;

  element_layers_map_[layer->element_id()] = layer;
}

void LayerTreeHost::RemoveFromElementMap(Layer* layer) {
  if (!layer->element_id())
    return;

  element_layers_map_.erase(layer->element_id());
}

void LayerTreeHost::AddLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.insert(layer);
}

void LayerTreeHost::RemoveLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.erase(layer);
}

std::unordered_set<Layer*>& LayerTreeHost::LayersThatShouldPushProperties() {
  return layers_that_should_push_properties_;
}

bool LayerTreeHost::LayerNeedsPushPropertiesForTesting(Layer* layer) {
  return layers_that_should_push_properties_.find(layer) !=
         layers_that_should_push_properties_.end();
}

void LayerTreeHost::RegisterLayer(Layer* layer) {
  DCHECK(!LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  layer_id_map_[layer->id()] = layer;
  if (layer->element_id()) {
    animation_host_->RegisterElement(layer->element_id(),
                                     ElementListType::ACTIVE);
  }
}

void LayerTreeHost::UnregisterLayer(Layer* layer) {
  DCHECK(LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  if (layer->element_id()) {
    animation_host_->UnregisterElement(layer->element_id(),
                                       ElementListType::ACTIVE);
  }
  RemoveLayerShouldPushProperties(layer);
  layer_id_map_.erase(layer->id());
}

bool LayerTreeHost::IsElementInList(ElementId element_id,
                                    ElementListType list_type) const {
  return list_type == ElementListType::ACTIVE && LayerByElementId(element_id);
}

void LayerTreeHost::SetMutatorsNeedCommit() {
  SetNeedsCommit();
}

void LayerTreeHost::SetMutatorsNeedRebuildPropertyTrees() {
  property_trees_.needs_rebuild = true;
}

void LayerTreeHost::SetElementFilterMutated(ElementId element_id,
                                            ElementListType list_type,
                                            const FilterOperations& filters) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnFilterAnimated(filters);
}

void LayerTreeHost::SetElementOpacityMutated(ElementId element_id,
                                             ElementListType list_type,
                                             float opacity) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnOpacityAnimated(opacity);
}

void LayerTreeHost::SetElementTransformMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::Transform& transform) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnTransformAnimated(transform);
}

void LayerTreeHost::SetElementScrollOffsetMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnScrollOffsetAnimated(scroll_offset);
}

void LayerTreeHost::ElementTransformIsAnimatingChanged(
    ElementId element_id,
    ElementListType list_type,
    AnimationChangeType change_type,
    bool is_animating) {
  Layer* layer = LayerByElementId(element_id);
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

void LayerTreeHost::ElementOpacityIsAnimatingChanged(
    ElementId element_id,
    ElementListType list_type,
    AnimationChangeType change_type,
    bool is_animating) {
  Layer* layer = LayerByElementId(element_id);
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

gfx::ScrollOffset LayerTreeHost::GetScrollOffsetForAnimation(
    ElementId element_id) const {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  return layer->ScrollOffsetForAnimation();
}

bool LayerTreeHost::ScrollOffsetAnimationWasInterrupted(
    const Layer* layer) const {
  return animation_host_->ScrollOffsetAnimationWasInterrupted(
      layer->element_id());
}

bool LayerTreeHost::IsAnimatingFilterProperty(const Layer* layer) const {
  return animation_host_->IsAnimatingFilterProperty(layer->element_id(),
                                                    ElementListType::ACTIVE);
}

bool LayerTreeHost::IsAnimatingOpacityProperty(const Layer* layer) const {
  return animation_host_->IsAnimatingOpacityProperty(layer->element_id(),
                                                     ElementListType::ACTIVE);
}

bool LayerTreeHost::IsAnimatingTransformProperty(const Layer* layer) const {
  return animation_host_->IsAnimatingTransformProperty(layer->element_id(),
                                                       ElementListType::ACTIVE);
}

bool LayerTreeHost::HasPotentiallyRunningFilterAnimation(
    const Layer* layer) const {
  return animation_host_->HasPotentiallyRunningFilterAnimation(
      layer->element_id(), ElementListType::ACTIVE);
}

bool LayerTreeHost::HasPotentiallyRunningOpacityAnimation(
    const Layer* layer) const {
  return animation_host_->HasPotentiallyRunningOpacityAnimation(
      layer->element_id(), ElementListType::ACTIVE);
}

bool LayerTreeHost::HasPotentiallyRunningTransformAnimation(
    const Layer* layer) const {
  return animation_host_->HasPotentiallyRunningTransformAnimation(
      layer->element_id(), ElementListType::ACTIVE);
}

bool LayerTreeHost::HasOnlyTranslationTransforms(const Layer* layer) const {
  return animation_host_->HasOnlyTranslationTransforms(layer->element_id(),
                                                       ElementListType::ACTIVE);
}

bool LayerTreeHost::MaximumTargetScale(const Layer* layer,
                                       float* max_scale) const {
  return animation_host_->MaximumTargetScale(
      layer->element_id(), ElementListType::ACTIVE, max_scale);
}

bool LayerTreeHost::AnimationStartScale(const Layer* layer,
                                        float* start_scale) const {
  return animation_host_->AnimationStartScale(
      layer->element_id(), ElementListType::ACTIVE, start_scale);
}

bool LayerTreeHost::HasAnyAnimationTargetingProperty(
    const Layer* layer,
    TargetProperty::Type property) const {
  return animation_host_->HasAnyAnimationTargetingProperty(layer->element_id(),
                                                           property);
}

bool LayerTreeHost::AnimationsPreserveAxisAlignment(const Layer* layer) const {
  return animation_host_->AnimationsPreserveAxisAlignment(layer->element_id());
}

bool LayerTreeHost::HasAnyAnimation(const Layer* layer) const {
  return animation_host_->HasAnyAnimation(layer->element_id());
}

bool LayerTreeHost::HasActiveAnimationForTesting(const Layer* layer) const {
  return animation_host_->HasActiveAnimationForTesting(layer->element_id());
}

bool LayerTreeHost::IsSingleThreaded() const {
  DCHECK(compositor_mode_ != CompositorMode::SINGLE_THREADED ||
         !task_runner_provider_->HasImplThread());
  return compositor_mode_ == CompositorMode::SINGLE_THREADED;
}

bool LayerTreeHost::IsThreaded() const {
  DCHECK(compositor_mode_ != CompositorMode::THREADED ||
         task_runner_provider_->HasImplThread());
  return compositor_mode_ == CompositorMode::THREADED;
}

bool LayerTreeHost::IsRemoteServer() const {
  // The LayerTreeHost on the server does not have an impl task runner.
  return compositor_mode_ == CompositorMode::REMOTE &&
         !task_runner_provider_->HasImplThread();
}

bool LayerTreeHost::IsRemoteClient() const {
  return compositor_mode_ == CompositorMode::REMOTE &&
         task_runner_provider_->HasImplThread();
}

void LayerTreeHost::ToProtobufForCommit(
    proto::LayerTreeHost* proto,
    std::vector<std::unique_ptr<SwapPromise>>* swap_promises) {
  DCHECK(engine_picture_cache_);
  // Not all fields are serialized, as they are either not needed for a commit,
  // or implementation isn't ready yet.
  // Unsupported items:
  // - animations
  // - UI resources
  // - instrumentation of stats
  // - histograms
  // Skipped items:
  // - SwapPromise as they are mostly used for perf measurements.
  // - The bitmap and GPU memory related items.
  // Other notes:
  // - The output surfaces are only valid on the client-side so they are
  //   therefore not serialized.
  // - LayerTreeSettings are needed only during construction of the
  //   LayerTreeHost, so they are serialized outside of the LayerTreeHost
  //   serialization.
  // - The |visible_| flag will be controlled from the client separately and
  //   will need special handling outside of the serialization of the
  //   LayerTreeHost.
  // TODO(nyquist): Figure out how to support animations. See crbug.com/570376.
  TRACE_EVENT0("cc.remote", "LayerTreeHost::ToProtobufForCommit");
  swap_promises->swap(swap_promise_list_);
  DCHECK(swap_promise_list_.empty());

  proto->set_needs_full_tree_sync(needs_full_tree_sync_);
  proto->set_needs_meta_info_recomputation(needs_meta_info_recomputation_);
  proto->set_source_frame_number(source_frame_number_);

  LayerProtoConverter::SerializeLayerHierarchy(root_layer_,
                                               proto->mutable_root_layer());

  // layers_that_should_push_properties_ should be serialized before layer
  // properties because it is cleared during the properties serialization.
  for (auto layer : layers_that_should_push_properties_)
    proto->add_layers_that_should_push_properties(layer->id());

  LayerProtoConverter::SerializeLayerProperties(this,
                                                proto->mutable_layer_updates());

  std::vector<PictureData> pictures =
      engine_picture_cache_->CalculateCacheUpdateAndFlush();
  proto::PictureDataVectorToSkPicturesProto(pictures,
                                            proto->mutable_pictures());

  proto->set_hud_layer_id(hud_layer_ ? hud_layer_->id() : Layer::INVALID_ID);
  debug_state_.ToProtobuf(proto->mutable_debug_state());
  SizeToProto(device_viewport_size_, proto->mutable_device_viewport_size());
  proto->set_top_controls_shrink_blink_size(top_controls_shrink_blink_size_);
  proto->set_top_controls_height(top_controls_height_);
  proto->set_top_controls_shown_ratio(top_controls_shown_ratio_);
  proto->set_device_scale_factor(device_scale_factor_);
  proto->set_painted_device_scale_factor(painted_device_scale_factor_);
  proto->set_page_scale_factor(page_scale_factor_);
  proto->set_min_page_scale_factor(min_page_scale_factor_);
  proto->set_max_page_scale_factor(max_page_scale_factor_);
  Vector2dFToProto(elastic_overscroll_, proto->mutable_elastic_overscroll());
  proto->set_has_gpu_rasterization_trigger(has_gpu_rasterization_trigger_);
  proto->set_content_is_suitable_for_gpu_rasterization(
      content_is_suitable_for_gpu_rasterization_);
  proto->set_background_color(background_color_);
  proto->set_has_transparent_background(has_transparent_background_);
  proto->set_have_scroll_event_handlers(have_scroll_event_handlers_);
  proto->set_wheel_event_listener_properties(static_cast<uint32_t>(
      event_listener_properties(EventListenerClass::kMouseWheel)));
  proto->set_touch_start_or_move_event_listener_properties(
      static_cast<uint32_t>(
          event_listener_properties(EventListenerClass::kTouchStartOrMove)));
  proto->set_touch_end_or_cancel_event_listener_properties(
      static_cast<uint32_t>(
          event_listener_properties(EventListenerClass::kTouchEndOrCancel)));
  proto->set_in_paint_layer_contents(in_paint_layer_contents_);
  proto->set_id(id_);
  proto->set_next_commit_forces_redraw(next_commit_forces_redraw_);

  // Viewport layers.
  proto->set_overscroll_elasticity_layer_id(
      overscroll_elasticity_layer_ ? overscroll_elasticity_layer_->id()
                                   : Layer::INVALID_ID);
  proto->set_page_scale_layer_id(page_scale_layer_ ? page_scale_layer_->id()
                                                   : Layer::INVALID_ID);
  proto->set_inner_viewport_scroll_layer_id(
      inner_viewport_scroll_layer_ ? inner_viewport_scroll_layer_->id()
                                   : Layer::INVALID_ID);
  proto->set_outer_viewport_scroll_layer_id(
      outer_viewport_scroll_layer_ ? outer_viewport_scroll_layer_->id()
                                   : Layer::INVALID_ID);

  LayerSelectionToProtobuf(selection_, proto->mutable_selection());

  property_trees_.ToProtobuf(proto->mutable_property_trees());

  proto->set_surface_id_namespace(surface_id_namespace_);
  proto->set_next_surface_sequence(next_surface_sequence_);

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      "cc.remote", "LayerTreeHostProto", source_frame_number_,
      ComputeLayerTreeHostProtoSizeSplitAsValue(proto));
}

void LayerTreeHost::FromProtobufForCommit(const proto::LayerTreeHost& proto) {
  DCHECK(client_picture_cache_);

  needs_full_tree_sync_ = proto.needs_full_tree_sync();
  needs_meta_info_recomputation_ = proto.needs_meta_info_recomputation();
  source_frame_number_ = proto.source_frame_number();

  // Layer hierarchy.
  scoped_refptr<Layer> new_root_layer =
      LayerProtoConverter::DeserializeLayerHierarchy(root_layer_,
                                                     proto.root_layer(), this);
  if (root_layer_ != new_root_layer) {
    root_layer_ = new_root_layer;
  }

  for (auto layer_id : proto.layers_that_should_push_properties())
    layers_that_should_push_properties_.insert(layer_id_map_[layer_id]);

  // Ensure ClientPictureCache contains all the necessary SkPictures before
  // deserializing the properties.
  proto::SkPictures proto_pictures = proto.pictures();
  std::vector<PictureData> pictures =
      SkPicturesProtoToPictureDataVector(proto_pictures);
  client_picture_cache_->ApplyCacheUpdate(pictures);

  LayerProtoConverter::DeserializeLayerProperties(root_layer_.get(),
                                                  proto.layer_updates());

  // The deserialization is finished, so now clear the cache.
  client_picture_cache_->Flush();

  debug_state_.FromProtobuf(proto.debug_state());
  device_viewport_size_ = ProtoToSize(proto.device_viewport_size());
  top_controls_shrink_blink_size_ = proto.top_controls_shrink_blink_size();
  top_controls_height_ = proto.top_controls_height();
  top_controls_shown_ratio_ = proto.top_controls_shown_ratio();
  device_scale_factor_ = proto.device_scale_factor();
  painted_device_scale_factor_ = proto.painted_device_scale_factor();
  page_scale_factor_ = proto.page_scale_factor();
  min_page_scale_factor_ = proto.min_page_scale_factor();
  max_page_scale_factor_ = proto.max_page_scale_factor();
  elastic_overscroll_ = ProtoToVector2dF(proto.elastic_overscroll());
  has_gpu_rasterization_trigger_ = proto.has_gpu_rasterization_trigger();
  content_is_suitable_for_gpu_rasterization_ =
      proto.content_is_suitable_for_gpu_rasterization();
  background_color_ = proto.background_color();
  has_transparent_background_ = proto.has_transparent_background();
  have_scroll_event_handlers_ = proto.have_scroll_event_handlers();
  event_listener_properties_[static_cast<size_t>(
      EventListenerClass::kMouseWheel)] =
      static_cast<EventListenerProperties>(
          proto.wheel_event_listener_properties());
  event_listener_properties_[static_cast<size_t>(
      EventListenerClass::kTouchStartOrMove)] =
      static_cast<EventListenerProperties>(
          proto.touch_start_or_move_event_listener_properties());
  event_listener_properties_[static_cast<size_t>(
      EventListenerClass::kTouchEndOrCancel)] =
      static_cast<EventListenerProperties>(
          proto.touch_end_or_cancel_event_listener_properties());
  in_paint_layer_contents_ = proto.in_paint_layer_contents();
  id_ = proto.id();
  next_commit_forces_redraw_ = proto.next_commit_forces_redraw();

  hud_layer_ = static_cast<HeadsUpDisplayLayer*>(
      UpdateAndGetLayer(hud_layer_.get(), proto.hud_layer_id(), layer_id_map_));
  overscroll_elasticity_layer_ =
      UpdateAndGetLayer(overscroll_elasticity_layer_.get(),
                        proto.overscroll_elasticity_layer_id(), layer_id_map_);
  page_scale_layer_ = UpdateAndGetLayer(
      page_scale_layer_.get(), proto.page_scale_layer_id(), layer_id_map_);
  inner_viewport_scroll_layer_ =
      UpdateAndGetLayer(inner_viewport_scroll_layer_.get(),
                        proto.inner_viewport_scroll_layer_id(), layer_id_map_);
  outer_viewport_scroll_layer_ =
      UpdateAndGetLayer(outer_viewport_scroll_layer_.get(),
                        proto.outer_viewport_scroll_layer_id(), layer_id_map_);

  LayerSelectionFromProtobuf(&selection_, proto.selection());

  // It is required to create new PropertyTrees before deserializing it.
  property_trees_ = PropertyTrees();
  property_trees_.FromProtobuf(proto.property_trees());

  // Forcefully override the sequence number of all layers in the tree to have
  // a valid sequence number. Changing the sequence number for a layer does not
  // need a commit, so the value will become out of date for layers that are not
  // updated for other reasons. All layers that at this point are part of the
  // layer tree are valid, so it is OK that they have a valid sequence number.
  int seq_num = property_trees_.sequence_number;
  LayerTreeHostCommon::CallFunctionForEveryLayer(this, [seq_num](Layer* layer) {
    layer->set_property_tree_sequence_number(seq_num);
  });

  surface_id_namespace_ = proto.surface_id_namespace();
  next_surface_sequence_ = proto.next_surface_sequence();
}

}  // namespace cc
