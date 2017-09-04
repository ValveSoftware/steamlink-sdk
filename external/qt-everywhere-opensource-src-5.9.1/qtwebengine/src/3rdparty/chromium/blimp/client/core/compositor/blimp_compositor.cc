// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blimp_compositor.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/compositor/blimp_compositor_frame_sink.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "blimp/net/blimp_stats.h"
#include "cc/animation/animation_host.h"
#include "cc/blimp/client_picture_cache.h"
#include "cc/blimp/compositor_state_deserializer.h"
#include "cc/blimp/image_serialization_processor.h"
#include "cc/layers/layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/layer_tree_host_in_process.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "net/base/net_errors.h"
#include "ui/gl/gl_surface.h"

namespace blimp {
namespace client {

namespace {

void SatisfyCallback(cc::SurfaceManager* manager,
                     const cc::SurfaceSequence& sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  manager->DidSatisfySequences(sequence.frame_sink_id, &sequences);
}

void RequireCallback(cc::SurfaceManager* manager,
                     const cc::SurfaceId& id,
                     const cc::SurfaceSequence& sequence) {
  cc::Surface* surface = manager->GetSurfaceForId(id);
  if (!surface) {
    LOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

}  // namespace

class BlimpCompositor::FrameTrackingSwapPromise : public cc::SwapPromise {
 public:
  FrameTrackingSwapPromise(
      std::unique_ptr<cc::CopyOutputRequest> copy_request,
      base::WeakPtr<BlimpCompositor> compositor,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
      : copy_request_(std::move(copy_request)),
        compositor_weak_ptr_(compositor),
        main_task_runner_(std::move(main_task_runner)) {}
  ~FrameTrackingSwapPromise() override = default;

  // cc::SwapPromise implementation.
  void DidActivate() override {}
  void DidSwap(cc::CompositorFrameMetadata* metadata) override {
    // DidSwap is called right before the CompositorFrame is submitted to the
    // CompositorFrameSink, so we make sure to delay the copy request till that
    // frame is submitted.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BlimpCompositor::MakeCopyRequestOnNextSwap,
                   compositor_weak_ptr_, base::Passed(&copy_request_)));
  }
  DidNotSwapAction DidNotSwap(DidNotSwapReason reason) override {
    switch (reason) {
      case DidNotSwapReason::SWAP_FAILS:
        // The swap will fail if there is no frame damage, we can queue the
        // request right away.
        main_task_runner_->PostTask(
            FROM_HERE, base::Bind(&BlimpCompositor::RequestCopyOfOutput,
                                  compositor_weak_ptr_,
                                  base::Passed(&copy_request_), false));
        break;
      case DidNotSwapReason::COMMIT_FAILS:
        // The commit fails when the host is going away.
        break;
      case DidNotSwapReason::COMMIT_NO_UPDATE:
        main_task_runner_->PostTask(
            FROM_HERE, base::Bind(&BlimpCompositor::RequestCopyOfOutput,
                                  compositor_weak_ptr_,
                                  base::Passed(&copy_request_), false));
        break;
      case DidNotSwapReason::ACTIVATION_FAILS:
        // Failure to activate the pending tree implies either the host in going
        // away or the FrameSink was lost.
        break;
    }
    return DidNotSwapAction::BREAK_PROMISE;
  }
  int64_t TraceId() const override { return 0; }

 private:
  std::unique_ptr<cc::CopyOutputRequest> copy_request_;
  base::WeakPtr<BlimpCompositor> compositor_weak_ptr_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

// static
std::unique_ptr<BlimpCompositor> BlimpCompositor::Create(
    BlimpCompositorDependencies* compositor_dependencies,
    BlimpCompositorClient* client) {
  std::unique_ptr<BlimpCompositor> compositor =
      base::WrapUnique(new BlimpCompositor(compositor_dependencies, client));
  compositor->Initialize();
  return compositor;
}

BlimpCompositor::BlimpCompositor(
    BlimpCompositorDependencies* compositor_dependencies,
    BlimpCompositorClient* client)
    : client_(client),
      compositor_dependencies_(compositor_dependencies),
      frame_sink_id_(compositor_dependencies_->GetEmbedderDependencies()
                         ->AllocateFrameSinkId()),
      proxy_client_(nullptr),
      bound_to_proxy_(false),
      compositor_frame_sink_request_pending_(false),
      layer_(cc::Layer::Create()),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void BlimpCompositor::Initialize() {
  surface_id_allocator_ = base::MakeUnique<cc::SurfaceIdAllocator>();
  GetEmbedderDeps()->GetSurfaceManager()->RegisterFrameSinkId(frame_sink_id_);
  surface_factory_ = base::MakeUnique<cc::SurfaceFactory>(
      frame_sink_id_, GetEmbedderDeps()->GetSurfaceManager(), this);
  animation_host_ = cc::AnimationHost::CreateMainInstance();
  host_ = CreateLayerTreeHost();

    std::unique_ptr<cc::ClientPictureCache> client_picture_cache =
        compositor_dependencies_->GetImageSerializationProcessor()
            ->CreateClientPictureCache();
    compositor_state_deserializer_ =
        base::MakeUnique<cc::CompositorStateDeserializer>(
            host_.get(), std::move(client_picture_cache), this);
}

BlimpCompositor::~BlimpCompositor() {
  DCHECK(thread_checker_.CalledOnValidThread());

  DestroyLayerTreeHost();
  GetEmbedderDeps()->GetSurfaceManager()->InvalidateFrameSinkId(frame_sink_id_);
}

void BlimpCompositor::SetVisible(bool visible) {
  host_->SetVisible(visible);
}

bool BlimpCompositor::IsVisible() const {
  return host_->IsVisible();
}

bool BlimpCompositor::HasPendingFrameUpdateFromEngine() const {
  return pending_frame_update_.get() != nullptr;
}

void BlimpCompositor::RequestCopyOfOutput(
    std::unique_ptr<cc::CopyOutputRequest> copy_request,
    bool flush_pending_update) {
  // If we don't have a FrameSink, fail right away.
  if (!bound_to_proxy_)
    return;

  if (flush_pending_update) {
    // Always request a commit when queuing the promise to make sure that any
    // frames pending draws are cleared from the pipeline.
    host_->QueueSwapPromise(base::MakeUnique<FrameTrackingSwapPromise>(
        std::move(copy_request), weak_ptr_factory_.GetWeakPtr(),
        base::ThreadTaskRunnerHandle::Get()));
    host_->SetNeedsCommit();
  } else if (local_frame_id_.is_valid()) {
    // Make a copy request for the surface directly.
    surface_factory_->RequestCopyOfSurface(local_frame_id_,
                                           std::move(copy_request));
  }
}

void BlimpCompositor::UpdateLayerTreeHost() {
  // UpdateLayerTreeHost marks the end of reporting of any deltas from the impl
  // thread. So send a client state update if the local state was modified now.
  FlushClientState();

  if (pending_frame_update_) {
    compositor_state_deserializer_->DeserializeCompositorUpdate(
        pending_frame_update_->layer_tree_host());
    pending_frame_update_ = nullptr;
    cc::proto::CompositorMessage frame_ack;
    frame_ack.set_frame_ack(true);
    client_->SendCompositorMessage(frame_ack);
  }

  // Send back any deltas that have not yet been resolved on the main thread
  // back to the impl thread.
  compositor_state_deserializer_->SendUnappliedDeltasToLayerTreeHost();
}

void BlimpCompositor::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
  compositor_state_deserializer_->ApplyViewportDeltas(
      inner_delta, outer_delta, elastic_overscroll_delta, page_scale,
      top_controls_delta);
}

void BlimpCompositor::RequestNewCompositorFrameSink() {
  DCHECK(!bound_to_proxy_);
  DCHECK(!compositor_frame_sink_request_pending_);

  compositor_frame_sink_request_pending_ = true;
  GetEmbedderDeps()->GetContextProviders(
      base::Bind(&BlimpCompositor::OnContextProvidersCreated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BlimpCompositor::DidInitializeCompositorFrameSink() {
  compositor_frame_sink_request_pending_ = false;
}

void BlimpCompositor::DidCommitAndDrawFrame() {}

void BlimpCompositor::OnCompositorMessageReceived(
    std::unique_ptr<cc::proto::CompositorMessage> message) {
  cc::proto::CompositorMessage* message_received = message.get();

  if (message_received->has_layer_tree_host()) {
    DCHECK(!pending_frame_update_)
        << "We should have only a single frame in flight";

    UMA_HISTOGRAM_MEMORY_KB("Blimp.Compositor.CommitSizeKb",
                            (float)message->ByteSize() / 1024);
    BlimpStats::GetInstance()->Add(BlimpStats::COMMIT, 1);

    pending_frame_update_ = std::move(message);
    host_->SetNeedsAnimate();
  }

  if (message_received->client_state_update_ack()) {
    DCHECK(client_state_update_ack_pending_);

    client_state_update_ack_pending_ = false;
    compositor_state_deserializer_->DidApplyStateUpdatesOnEngine();

    // If there are any updates that we have queued because we were waiting for
    // an ack, send them now.
    FlushClientState();
  }
}

const base::WeakPtr<cc::InputHandler>& BlimpCompositor::GetInputHandler() {
  return host_->GetInputHandler();
}

void BlimpCompositor::OnContextProvidersCreated(
    const scoped_refptr<cc::ContextProvider>& compositor_context_provider,
    const scoped_refptr<cc::ContextProvider>& worker_context_provider) {
  DCHECK(!bound_to_proxy_) << "Any connection to the old CompositorFrameSink "
                              "should have been destroyed";

  // Make sure we still have a host and we're still expecting a
  // CompositorFrameSink. This can happen if the host dies while the request is
  // outstanding and we build a new one that hasn't asked for a surface yet.
  if (!compositor_frame_sink_request_pending_)
    return;

  // Try again if the context creation failed.
  if (!compositor_context_provider) {
    GetEmbedderDeps()->GetContextProviders(
        base::Bind(&BlimpCompositor::OnContextProvidersCreated,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  auto compositor_frame_sink = base::MakeUnique<BlimpCompositorFrameSink>(
      std::move(compositor_context_provider),
      std::move(worker_context_provider),
      GetEmbedderDeps()->GetGpuMemoryBufferManager(), nullptr,
      base::ThreadTaskRunnerHandle::Get(), weak_ptr_factory_.GetWeakPtr());

  host_->SetCompositorFrameSink(std::move(compositor_frame_sink));
}

void BlimpCompositor::BindToProxyClient(
    base::WeakPtr<BlimpCompositorFrameSinkProxyClient> proxy_client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!bound_to_proxy_);

  bound_to_proxy_ = true;
  proxy_client_ = proxy_client;
}

void BlimpCompositor::SubmitCompositorFrame(cc::CompositorFrame frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(bound_to_proxy_);

  cc::RenderPass* root_pass =
      frame.delegated_frame_data->render_pass_list.back().get();
  gfx::Size surface_size = root_pass->output_rect.size();

  if (!local_frame_id_.is_valid() || current_surface_size_ != surface_size) {
    DestroyDelegatedContent();
    DCHECK(layer_->children().empty());

    local_frame_id_ = surface_id_allocator_->GenerateId();
    surface_factory_->Create(local_frame_id_);
    current_surface_size_ = surface_size;

    // manager must outlive compositors using it.
    cc::SurfaceManager* surface_manager =
        GetEmbedderDeps()->GetSurfaceManager();
    scoped_refptr<cc::SurfaceLayer> content_layer = cc::SurfaceLayer::Create(
        base::Bind(&SatisfyCallback, base::Unretained(surface_manager)),
        base::Bind(&RequireCallback, base::Unretained(surface_manager)));
    content_layer->SetSurfaceId(
        cc::SurfaceId(surface_factory_->frame_sink_id(), local_frame_id_), 1.f,
        surface_size);
    content_layer->SetBounds(current_surface_size_);
    content_layer->SetIsDrawable(true);
    content_layer->SetContentsOpaque(true);

    layer_->AddChild(content_layer);
  }

  surface_factory_->SubmitCompositorFrame(
      local_frame_id_, std::move(frame),
      base::Bind(&BlimpCompositor::SubmitCompositorFrameAck,
                 weak_ptr_factory_.GetWeakPtr()));

  for (auto& copy_request : copy_requests_for_next_swap_) {
    surface_factory_->RequestCopyOfSurface(local_frame_id_,
                                           std::move(copy_request));
  }
  copy_requests_for_next_swap_.clear();
}

void BlimpCompositor::SubmitCompositorFrameAck() {
  compositor_dependencies_->GetCompositorTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&BlimpCompositorFrameSinkProxyClient::SubmitCompositorFrameAck,
                 proxy_client_));
}

void BlimpCompositor::MakeCopyRequestOnNextSwap(
    std::unique_ptr<cc::CopyOutputRequest> copy_request) {
  copy_requests_for_next_swap_.push_back(std::move(copy_request));
}

void BlimpCompositor::UnbindProxyClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(bound_to_proxy_);

  DestroyDelegatedContent();
  surface_factory_->Reset();
  bound_to_proxy_ = false;
  proxy_client_ = nullptr;
}

void BlimpCompositor::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(bound_to_proxy_);
  compositor_dependencies_->GetCompositorTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &BlimpCompositorFrameSinkProxyClient::ReclaimCompositorResources,
          proxy_client_, resources));
}

void BlimpCompositor::DidUpdateLocalState() {
  client_state_dirty_ = true;
}

void BlimpCompositor::FlushClientState() {
  // If the client state has not been modified, we don't need to send an update.
  if (!client_state_dirty_)
    return;

  // If we had sent an update and an ack for it is still pending, we can't send
  // another update till the ack is received.
  if (client_state_update_ack_pending_)
    return;

  cc::proto::CompositorMessage message;
  message.set_frame_ack(false);
  compositor_state_deserializer_->PullClientStateUpdate(
      message.mutable_client_state_update());

  client_state_dirty_ = false;
  client_state_update_ack_pending_ = true;
  client_->SendCompositorMessage(message);
}

CompositorDependencies* BlimpCompositor::GetEmbedderDeps() {
  return compositor_dependencies_->GetEmbedderDependencies();
}

void BlimpCompositor::DestroyDelegatedContent() {
  if (!local_frame_id_.is_valid())
    return;

  // Remove any references for the surface layer that uses this
  // |local_frame_id_|.
  layer_->RemoveAllChildren();
  surface_factory_->Destroy(local_frame_id_);
  local_frame_id_ = cc::LocalFrameId();
}

std::unique_ptr<cc::LayerTreeHostInProcess>
BlimpCompositor::CreateLayerTreeHost() {
  DCHECK(animation_host_);
  std::unique_ptr<cc::LayerTreeHostInProcess> host;

  cc::LayerTreeHostInProcess::InitParams params;
  params.client = this;
  params.task_graph_runner = compositor_dependencies_->GetTaskGraphRunner();
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
    params.image_serialization_processor =
        compositor_dependencies_->GetImageSerializationProcessor();

  cc::LayerTreeSettings* settings =
      compositor_dependencies_->GetLayerTreeSettings();
  params.settings = settings;
  params.mutator_host = animation_host_.get();

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner =
      compositor_dependencies_->GetCompositorTaskRunner();

    host = cc::LayerTreeHostInProcess::CreateThreaded(compositor_task_runner,
                                                      &params);

  return host;
}

void BlimpCompositor::DestroyLayerTreeHost() {
  DCHECK(host_);

  // Tear down the output surface connection with the old LayerTreeHost
  // instance.
  DestroyDelegatedContent();

  // Destroy the old LayerTreeHost state.
  host_.reset();

  // Cancel any outstanding CompositorFrameSink requests.  That way if we get an
  // async callback related to the old request we know to drop it.
  compositor_frame_sink_request_pending_ = false;
}

}  // namespace client
}  // namespace blimp
