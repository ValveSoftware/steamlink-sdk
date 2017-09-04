// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/offscreen_browser_compositor_output_surface.h"

#include <utility>

#include "base/logging.h"
#include "build/build_config.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
#include "cc/resources/resource_provider.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "content/browser/compositor/reflector_impl.h"
#include "content/browser/compositor/reflector_texture.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using gpu::gles2::GLES2Interface;

namespace content {

static cc::ResourceFormat kFboTextureFormat = cc::RGBA_8888;

OffscreenBrowserCompositorOutputSurface::
    OffscreenBrowserCompositorOutputSurface(
        scoped_refptr<ContextProviderCommandBuffer> context,
        scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
        cc::SyntheticBeginFrameSource* begin_frame_source,
        std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
            overlay_candidate_validator)
    : BrowserCompositorOutputSurface(std::move(context),
                                     std::move(vsync_manager),
                                     begin_frame_source,
                                     std::move(overlay_candidate_validator)),
      weak_ptr_factory_(this) {
  capabilities_.uses_default_gl_framebuffer = false;
}

OffscreenBrowserCompositorOutputSurface::
    ~OffscreenBrowserCompositorOutputSurface() {
  DiscardBackbuffer();
}

void OffscreenBrowserCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void OffscreenBrowserCompositorOutputSurface::EnsureBackbuffer() {
  bool update_source_texture = !reflector_texture_ || reflector_changed_;
  reflector_changed_ = false;
  if (!reflector_texture_) {
    reflector_texture_.reset(new ReflectorTexture(context_provider()));

    GLES2Interface* gl = context_provider_->ContextGL();

    const int max_texture_size =
        context_provider_->ContextCapabilities().max_texture_size;
    int texture_width = std::min(max_texture_size, reshape_size_.width());
    int texture_height = std::min(max_texture_size, reshape_size_.height());

    gl->BindTexture(GL_TEXTURE_2D, reflector_texture_->texture_id());
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GLInternalFormat(kFboTextureFormat),
                   texture_width, texture_height, 0,
                   GLDataFormat(kFboTextureFormat),
                   GLDataType(kFboTextureFormat), nullptr);
    if (!fbo_)
      gl->GenFramebuffers(1, &fbo_);

    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, reflector_texture_->texture_id(),
                             0);
  }

  // The reflector may be created later or detached and re-attached,
  // so don't assume it always exists. For example, ChromeOS always
  // creates a reflector asynchronosly when creating this for software
  // mirroring.  See |DisplayManager::CreateMirrorWindowAsyncIfAny|.
  if (reflector_ && update_source_texture)
    reflector_->OnSourceTextureMailboxUpdated(reflector_texture_->mailbox());
}

void OffscreenBrowserCompositorOutputSurface::DiscardBackbuffer() {
  GLES2Interface* gl = context_provider_->ContextGL();

  if (reflector_texture_) {
    reflector_texture_.reset();
    if (reflector_)
      reflector_->OnSourceTextureMailboxUpdated(nullptr);
  }

  if (fbo_) {
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl->DeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }
}

void OffscreenBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float scale_factor,
    const gfx::ColorSpace& color_space,
    bool alpha) {
  reshape_size_ = size;
  DiscardBackbuffer();
  EnsureBackbuffer();
}

void OffscreenBrowserCompositorOutputSurface::BindFramebuffer() {
  bool need_to_bind = !!reflector_texture_.get();

  EnsureBackbuffer();
  DCHECK(reflector_texture_.get());
  DCHECK(fbo_);

  if (need_to_bind) {
    GLES2Interface* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  }
}

void OffscreenBrowserCompositorOutputSurface::SwapBuffers(
    cc::OutputSurfaceFrame frame) {
  gfx::Size surface_size = frame.size;
  DCHECK(surface_size == reshape_size_);
  gfx::Rect swap_rect = frame.sub_buffer_rect;

  if (reflector_) {
    if (swap_rect == gfx::Rect(surface_size))
      reflector_->OnSourceSwapBuffers(surface_size);
    else
      reflector_->OnSourcePostSubBuffer(swap_rect, surface_size);
  }

  // TODO(oshima): sync with the reflector's SwapBuffersComplete
  // (crbug.com/520567).
  // The original implementation had a flickering issue (crbug.com/515332).
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();

  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());
  context_provider_->ContextSupport()->SignalSyncToken(
      sync_token,
      base::Bind(
          &OffscreenBrowserCompositorOutputSurface::OnSwapBuffersComplete,
          weak_ptr_factory_.GetWeakPtr()));
}

bool OffscreenBrowserCompositorOutputSurface::IsDisplayedAsOverlayPlane()
    const {
  return false;
}

unsigned OffscreenBrowserCompositorOutputSurface::GetOverlayTextureId() const {
  return 0;
}

bool OffscreenBrowserCompositorOutputSurface::SurfaceIsSuspendForRecycle()
    const {
  return false;
}

GLenum
OffscreenBrowserCompositorOutputSurface::GetFramebufferCopyTextureFormat() {
  return GLCopyTextureInternalFormat(kFboTextureFormat);
}

void OffscreenBrowserCompositorOutputSurface::OnReflectorChanged() {
  if (reflector_) {
    reflector_changed_ = true;
    EnsureBackbuffer();
  }
}

void OffscreenBrowserCompositorOutputSurface::OnSwapBuffersComplete() {
  client_->DidReceiveSwapBuffersAck();
}

}  // namespace content
