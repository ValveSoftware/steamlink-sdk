// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_frame_sink.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_frame.h"
#include "cc/output/renderer_settings.h"
#include "cc/output/software_output_device.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/renderer/android/synchronous_compositor_filter.h"
#include "content/renderer/android/synchronous_compositor_registry.h"
#include "content/renderer/gpu/frame_swap_message_queue.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_sender.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace content {

namespace {

const int64_t kFallbackTickTimeoutInMilliseconds = 100;
const cc::FrameSinkId kFrameSinkId(1, 1);

// Do not limit number of resources, so use an unrealistically high value.
const size_t kNumResourcesLimit = 10 * 1000 * 1000;

class SoftwareDevice : public cc::SoftwareOutputDevice {
 public:
  SoftwareDevice(SkCanvas** canvas) : canvas_(canvas) {}

  void Resize(const gfx::Size& pixel_size, float device_scale_factor) override {
    // Intentional no-op: canvas size is controlled by the embedder.
  }
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override {
    DCHECK(*canvas_) << "BeginPaint with no canvas set";
    return *canvas_;
  }
  void EndPaint() override {}

 private:
  SkCanvas** canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareDevice);
};

}  // namespace

class SynchronousCompositorFrameSink::SoftwareOutputSurface
    : public cc::OutputSurface {
 public:
  SoftwareOutputSurface(std::unique_ptr<SoftwareDevice> software_device)
      : cc::OutputSurface(std::move(software_device)) {}

  // cc::OutputSurface implementation.
  void BindToClient(cc::OutputSurfaceClient* client) override {}
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override {}
  void SwapBuffers(cc::OutputSurfaceFrame frame) override {}
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override {}
  uint32_t GetFramebufferCopyTextureFormat() override { return 0; }
  cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override {
    return nullptr;
  }
  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  bool SurfaceIsSuspendForRecycle() const override { return false; }
  bool HasExternalStencilTest() const override { return false; }
  void ApplyExternalStencil() override {}
};

SynchronousCompositorFrameSink::SynchronousCompositorFrameSink(
    scoped_refptr<cc::ContextProvider> context_provider,
    scoped_refptr<cc::ContextProvider> worker_context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    int routing_id,
    uint32_t compositor_frame_sink_id,
    std::unique_ptr<cc::BeginFrameSource> begin_frame_source,
    SynchronousCompositorRegistry* registry,
    scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue)
    : cc::CompositorFrameSink(std::move(context_provider),
                              std::move(worker_context_provider),
                              gpu_memory_buffer_manager,
                              nullptr),
      routing_id_(routing_id),
      compositor_frame_sink_id_(compositor_frame_sink_id),
      registry_(registry),
      sender_(RenderThreadImpl::current()->sync_compositor_message_filter()),
      memory_policy_(0u),
      frame_swap_message_queue_(frame_swap_message_queue),
      surface_manager_(new cc::SurfaceManager),
      surface_id_allocator_(new cc::SurfaceIdAllocator()),
      surface_factory_(
          new cc::SurfaceFactory(kFrameSinkId, surface_manager_.get(), this)),
      begin_frame_source_(std::move(begin_frame_source)) {
  DCHECK(registry_);
  DCHECK(sender_);
  DCHECK(begin_frame_source_);
  thread_checker_.DetachFromThread();
  memory_policy_.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;
}

SynchronousCompositorFrameSink::~SynchronousCompositorFrameSink() = default;

void SynchronousCompositorFrameSink::SetSyncClient(
    SynchronousCompositorFrameSinkClient* compositor) {
  DCHECK(CalledOnValidThread());
  sync_client_ = compositor;
  if (sync_client_)
    Send(new SyncCompositorHostMsg_CompositorFrameSinkCreated(routing_id_));
}

