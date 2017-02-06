// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OUTPUT_SURFACE_H_
#define CC_OUTPUT_OUTPUT_SURFACE_H_

#include <deque>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/output/software_output_device.h"
#include "cc/output/vulkan_context_provider.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"

namespace base { class SingleThreadTaskRunner; }

namespace ui {
class LatencyInfo;
}

namespace gfx {
class ColorSpace;
class Rect;
class Size;
class Transform;
}

namespace cc {

class CompositorFrame;
class CompositorFrameAck;
struct ManagedMemoryPolicy;
class OutputSurfaceClient;

// Represents the output surface for a compositor. The compositor owns
// and manages its destruction. Its lifetime is:
//   1. Created on the main thread by the LayerTreeHost through its client.
//   2. Passed to the compositor thread and bound to a client via BindToClient.
//      From here on, it will only be used on the compositor thread.
//   3. If the 3D context is lost, then the compositor will delete the output
//      surface (on the compositor thread) and go back to step 1.
class CC_EXPORT OutputSurface : public base::trace_event::MemoryDumpProvider {
 public:
  // Constructor for GL-based and/or software compositing.
  explicit OutputSurface(scoped_refptr<ContextProvider> context_provider,
                         scoped_refptr<ContextProvider> worker_context_provider,
                         std::unique_ptr<SoftwareOutputDevice> software_device);

  // Constructor for Vulkan-based compositing.
  explicit OutputSurface(
      scoped_refptr<VulkanContextProvider> vulkan_context_provider);

  ~OutputSurface() override;

  struct Capabilities {
    Capabilities()
        : delegated_rendering(false),
          max_frames_pending(1),
          adjust_deadline_for_parent(true),
          uses_default_gl_framebuffer(true),
          flipped_output_surface(false),
          can_force_reclaim_resources(false),
          delegated_sync_points_required(true) {}
    bool delegated_rendering;
    int max_frames_pending;
    // This doesn't handle the <webview> case, but once BeginFrame is
    // supported natively, we shouldn't need adjust_deadline_for_parent.
    bool adjust_deadline_for_parent;
    // Whether this output surface renders to the default OpenGL zero
    // framebuffer or to an offscreen framebuffer.
    bool uses_default_gl_framebuffer;
    // Whether this OutputSurface is flipped or not.
    bool flipped_output_surface;
    // Whether ForceReclaimResources can be called to reclaim all resources
    // from the OutputSurface.
    bool can_force_reclaim_resources;
    // True if sync points for resources are needed when swapping delegated
    // frames.
    bool delegated_sync_points_required;
  };

  const Capabilities& capabilities() const {
    return capabilities_;
  }

  virtual bool HasExternalStencilTest() const;
  virtual void ApplyExternalStencil();

  // Obtain the 3d context or the software device associated with this output
  // surface. Either of these may return a null pointer, but not both.
  // In the event of a lost context, the entire output surface should be
  // recreated.
  ContextProvider* context_provider() const { return context_provider_.get(); }
  ContextProvider* worker_context_provider() const {
    return worker_context_provider_.get();
  }
  VulkanContextProvider* vulkan_context_provider() const {
    return vulkan_context_provider_.get();
  }
  SoftwareOutputDevice* software_device() const {
    return software_device_.get();
  }

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be initialized, since from
  // this point to when DetachFromClient() is called the output surface will
  // only be used on the compositor thread.
  // The caller should call DetachFromClient() on the same thread before
  // destroying the OutputSurface, even if this fails. And BindToClient should
  // not be called twice for a given OutputSurface.
  virtual bool BindToClient(OutputSurfaceClient* client);

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be uninitialized.
  virtual void DetachFromClient();

  virtual void EnsureBackbuffer();
  virtual void DiscardBackbuffer();

  virtual void Reshape(const gfx::Size& size,
                       float scale_factor,
                       const gfx::ColorSpace& color_space,
                       bool alpha);
  gfx::Size SurfaceSize() const { return surface_size_; }
  float device_scale_factor() const { return device_scale_factor_; }

  // If supported, this causes a ReclaimResources for all resources that are
  // currently in use.
  virtual void ForceReclaimResources() {}

  virtual void BindFramebuffer();
  // Gives the GL internal format that should be used for calling CopyTexImage2D
  // when the framebuffer is bound via BindFramebuffer().
  virtual uint32_t GetFramebufferCopyTextureFormat() = 0;

  // The implementation may destroy or steal the contents of the CompositorFrame
  // passed in (though it will not take ownership of the CompositorFrame
  // itself). For successful swaps, the implementation must call
  // OutputSurfaceClient::DidSwapBuffers() and eventually
  // DidSwapBuffersComplete().
  virtual void SwapBuffers(CompositorFrame frame) = 0;
  virtual void OnSwapBuffersComplete();

  // Called by subclasses after receiving a response from the gpu process to a
  // query about whether a given set of textures is still in use by the OS
  // compositor.
  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses);

  bool HasClient() { return !!client_; }

  // Get the class capable of informing cc of hardware overlay capability.
  virtual OverlayCandidateValidator* GetOverlayCandidateValidator() const;

  // Returns true if a main image overlay plane should be scheduled.
  virtual bool IsDisplayedAsOverlayPlane() const;

  // Get the texture for the main image's overlay.
  virtual unsigned GetOverlayTextureId() const;

  virtual void DidLoseOutputSurface();
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy);

  // Support for a pull-model where draws are requested by the output surface.
  //
  // OutputSurface::Invalidate is called by the compositor to notify that
  // there's new content.
  virtual void Invalidate() {}

  // Updates the worker context provider's visibility, freeing GPU resources if
  // appropriate.
  virtual void SetWorkerContextShouldAggressivelyFreeResources(bool is_visible);

  // If this returns true, then the surface will not attempt to draw.
  virtual bool SurfaceIsSuspendForRecycle() const;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 protected:
  void PostSwapBuffersComplete();

  OutputSurfaceClient* client_ = nullptr;

  struct OutputSurface::Capabilities capabilities_;
  scoped_refptr<ContextProvider> context_provider_;
  scoped_refptr<ContextProvider> worker_context_provider_;
  scoped_refptr<VulkanContextProvider> vulkan_context_provider_;
  std::unique_ptr<SoftwareOutputDevice> software_device_;
  gfx::Size surface_size_;
  float device_scale_factor_ = -1;
  bool has_alpha_ = true;
  base::ThreadChecker client_thread_checker_;

  void SetNeedsRedrawRect(const gfx::Rect& damage_rect);
  void ReclaimResources(const CompositorFrameAck* ack);
  void SetExternalStencilTest(bool enabled);
  void DetachFromClientInternal();

 private:
  bool external_stencil_test_enabled_ = false;

  base::WeakPtrFactory<OutputSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurface);
};

}  // namespace cc

#endif  // CC_OUTPUT_OUTPUT_SURFACE_H_
