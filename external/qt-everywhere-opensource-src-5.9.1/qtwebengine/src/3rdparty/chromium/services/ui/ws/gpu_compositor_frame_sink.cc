// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/gpu_compositor_frame_sink.h"

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display_scheduler.h"
#include "services/ui/surfaces/direct_output_surface.h"
#include "services/ui/surfaces/display_compositor.h"

#if defined(USE_OZONE)
#include "gpu/command_buffer/client/gles2_interface.h"
#include "services/ui/surfaces/direct_output_surface_ozone.h"
#endif

namespace ui {
namespace ws {

GpuCompositorFrameSink::GpuCompositorFrameSink(
    scoped_refptr<DisplayCompositor> display_compositor,
    const cc::FrameSinkId& frame_sink_id,
    gfx::AcceleratedWidget widget,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    scoped_refptr<SurfacesContextProvider> context_provider,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client)
    : frame_sink_id_(frame_sink_id),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      display_compositor_(display_compositor),
      surface_factory_(frame_sink_id_, display_compositor_->manager(), this),
      client_(std::move(client)),
      binding_(this, std::move(request)) {
  display_compositor_->manager()->RegisterFrameSinkId(frame_sink_id_);
  display_compositor_->manager()->RegisterSurfaceFactoryClient(frame_sink_id_,
                                                               this);

  if (widget != gfx::kNullAcceleratedWidget)
    InitDisplay(widget, gpu_memory_buffer_manager, std::move(context_provider));
}

GpuCompositorFrameSink::~GpuCompositorFrameSink() {
  // SurfaceFactory's destructor will attempt to return resources which will
  // call back into here and access |client_| so we should destroy
  // |surface_factory_|'s resources early on.
  surface_factory_.DestroyAll();
  display_compositor_->manager()->UnregisterSurfaceFactoryClient(
      frame_sink_id_);
  display_compositor_->manager()->InvalidateFrameSinkId(frame_sink_id_);
}

void GpuCompositorFrameSink::SetNeedsBeginFrame(bool needs_begin_frame) {
  needs_begin_frame_ = needs_begin_frame;
  UpdateNeedsBeginFramesInternal();
}

void GpuCompositorFrameSink::SubmitCompositorFrame(cc::CompositorFrame frame) {
  gfx::Size frame_size =
      frame.delegated_frame_data->render_pass_list[0]->output_rect.size();
  // If the size of the CompostiorFrame has changed then destroy the existing
  // Surface and create a new one of the appropriate size.
  if (!local_frame_id_.is_valid() || frame_size != last_submitted_frame_size_) {
    if (local_frame_id_.is_valid())
      surface_factory_.Destroy(local_frame_id_);
    local_frame_id_ = surface_id_allocator_.GenerateId();
    surface_factory_.Create(local_frame_id_);
    if (display_)
      display_->Resize(frame_size);
  }
  ++ack_pending_count_;
  surface_factory_.SubmitCompositorFrame(
      local_frame_id_, std::move(frame),
      base::Bind(&GpuCompositorFrameSink::DidReceiveCompositorFrameAck,
                 base::Unretained(this)));
  if (display_) {
    display_->SetLocalFrameId(local_frame_id_,
                              frame.metadata.device_scale_factor);
  }
  last_submitted_frame_size_ = frame_size;
}

void GpuCompositorFrameSink::DidReceiveCompositorFrameAck() {
  if (!client_)
    return;
  client_->DidReceiveCompositorFrameAck();
  DCHECK_GT(ack_pending_count_, 0);
  if (!surface_returned_resources_.empty()) {
    client_->ReclaimResources(surface_returned_resources_);
    surface_returned_resources_.clear();
  }
  ack_pending_count_--;
}

void GpuCompositorFrameSink::AddChildFrameSink(
    const cc::FrameSinkId& child_frame_sink_id) {
  cc::SurfaceManager* surface_manager = display_compositor_->manager();
  surface_manager->RegisterFrameSinkHierarchy(frame_sink_id_,
                                              child_frame_sink_id);
}

void GpuCompositorFrameSink::RemoveChildFrameSink(
    const cc::FrameSinkId& child_frame_sink_id) {
  cc::SurfaceManager* surface_manager = display_compositor_->manager();
  surface_manager->UnregisterFrameSinkHierarchy(frame_sink_id_,
                                                child_frame_sink_id);
}

void GpuCompositorFrameSink::InitDisplay(
    gfx::AcceleratedWidget widget,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    scoped_refptr<SurfacesContextProvider> context_provider) {
  // TODO(rjkroege): If there is something better to do than CHECK, add it.
  CHECK(context_provider->BindToCurrentThread());

  std::unique_ptr<cc::SyntheticBeginFrameSource> synthetic_begin_frame_source(
      new cc::DelayBasedBeginFrameSource(
          base::MakeUnique<cc::DelayBasedTimeSource>(task_runner_.get())));

  std::unique_ptr<cc::OutputSurface> display_output_surface;
  if (context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE)
    display_output_surface = base::MakeUnique<DirectOutputSurfaceOzone>(
        context_provider, widget, synthetic_begin_frame_source.get(),
        gpu_memory_buffer_manager, GL_TEXTURE_2D, GL_RGB);
#else
    NOTREACHED();
#endif
  } else {
    display_output_surface = base::MakeUnique<DirectOutputSurface>(
        context_provider, synthetic_begin_frame_source.get());
  }

