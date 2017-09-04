// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blimp/layer_tree_host_remote.h"

#include "base/atomic_sequence_num.h"
#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "cc/blimp/compositor_proto_state.h"
#include "cc/blimp/engine_picture_cache.h"
#include "cc/blimp/picture_data_conversions.h"
#include "cc/blimp/remote_compositor_bridge.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/proto/layer_tree_host.pb.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/task_runner_provider.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {
namespace {
// We use a 16ms default frame interval because the rate at which the engine
// produces main frames doesn't matter.
base::TimeDelta kDefaultFrameInterval = base::TimeDelta::FromMilliseconds(16);

static base::StaticAtomicSequenceNumber s_layer_tree_host_sequence_number;

bool ShouldUpdateLayer(Layer* layer) {
  // If the embedder has marked the layer as non-drawable.
  if (!layer->DrawsContent())
    return false;

  // If the layer bounds are empty.
  if (layer->bounds().IsEmpty())
    return false;

  // If the layer is transparent and has no background filters applied.
  // See EffectTree::UpdateIsDrawn for the logic details.
  // A few things have been ignored:
  // 1) We don't support threaded animations at the moment, so the opacity can
  //    not change on the client.
  // 2) This does not account for layer hiding its subtree, but that is never
  //    used by the renderer.
  // 3) Also updates a transparent layer's subtree when it will be skipped while
  //    drawing on the client.
  if (layer->opacity() == 0.f && layer->background_filters().IsEmpty())
    return false;

  return true;
}

}  // namespace

LayerTreeHostRemote::InitParams::InitParams() = default;

LayerTreeHostRemote::InitParams::~InitParams() = default;

LayerTreeHostRemote::LayerTreeHostRemote(InitParams* params)
    : LayerTreeHostRemote(
          params,
          base::MakeUnique<LayerTree>(params->mutator_host, this)) {}

LayerTreeHostRemote::LayerTreeHostRemote(InitParams* params,
                                         std::unique_ptr<LayerTree> layer_tree)
    : id_(s_layer_tree_host_sequence_number.GetNext() + 1),
      main_frame_requested_from_bridge_(false),
      client_(params->client),
      task_runner_provider_(
          TaskRunnerProvider::Create(std::move(params->main_task_runner),
                                     nullptr)),
      remote_compositor_bridge_(std::move(params->remote_compositor_bridge)),
      engine_picture_cache_(std::move(params->engine_picture_cache)),
      settings_(*params->settings),
      layer_tree_(std::move(layer_tree)),
      weak_factory_(this) {
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(remote_compositor_bridge_);
  DCHECK(client_);
  remote_compositor_bridge_->BindToClient(this);
  layer_tree_->set_engine_picture_cache(engine_picture_cache_.get());
}

LayerTreeHostRemote::~LayerTreeHostRemote() = default;

int LayerTreeHostRemote::GetId() const {
  return id_;
}

int LayerTreeHostRemote::SourceFrameNumber() const {
  return source_frame_number_;
}

LayerTree* LayerTreeHostRemote::GetLayerTree() {
  return layer_tree_.get();
}

const LayerTree* LayerTreeHostRemote::GetLayerTree() const {
  return layer_tree_.get();
}

UIResourceManager* LayerTreeHostRemote::GetUIResourceManager() const {
  // We shouldn't need a UIResourceManager. The layers which need this
  // (UIResourceLayers and PaintedScrollbarLayers) are never used by the
  // renderer compositor in remote mode.
  NOTREACHED() << "UIResourceManager requested. Unsupported Layer type used";
  return nullptr;
}

TaskRunnerProvider* LayerTreeHostRemote::GetTaskRunnerProvider() const {
  return task_runner_provider_.get();
}

const LayerTreeSettings& LayerTreeHostRemote::GetSettings() const {
  return settings_;
}

void LayerTreeHostRemote::SetFrameSinkId(const FrameSinkId& frame_sink_id) {
  // We don't need to care about SurfaceLayers. The Surfaces system is
  // relevant on the client only.
}

