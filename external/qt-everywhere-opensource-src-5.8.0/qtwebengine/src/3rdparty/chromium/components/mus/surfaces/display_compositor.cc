// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/display_compositor.h"

#include "cc/output/copy_output_request.h"
#include "cc/output/output_surface.h"
#include "cc/output/renderer_settings.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "components/mus/surfaces/direct_output_surface.h"
#include "components/mus/surfaces/surfaces_context_provider.h"

#if defined(USE_OZONE)
#include "components/mus/surfaces/direct_output_surface_ozone.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#endif

namespace mus {

DisplayCompositor::DisplayCompositor(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gfx::AcceleratedWidget widget,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<SurfacesState>& surfaces_state)
    : task_runner_(task_runner),
      surfaces_state_(surfaces_state),
      factory_(surfaces_state->manager(), this),
      allocator_(surfaces_state->next_id_namespace()) {
  allocator_.RegisterSurfaceIdNamespace(surfaces_state_->manager());
  surfaces_state_->manager()->RegisterSurfaceFactoryClient(
      allocator_.id_namespace(), this);

  scoped_refptr<SurfacesContextProvider> surfaces_context_provider(
      new SurfacesContextProvider(widget, gpu_state));
  // TODO(rjkroege): If there is something better to do than CHECK, add it.
  CHECK(surfaces_context_provider->BindToCurrentThread());

  std::unique_ptr<cc::SyntheticBeginFrameSource> synthetic_begin_frame_source(
      new cc::DelayBasedBeginFrameSource(
          base::MakeUnique<cc::DelayBasedTimeSource>(task_runner_.get())));

  std::unique_ptr<cc::OutputSurface> display_output_surface;
  if (surfaces_context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE)
    display_output_surface = base::WrapUnique(new DirectOutputSurfaceOzone(
        surfaces_context_provider, widget, synthetic_begin_frame_source.get(),
        GL_TEXTURE_2D, GL_RGB));
#else
    NOTREACHED();
#endif
  } else {
    display_output_surface = base::WrapUnique(new DirectOutputSurface(
        surfaces_context_provider, synthetic_begin_frame_source.get()));
  }

  int max_frames_pending =
      display_output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  std::unique_ptr<cc::DisplayScheduler> scheduler(
      new cc::DisplayScheduler(synthetic_begin_frame_source.get(),
                               task_runner_.get(), max_frames_pending));

  display_.reset(new cc::Display(
      surfaces_state_->manager(), nullptr /* bitmap_manager */,
      nullptr /* gpu_memory_buffer_manager */, cc::RendererSettings(),
      allocator_.id_namespace(), std::move(synthetic_begin_frame_source),
      std::move(display_output_surface), std::move(scheduler),
      base::MakeUnique<cc::TextureMailboxDeleter>(task_runner_.get())));
  display_->Initialize(this);
}

DisplayCompositor::~DisplayCompositor() {
  surfaces_state_->manager()->UnregisterSurfaceFactoryClient(
      allocator_.id_namespace());
}

void DisplayCompositor::SubmitCompositorFrame(
    cc::CompositorFrame frame,
    const base::Callback<void(cc::SurfaceDrawStatus)>& callback) {
  gfx::Size frame_size =
      frame.delegated_frame_data->render_pass_list.back()->output_rect.size();
  if (frame_size.IsEmpty() || frame_size != display_size_) {
    if (!surface_id_.is_null())
      factory_.Destroy(surface_id_);
    surface_id_ = allocator_.GenerateId();
    factory_.Create(surface_id_);
    display_size_ = frame_size;
    display_->Resize(display_size_);
  }
  display_->SetSurfaceId(surface_id_, frame.metadata.device_scale_factor);
  factory_.SubmitCompositorFrame(surface_id_, std::move(frame), callback);
}

void DisplayCompositor::RequestCopyOfOutput(
    std::unique_ptr<cc::CopyOutputRequest> output_request) {
  factory_.RequestCopyOfSurface(surface_id_, std::move(output_request));
}

void DisplayCompositor::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  // TODO(fsamuel): Implement this.
}

void DisplayCompositor::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(fsamuel): Implement this.
}

void DisplayCompositor::DisplayOutputSurfaceLost() {
  // TODO(fsamuel): This looks like it would crash if a frame was in flight and
  // will be submitted.
  display_.reset();
}

void DisplayCompositor::DisplaySetMemoryPolicy(
    const cc::ManagedMemoryPolicy& policy) {}

}  // namespace mus
