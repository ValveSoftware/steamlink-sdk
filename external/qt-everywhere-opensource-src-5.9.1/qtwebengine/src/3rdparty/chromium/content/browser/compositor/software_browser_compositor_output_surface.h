// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SOFTWARE_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_SOFTWARE_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/common/content_export.h"

namespace cc {
class SoftwareOutputDevice;
}

namespace ui {
class CompositorVSyncManager;
}

namespace content {

class CONTENT_EXPORT SoftwareBrowserCompositorOutputSurface
    : public BrowserCompositorOutputSurface {
 public:
  SoftwareBrowserCompositorOutputSurface(
      std::unique_ptr<cc::SoftwareOutputDevice> software_device,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
      cc::SyntheticBeginFrameSource* begin_frame_source,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~SoftwareBrowserCompositorOutputSurface() override;

  // OutputSurface implementation.
  void BindToClient(cc::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override;
  void SwapBuffers(cc::OutputSurfaceFrame frame) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  bool SurfaceIsSuspendForRecycle() const override;
  uint32_t GetFramebufferCopyTextureFormat() override;

 private:
  // BrowserCompositorOutputSurface implementation.
#if defined(OS_MACOSX)
  void SetSurfaceSuspendedForRecycle(bool suspended) override;
#endif

  void SwapBuffersCallback();

  cc::OutputSurfaceClient* client_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<SoftwareBrowserCompositorOutputSurface> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareBrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SOFTWARE_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
