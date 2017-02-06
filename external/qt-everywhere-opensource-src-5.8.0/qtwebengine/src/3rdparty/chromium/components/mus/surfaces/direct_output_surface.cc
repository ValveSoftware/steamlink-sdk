// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/direct_output_surface.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "cc/scheduler/begin_frame_source.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace mus {

DirectOutputSurface::DirectOutputSurface(
    scoped_refptr<SurfacesContextProvider> context_provider,
    cc::SyntheticBeginFrameSource* synthetic_begin_frame_source)
    : cc::OutputSurface(context_provider, nullptr, nullptr),
      synthetic_begin_frame_source_(synthetic_begin_frame_source),
      weak_ptr_factory_(this) {
  context_provider->SetDelegate(this);
}

DirectOutputSurface::~DirectOutputSurface() = default;

bool DirectOutputSurface::BindToClient(cc::OutputSurfaceClient* client) {
  if (!cc::OutputSurface::BindToClient(client))
    return false;

  if (capabilities_.uses_default_gl_framebuffer) {
    capabilities_.flipped_output_surface =
        context_provider()->ContextCapabilities().flips_vertically;
  }
  return true;
}

void DirectOutputSurface::OnVSyncParametersUpdated(
    const base::TimeTicks& timebase,
    const base::TimeDelta& interval) {
  // TODO(brianderson): We should not be receiving 0 intervals.
  synthetic_begin_frame_source_->OnUpdateVSyncParameters(
      timebase,
      interval.is_zero() ? cc::BeginFrameArgs::DefaultInterval() : interval);
}

void DirectOutputSurface::SwapBuffers(cc::CompositorFrame frame) {
  DCHECK(context_provider_);
  DCHECK(frame.gl_frame_data);
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

  context_provider_->ContextSupport()->SignalSyncToken(
      sync_token, base::Bind(&OutputSurface::OnSwapBuffersComplete,
                             weak_ptr_factory_.GetWeakPtr()));
  client_->DidSwapBuffers();
}

uint32_t DirectOutputSurface::GetFramebufferCopyTextureFormat() {
  // TODO(danakj): What attributes are used for the default framebuffer here?
  // Can it have alpha? SurfacesContextProvider doesn't take any attributes.
  return GL_RGB;
}

}  // namespace mus
