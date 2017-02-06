// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/pass_through_image_transport_surface.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_command_buffer_stub.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

PassThroughImageTransportSurface::PassThroughImageTransportSurface(
    GpuChannelManager* /* manager */,
    GpuCommandBufferStub* stub,
    gl::GLSurface* surface)
    : GLSurfaceAdapter(surface),
      stub_(stub->AsWeakPtr()),
      did_set_swap_interval_(false),
      weak_ptr_factory_(this) {}

bool PassThroughImageTransportSurface::Initialize(
    gl::GLSurface::Format format) {
  // The surface is assumed to have already been initialized.
  if (!stub_.get() || !stub_->decoder())
    return false;
  stub_->SetLatencyInfoCallback(
      base::Bind(&PassThroughImageTransportSurface::SetLatencyInfo,
                 base::Unretained(this)));
  return true;
}

void PassThroughImageTransportSurface::Destroy() {
  GLSurfaceAdapter::Destroy();
}

gfx::SwapResult PassThroughImageTransportSurface::SwapBuffers() {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gfx::SwapResult result = gl::GLSurfaceAdapter::SwapBuffers();
  FinishSwapBuffers(std::move(latency_info), result);
  return result;
}

void PassThroughImageTransportSurface::SwapBuffersAsync(
    const GLSurface::SwapCompletionCallback& callback) {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();

  // We use WeakPtr here to avoid manual management of life time of an instance
  // of this class. Callback will not be called once the instance of this class
  // is destroyed. However, this also means that the callback can be run on
  // the calling thread only.
  gl::GLSurfaceAdapter::SwapBuffersAsync(base::Bind(
      &PassThroughImageTransportSurface::FinishSwapBuffersAsync,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(&latency_info), callback));
}

gfx::SwapResult PassThroughImageTransportSurface::PostSubBuffer(int x,
                                                                int y,
                                                                int width,
                                                                int height) {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gfx::SwapResult result =
      gl::GLSurfaceAdapter::PostSubBuffer(x, y, width, height);
  FinishSwapBuffers(std::move(latency_info), result);
  return result;
}

void PassThroughImageTransportSurface::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const GLSurface::SwapCompletionCallback& callback) {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gl::GLSurfaceAdapter::PostSubBufferAsync(
      x, y, width, height,
      base::Bind(&PassThroughImageTransportSurface::FinishSwapBuffersAsync,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&latency_info),
                 callback));
}

gfx::SwapResult PassThroughImageTransportSurface::CommitOverlayPlanes() {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gfx::SwapResult result = gl::GLSurfaceAdapter::CommitOverlayPlanes();
  FinishSwapBuffers(std::move(latency_info), result);
  return result;
}

void PassThroughImageTransportSurface::CommitOverlayPlanesAsync(
    const GLSurface::SwapCompletionCallback& callback) {
  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info =
      StartSwapBuffers();
  gl::GLSurfaceAdapter::CommitOverlayPlanesAsync(base::Bind(
      &PassThroughImageTransportSurface::FinishSwapBuffersAsync,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(&latency_info), callback));
}

bool PassThroughImageTransportSurface::OnMakeCurrent(gl::GLContext* context) {
  if (!did_set_swap_interval_) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableGpuVsync))
      context->ForceSwapIntervalZero(true);
    else
      context->SetSwapInterval(1);
    did_set_swap_interval_ = true;
  }
  return true;
}

PassThroughImageTransportSurface::~PassThroughImageTransportSurface() {
  if (stub_.get()) {
    stub_->SetLatencyInfoCallback(
        base::Callback<void(const std::vector<ui::LatencyInfo>&)>());
  }
}

void PassThroughImageTransportSurface::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  latency_info_.insert(latency_info_.end(), latency_info.begin(),
                       latency_info.end());
}

void PassThroughImageTransportSurface::SendVSyncUpdateIfAvailable() {
  gfx::VSyncProvider* vsync_provider = GetVSyncProvider();
  if (vsync_provider) {
    vsync_provider->GetVSyncParameters(base::Bind(
        &GpuCommandBufferStub::SendUpdateVSyncParameters, stub_->AsWeakPtr()));
  }
}

std::unique_ptr<std::vector<ui::LatencyInfo>>
PassThroughImageTransportSurface::StartSwapBuffers() {
  // GetVsyncValues before SwapBuffers to work around Mali driver bug:
  // crbug.com/223558.
  SendVSyncUpdateIfAvailable();

  base::TimeTicks swap_time = base::TimeTicks::Now();
  for (auto& latency : latency_info_) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0, 0, swap_time, 1);
  }

  std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info(
      new std::vector<ui::LatencyInfo>());
  latency_info->swap(latency_info_);

  return latency_info;
}

void PassThroughImageTransportSurface::FinishSwapBuffers(
    std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info,
    gfx::SwapResult result) {
  base::TimeTicks swap_ack_time = base::TimeTicks::Now();
  for (auto& latency : *latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0, 0,
        swap_ack_time, 1);
  }

  GpuCommandBufferMsg_SwapBuffersCompleted_Params params;
  params.latency_info = *latency_info;
  params.result = result;
  stub_->SendSwapBuffersCompleted(params);
}

void PassThroughImageTransportSurface::FinishSwapBuffersAsync(
    std::unique_ptr<std::vector<ui::LatencyInfo>> latency_info,
    GLSurface::SwapCompletionCallback callback,
    gfx::SwapResult result) {
  FinishSwapBuffers(std::move(latency_info), result);
  callback.Run(result);
}

}  // namespace gpu
