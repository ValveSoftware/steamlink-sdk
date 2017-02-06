// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_surface_factory.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/wayland/wayland_display.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/public/surface_ozone_egl.h"

#if defined(USE_WAYLAND_EGL)
#include "ui/ozone/platform/wayland/wayland_egl_surface.h"
#endif

namespace ui {

static void DeleteSharedMemory(void* pixels, void* context) {
  delete static_cast<base::SharedMemory*>(context);
}

class WaylandCanvasSurface : public SurfaceOzoneCanvas {
 public:
  WaylandCanvasSurface(WaylandDisplay* display, WaylandWindow* window_);
  ~WaylandCanvasSurface() override;

  // SurfaceOzoneCanvas
  sk_sp<SkSurface> GetSurface() override;
  void ResizeCanvas(const gfx::Size& viewport_size) override;
  void PresentCanvas(const gfx::Rect& damage) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;

 private:
  WaylandDisplay* display_;
  WaylandWindow* window_;

  gfx::Size size_;
  sk_sp<SkSurface> sk_surface_;
  wl::Object<wl_shm_pool> pool_;
  wl::Object<wl_buffer> buffer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandCanvasSurface);
};

WaylandCanvasSurface::WaylandCanvasSurface(WaylandDisplay* display,
                                           WaylandWindow* window)
    : display_(display), window_(window), size_(window->GetBounds().size()) {}

WaylandCanvasSurface::~WaylandCanvasSurface() {}

sk_sp<SkSurface> WaylandCanvasSurface::GetSurface() {
  if (sk_surface_)
    return sk_surface_;

  size_t length = size_.width() * size_.height() * 4;
  auto shared_memory = base::WrapUnique(new base::SharedMemory);
  if (!shared_memory->CreateAndMapAnonymous(length))
    return nullptr;

  wl::Object<wl_shm_pool> pool(
      wl_shm_create_pool(display_->shm(), shared_memory->handle().fd, length));
  if (!pool)
    return nullptr;
  wl::Object<wl_buffer> buffer(
      wl_shm_pool_create_buffer(pool.get(), 0, size_.width(), size_.height(),
                                size_.width() * 4, WL_SHM_FORMAT_ARGB8888));
  if (!buffer)
    return nullptr;

  sk_surface_ = SkSurface::MakeRasterDirectReleaseProc(
      SkImageInfo::MakeN32Premul(size_.width(), size_.height()),
      shared_memory->memory(), size_.width() * 4, &DeleteSharedMemory,
      shared_memory.get(), nullptr);
  if (!sk_surface_)
    return nullptr;
  pool_ = std::move(pool);
  buffer_ = std::move(buffer);
  (void)shared_memory.release();
  return sk_surface_;
}

void WaylandCanvasSurface::ResizeCanvas(const gfx::Size& viewport_size) {
  if (size_ == viewport_size)
    return;
  // TODO(forney): We could implement more efficient resizes by allocating
  // buffers rounded up to larger sizes, and then reusing them if the new size
  // still fits (but still reallocate if the new size is much smaller than the
  // old size).
  if (sk_surface_) {
    sk_surface_.reset();
    buffer_.reset();
    pool_.reset();
  }
  size_ = viewport_size;
}

void WaylandCanvasSurface::PresentCanvas(const gfx::Rect& damage) {
  // TODO(forney): This is just a naive implementation that allows chromium to
  // draw to the buffer at any time, even if it is being used by the Wayland
  // compositor. Instead, we should track buffer releases and frame callbacks
  // from Wayland to ensure perfect frames (while minimizing copies).
  wl_surface* surface = window_->surface();
  wl_surface_damage(surface, damage.x(), damage.y(), damage.width(),
                    damage.height());
  wl_surface_attach(surface, buffer_.get(), 0, 0);
  wl_surface_commit(surface);
  display_->ScheduleFlush();
}

std::unique_ptr<gfx::VSyncProvider>
WaylandCanvasSurface::CreateVSyncProvider() {
  // TODO(forney): This can be implemented with information from frame
  // callbacks, and possibly output refresh rate.
  NOTIMPLEMENTED();
  return nullptr;
}

WaylandSurfaceFactory::WaylandSurfaceFactory(WaylandDisplay* display)
    : display_(display) {}

WaylandSurfaceFactory::~WaylandSurfaceFactory() {}

intptr_t WaylandSurfaceFactory::GetNativeDisplay() {
  return reinterpret_cast<intptr_t>(display_->display());
}

bool WaylandSurfaceFactory::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
#if defined(USE_WAYLAND_EGL)
  if (!display_)
    return false;
  setenv("EGL_PLATFORM", "wayland", 0);
  return LoadDefaultEGLGLES2Bindings(add_gl_library, set_gl_get_proc_address);
#else
  return false;
#endif
}

std::unique_ptr<SurfaceOzoneCanvas>
WaylandSurfaceFactory::CreateCanvasForWidget(gfx::AcceleratedWidget widget) {
  WaylandWindow* window = display_->GetWindow(widget);
  DCHECK(window);
  return base::WrapUnique(new WaylandCanvasSurface(display_, window));
}

std::unique_ptr<SurfaceOzoneEGL>
WaylandSurfaceFactory::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
#if defined(USE_WAYLAND_EGL)
  WaylandWindow* window = display_->GetWindow(widget);
  DCHECK(window);
  auto surface = base::WrapUnique(
      new WaylandEGLSurface(window, window->GetBounds().size()));
  if (!surface->Initialize())
    return nullptr;
  return std::move(surface);
#else
  return nullptr;
#endif
}

scoped_refptr<NativePixmap> WaylandSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  NOTIMPLEMENTED();
  return nullptr;
}

scoped_refptr<NativePixmap> WaylandSurfaceFactory::CreateNativePixmapFromHandle(
    gfx::Size size,
    gfx::BufferFormat format,
    const gfx::NativePixmapHandle& handle) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ui
