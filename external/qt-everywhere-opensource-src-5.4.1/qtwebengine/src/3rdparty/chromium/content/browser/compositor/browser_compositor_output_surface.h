// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/id_map.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/output_surface.h"
#include "content/common/content_export.h"
#include "ui/compositor/compositor_vsync_manager.h"

namespace cc {
class SoftwareOutputDevice;
}

namespace content {
class ContextProviderCommandBuffer;
class ReflectorImpl;
class WebGraphicsContext3DCommandBufferImpl;

class CONTENT_EXPORT BrowserCompositorOutputSurface
    : public cc::OutputSurface,
      public ui::CompositorVSyncManager::Observer,
      public base::NonThreadSafe {
 public:
  virtual ~BrowserCompositorOutputSurface();

  // cc::OutputSurface implementation.
  virtual bool BindToClient(cc::OutputSurfaceClient* client) OVERRIDE;
  virtual void OnSwapBuffersComplete() OVERRIDE;

  // ui::CompositorOutputSurface::Observer implementation.
  virtual void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                       base::TimeDelta interval) OVERRIDE;

  void OnUpdateVSyncParametersFromGpu(base::TimeTicks tiembase,
                                      base::TimeDelta interval);

  void SetReflector(ReflectorImpl* reflector);

#if defined(OS_MACOSX)
  void OnSurfaceDisplayed();
#endif

 protected:
  // Constructor used by the accelerated implementation.
  BrowserCompositorOutputSurface(
      const scoped_refptr<ContextProviderCommandBuffer>& context,
      int surface_id,
      IDMap<BrowserCompositorOutputSurface>* output_surface_map,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager);

  // Constructor used by the software implementation.
  BrowserCompositorOutputSurface(
      scoped_ptr<cc::SoftwareOutputDevice> software_device,
      int surface_id,
      IDMap<BrowserCompositorOutputSurface>* output_surface_map,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager);

  int surface_id_;
  IDMap<BrowserCompositorOutputSurface>* output_surface_map_;

  scoped_refptr<ui::CompositorVSyncManager> vsync_manager_;
  scoped_refptr<ReflectorImpl> reflector_;

 private:
  void Initialize();

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