bool SynchronousCompositorFrameSink::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorFrameSink, message)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_SetMemoryPolicy, SetMemoryPolicy)
    IPC_MESSAGE_HANDLER(SyncCompositorMsg_ReclaimResources, OnReclaimResources)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool SynchronousCompositorFrameSink::BindToClient(
    cc::CompositorFrameSinkClient* sink_client) {
  DCHECK(CalledOnValidThread());
  if (!cc::CompositorFrameSink::BindToClient(sink_client))
    return false;

  DCHECK(begin_frame_source_);
  client_->SetBeginFrameSource(begin_frame_source_.get());
  client_->SetMemoryPolicy(memory_policy_);
  client_->SetTreeActivationCallback(
      base::Bind(&SynchronousCompositorFrameSink::DidActivatePendingTree,
                 base::Unretained(this)));
  registry_->RegisterCompositorFrameSink(routing_id_, this);

  surface_manager_->RegisterFrameSinkId(kFrameSinkId);
  surface_manager_->RegisterSurfaceFactoryClient(kFrameSinkId, this);

  cc::RendererSettings software_renderer_settings;

  auto output_surface = base::MakeUnique<SoftwareOutputSurface>(
      base::MakeUnique<SoftwareDevice>(&current_sw_canvas_));
  software_output_surface_ = output_surface.get();

  // The shared_bitmap_manager and gpu_memory_buffer_manager here are null as
  // this Display is only used for resourcesless software draws, where no
  // resources are included in the frame swapped from the compositor. So there
  // is no need for these.
  display_.reset(new cc::Display(
      nullptr /* shared_bitmap_manager */,
      nullptr /* gpu_memory_buffer_manager */, software_renderer_settings,
      kFrameSinkId, nullptr /* begin_frame_source */, std::move(output_surface),
      nullptr /* scheduler */, nullptr /* texture_mailbox_deleter */));
  display_->Initialize(&display_client_, surface_manager_.get());
  display_->SetVisible(true);
  return true;
}

void SynchronousCompositorFrameSink::DetachFromClient() {
  DCHECK(CalledOnValidThread());
  client_->SetBeginFrameSource(nullptr);
  // Destroy the begin frame source on the same thread it was bound on.
  begin_frame_source_ = nullptr;
  registry_->UnregisterCompositorFrameSink(routing_id_, this);
  client_->SetTreeActivationCallback(base::Closure());
  if (root_local_frame_id_.is_valid()) {
    surface_factory_->Destroy(root_local_frame_id_);
    surface_factory_->Destroy(child_local_frame_id_);
  }
  surface_manager_->UnregisterSurfaceFactoryClient(kFrameSinkId);
  surface_manager_->InvalidateFrameSinkId(kFrameSinkId);
  software_output_surface_ = nullptr;
  display_ = nullptr;
  surface_factory_ = nullptr;
  surface_id_allocator_ = nullptr;
  surface_manager_ = nullptr;
  cc::CompositorFrameSink::DetachFromClient();
  CancelFallbackTick();
}

static void NoOpDrawCallback() {}

void SynchronousCompositorFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  DCHECK(CalledOnValidThread());
  DCHECK(sync_client_);

  if (fallback_tick_running_) {
    DCHECK(frame.delegated_frame_data->resource_list.empty());
    cc::ReturnedResourceArray return_resources;
    ReturnResources(return_resources);
    did_submit_frame_ = true;
    return;
  }

  cc::CompositorFrame submit_frame;

  if (in_software_draw_) {
    // The frame we send to the client is actually just the metadata. Preserve
    // the |frame| for the software path below.
    submit_frame.metadata = frame.metadata.Clone();

    if (!root_local_frame_id_.is_valid()) {
      root_local_frame_id_ = surface_id_allocator_->GenerateId();
      surface_factory_->Create(root_local_frame_id_);
      child_local_frame_id_ = surface_id_allocator_->GenerateId();
      surface_factory_->Create(child_local_frame_id_);
    }

    display_->SetLocalFrameId(root_local_frame_id_,
                              frame.metadata.device_scale_factor);

    // The layer compositor should be giving a frame that covers the
    // |sw_viewport_for_current_draw_| but at 0,0.
    gfx::Size child_size = sw_viewport_for_current_draw_.size();
    DCHECK(gfx::Rect(child_size) ==
           frame.delegated_frame_data->render_pass_list.back()->output_rect);

    // Make a size that covers from 0,0 and includes the area coming from the
    // layer compositor.
    gfx::Size display_size(sw_viewport_for_current_draw_.right(),
                           sw_viewport_for_current_draw_.bottom());
    display_->Resize(display_size);

    // The offset for the child frame relative to the origin of the canvas being
    // drawn into.
    gfx::Transform child_transform;
    child_transform.Translate(
        gfx::Vector2dF(sw_viewport_for_current_draw_.OffsetFromOrigin()));

    // Make a root frame that embeds the frame coming from the layer compositor
    // and positions it based on the provided viewport.
    // TODO(danakj): We could apply the transform here instead of passing it to
    // the CompositorFrameSink client too? (We'd have to do the same for
    // hardware frames in SurfacesInstance?)
    cc::CompositorFrame embed_frame;
    embed_frame.delegated_frame_data =
        base::MakeUnique<cc::DelegatedFrameData>();
    embed_frame.delegated_frame_data->render_pass_list.push_back(
        cc::RenderPass::Create());

    // The embedding RenderPass covers the entire Display's area.
    const auto& embed_render_pass =
        embed_frame.delegated_frame_data->render_pass_list.back();
    embed_render_pass->SetAll(cc::RenderPassId(1, 1), gfx::Rect(display_size),
                              gfx::Rect(display_size), gfx::Transform(), false);

    // The RenderPass has a single SurfaceDrawQuad (and SharedQuadState for it).
    auto* shared_quad_state =
        embed_render_pass->CreateAndAppendSharedQuadState();
    auto* surface_quad =
        embed_render_pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    shared_quad_state->SetAll(
        child_transform, child_size, gfx::Rect(child_size),
        gfx::Rect() /* clip_rect */, false /* is_clipped */, 1.f /* opacity */,
        SkXfermode::kSrcOver_Mode, 0 /* sorting_context_id */);
    surface_quad->SetNew(shared_quad_state, gfx::Rect(child_size),
                         gfx::Rect(child_size),
                         cc::SurfaceId(kFrameSinkId, child_local_frame_id_));

    surface_factory_->SubmitCompositorFrame(
        child_local_frame_id_, std::move(frame), base::Bind(&NoOpDrawCallback));
    surface_factory_->SubmitCompositorFrame(root_local_frame_id_,
                                            std::move(embed_frame),
                                            base::Bind(&NoOpDrawCallback));
    display_->DrawAndSwap();
  } else {
    // For hardware draws we send the whole frame to the client so it can draw
    // the content in it.
    submit_frame = std::move(frame);
  }

  sync_client_->SubmitCompositorFrame(compositor_frame_sink_id_,
                                      std::move(submit_frame));
  DeliverMessages();
  did_submit_frame_ = true;
}

void SynchronousCompositorFrameSink::CancelFallbackTick() {
  fallback_tick_.Cancel();
  fallback_tick_pending_ = false;
}

void SynchronousCompositorFrameSink::FallbackTickFired() {
  DCHECK(CalledOnValidThread());
  TRACE_EVENT0("renderer", "SynchronousCompositorFrameSink::FallbackTickFired");
  base::AutoReset<bool> in_fallback_tick(&fallback_tick_running_, true);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(0);
  SkCanvas canvas(bitmap);
  fallback_tick_pending_ = false;
  DemandDrawSw(&canvas);
}

void SynchronousCompositorFrameSink::Invalidate() {
  DCHECK(CalledOnValidThread());
  if (sync_client_)
    sync_client_->Invalidate();

  if (!fallback_tick_pending_) {
    fallback_tick_.Reset(
        base::Bind(&SynchronousCompositorFrameSink::FallbackTickFired,
                   base::Unretained(this)));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, fallback_tick_.callback(),
        base::TimeDelta::FromMilliseconds(kFallbackTickTimeoutInMilliseconds));
    fallback_tick_pending_ = true;
  }
}

void SynchronousCompositorFrameSink::DemandDrawHw(
    const gfx::Size& viewport_size,
    const gfx::Rect& viewport_rect_for_tile_priority,
    const gfx::Transform& transform_for_tile_priority) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  DCHECK(context_provider_.get());
  CancelFallbackTick();

  client_->SetExternalTilePriorityConstraints(viewport_rect_for_tile_priority,
                                              transform_for_tile_priority);
  InvokeComposite(gfx::Transform(), gfx::Rect(viewport_size));
}

