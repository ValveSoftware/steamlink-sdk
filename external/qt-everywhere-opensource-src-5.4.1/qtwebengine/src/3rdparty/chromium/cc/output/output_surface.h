// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_OUTPUT_SURFACE_H_
#define CC_OUTPUT_OUTPUT_SURFACE_H_

#include <deque>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/rolling_time_delta_history.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/context_provider.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/output/software_output_device.h"

namespace base { class SingleThreadTaskRunner; }

namespace ui { struct LatencyInfo; }

namespace gfx {
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
class CC_EXPORT OutputSurface {
 public:
  enum {
    DEFAULT_MAX_FRAMES_PENDING = 2
  };

  explicit OutputSurface(scoped_refptr<ContextProvider> context_provider);

  explicit OutputSurface(scoped_ptr<SoftwareOutputDevice> software_device);

  OutputSurface(scoped_refptr<ContextProvider> context_provider,
                scoped_ptr<SoftwareOutputDevice> software_device);

  virtual ~OutputSurface();

  struct Capabilities {
    Capabilities()
        : delegated_rendering(false),
          max_frames_pending(0),
          deferred_gl_initialization(false),
          draw_and_swap_full_viewport_every_frame(false),
          adjust_deadline_for_parent(true),
          uses_default_gl_framebuffer(true) {}
    bool delegated_rendering;
    int max_frames_pending;
    bool deferred_gl_initialization;
    bool draw_and_swap_full_viewport_every_frame;
    // This doesn't handle the <webview> case, but once BeginFrame is
    // supported natively, we shouldn't need adjust_deadline_for_parent.
    bool adjust_deadline_for_parent;
    // Whether this output surface renders to the default OpenGL zero
    // framebuffer or to an offscreen framebuffer.
    bool uses_default_gl_framebuffer;
  };

  const Capabilities& capabilities() const {
    return capabilities_;
  }

  virtual bool HasExternalStencilTest() const;

  // Obtain the 3d context or the software device associated with this output
  // surface. Either of these may return a null pointer, but not both.
  // In the event of a lost context, the entire output surface should be
  // recreated.
  scoped_refptr<ContextProvider> context_provider() const {
    return context_provider_.get();
  }
  SoftwareOutputDevice* software_device() const {
    return software_device_.get();
  }

  // In the case where both the context3d and software_device are present
  // (namely Android WebView), this is called to determine whether the software
  // device should be used on the current frame.
  virtual bool ForcedDrawToSoftwareDevice() const;

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be initialized, since from
  // this point on the output surface will only be used on the compositor
  // thread.
  virtual bool BindToClient(OutputSurfaceClient* client);

  // This is called by the compositor on the compositor thread inside ReleaseGL
  // in order to release the ContextProvider. Only used with
  // deferred_gl_initialization capability.
  void ReleaseContextProvider();

  // Enable or disable vsync.
  void SetThrottleFrameProduction(bool enable);

  virtual void EnsureBackbuffer();
  virtual void DiscardBackbuffer();

  virtual void Reshape(const gfx::Size& size, float scale_factor);
  virtual gfx::Size SurfaceSize() const;

  virtual void BindFramebuffer();

  // The implementation may destroy or steal the contents of the CompositorFrame
  // passed in (though it will not take ownership of the CompositorFrame
  // itself).
  virtual void SwapBuffers(CompositorFrame* frame);
  virtual void OnSwapBuffersComplete();

  // Notifies frame-rate smoothness preference. If true, all non-critical
  // processing should be stopped, or lowered in priority.
  virtual void UpdateSmoothnessTakesPriority(bool prefer_smoothness) {}

  // Requests a BeginFrame notification from the output surface. The
  // notification will be delivered by calling
  // OutputSurfaceClient::BeginFrame until the callback is disabled.
  virtual void SetNeedsBeginFrame(bool enable) {}

  bool HasClient() { return !!client_; }

  // Returns an estimate of the current GPU latency. When only a software
  // device is present, returns 0.
  base::TimeDelta GpuLatencyEstimate();

  // Get the class capable of informing cc of hardware overlay capability.
  OverlayCandidateValidator* overlay_candidate_validator() const {
    return overlay_candidate_validator_.get();
  }

 protected:
  OutputSurfaceClient* client_;

  // Synchronously initialize context3d and enter hardware mode.
  // This can only supported in threaded compositing mode.
  bool InitializeAndSetContext3d(
      scoped_refptr<ContextProvider> context_provider);
  void ReleaseGL();

  void PostSwapBuffersComplete();

  struct OutputSurface::Capabilities capabilities_;
  scoped_refptr<ContextProvider> context_provider_;
  scoped_ptr<SoftwareOutputDevice> software_device_;
  scoped_ptr<OverlayCandidateValidator> overlay_candidate_validator_;
  gfx::Size surface_size_;
  float device_scale_factor_;

  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval);

  void SetNeedsRedrawRect(const gfx::Rect& damage_rect);
  void ReclaimResources(const CompositorFrameAck* ack);
  void DidLoseOutputSurface();
  void SetExternalStencilTest(bool enabled);
  void SetExternalDrawConstraints(const gfx::Transform& transform,
                                  const gfx::Rect& viewport,
                                  const gfx::Rect& clip,
                                  bool valid_for_tile_management);

 private:
  void SetUpContext3d();
  void ResetContext3d();
  void SetMemoryPolicy(const ManagedMemoryPolicy& policy);
  void UpdateAndMeasureGpuLatency();

  bool external_stencil_test_enabled_;

  base::WeakPtrFactory<OutputSurface> weak_ptr_factory_;

  std::deque<unsigned> available_gpu_latency_query_ids_;
  std::deque<unsigned> pending_gpu_latency_query_ids_;
  RollingTimeDeltaHistory gpu_latency_history_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurface);
};

}  // namespace cc

#endif  // CC_OUTPUT_OUTPUT_SURFACE_H_