  int max_frames_pending =
      display_output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  std::unique_ptr<cc::DisplayScheduler> scheduler(
      new cc::DisplayScheduler(synthetic_begin_frame_source.get(),
                               task_runner_.get(), max_frames_pending));

  display_.reset(new cc::Display(
      nullptr /* bitmap_manager */, gpu_memory_buffer_manager,
      cc::RendererSettings(), frame_sink_id_,
      std::move(synthetic_begin_frame_source),
      std::move(display_output_surface), std::move(scheduler),
      base::MakeUnique<cc::TextureMailboxDeleter>(task_runner_.get())));
  display_->Initialize(this, display_compositor_->manager());
  display_->SetVisible(true);
}

void GpuCompositorFrameSink::DisplayOutputSurfaceLost() {}

void GpuCompositorFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    const cc::RenderPassList& render_passes) {}

void GpuCompositorFrameSink::DisplayDidDrawAndSwap() {}

void GpuCompositorFrameSink::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (resources.empty())
    return;

  if (!ack_pending_count_ && client_) {
    client_->ReclaimResources(resources);
    return;
  }

  std::copy(resources.begin(), resources.end(),
            std::back_inserter(surface_returned_resources_));
}

void GpuCompositorFrameSink::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(tansell): Implement this.
  if (begin_frame_source_ && added_frame_observer_) {
    begin_frame_source_->RemoveObserver(this);
    added_frame_observer_ = false;
  }
  begin_frame_source_ = begin_frame_source;
  UpdateNeedsBeginFramesInternal();
}

void GpuCompositorFrameSink::OnBeginFrame(const cc::BeginFrameArgs& args) {
  UpdateNeedsBeginFramesInternal();
  last_begin_frame_args_ = args;
  if (client_)
    client_->OnBeginFrame(args);
}

const cc::BeginFrameArgs& GpuCompositorFrameSink::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void GpuCompositorFrameSink::OnBeginFrameSourcePausedChanged(bool paused) {}

void GpuCompositorFrameSink::UpdateNeedsBeginFramesInternal() {
  if (!begin_frame_source_)
    return;

  if (needs_begin_frame_ == added_frame_observer_)
    return;

  added_frame_observer_ = needs_begin_frame_;
  if (needs_begin_frame_)
    begin_frame_source_->AddObserver(this);
  else
    begin_frame_source_->RemoveObserver(this);
}

}  // namespace ws
}  // namespace ui
