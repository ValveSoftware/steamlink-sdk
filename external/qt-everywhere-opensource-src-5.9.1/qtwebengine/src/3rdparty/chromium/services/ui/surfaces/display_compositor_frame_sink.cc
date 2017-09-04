// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_compositor_frame_sink.h"

#include "cc/output/copy_output_request.h"
#include "cc/output/output_surface.h"
#include "cc/output/renderer_settings.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/frame_sink_id.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/surfaces/direct_output_surface.h"
#include "services/ui/surfaces/surfaces_context_provider.h"

#if defined(USE_OZONE)
#include "gpu/command_buffer/client/gles2_interface.h"
#include "services/ui/surfaces/direct_output_surface_ozone.h"
#endif

namespace ui {

DisplayCompositorFrameSink::DisplayCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gfx::AcceleratedWidget widget,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel,
    const scoped_refptr<DisplayCompositor>& display_compositor)
    : frame_sink_id_(frame_sink_id),
      task_runner_(task_runner),
      display_compositor_(display_compositor),
      factory_(frame_sink_id_, display_compositor->manager(), this) {
  display_compositor_->manager()->RegisterFrameSinkId(frame_sink_id_);
  display_compositor_->manager()->RegisterSurfaceFactoryClient(frame_sink_id_,
                                                               this);

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      gpu_channel->gpu_memory_buffer_manager();
  scoped_refptr<SurfacesContextProvider> surfaces_context_provider(
      new SurfacesContextProvider(widget, std::move(gpu_channel)));
  // TODO(rjkroege): If there is something better to do than CHECK, add it.
  CHECK(surfaces_context_provider->BindToCurrentThread());

  std::unique_ptr<cc::SyntheticBeginFrameSource> synthetic_begin_frame_source(
      new cc::DelayBasedBeginFrameSource(
          base::MakeUnique<cc::DelayBasedTimeSource>(task_runner_.get())));

  std::unique_ptr<cc::OutputSurface> display_output_surface;
  if (surfaces_context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE)
    display_output_surface = base::MakeUnique<DirectOutputSurfaceOzone>(
        surfaces_context_provider, widget, synthetic_begin_frame_source.get(),
        gpu_memory_buffer_manager, GL_TEXTURE_2D, GL_RGB);
#else
    NOTREACHED();
#endif
  } else {
    display_output_surface = base::MakeUnique<DirectOutputSurface>(
        surfaces_context_provider, synthetic_begin_frame_source.get());
  }

  int max_frames_pending =
      display_output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  std::unique_ptr<cc::DisplayScheduler> scheduler(
      new cc::DisplayScheduler(synthetic_begin_frame_source.get(),
                               task_runner_.get(), max_frames_pending));

  display_.reset(new cc::Display(
      nullptr /* bitmap_manager */, gpu_memory_buffer_manager,
      cc::RendererSettings(), std::move(synthetic_begin_frame_source),
      std::move(display_output_surface), std::move(scheduler),
      base::MakeUnique<cc::TextureMailboxDeleter>(task_runner_.get())));
  display_->Initialize(this, display_compositor_->manager(), frame_sink_id_);
  display_->SetVisible(true);
}

DisplayCompositorFrameSink::~DisplayCompositorFrameSink() {
  display_compositor_->manager()->UnregisterSurfaceFactoryClient(
      frame_sink_id_);
  display_compositor_->manager()->InvalidateFrameSinkId(frame_sink_id_);
}

void DisplayCompositorFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame,
    const base::Callback<void()>& callback) {
  gfx::Size frame_size =
      frame.delegated_frame_data->render_pass_list.back()->output_rect.size();
  if (frame_size.IsEmpty() || frame_size != display_size_) {
    if (!local_frame_id_.is_null())
      factory_.Destroy(local_frame_id_);
    local_frame_id_ = allocator_.GenerateId();
    factory_.Create(local_frame_id_);
    display_size_ = frame_size;
    display_->Resize(display_size_);
  }
  display_->SetSurfaceId(cc::SurfaceId(frame_sink_id_, local_frame_id_),
                         frame.metadata.device_scale_factor);
  factory_.SubmitCompositorFrame(local_frame_id_, std::move(frame), callback);
}

void DisplayCompositorFrameSink::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  // TODO(fsamuel): Implement this.
}

void DisplayCompositorFrameSink::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(fsamuel): Implement this.
}

void DisplayCompositorFrameSink::DisplayOutputSurfaceLost() {
  // TODO(fsamuel): This looks like it would crash if a frame was in flight and
  // will be submitted.
  display_.reset();
}

void DisplayCompositorFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    const cc::RenderPassList& render_passes) {
  // This notification is not relevant to our client outside of tests.
}

void DisplayCompositorFrameSink::DisplayDidDrawAndSwap() {
  // This notification is not relevant to our client outside of tests. We
  // unblock the client from the DrawCallback when the surface is going to
  // be drawn.
}

}  // namespace ui
