// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_FBO_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_FBO_MAC_H_

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/image_transport_surface.h"
#include "ui/gl/gl_bindings.h"

namespace content {

// We are backed by an offscreen surface for the purposes of creating
// a context, but use FBOs to render to texture. The texture may be backed by
// an IOSurface, or it may be presented to the screen via a CALayer, depending
// on the StorageProvider class specified.
class ImageTransportSurfaceFBO
    : public gfx::GLSurface,
      public ImageTransportSurface,
      public GpuCommandBufferStub::DestructionObserver {
 public:
  // The interface through which storage for the color buffer of the FBO is
  // allocated.
  class StorageProvider {
   public:
    virtual ~StorageProvider() {}
    // IOSurfaces cause too much address space fragmentation if they are
    // allocated on every resize. This gets a rounded size for allocation.
    virtual gfx::Size GetRoundedSize(gfx::Size size) = 0;

    // Allocate the storage for the color buffer. The specified context is
    // current, and there is a texture bound to GL_TEXTURE_RECTANGLE_ARB.
    virtual bool AllocateColorBufferStorage(
        CGLContextObj context, gfx::Size size) = 0;

    // Free the storage allocated in the AllocateColorBufferStorage call. The
    // GL texture that was bound has already been deleted by the caller.
    virtual void FreeColorBufferStorage() = 0;

    // Retrieve the handle for the surface to send to the browser process to
    // display.
    virtual uint64 GetSurfaceHandle() const = 0;
  };

  ImageTransportSurfaceFBO(StorageProvider* storage_provider,
                           GpuChannelManager* manager,
                           GpuCommandBufferStub* stub,
                           gfx::PluginWindowHandle handle);

  // GLSurface implementation
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool DeferDraws() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;
  virtual bool SupportsPostSubBuffer() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void* GetHandle() OVERRIDE;
  virtual void* GetDisplay() OVERRIDE;
  virtual bool OnMakeCurrent(gfx::GLContext* context) OVERRIDE;
  virtual unsigned int GetBackingFrameBufferObject() OVERRIDE;
  virtual bool SetBackbufferAllocation(bool allocated) OVERRIDE;
  virtual void SetFrontbufferAllocation(bool allocated) OVERRIDE;

 protected:
  // ImageTransportSurface implementation
  virtual void OnBufferPresented(
      const AcceleratedSurfaceMsg_BufferPresented_Params& params) OVERRIDE;
  virtual void OnResize(gfx::Size size, float scale_factor) OVERRIDE;
  virtual void SetLatencyInfo(
      const std::vector<ui::LatencyInfo>&) OVERRIDE;
  virtual void WakeUpGpu() OVERRIDE;

  // GpuCommandBufferStub::DestructionObserver implementation.
  virtual void OnWillDestroyStub() OVERRIDE;

 private:
  virtual ~ImageTransportSurfaceFBO() OVERRIDE;

  void AdjustBufferAllocation();
  void DestroyFramebuffer();
  void CreateFramebuffer();

  scoped_ptr<StorageProvider> storage_provider_;

  // Tracks the current buffer allocation state.
  bool backbuffer_suggested_allocation_;
  bool frontbuffer_suggested_allocation_;

  uint32 fbo_id_;
  GLuint texture_id_;
  GLuint depth_stencil_renderbuffer_id_;
  bool has_complete_framebuffer_;

  // Weak pointer to the context that this was last made current to.
  gfx::GLContext* context_;

  gfx::Size size_;
  gfx::Size rounded_size_;
  float scale_factor_;

  // Whether or not we've successfully made the surface current once.
  bool made_current_;

  // Whether a SwapBuffers is pending.
  bool is_swap_buffers_pending_;

  // Whether we unscheduled command buffer because of pending SwapBuffers.
  bool did_unschedule_;

  std::vector<ui::LatencyInfo> latency_info_;

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(ImageTransportSurfaceFBO);
};

}  // namespace content

#endif  //  CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_MAC_H_
