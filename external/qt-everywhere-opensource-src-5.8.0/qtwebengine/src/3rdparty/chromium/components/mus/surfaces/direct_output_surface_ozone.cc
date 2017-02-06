// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/direct_output_surface_ozone.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "cc/scheduler/begin_frame_source.h"
#include "components/display_compositor/buffer_queue.h"
#include "components/mus/common/gpu_service.h"
#include "components/mus/common/mojo_gpu_memory_buffer_manager.h"
#include "components/mus/gpu/mus_gpu_memory_buffer_manager.h"
#include "components/mus/surfaces/surfaces_context_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"

using display_compositor::BufferQueue;

namespace mus {

DirectOutputSurfaceOzone::DirectOutputSurfaceOzone(
    scoped_refptr<SurfacesContextProvider> context_provider,
    gfx::AcceleratedWidget widget,
    cc::SyntheticBeginFrameSource* synthetic_begin_frame_source,
    uint32_t target,
    uint32_t internalformat)
    : cc::OutputSurface(context_provider, nullptr, nullptr),
      gl_helper_(context_provider->ContextGL(),
                 context_provider->ContextSupport()),
      synthetic_begin_frame_source_(synthetic_begin_frame_source),
      weak_ptr_factory_(this) {
  if (!GpuService::UseChromeGpuCommandBuffer()) {
    ozone_gpu_memory_buffer_manager_.reset(new OzoneGpuMemoryBufferManager());
    buffer_queue_.reset(new BufferQueue(
        context_provider->ContextGL(), target, internalformat, &gl_helper_,
        ozone_gpu_memory_buffer_manager_.get(), widget));
  } else {
    buffer_queue_.reset(new BufferQueue(
        context_provider->ContextGL(), target, internalformat, &gl_helper_,
        MusGpuMemoryBufferManager::current(), widget));
  }

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

  buffer_queue_->Initialize();

  context_provider->SetSwapBuffersCompletionCallback(
      base::Bind(&DirectOutputSurfaceOzone::OnGpuSwapBuffersCompleted,
                 base::Unretained(this)));
}

DirectOutputSurfaceOzone::~DirectOutputSurfaceOzone() {
  // TODO(rjkroege): Support cleanup.
}

bool DirectOutputSurfaceOzone::IsDisplayedAsOverlayPlane() const {
  // TODO(rjkroege): implement remaining overlay functionality.
  return true;
}

unsigned DirectOutputSurfaceOzone::GetOverlayTextureId() const {
  DCHECK(buffer_queue_);
  return buffer_queue_->current_texture_id();
}

void DirectOutputSurfaceOzone::SwapBuffers(cc::CompositorFrame frame) {
  DCHECK(buffer_queue_);
  DCHECK(frame.gl_frame_data);

  buffer_queue_->SwapBuffers(frame.gl_frame_data->sub_buffer_rect);

  // Code combining GpuBrowserCompositorOutputSurface + DirectOutputSurface
  if (frame.gl_frame_data->sub_buffer_rect ==
      gfx::Rect(frame.gl_frame_data->size)) {
    context_provider_->ContextSupport()->Swap();
  } else {
    context_provider_->ContextSupport()->PartialSwapBuffers(
        frame.gl_frame_data->sub_buffer_rect);
  }

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  client_->DidSwapBuffers();
}

bool DirectOutputSurfaceOzone::BindToClient(cc::OutputSurfaceClient* client) {
  if (!cc::OutputSurface::BindToClient(client))
    return false;

  if (capabilities_.uses_default_gl_framebuffer) {
    capabilities_.flipped_output_surface =
        context_provider()->ContextCapabilities().flips_vertically;
  }
  return true;
}

void DirectOutputSurfaceOzone::OnUpdateVSyncParametersFromGpu(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK(HasClient());
  synthetic_begin_frame_source_->OnUpdateVSyncParameters(timebase, interval);
}

void DirectOutputSurfaceOzone::OnGpuSwapBuffersCompleted(
    gfx::SwapResult result) {
  DCHECK(buffer_queue_);
  bool force_swap = false;
  if (result == gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // Even through the swap failed, this is a fixable error so we can pretend
    // it succeeded to the rest of the system.
    result = gfx::SwapResult::SWAP_ACK;
    buffer_queue_->RecreateBuffers();
    force_swap = true;
  }

  buffer_queue_->PageFlipComplete();
  OnSwapBuffersComplete();

  if (force_swap)
    client_->SetNeedsRedrawRect(gfx::Rect(SurfaceSize()));
}

void DirectOutputSurfaceOzone::BindFramebuffer() {
  DCHECK(buffer_queue_);
  buffer_queue_->BindFramebuffer();
}

uint32_t DirectOutputSurfaceOzone::GetFramebufferCopyTextureFormat() {
  return buffer_queue_->internal_format();
}

// We call this on every frame but changing the size once we've allocated
// backing NativePixmapOzone instances will cause a DCHECK because
// Chrome never Reshape(s) after the first one from (0,0). NB: this implies
// that screen size changes need to be plumbed differently. In particular, we
// must create the native window in the size that the hardware reports.
void DirectOutputSurfaceOzone::Reshape(const gfx::Size& size,
                                       float scale_factor,
                                       const gfx::ColorSpace& color_space,
                                       bool alpha) {
  OutputSurface::Reshape(size, scale_factor, color_space, alpha);
  DCHECK(buffer_queue_);
  buffer_queue_->Reshape(SurfaceSize(), scale_factor);
}

}  // namespace mus
