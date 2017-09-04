// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/software_browser_compositor_output_surface.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
#include "cc/output/software_output_device.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/vsync_provider.h"

namespace content {

SoftwareBrowserCompositorOutputSurface::SoftwareBrowserCompositorOutputSurface(
    std::unique_ptr<cc::SoftwareOutputDevice> software_device,
    const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager,
    cc::SyntheticBeginFrameSource* begin_frame_source,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : BrowserCompositorOutputSurface(std::move(software_device),
                                     vsync_manager,
                                     begin_frame_source),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {}

SoftwareBrowserCompositorOutputSurface::
    ~SoftwareBrowserCompositorOutputSurface() {
}

void SoftwareBrowserCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void SoftwareBrowserCompositorOutputSurface::EnsureBackbuffer() {
  software_device()->EnsureBackbuffer();
}

void SoftwareBrowserCompositorOutputSurface::DiscardBackbuffer() {
  software_device()->DiscardBackbuffer();
}

void SoftwareBrowserCompositorOutputSurface::BindFramebuffer() {
  // Not used for software surfaces.
  NOTREACHED();
}

void SoftwareBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha) {
  software_device()->Resize(size, device_scale_factor);
}

void SoftwareBrowserCompositorOutputSurface::SwapBuffers(
    cc::OutputSurfaceFrame frame) {
  DCHECK(client_);
  base::TimeTicks swap_time = base::TimeTicks::Now();
  for (auto& latency : frame.latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0, swap_time, 1);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
        swap_time, 1);
  }
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&RenderWidgetHostImpl::CompositorFrameDrawn,
                                    frame.latency_info));

  gfx::VSyncProvider* vsync_provider = software_device()->GetVSyncProvider();
  if (vsync_provider) {
    vsync_provider->GetVSyncParameters(base::Bind(
        &BrowserCompositorOutputSurface::OnUpdateVSyncParametersFromGpu,
        weak_factory_.GetWeakPtr()));
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SoftwareBrowserCompositorOutputSurface::SwapBuffersCallback,
                 weak_factory_.GetWeakPtr()));
}

void SoftwareBrowserCompositorOutputSurface::SwapBuffersCallback() {
  client_->DidReceiveSwapBuffersAck();
}

bool SoftwareBrowserCompositorOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned SoftwareBrowserCompositorOutputSurface::GetOverlayTextureId() const {
  return 0;
}

bool SoftwareBrowserCompositorOutputSurface::SurfaceIsSuspendForRecycle()
    const {
  return false;
}

GLenum
SoftwareBrowserCompositorOutputSurface::GetFramebufferCopyTextureFormat() {
  // Not used for software surfaces.
  NOTREACHED();
  return 0;
}

#if defined(OS_MACOSX)
void SoftwareBrowserCompositorOutputSurface::SetSurfaceSuspendedForRecycle(
    bool suspended) {
}
#endif

}  // namespace content
