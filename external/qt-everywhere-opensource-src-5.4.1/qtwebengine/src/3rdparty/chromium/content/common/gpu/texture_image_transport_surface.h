// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_TEXTURE_IMAGE_TRANSPORT_SURFACE_H_
#define CONTENT_COMMON_GPU_TEXTURE_IMAGE_TRANSPORT_SURFACE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/image_transport_surface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace content {
class GpuChannelManager;

class TextureImageTransportSurface
    : public ImageTransportSurface,
      public GpuCommandBufferStub::DestructionObserver,
      public gfx::GLSurface {
 public:
  TextureImageTransportSurface(GpuChannelManager* manager,
                               GpuCommandBufferStub* stub,
                               const gfx::GLSurfaceHandle& handle);

  // gfx::GLSurface implementation.
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool DeferDraws() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void* GetHandle() OVERRIDE;
  virtual unsigned GetFormat() OVERRIDE;
  virtual bool SupportsPostSubBuffer() OVERRIDE;
  virtual unsigned int GetBackingFrameBufferObject() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;
  virtual bool SetBackbufferAllocation(bool allocated) OVERRIDE;
  virtual void SetFrontbufferAllocation(bool allocated) OVERRIDE;
  virtual void* GetShareHandle() OVERRIDE;
  virtual void* GetDisplay() OVERRIDE;
  virtual void* GetConfig() OVERRIDE;

 protected:
  // ImageTransportSurface implementation.
  virtual void OnBufferPresented(
      const AcceleratedSurfaceMsg_BufferPresented_Params& params) OVERRIDE;
  virtual void OnResize(gfx::Size size, float scale_factor) OVERRIDE;
  virtual void SetLatencyInfo(
      const std::vector<ui::LatencyInfo>& latency_info) OVERRIDE;
  virtual void WakeUpGpu() OVERRIDE;

  // GpuCommandBufferStub::DestructionObserver implementation.
  virtual void OnWillDestroyStub() OVERRIDE;

 private:

  gfx::Size backbuffer_size() const {
    DCHECK(backbuffer_.get());
    GLsizei width = 0;
    GLsizei height = 0;
    backbuffer_->texture()->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height);
    return gfx::Size(width, height);
  }

  virtual ~TextureImageTransportSurface();
  void CreateBackTexture();
  void AttachBackTextureToFBO();
  void ReleaseBackTexture();
  void ReleaseFrontTexture();
  void BufferPresentedImpl(const gpu::Mailbox& mailbox_name);

  // The framebuffer that represents this surface (service id). Allocated lazily
  // in OnMakeCurrent.
  uint32 fbo_id_;

  // The current backbuffer.
  scoped_refptr<gpu::gles2::TextureRef> backbuffer_;
  scoped_refptr<gpu::gles2::TextureRef> frontbuffer_;

  // The mailbox name for the current backbuffer texture. Needs to be unique per
  // GL texture and is invalid while service_id is zero.
  gpu::Mailbox back_mailbox_;
  gpu::Mailbox front_mailbox_;

  // The current size of the GLSurface. Used to disambiguate from the current
  // texture size which might be outdated (since we use two buffers).
  gfx::Size current_size_;
  float scale_factor_;

  // Whether or not the command buffer stub has been destroyed.
  bool stub_destroyed_;

  bool backbuffer_suggested_allocation_;
  bool frontbuffer_suggested_allocation_;

  scoped_ptr<ImageTransportHelper> helper_;
  gfx::GLSurfaceHandle handle_;

  // The offscreen surface used to make the context current. However note that
  // the actual rendering is always redirected to an FBO.
  scoped_refptr<gfx::GLSurface> surface_;

  // Whether a SwapBuffers is pending.
  bool is_swap_buffers_pending_;

  // Whether we unscheduled command buffer because of pending SwapBuffers.
  bool did_unschedule_;

  // Holds a reference to the mailbox manager for cleanup.
  scoped_refptr<gpu::gles2::MailboxManager> mailbox_manager_;

  std::vector<ui::LatencyInfo> latency_info_;
  DISALLOW_COPY_AND_ASSIGN(TextureImageTransportSurface);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_TEXTURE_IMAGE_TRANSPORT_SURFACE_H_
