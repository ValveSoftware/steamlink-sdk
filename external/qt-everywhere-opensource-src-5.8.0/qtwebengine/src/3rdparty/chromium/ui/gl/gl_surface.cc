// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

namespace gl {

namespace {
base::LazyInstance<base::ThreadLocalPointer<GLSurface> >::Leaky
    current_surface_ = LAZY_INSTANCE_INITIALIZER;
}  // namespace

GLSurface::GLSurface() {}

bool GLSurface::Initialize() {
  return Initialize(SURFACE_DEFAULT);
}

bool GLSurface::Initialize(GLSurface::Format format) {
  return true;
}

bool GLSurface::Resize(const gfx::Size& size,
                       float scale_factor,
                       bool has_alpha) {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::Recreate() {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::DeferDraws() {
  return false;
}

bool GLSurface::SupportsPostSubBuffer() {
  return false;
}

bool GLSurface::SupportsCommitOverlayPlanes() {
  return false;
}

bool GLSurface::SupportsAsyncSwap() {
  return false;
}

unsigned int GLSurface::GetBackingFrameBufferObject() {
  return 0;
}

void GLSurface::SwapBuffersAsync(const SwapCompletionCallback& callback) {
  NOTREACHED();
}

gfx::SwapResult GLSurface::PostSubBuffer(int x, int y, int width, int height) {
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurface::PostSubBufferAsync(int x,
                                   int y,
                                   int width,
                                   int height,
                                   const SwapCompletionCallback& callback) {
  NOTREACHED();
}

gfx::SwapResult GLSurface::CommitOverlayPlanes() {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GLSurface::CommitOverlayPlanesAsync(
    const SwapCompletionCallback& callback) {
  NOTREACHED();
}

bool GLSurface::OnMakeCurrent(GLContext* context) {
  return true;
}

bool GLSurface::SetBackbufferAllocation(bool allocated) {
  return true;
}

void GLSurface::SetFrontbufferAllocation(bool allocated) {
}

void* GLSurface::GetShareHandle() {
  NOTIMPLEMENTED();
  return NULL;
}

void* GLSurface::GetDisplay() {
  NOTIMPLEMENTED();
  return NULL;
}

void* GLSurface::GetConfig() {
  NOTIMPLEMENTED();
  return NULL;
}

GLSurface::Format GLSurface::GetFormat() {
  NOTIMPLEMENTED();
  return SURFACE_DEFAULT;
}

gfx::VSyncProvider* GLSurface::GetVSyncProvider() {
  return NULL;
}

bool GLSurface::ScheduleOverlayPlane(int z_order,
                                     gfx::OverlayTransform transform,
                                     GLImage* image,
                                     const gfx::Rect& bounds_rect,
                                     const gfx::RectF& crop_rect) {
  NOTIMPLEMENTED();
  return false;
}

bool GLSurface::ScheduleCALayer(GLImage* contents_image,
                                const gfx::RectF& contents_rect,
                                float opacity,
                                unsigned background_color,
                                unsigned edge_aa_mask,
                                const gfx::RectF& rect,
                                bool is_clipped,
                                const gfx::RectF& clip_rect,
                                const gfx::Transform& transform,
                                int sorting_content_id,
                                unsigned filter) {
  NOTIMPLEMENTED();
  return false;
}

void GLSurface::ScheduleCALayerInUseQuery(
    std::vector<CALayerInUseQuery> queries) {
  NOTIMPLEMENTED();
}

bool GLSurface::IsSurfaceless() const {
  return false;
}

bool GLSurface::FlipsVertically() const {
  return false;
}

bool GLSurface::BuffersFlipped() const {
  return false;
}

GLSurface* GLSurface::GetCurrent() {
  return current_surface_.Pointer()->Get();
}

GLSurface::~GLSurface() {
  if (GetCurrent() == this)
    SetCurrent(NULL);
}

void GLSurface::SetCurrent(GLSurface* surface) {
  current_surface_.Pointer()->Set(surface);
}

bool GLSurface::ExtensionsContain(const char* c_extensions, const char* name) {
  DCHECK(name);
  if (!c_extensions)
    return false;
  std::string extensions(c_extensions);
  extensions += " ";

  std::string delimited_name(name);
  delimited_name += " ";

  return extensions.find(delimited_name) != std::string::npos;
}

void GLSurface::OnSetSwapInterval(int interval) {
}

GLSurfaceAdapter::GLSurfaceAdapter(GLSurface* surface) : surface_(surface) {}

bool GLSurfaceAdapter::Initialize(GLSurface::Format format) {
  return surface_->Initialize(format);
}

void GLSurfaceAdapter::Destroy() {
  surface_->Destroy();
}

bool GLSurfaceAdapter::Resize(const gfx::Size& size,
                              float scale_factor,
                              bool has_alpha) {
  return surface_->Resize(size, scale_factor, has_alpha);
}

bool GLSurfaceAdapter::Recreate() {
  return surface_->Recreate();
}

bool GLSurfaceAdapter::DeferDraws() {
  return surface_->DeferDraws();
}

bool GLSurfaceAdapter::IsOffscreen() {
  return surface_->IsOffscreen();
}

gfx::SwapResult GLSurfaceAdapter::SwapBuffers() {
  return surface_->SwapBuffers();
}

void GLSurfaceAdapter::SwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  surface_->SwapBuffersAsync(callback);
}

gfx::SwapResult GLSurfaceAdapter::PostSubBuffer(int x,
                                                int y,
                                                int width,
                                                int height) {
  return surface_->PostSubBuffer(x, y, width, height);
}

void GLSurfaceAdapter::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const SwapCompletionCallback& callback) {
  surface_->PostSubBufferAsync(x, y, width, height, callback);
}

gfx::SwapResult GLSurfaceAdapter::CommitOverlayPlanes() {
  return surface_->CommitOverlayPlanes();
}

void GLSurfaceAdapter::CommitOverlayPlanesAsync(
    const SwapCompletionCallback& callback) {
  surface_->CommitOverlayPlanesAsync(callback);
}

bool GLSurfaceAdapter::SupportsPostSubBuffer() {
  return surface_->SupportsPostSubBuffer();
}

bool GLSurfaceAdapter::SupportsCommitOverlayPlanes() {
  return surface_->SupportsCommitOverlayPlanes();
}

bool GLSurfaceAdapter::SupportsAsyncSwap() {
  return surface_->SupportsAsyncSwap();
}

gfx::Size GLSurfaceAdapter::GetSize() {
  return surface_->GetSize();
}

void* GLSurfaceAdapter::GetHandle() {
  return surface_->GetHandle();
}

unsigned int GLSurfaceAdapter::GetBackingFrameBufferObject() {
  return surface_->GetBackingFrameBufferObject();
}

bool GLSurfaceAdapter::OnMakeCurrent(GLContext* context) {
  return surface_->OnMakeCurrent(context);
}

bool GLSurfaceAdapter::SetBackbufferAllocation(bool allocated) {
  return surface_->SetBackbufferAllocation(allocated);
}

void GLSurfaceAdapter::SetFrontbufferAllocation(bool allocated) {
  surface_->SetFrontbufferAllocation(allocated);
}

void* GLSurfaceAdapter::GetShareHandle() {
  return surface_->GetShareHandle();
}

void* GLSurfaceAdapter::GetDisplay() {
  return surface_->GetDisplay();
}

void* GLSurfaceAdapter::GetConfig() {
  return surface_->GetConfig();
}

GLSurface::Format GLSurfaceAdapter::GetFormat() {
  return surface_->GetFormat();
}

gfx::VSyncProvider* GLSurfaceAdapter::GetVSyncProvider() {
  return surface_->GetVSyncProvider();
}

bool GLSurfaceAdapter::ScheduleOverlayPlane(int z_order,
                                            gfx::OverlayTransform transform,
                                            GLImage* image,
                                            const gfx::Rect& bounds_rect,
                                            const gfx::RectF& crop_rect) {
  return surface_->ScheduleOverlayPlane(
      z_order, transform, image, bounds_rect, crop_rect);
}

bool GLSurfaceAdapter::IsSurfaceless() const {
  return surface_->IsSurfaceless();
}

bool GLSurfaceAdapter::FlipsVertically() const {
  return surface_->FlipsVertically();
}

bool GLSurfaceAdapter::BuffersFlipped() const {
  return surface_->BuffersFlipped();
}

GLSurfaceAdapter::~GLSurfaceAdapter() {}

scoped_refptr<GLSurface> InitializeGLSurface(scoped_refptr<GLSurface> surface) {
  if (!surface->Initialize())
    return nullptr;
  return surface;
}

GLSurface::CALayerInUseQuery::CALayerInUseQuery() = default;
GLSurface::CALayerInUseQuery::CALayerInUseQuery(const CALayerInUseQuery&) =
    default;
GLSurface::CALayerInUseQuery::~CALayerInUseQuery() = default;

}  // namespace gl