void LayerTreeHostRemote::SetLayerTreeMutator(
    std::unique_ptr<LayerTreeMutator> mutator) {
  // TODO(khushalsagar): Compositor-worker not supported. See crbug.com/650876.
}

void LayerTreeHostRemote::QueueSwapPromise(
    std::unique_ptr<SwapPromise> swap_promise) {
  swap_promise_manager_.QueueSwapPromise(std::move(swap_promise));
}

SwapPromiseManager* LayerTreeHostRemote::GetSwapPromiseManager() {
  return &swap_promise_manager_;
}

void LayerTreeHostRemote::SetHasGpuRasterizationTrigger(bool has_trigger) {
  // TODO(khushalsagar) : Take care of Gpu raster. See crbug.com/650431.
}

void LayerTreeHostRemote::SetVisible(bool visible) {
  // The visibility of the compositor is controlled on the client, which is
  // why this value is not sent there, since the client has the current true
  // state.
  visible_ = visible;
}

bool LayerTreeHostRemote::IsVisible() const {
  return visible_;
}

void LayerTreeHostRemote::SetCompositorFrameSink(
    std::unique_ptr<CompositorFrameSink> compositor_frame_sink) {
  NOTREACHED()
      << "The LayerTreeHostClient is never asked for a CompositorFrameSink";
}

std::unique_ptr<CompositorFrameSink>
LayerTreeHostRemote::ReleaseCompositorFrameSink() {
  // Since we never have a CompositorFrameSink, this is always a no-op.
  return nullptr;
}

void LayerTreeHostRemote::SetNeedsAnimate() {
  MainFrameRequested(FramePipelineStage::ANIMATE);
}

void LayerTreeHostRemote::SetNeedsUpdateLayers() {
  MainFrameRequested(FramePipelineStage::UPDATE_LAYERS);
}

void LayerTreeHostRemote::SetNeedsCommit() {
  MainFrameRequested(FramePipelineStage::COMMIT);
}

void LayerTreeHostRemote::SetNeedsRecalculateRasterScales() {
  // This is used by devtools to reraster content after changing device
  // emulation modes, so doesn't need to be supported by Blimp.
}

bool LayerTreeHostRemote::BeginMainFrameRequested() const {
  return requested_pipeline_stage_for_next_frame_ != FramePipelineStage::NONE;
}

bool LayerTreeHostRemote::CommitRequested() const {
  // We report that a commit is in progress when synchronizing scroll and scale
  // updates because in threaded mode, scroll/scale synchronization from the
  // impl thread happens only during the main frame.
  return synchronizing_client_updates_ ||
         requested_pipeline_stage_for_next_frame_ == FramePipelineStage::COMMIT;
}

void LayerTreeHostRemote::SetDeferCommits(bool defer_commits) {
  defer_commits_ = defer_commits;
  ScheduleMainFrameIfNecessary();
}

void LayerTreeHostRemote::LayoutAndUpdateLayers() {
  NOTREACHED() << "Only supported in single-threaded mode and this class"
               << " does not support single-thread since it is out of process";
}

void LayerTreeHostRemote::Composite(base::TimeTicks frame_begin_time) {
  NOTREACHED() << "Only supported in single-threaded mode and this class"
               << " does not support single-thread since it is out of process";
}

void LayerTreeHostRemote::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  // The engine shouldn't need to care about draws. CompositorFrames are never
  // used here.
  // TODO(khushalsagar): The caller could be waiting for an Ack for this redraw.
  // We need a better solution for this. See crbug.com/651141.
  NOTIMPLEMENTED();
}

void LayerTreeHostRemote::SetNextCommitForcesRedraw() {
  // Ideally the engine shouldn't need to care about draw requests at all. The
  // compositor that produces CompositorFrames is on the client and draw
  // requests should be made directly to it on the client itself.
  NOTIMPLEMENTED();
}

