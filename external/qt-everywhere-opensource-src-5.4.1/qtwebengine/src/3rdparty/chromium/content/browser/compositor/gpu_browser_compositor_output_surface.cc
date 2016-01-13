// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"

#include "cc/output/compositor_frame.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace content {

GpuBrowserCompositorOutputSurface::GpuBrowserCompositorOutputSurface(
    const scoped_refptr<ContextProviderCommandBuffer>& context,
    int surface_id,
    IDMap<BrowserCompositorOutputSurface>* output_surface_map,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
    scoped_ptr<cc::OverlayCandidateValidator> overlay_candidate_validator)
    : BrowserCompositorOutputSurface(context,
                                     surface_id,
                                     output_surface_map,
                                     vsync_manager) {
  overlay_candidate_validator_ = overlay_candidate_validator.Pass();
}

GpuBrowserCompositorOutputSurface::~GpuBrowserCompositorOutputSurface() {}

void GpuBrowserCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  DCHECK(frame->gl_frame_data);

  ContextProviderCommandBuffer* provider_command_buffer =
      static_cast<ContextProviderCommandBuffer*>(context_provider_.get());
  CommandBufferProxyImpl* command_buffer_proxy =
      provider_command_buffer->GetCommandBufferProxy();
  DCHECK(command_buffer_proxy);
  context_provider_->ContextGL()->ShallowFlushCHROMIUM();
  command_buffer_proxy->SetLatencyInfo(frame->metadata.latency_info);

  if (reflector_.get()) {
    if (frame->gl_frame_data->sub_buffer_rect ==
        gfx::Rect(frame->gl_frame_data->size))
      reflector_->OnSwapBuffers();
    else
      reflector_->OnPostSubBuffer(frame->gl_frame_data->sub_buffer_rect);
  }

  OutputSurface::SwapBuffers(frame);
}

}  // namespace content
