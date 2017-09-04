// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OUTPUT_SURFACE_H_
#define CC_OUTPUT_OUTPUT_SURFACE_H_

#include <deque>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/output/software_output_device.h"
#include "cc/output/vulkan_context_provider.h"
#include "cc/resources/returned_resource.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/color_space.h"

namespace gfx {
class ColorSpace;
class Size;
}

namespace cc {

class OutputSurfaceClient;
class OutputSurfaceFrame;

// This class represents a platform-independent API for presenting
// buffers to display via GPU or software compositing. Implementations
// can provide platform-specific behaviour.
class CC_EXPORT OutputSurface {
 public:
  struct Capabilities {
    Capabilities() = default;

    int max_frames_pending = 1;
    // Whether this output surface renders to the default OpenGL zero
    // framebuffer or to an offscreen framebuffer.
    bool uses_default_gl_framebuffer = true;
    // Whether this OutputSurface is flipped or not.
    bool flipped_output_surface = false;
  };

  // Constructor for GL-based compositing.
  explicit OutputSurface(scoped_refptr<ContextProvider> context_provider);
  // Constructor for software compositing.
  explicit OutputSurface(std::unique_ptr<SoftwareOutputDevice> software_device);
  // Constructor for Vulkan-based compositing.
  explicit OutputSurface(
      scoped_refptr<VulkanContextProvider> vulkan_context_provider);

  virtual ~OutputSurface();

  const Capabilities& capabilities() const { return capabilities_; }

  // Obtain the 3d context or the software device associated with this output
  // surface. Either of these may return a null pointer, but not both.
  // In the event of a lost context, the entire output surface should be
  // recreated.
  ContextProvider* context_provider() const { return context_provider_.get(); }
  VulkanContextProvider* vulkan_context_provider() const {
    return vulkan_context_provider_.get();
  }
  SoftwareOutputDevice* software_device() const {
    return software_device_.get();
  }

  virtual void BindToClient(OutputSurfaceClient* client) = 0;

  virtual void EnsureBackbuffer() = 0;
  virtual void DiscardBackbuffer() = 0;

  // Bind the default framebuffer for drawing to, only valid for GL backed
  // OutputSurfaces.
  virtual void BindFramebuffer() = 0;

  // Get the class capable of informing cc of hardware overlay capability.
  virtual OverlayCandidateValidator* GetOverlayCandidateValidator() const = 0;

  // Returns true if a main image overlay plane should be scheduled.
  virtual bool IsDisplayedAsOverlayPlane() const = 0;

  // Get the texture for the main image's overlay.
  virtual unsigned GetOverlayTextureId() const = 0;

  // If this returns true, then the surface will not attempt to draw.
  virtual bool SurfaceIsSuspendForRecycle() const = 0;

  virtual void Reshape(const gfx::Size& size,
                       float device_scale_factor,
                       const gfx::ColorSpace& color_space,
                       bool has_alpha) = 0;

  virtual bool HasExternalStencilTest() const = 0;
  virtual void ApplyExternalStencil() = 0;

  // Gives the GL internal format that should be used for calling CopyTexImage2D
  // when the framebuffer is bound via BindFramebuffer().
  virtual uint32_t GetFramebufferCopyTextureFormat() = 0;

  // Swaps the current backbuffer to the screen. For successful swaps, the
  // implementation must call OutputSurfaceClient::DidReceiveSwapBuffersAck()
  // after returning from this method in order to unblock the next frame.
  virtual void SwapBuffers(OutputSurfaceFrame frame) = 0;

 protected:
  struct OutputSurface::Capabilities capabilities_;
  scoped_refptr<ContextProvider> context_provider_;
  scoped_refptr<VulkanContextProvider> vulkan_context_provider_;
  std::unique_ptr<SoftwareOutputDevice> software_device_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OutputSurface);
};

}  // namespace cc

#endif  // CC_OUTPUT_OUTPUT_SURFACE_H_
