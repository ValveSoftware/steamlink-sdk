// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"

#include <utility>

#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface_client.h"
#include "components/display_compositor/buffer_queue.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "components/display_compositor/gl_helper.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace content {

GpuSurfacelessBrowserCompositorOutputSurface::
    GpuSurfacelessBrowserCompositorOutputSurface(
        scoped_refptr<ContextProviderCommandBuffer> context,
        gpu::SurfaceHandle surface_handle,
        scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
        cc::SyntheticBeginFrameSource* begin_frame_source,
        std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
            overlay_candidate_validator,
        unsigned int target,
        unsigned int internalformat,
        gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : GpuBrowserCompositorOutputSurface(std::move(context),
                                        std::move(vsync_manager),
                                        begin_frame_source,
                                        std::move(overlay_candidate_validator)),
      internalformat_(internalformat),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.flipped_output_surface = true;
  // Set |max_frames_pending| to 2 for surfaceless, which aligns scheduling
  // more closely with the previous surfaced behavior.
  // With a surface, swap buffer ack used to return early, before actually
  // presenting the back buffer, enabling the browser compositor to run ahead.
  // Surfaceless implementation acks at the time of actual buffer swap, which
  // shifts the start of the new frame forward relative to the old
  // implementation.
  capabilities_.max_frames_pending = 2;

  gl_helper_.reset(new display_compositor::GLHelper(
      context_provider_->ContextGL(), context_provider_->ContextSupport()));
  buffer_queue_.reset(new display_compositor::BufferQueue(
      context_provider_->ContextGL(), target, internalformat_, gl_helper_.get(),
      gpu_memory_buffer_manager_, surface_handle));
  buffer_queue_->Initialize();
}

GpuSurfacelessBrowserCompositorOutputSurface::
    ~GpuSurfacelessBrowserCompositorOutputSurface() {
}

bool GpuSurfacelessBrowserCompositorOutputSurface::IsDisplayedAsOverlayPlane()
    const {
  return true;
}

unsigned GpuSurfacelessBrowserCompositorOutputSurface::GetOverlayTextureId()
    const {
  return buffer_queue_->current_texture_id();
}

void GpuSurfacelessBrowserCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame frame) {
  DCHECK(buffer_queue_);
  buffer_queue_->SwapBuffers(frame.gl_frame_data->sub_buffer_rect);
  GpuBrowserCompositorOutputSurface::SwapBuffers(std::move(frame));
}

void GpuSurfacelessBrowserCompositorOutputSurface::OnSwapBuffersComplete() {
  DCHECK(buffer_queue_);
  buffer_queue_->PageFlipComplete();
  GpuBrowserCompositorOutputSurface::OnSwapBuffersComplete();
}

void GpuSurfacelessBrowserCompositorOutputSurface::BindFramebuffer() {
  DCHECK(buffer_queue_);
  buffer_queue_->BindFramebuffer();
}

GLenum GpuSurfacelessBrowserCompositorOutputSurface::
    GetFramebufferCopyTextureFormat() {
  return buffer_queue_->internal_format();
}

void GpuSurfacelessBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float scale_factor,
    const gfx::ColorSpace& color_space,
    bool alpha) {
  GpuBrowserCompositorOutputSurface::Reshape(size, scale_factor, color_space,
                                             alpha);
  DCHECK(buffer_queue_);
  // TODO(ccameron): Plumb the color profile to the output GpuMemoryBuffer.
  // https://crbug.com/622133
  buffer_queue_->Reshape(SurfaceSize(), scale_factor);
}

void GpuSurfacelessBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result,
    const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) {
  bool force_swap = false;
  if (result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // Even through the swap failed, this is a fixable error so we can pretend
    // it succeeded to the rest of the system.
    result = gfx::SwapResult::SWAP_ACK;
    buffer_queue_->RecreateBuffers();
    force_swap = true;
  }
  GpuBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
      latency_info, result, params_mac);
  if (force_swap)
    client_->SetNeedsRedrawRect(gfx::Rect(SurfaceSize()));
}

}  // namespace content
