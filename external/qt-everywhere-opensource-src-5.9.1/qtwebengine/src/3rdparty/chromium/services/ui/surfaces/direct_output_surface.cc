// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/direct_output_surface.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
#include "cc/scheduler/begin_frame_source.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace ui {

DirectOutputSurface::DirectOutputSurface(
    scoped_refptr<SurfacesContextProvider> context_provider,
    cc::SyntheticBeginFrameSource* synthetic_begin_frame_source)
    : cc::OutputSurface(context_provider),
      synthetic_begin_frame_source_(synthetic_begin_frame_source),
      weak_ptr_factory_(this) {
  capabilities_.flipped_output_surface =
      context_provider->ContextCapabilities().flips_vertically;
  context_provider->SetDelegate(this);
}

DirectOutputSurface::~DirectOutputSurface() {}

void DirectOutputSurface::BindToClient(cc::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void DirectOutputSurface::EnsureBackbuffer() {}

void DirectOutputSurface::DiscardBackbuffer() {
  context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
}

void DirectOutputSurface::BindFramebuffer() {
  context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DirectOutputSurface::Reshape(const gfx::Size& size,
                                  float device_scale_factor,
                                  const gfx::ColorSpace& color_space,
                                  bool has_alpha) {
  context_provider()->ContextGL()->ResizeCHROMIUM(
      size.width(), size.height(), device_scale_factor, has_alpha);
}

void DirectOutputSurface::SwapBuffers(cc::OutputSurfaceFrame frame) {
  DCHECK(context_provider_);
  if (frame.sub_buffer_rect == gfx::Rect(frame.size)) {
    context_provider_->ContextSupport()->Swap();
  } else {
    context_provider_->ContextSupport()->PartialSwapBuffers(
        frame.sub_buffer_rect);
  }

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

  context_provider_->ContextSupport()->SignalSyncToken(
      sync_token, base::Bind(&DirectOutputSurface::OnSwapBuffersComplete,
                             weak_ptr_factory_.GetWeakPtr()));
}

uint32_t DirectOutputSurface::GetFramebufferCopyTextureFormat() {
  // TODO(danakj): What attributes are used for the default framebuffer here?
  // Can it have alpha? SurfacesContextProvider doesn't take any attributes.
  return GL_RGB;
}

cc::OverlayCandidateValidator*
DirectOutputSurface::GetOverlayCandidateValidator() const {
  return nullptr;
}

bool DirectOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned DirectOutputSurface::GetOverlayTextureId() const {
  return 0;
}

bool DirectOutputSurface::SurfaceIsSuspendForRecycle() const {
  return false;
}

bool DirectOutputSurface::HasExternalStencilTest() const {
  return false;
}

void DirectOutputSurface::ApplyExternalStencil() {}

void DirectOutputSurface::OnVSyncParametersUpdated(
    const base::TimeTicks& timebase,
    const base::TimeDelta& interval) {
  // TODO(brianderson): We should not be receiving 0 intervals.
  synthetic_begin_frame_source_->OnUpdateVSyncParameters(
      timebase,
      interval.is_zero() ? cc::BeginFrameArgs::DefaultInterval() : interval);
}

void DirectOutputSurface::OnSwapBuffersComplete() {
  client_->DidReceiveSwapBuffersAck();
}

}  // namespace ui
