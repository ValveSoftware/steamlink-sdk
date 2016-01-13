// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "ui/gfx/transform.h"

namespace cc {
class ContextProvider;
class CompositorFrameMetadata;
}

namespace content {

class SynchronousCompositorClient;
class SynchronousCompositorOutputSurface;
class WebGraphicsContext3DCommandBufferImpl;

class SynchronousCompositorOutputSurfaceDelegate {
 public:
   virtual void DidBindOutputSurface(
      SynchronousCompositorOutputSurface* output_surface) = 0;
  virtual void DidDestroySynchronousOutputSurface(
      SynchronousCompositorOutputSurface* output_surface) = 0;
  virtual void SetContinuousInvalidate(bool enable) = 0;
  virtual void DidActivatePendingTree() = 0;

 protected:
  SynchronousCompositorOutputSurfaceDelegate() {}
  virtual ~SynchronousCompositorOutputSurfaceDelegate() {}
};

// Specialization of the output surface that adapts it to implement the
// content::SynchronousCompositor public API. This class effects an "inversion
// of control" - enabling drawing to be  orchestrated by the embedding
// layer, instead of driven by the compositor internals - hence it holds two
// 'client' pointers (|client_| in the OutputSurface baseclass and
// GetDelegate()) which represent the consumers of the two roles in plays.
// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when BindToClient is called.
class SynchronousCompositorOutputSurface
    : NON_EXPORTED_BASE(public cc::OutputSurface) {
 public:
  explicit SynchronousCompositorOutputSurface(int routing_id);
  virtual ~SynchronousCompositorOutputSurface();

  // OutputSurface.
  virtual bool ForcedDrawToSoftwareDevice() const OVERRIDE;
  virtual bool BindToClient(cc::OutputSurfaceClient* surface_client) OVERRIDE;
  virtual void Reshape(const gfx::Size& size, float scale_factor) OVERRIDE;
  virtual void SetNeedsBeginFrame(bool enable) OVERRIDE;
  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE;

  // Partial SynchronousCompositor API implementation.
  bool InitializeHwDraw(
      scoped_refptr<cc::ContextProvider> onscreen_context_provider);
  void ReleaseHwDraw();
  scoped_ptr<cc::CompositorFrame> DemandDrawHw(gfx::Size surface_size,
                                               const gfx::Transform& transform,
                                               gfx::Rect viewport,
                                               gfx::Rect clip);
  void ReturnResources(const cc::CompositorFrameAck& frame_ack);
  scoped_ptr<cc::CompositorFrame> DemandDrawSw(SkCanvas* canvas);
  void SetMemoryPolicy(const SynchronousCompositorMemoryPolicy& policy);

 private:
  class SoftwareDevice;
  friend class SoftwareDevice;

  void InvokeComposite(const gfx::Transform& transform,
                       gfx::Rect viewport,
                       gfx::Rect clip,
                       bool valid_for_tile_management);
  bool CalledOnValidThread() const;
  SynchronousCompositorOutputSurfaceDelegate* GetDelegate();

  int routing_id_;
  bool needs_begin_frame_;
  bool invoking_composite_;

  gfx::Transform cached_hw_transform_;
  gfx::Rect cached_hw_viewport_;
  gfx::Rect cached_hw_clip_;

  // Only valid (non-NULL) during a DemandDrawSw() call.
  SkCanvas* current_sw_canvas_;

  cc::ManagedMemoryPolicy memory_policy_;

  cc::OutputSurfaceClient* output_surface_client_;
  scoped_ptr<cc::CompositorFrame> frame_holder_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_IN_PROCESS_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_