void LayerTreeHostRemote::NotifyInputThrottledUntilCommit() {
  // This notification is used in the case where the renderer handles an input
  // event, and needs to send an Ack to the browser when the resulting main
  // frame is committed. If the compositor is taking too long on the pending
  // tree, the commit processing will be delayed blocking all input as a result.
  // So this is used to have the compositor activate the pending tree faster, so
  // the pending commit can be processed.
  // In remote mode, we don't send such notifications to the client because the
  // most likely bottleneck is the transport instead of raster. Also, input is
  // queued on the client, so if raster does end up being a bottleneck, the
  // input handling code on the client informs the LayerTreeHostInProcess
  // directly.
  NOTIMPLEMENTED();
}

void LayerTreeHostRemote::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {
  NOTREACHED() << "Using BrowserControls animations is not supported";
}

const base::WeakPtr<InputHandler>& LayerTreeHostRemote::GetInputHandler()
    const {
  // Input on the compositor thread is handled on the client, so this is always
  // null.
  return input_handler_weak_ptr_;
}

void LayerTreeHostRemote::DidStopFlinging() {
  // TODO(khushalsagar): This should not happen. See crbug.com/652000.
  NOTIMPLEMENTED() << "We shouldn't be sending fling gestures to the engine";
}

void LayerTreeHostRemote::SetDebugState(
    const LayerTreeDebugState& debug_state) {
  // If any debugging needs to be enabled, ideally it should be using the
  // compositor on the client directly. But the setup code will always set this
  // on initialization, even if the settings end up being a no-op.
}

const LayerTreeDebugState& LayerTreeHostRemote::GetDebugState() const {
  return debug_state_;
}

int LayerTreeHostRemote::ScheduleMicroBenchmark(
    const std::string& benchmark_name,
    std::unique_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback) {
  NOTREACHED();
  return 0;
}

bool LayerTreeHostRemote::SendMessageToMicroBenchmark(
    int id,
    std::unique_ptr<base::Value> value) {
  NOTREACHED();
  return false;
}

SurfaceSequenceGenerator* LayerTreeHostRemote::GetSurfaceSequenceGenerator() {
  // TODO(khushalsagar): Eliminate the use of this in blink. See
  // crbug.com/650876.
  return &surface_sequence_generator_;
}

void LayerTreeHostRemote::SetNextCommitWaitsForActivation() {
  // This is used only by layers that need resource synchronization, i.e.,
  // texture and surface layers, both of which are not supported.
  NOTIMPLEMENTED() << "Unsupported Layer type used";
}

void LayerTreeHostRemote::ResetGpuRasterizationTracking() {
  // TODO(khushalsagar): Take care of Gpu raster. See crbug.com/650431.
}

void LayerTreeHostRemote::MainFrameRequested(
    FramePipelineStage requested_pipeline_stage) {
  DCHECK_NE(FramePipelineStage::NONE, requested_pipeline_stage);

  swap_promise_manager_.NotifySwapPromiseMonitorsOfSetNeedsCommit();

  // If we are inside a main frame update right now and the requested pipeline
  // stage is higher than the pipeline stage that we are at, then we'll get to
  // in this main frame update itself. Update the
  // |max_pipeline_stage_for_current_frame_| to ensure we go through the
  // requested pipeline stage.
  if (current_pipeline_stage_ != FramePipelineStage::NONE &&
      requested_pipeline_stage > current_pipeline_stage_) {
    max_pipeline_stage_for_current_frame_ = std::max(
        max_pipeline_stage_for_current_frame_, requested_pipeline_stage);
    return;
  }

  // Update the pipeline stage for the next frame and schedule an update if it
  // has not been scheduled already.
  requested_pipeline_stage_for_next_frame_ = std::max(
      requested_pipeline_stage_for_next_frame_, requested_pipeline_stage);

  ScheduleMainFrameIfNecessary();
}

void LayerTreeHostRemote::ScheduleMainFrameIfNecessary() {
  // If the client hasn't asked for a main frame, don't schedule one.
  if (requested_pipeline_stage_for_next_frame_ == FramePipelineStage::NONE)
    return;

  // If the client does not want us to run main frame updates right now, don't
  // schedule one.
  if (defer_commits_)
    return;

  // If a main frame request is already pending with the
  // RemoteCompositorBridge, we don't need to scheduler another one.
  if (main_frame_requested_from_bridge_)
    return;

  remote_compositor_bridge_->ScheduleMainFrame();
  main_frame_requested_from_bridge_ = true;
}

