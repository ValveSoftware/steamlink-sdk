// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/worker_pool.h"
#include "base/trace_event/trace_event.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/drm/gpu/drm_vsync_provider.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {

namespace {

void WaitForFence(EGLDisplay display, EGLSyncKHR fence) {
  eglClientWaitSyncKHR(display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
                       EGL_FOREVER_KHR);
}

}  // namespace

GbmSurfaceless::GbmSurfaceless(GbmSurfaceFactory* surface_factory,
                               std::unique_ptr<DrmWindowProxy> window,
                               gfx::AcceleratedWidget widget)
    : SurfacelessEGL(gfx::Size()),
      surface_factory_(surface_factory),
      window_(std::move(window)),
      widget_(widget),
      has_implicit_external_sync_(
          HasEGLExtension("EGL_ARM_implicit_external_sync")),
      has_image_flush_external_(
          HasEGLExtension("EGL_EXT_image_flush_external")),
      weak_factory_(this) {
  surface_factory_->RegisterSurface(window_->widget(), this);
  unsubmitted_frames_.push_back(new PendingFrame());
}

void GbmSurfaceless::QueueOverlayPlane(const OverlayPlane& plane) {
  planes_.push_back(plane);
}

bool GbmSurfaceless::Initialize(gl::GLSurface::Format format) {
  if (!SurfacelessEGL::Initialize(format))
    return false;
  vsync_provider_ = base::MakeUnique<DrmVSyncProvider>(window_.get());
  if (!vsync_provider_)
    return false;
  return true;
}

gfx::SwapResult GbmSurfaceless::SwapBuffers() {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

bool GbmSurfaceless::ScheduleOverlayPlane(int z_order,
                                          gfx::OverlayTransform transform,
                                          gl::GLImage* image,
                                          const gfx::Rect& bounds_rect,
                                          const gfx::RectF& crop_rect) {
  unsubmitted_frames_.back()->overlays.push_back(
      gl::GLSurfaceOverlay(z_order, transform, image, bounds_rect, crop_rect));
  return true;
}

bool GbmSurfaceless::IsOffscreen() {
  return false;
}

gfx::VSyncProvider* GbmSurfaceless::GetVSyncProvider() {
  return vsync_provider_.get();
}

bool GbmSurfaceless::SupportsAsyncSwap() {
  return true;
}

bool GbmSurfaceless::SupportsPostSubBuffer() {
  return true;
}

gfx::SwapResult GbmSurfaceless::PostSubBuffer(int x,
                                              int y,
                                              int width,
                                              int height) {
  // The actual sub buffer handling is handled at higher layers.
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GbmSurfaceless::SwapBuffersAsync(const SwapCompletionCallback& callback) {
  TRACE_EVENT0("drm", "GbmSurfaceless::SwapBuffersAsync");
  // If last swap failed, don't try to schedule new ones.
  if (!last_swap_buffers_result_) {
    callback.Run(gfx::SwapResult::SWAP_FAILED);
    return;
  }

  glFlush();
  unsubmitted_frames_.back()->Flush();

  SwapCompletionCallback surface_swap_callback = base::Bind(
      &GbmSurfaceless::SwapCompleted, weak_factory_.GetWeakPtr(), callback);

  PendingFrame* frame = unsubmitted_frames_.back();
  frame->callback = surface_swap_callback;
  unsubmitted_frames_.push_back(new PendingFrame());

  // TODO: the following should be replaced by a per surface flush as it gets
  // implemented in GL drivers.
  if (has_implicit_external_sync_ || has_image_flush_external_) {
    EGLSyncKHR fence = InsertFence(has_implicit_external_sync_);
    if (!fence) {
      callback.Run(gfx::SwapResult::SWAP_FAILED);
      return;
    }

    base::Closure fence_wait_task =
        base::Bind(&WaitForFence, GetDisplay(), fence);

    base::Closure fence_retired_callback =
        base::Bind(&GbmSurfaceless::FenceRetired, weak_factory_.GetWeakPtr(),
                   fence, frame);

    base::WorkerPool::PostTaskAndReply(FROM_HERE, fence_wait_task,
                                       fence_retired_callback, false);
    return;  // Defer frame submission until fence signals.
  }

  frame->ready = true;
  SubmitFrame();
}

void GbmSurfaceless::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const SwapCompletionCallback& callback) {
  // The actual sub buffer handling is handled at higher layers.
  SwapBuffersAsync(callback);
}

EGLConfig GbmSurfaceless::GetConfig() {
  if (!config_) {
    EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                               32,
                               EGL_ALPHA_SIZE,
                               8,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_RED_SIZE,
                               8,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_SURFACE_TYPE,
                               EGL_DONT_CARE,
                               EGL_NONE};
    config_ = ChooseEGLConfig(GetDisplay(), config_attribs);
  }
  return config_;
}

GbmSurfaceless::~GbmSurfaceless() {
  Destroy();  // The EGL surface must be destroyed before SurfaceOzone.
  surface_factory_->UnregisterSurface(window_->widget());
}

GbmSurfaceless::PendingFrame::PendingFrame() {}

GbmSurfaceless::PendingFrame::~PendingFrame() {}

bool GbmSurfaceless::PendingFrame::ScheduleOverlayPlanes(
    gfx::AcceleratedWidget widget) {
  for (const auto& overlay : overlays)
    if (!overlay.ScheduleOverlayPlane(widget))
      return false;
  return true;
}

void GbmSurfaceless::PendingFrame::Flush() {
  for (const auto& overlay : overlays)
    overlay.Flush();
}

void GbmSurfaceless::SubmitFrame() {
  DCHECK(!unsubmitted_frames_.empty());

  if (unsubmitted_frames_.front()->ready && !swap_buffers_pending_) {
    std::unique_ptr<PendingFrame> frame(unsubmitted_frames_.front());
    unsubmitted_frames_.weak_erase(unsubmitted_frames_.begin());
    swap_buffers_pending_ = true;

    if (!frame->ScheduleOverlayPlanes(widget_)) {
      // |callback| is a wrapper for SwapCompleted(). Call it to properly
      // propagate the failed state.
      frame->callback.Run(gfx::SwapResult::SWAP_FAILED);
      return;
    }

    if (IsUniversalDisplayLinkDevice())
      glFinish();

    window_->SchedulePageFlip(planes_, frame->callback);
    planes_.clear();
  }
}

EGLSyncKHR GbmSurfaceless::InsertFence(bool implicit) {
  const EGLint attrib_list[] = {EGL_SYNC_CONDITION_KHR,
                                EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM,
                                EGL_NONE};
  return eglCreateSyncKHR(GetDisplay(), EGL_SYNC_FENCE_KHR,
                          implicit ? attrib_list : NULL);
}

void GbmSurfaceless::FenceRetired(EGLSyncKHR fence, PendingFrame* frame) {
  eglDestroySyncKHR(GetDisplay(), fence);
  frame->ready = true;
  SubmitFrame();
}

void GbmSurfaceless::SwapCompleted(const SwapCompletionCallback& callback,
                                   gfx::SwapResult result) {
  callback.Run(result);
  swap_buffers_pending_ = false;
  if (result == gfx::SwapResult::SWAP_FAILED) {
    last_swap_buffers_result_ = false;
    return;
  }

  SubmitFrame();
}

bool GbmSurfaceless::IsUniversalDisplayLinkDevice() {
  return planes_.empty() ? false : planes_[0].buffer->RequiresGlFinish();
}

}  // namespace ui
