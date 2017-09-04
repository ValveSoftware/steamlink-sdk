// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"

#include <utility>

#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
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
        gfx::BufferFormat format,
        gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : GpuBrowserCompositorOutputSurface(std::move(context),
                                        std::move(vsync_manager),
                                        begin_frame_source,
                                        std::move(overlay_candidate_validator)),
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
      context_provider_->ContextGL(), target, internalformat, format,
      gl_helper_.get(), gpu_memory_buffer_manager_, surface_handle));
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
    cc::OutputSurfaceFrame frame) {
  DCHECK(buffer_queue_);
  DCHECK(reshape_size_ == frame.size);
  // TODO(ccameron): What if a swap comes again before OnGpuSwapBuffersCompleted
  // happens, we'd see the wrong swap size there?
  swap_size_ = reshape_size_;
  buffer_queue_->SwapBuffers(frame.sub_buffer_rect);
  GpuBrowserCompositorOutputSurface::SwapBuffers(std::move(frame));
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
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha) {
  reshape_size_ = size;
  GpuBrowserCompositorOutputSurface::Reshape(size, device_scale_factor,
                                             color_space, has_alpha);
  DCHECK(buffer_queue_);
  buffer_queue_->Reshape(size, device_scale_factor, color_space);
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
  buffer_queue_->PageFlipComplete();
  GpuBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
      latency_info, result, params_mac);
  if (force_swap)
    client_->SetNeedsRedrawRect(gfx::Rect(swap_size_));
}

}  // namespace content
