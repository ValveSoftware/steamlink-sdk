// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_helper.h"

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurfaceAPI.h>

#include "base/bind.h"
#include "content/browser/compositor/browser_compositor_view_mac.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/gpu_messages.h"

namespace {

void OnNativeSurfaceBuffersSwappedOnUIThread(
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  gfx::AcceleratedWidget native_widget =
      content::GpuSurfaceTracker::Get()->AcquireNativeWidget(params.surface_id);
  IOSurfaceID io_surface_handle = static_cast<IOSurfaceID>(
      params.surface_handle);
  [native_widget gotAcceleratedIOSurfaceFrame:io_surface_handle
                          withOutputSurfaceID:params.surface_id
                                withPixelSize:params.size
                              withScaleFactor:params.scale_factor];
}

}  // namespace

namespace content {

void RenderWidgetHelper::OnNativeSurfaceBuffersSwappedOnIOThread(
    GpuProcessHost* gpu_process_host,
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Immediately acknowledge this frame on the IO thread instead of the UI
  // thread. The UI thread will wait on the GPU process. If the UI thread
  // were to be responsible for acking swaps, then there would be a cycle
  // and a potential deadlock.
  // TODO(ccameron): This immediate ack circumvents GPU back-pressure that
  // is necessary to throttle renderers. Fix that.
  AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
  ack_params.sync_point = 0;
  ack_params.renderer_id = 0;
  gpu_process_host->Send(new AcceleratedSurfaceMsg_BufferPresented(
      params.route_id, ack_params));

  // Open the IOSurface handle before returning, to ensure that it is not
  // closed as soon as the frame is acknowledged.
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface(IOSurfaceLookup(
          static_cast<uint32>(params.surface_handle)));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OnNativeSurfaceBuffersSwappedOnUIThread, io_surface, params));
}

}  // namespace content