void LayerTreeHostRemote::BeginMainFrame() {
  DCHECK(main_frame_requested_from_bridge_);
  DCHECK(task_runner_provider_->IsMainThread());

  main_frame_requested_from_bridge_ = false;

  // The client might have suspended main frames in the meantime. Early out now,
  // we'll come back here when they enable main frames again.
  if (defer_commits_)
    return;

  DCHECK_EQ(current_pipeline_stage_, FramePipelineStage::NONE);
  DCHECK_EQ(max_pipeline_stage_for_current_frame_, FramePipelineStage::NONE);
  DCHECK_NE(requested_pipeline_stage_for_next_frame_, FramePipelineStage::NONE);

  // Start the main frame. It should go till the requested pipeline stage.
  max_pipeline_stage_for_current_frame_ =
      requested_pipeline_stage_for_next_frame_;
  requested_pipeline_stage_for_next_frame_ = FramePipelineStage::NONE;

  client_->WillBeginMainFrame();

  current_pipeline_stage_ = FramePipelineStage::ANIMATE;
  base::TimeTicks now = base::TimeTicks::Now();
  client_->BeginMainFrame(BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, now, now + kDefaultFrameInterval,
      kDefaultFrameInterval, BeginFrameArgs::NORMAL));
  // We don't run any animations on the layer because threaded animations are
  // disabled.
  // TODO(khushalsagar): Revisit this when adding support for animations.
  client_->UpdateLayerTreeHost();

  current_pipeline_stage_ = FramePipelineStage::UPDATE_LAYERS;
  LayerList layer_list;
  if (max_pipeline_stage_for_current_frame_ >=
      FramePipelineStage::UPDATE_LAYERS) {
    // Pull updates for all layers from the client.
    // TODO(khushalsagar): Investigate the data impact from updating all the
    // layers. See crbug.com/650885.
    LayerTreeHostCommon::CallFunctionForEveryLayer(
        layer_tree_.get(), [&layer_list](Layer* layer) {
          if (!ShouldUpdateLayer(layer))
            return;
          layer->SavePaintProperties();
          layer_list.push_back(layer);
        });

    bool content_is_suitable_for_gpu = false;
    bool layers_updated =
        layer_tree_->UpdateLayers(layer_list, &content_is_suitable_for_gpu);

    // If pulling layer updates resulted in any content updates, we need to go
    // till the commit stage.
    if (layers_updated)
      max_pipeline_stage_for_current_frame_ = FramePipelineStage::COMMIT;
  }

  current_pipeline_stage_ = FramePipelineStage::COMMIT;
  client_->WillCommit();

  if (max_pipeline_stage_for_current_frame_ < current_pipeline_stage_) {
    // There is nothing to commit so break the swap promises.
    swap_promise_manager_.BreakSwapPromises(
        SwapPromise::DidNotSwapReason::COMMIT_NO_UPDATE);

    // For the client, the commit was successful.
    MainFrameComplete();
    return;
  }

  std::unique_ptr<CompositorProtoState> compositor_state =
      base::MakeUnique<CompositorProtoState>();
  compositor_state->swap_promises = swap_promise_manager_.TakeSwapPromises();
  compositor_state->compositor_message =
      base::MakeUnique<proto::CompositorMessage>();
  SerializeCurrentState(
      compositor_state->compositor_message->mutable_layer_tree_host());
  remote_compositor_bridge_->ProcessCompositorStateUpdate(
      std::move(compositor_state));

  MainFrameComplete();

  // We can not wait for updates dispatched from the client about the state of
  // drawing or swaps for frames sent. Since these calls can be used by the
  // LayerTreeHostClient to throttle further frame updates, so dispatch them
  // right after the update is processed by the bridge.
  // TODO(khushalsagar): We can not really know what these callbacks end up
  // being used for. Consider migrating clients to understand/cope with the fact
  // that there is no actual compositing happening here.
  task_runner_provider_->MainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&LayerTreeHostRemote::DispatchDrawAndSubmitCallbacks,
                 weak_factory_.GetWeakPtr()));
}