void SynchronousCompositorFrameSink::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(canvas);
  DCHECK(!current_sw_canvas_);
  CancelFallbackTick();

  base::AutoReset<SkCanvas*> canvas_resetter(&current_sw_canvas_, canvas);

  SkIRect canvas_clip;
  canvas->getClipDeviceBounds(&canvas_clip);
  gfx::Rect viewport = gfx::SkIRectToRect(canvas_clip);

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = canvas->getTotalMatrix();  // Converts 3x3 matrix to 4x4.

  // We will resize the Display to ensure it covers the entire |viewport|, so
  // save it for later.
  sw_viewport_for_current_draw_ = viewport;

  base::AutoReset<bool> set_in_software_draw(&in_software_draw_, true);
  InvokeComposite(transform, viewport);
}

void SynchronousCompositorFrameSink::InvokeComposite(
    const gfx::Transform& transform,
    const gfx::Rect& viewport) {
  did_submit_frame_ = false;
  // Adjust transform so that the layer compositor draws the |viewport| rect
  // at its origin. The offset of the |viewport| we pass to the layer compositor
  // is ignored for drawing, so its okay to not match the transform.
  // TODO(danakj): Why do we pass a viewport origin and then not really use it
  // (only for comparing to the viewport passed in
  // SetExternalTilePriorityConstraints), surely this could be more clear?
  gfx::Transform adjusted_transform = transform;
  adjusted_transform.matrix().postTranslate(-viewport.x(), -viewport.y(), 0);
  client_->OnDraw(adjusted_transform, viewport, in_software_draw_);

  if (did_submit_frame_) {
    // This must happen after unwinding the stack and leaving the compositor.
    // Usually it is a separate task but we just defer it until OnDraw completes
    // instead.
    client_->DidReceiveCompositorFrameAck();
  }
}

void SynchronousCompositorFrameSink::OnReclaimResources(
    uint32_t compositor_frame_sink_id,
    const cc::ReturnedResourceArray& resources) {
  // Ignore message if it's a stale one coming from a different output surface
  // (e.g. after a lost context).
  if (compositor_frame_sink_id != compositor_frame_sink_id_)
    return;
  client_->ReclaimResources(resources);
}

void SynchronousCompositorFrameSink::SetMemoryPolicy(size_t bytes_limit) {
  DCHECK(CalledOnValidThread());
  bool became_zero = memory_policy_.bytes_limit_when_visible && !bytes_limit;
  bool became_non_zero =
      !memory_policy_.bytes_limit_when_visible && bytes_limit;
  memory_policy_.bytes_limit_when_visible = bytes_limit;
  memory_policy_.num_resources_limit = kNumResourcesLimit;

  if (client_)
    client_->SetMemoryPolicy(memory_policy_);

  if (became_zero) {
    // This is small hack to drop context resources without destroying it
    // when this compositor is put into the background.
    context_provider()->ContextSupport()->SetAggressivelyFreeResources(
        true /* aggressively_free_resources */);
  } else if (became_non_zero) {
    context_provider()->ContextSupport()->SetAggressivelyFreeResources(
        false /* aggressively_free_resources */);
  }
}

void SynchronousCompositorFrameSink::DidActivatePendingTree() {
  DCHECK(CalledOnValidThread());
  if (sync_client_)
    sync_client_->DidActivatePendingTree();
  DeliverMessages();
}

void SynchronousCompositorFrameSink::DeliverMessages() {
  std::vector<std::unique_ptr<IPC::Message>> messages;
  std::unique_ptr<FrameSwapMessageQueue::SendMessageScope> send_message_scope =
      frame_swap_message_queue_->AcquireSendMessageScope();
  frame_swap_message_queue_->DrainMessages(&messages);
  for (auto& msg : messages) {
    Send(msg.release());
  }
}

bool SynchronousCompositorFrameSink::Send(IPC::Message* message) {
  DCHECK(CalledOnValidThread());
  return sender_->Send(message);
}

bool SynchronousCompositorFrameSink::CalledOnValidThread() const {
  return thread_checker_.CalledOnValidThread();
}

void SynchronousCompositorFrameSink::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(resources.empty());
  client_->ReclaimResources(resources);
}

void SynchronousCompositorFrameSink::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // Software output is synchronous and doesn't use a BeginFrameSource.
  NOTREACHED();
}

}  // namespace content
