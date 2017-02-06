// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_output_surface_mac.h"

#include "cc/output/compositor_frame.h"
#include "components/display_compositor/compositor_overlay_candidate_validator.h"
#include "content/browser/gpu/gpu_surface_tracker.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/ipc/client/gpu_process_hosted_ca_layer_tree_params.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/mac/io_surface.h"

namespace content {

struct GpuOutputSurfaceMac::RemoteLayers {
  void UpdateLayers(CAContextID content_ca_context_id,
                    CAContextID fullscreen_low_power_ca_context_id) {
    if (content_ca_context_id) {
      if ([content_layer contextId] != content_ca_context_id) {
        content_layer.reset([[CALayerHost alloc] init]);
        [content_layer setContextId:content_ca_context_id];
        [content_layer
            setAutoresizingMask:kCALayerMaxXMargin | kCALayerMaxYMargin];
      }
    } else {
      content_layer.reset();
    }

    if (fullscreen_low_power_ca_context_id) {
      if ([fullscreen_low_power_layer contextId] !=
          fullscreen_low_power_ca_context_id) {
        fullscreen_low_power_layer.reset([[CALayerHost alloc] init]);
        [fullscreen_low_power_layer
            setContextId:fullscreen_low_power_ca_context_id];
      }
    } else {
      fullscreen_low_power_layer.reset();
    }
  }

  base::scoped_nsobject<CALayerHost> content_layer;
  base::scoped_nsobject<CALayerHost> fullscreen_low_power_layer;
};

GpuOutputSurfaceMac::GpuOutputSurfaceMac(
    scoped_refptr<ContextProviderCommandBuffer> context,
    gpu::SurfaceHandle surface_handle,
    scoped_refptr<ui::CompositorVSyncManager> vsync_manager,
    cc::SyntheticBeginFrameSource* begin_frame_source,
    std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
        overlay_candidate_validator,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : GpuSurfacelessBrowserCompositorOutputSurface(
          std::move(context),
          surface_handle,
          std::move(vsync_manager),
          begin_frame_source,
          std::move(overlay_candidate_validator),
          GL_TEXTURE_RECTANGLE_ARB,
          GL_RGBA,
          gpu_memory_buffer_manager),
      remote_layers_(new RemoteLayers) {}

GpuOutputSurfaceMac::~GpuOutputSurfaceMac() {}

void GpuOutputSurfaceMac::SwapBuffers(cc::CompositorFrame frame) {
  GpuSurfacelessBrowserCompositorOutputSurface::SwapBuffers(std::move(frame));

  if (should_show_frames_state_ ==
      SHOULD_NOT_SHOW_FRAMES_NO_SWAP_AFTER_SUSPENDED) {
    should_show_frames_state_ = SHOULD_SHOW_FRAMES;
  }
}

void GpuOutputSurfaceMac::OnGpuSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result,
    const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) {
  remote_layers_->UpdateLayers(params_mac->ca_context_id,
                               params_mac->fullscreen_low_power_ca_context_id);
  if (should_show_frames_state_ == SHOULD_SHOW_FRAMES) {
    ui::AcceleratedWidgetMac* widget = ui::AcceleratedWidgetMac::Get(
        content::GpuSurfaceTracker::Get()->AcquireNativeWidget(
            params_mac->surface_handle));
    if (widget) {
      if (remote_layers_->content_layer) {
        widget->GotCALayerFrame(
            base::scoped_nsobject<CALayer>(remote_layers_->content_layer.get(),
                                           base::scoped_policy::RETAIN),
            params_mac->fullscreen_low_power_ca_context_valid,
            base::scoped_nsobject<CALayer>(
                remote_layers_->fullscreen_low_power_layer.get(),
                base::scoped_policy::RETAIN),
            params_mac->pixel_size, params_mac->scale_factor);
      } else {
        widget->GotIOSurfaceFrame(params_mac->io_surface,
                                  params_mac->pixel_size,
                                  params_mac->scale_factor);
      }
    }
  }
  DidReceiveTextureInUseResponses(params_mac->responses);
  GpuSurfacelessBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
      latency_info, result, params_mac);
}

void GpuOutputSurfaceMac::SetSurfaceSuspendedForRecycle(bool suspended) {
  if (suspended) {
    // It may be that there are frames in-flight from the GPU process back to
    // the browser. Make sure that these frames are not displayed by ignoring
    // them in GpuProcessHostUIShim, until the browser issues a SwapBuffers for
    // the new content.
    should_show_frames_state_ = SHOULD_NOT_SHOW_FRAMES_SUSPENDED;
  } else {
    // Discard the backbuffer before drawing the new frame. This is necessary
    // only when using a ImageTransportSurfaceFBO with a
    // CALayerStorageProvider. Discarding the backbuffer results in the next
    // frame using a new CALayer and CAContext, which guarantees that the
    // browser will not flash stale content when adding the remote CALayer to
    // the NSView hierarchy (it could flash stale content because the system
    // window server is not synchronized with any signals we control or
    // observe).
    if (should_show_frames_state_ == SHOULD_NOT_SHOW_FRAMES_SUSPENDED) {
      DiscardBackbuffer();
      should_show_frames_state_ =
          SHOULD_NOT_SHOW_FRAMES_NO_SWAP_AFTER_SUSPENDED;
    }
  }
}

bool GpuOutputSurfaceMac::SurfaceIsSuspendForRecycle() const {
  return should_show_frames_state_ == SHOULD_NOT_SHOW_FRAMES_SUSPENDED;
}

}  // namespace content