void LayerTreeHostRemote::ApplyStateUpdateFromClient(
    const proto::ClientStateUpdate& client_state_update) {
  DCHECK(!synchronizing_client_updates_);
  base::AutoReset<bool> synchronizing_updates(&synchronizing_client_updates_,
                                              true);

  gfx::Vector2dF inner_viewport_delta;
  for (int i = 0; i < client_state_update.scroll_updates_size(); ++i) {
    const proto::ScrollUpdate& scroll_update =
        client_state_update.scroll_updates(i);
    int layer_id = scroll_update.layer_id();
    Layer* layer = layer_tree_->LayerById(layer_id);
    gfx::Vector2dF scroll_delta =
        ProtoToVector2dF(scroll_update.scroll_delta());

    if (!layer)
      continue;

    if (layer == layer_tree_->inner_viewport_scroll_layer()) {
      inner_viewport_delta = scroll_delta;
    } else {
      layer->SetScrollOffsetFromImplSide(
          gfx::ScrollOffsetWithDelta(layer->scroll_offset(), scroll_delta));
      SetNeedsUpdateLayers();
    }
  }

  if (!inner_viewport_delta.IsZero()) {
    layer_tree_->inner_viewport_scroll_layer()->SetScrollOffsetFromImplSide(
        gfx::ScrollOffsetWithDelta(
            layer_tree_->inner_viewport_scroll_layer()->scroll_offset(),
            inner_viewport_delta));
  }

  float page_scale_delta = 1.0f;
  if (client_state_update.has_page_scale_delta()) {
    page_scale_delta = client_state_update.page_scale_delta();
    layer_tree_->SetPageScaleFromImplSide(layer_tree_->page_scale_factor() *
                                          page_scale_delta);
  }

  if (!inner_viewport_delta.IsZero() || page_scale_delta != 1.0f) {
    client_->ApplyViewportDeltas(inner_viewport_delta, gfx::Vector2dF(),
                                 gfx::Vector2dF(), page_scale_delta, 0.0f);
    SetNeedsUpdateLayers();
  }
}

void LayerTreeHostRemote::MainFrameComplete() {
  DCHECK_EQ(current_pipeline_stage_, FramePipelineStage::COMMIT);

  current_pipeline_stage_ = FramePipelineStage::NONE;
  max_pipeline_stage_for_current_frame_ = FramePipelineStage::NONE;
  source_frame_number_++;

  client_->DidCommit();
  client_->DidBeginMainFrame();
}

void LayerTreeHostRemote::DispatchDrawAndSubmitCallbacks() {
  client_->DidCommitAndDrawFrame();
  client_->DidReceiveCompositorFrameAck();
}

void LayerTreeHostRemote::SetTaskRunnerProviderForTesting(
    std::unique_ptr<TaskRunnerProvider> task_runner_provider) {
  task_runner_provider_ = std::move(task_runner_provider);
}

void LayerTreeHostRemote::SerializeCurrentState(
    proto::LayerTreeHost* layer_tree_host_proto) {
  // Serialize the LayerTree.
  layer_tree_->ToProtobuf(layer_tree_host_proto->mutable_layer_tree());

  // Serialize the dirty layers.
  std::unordered_set<Layer*> layers_need_push_properties;
  layers_need_push_properties.swap(
      layer_tree_->LayersThatShouldPushProperties());

  for (auto* layer : layers_need_push_properties) {
    proto::LayerProperties* layer_properties =
        layer_tree_host_proto->mutable_layer_updates()->add_layers();
    layer->ToLayerPropertiesProto(layer_properties);
  }

  std::vector<PictureData> pictures =
      engine_picture_cache_->CalculateCacheUpdateAndFlush();
  proto::PictureDataVectorToSkPicturesProto(
      pictures, layer_tree_host_proto->mutable_pictures());
}

}  // namespace cc
