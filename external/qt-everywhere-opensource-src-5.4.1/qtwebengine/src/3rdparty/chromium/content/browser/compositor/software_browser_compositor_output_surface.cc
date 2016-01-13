// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/software_browser_compositor_output_surface.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/software_output_device.h"
#include "content/browser/compositor/browser_compositor_output_surface_proxy.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/vsync_provider.h"

namespace content {

SoftwareBrowserCompositorOutputSurface::SoftwareBrowserCompositorOutputSurface(
    scoped_refptr<BrowserCompositorOutputSurfaceProxy> surface_proxy,
    scoped_ptr<cc::SoftwareOutputDevice> software_device,
    int surface_id,
    IDMap<BrowserCompositorOutputSurface>* output_surface_map,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager)
    : BrowserCompositorOutputSurface(software_device.Pass(),
                                     surface_id,
                                     output_surface_map,
                                     vsync_manager),
      output_surface_proxy_(surface_proxy) {}

SoftwareBrowserCompositorOutputSurface::
    ~SoftwareBrowserCompositorOutputSurface() {}

void SoftwareBrowserCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  for (size_t i = 0; i < frame->metadata.latency_info.size(); i++) {
    frame->metadata.latency_info[i].AddLatencyNumber(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0);
  }
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          &RenderWidgetHostImpl::CompositorFrameDrawn,
          frame->metadata.latency_info));

  gfx::VSyncProvider* vsync_provider = software_device()->GetVSyncProvider();
  if (vsync_provider) {
    vsync_provider->GetVSyncParameters(
        base::Bind(&BrowserCompositorOutputSurfaceProxy::
                        OnUpdateVSyncParametersOnCompositorThread,
                   output_surface_proxy_,
                   surface_id_));
  }
}

}  // namespace content
