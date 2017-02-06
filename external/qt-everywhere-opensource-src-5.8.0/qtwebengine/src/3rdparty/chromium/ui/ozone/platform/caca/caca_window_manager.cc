// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/caca_window_manager.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/caca/caca_window.h"
#include "ui/ozone/platform/caca/scoped_caca_types.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

namespace {

class CacaSurface : public ui::SurfaceOzoneCanvas {
 public:
  CacaSurface(CacaWindow* window);
  ~CacaSurface() override;

  bool Initialize();

  // ui::SurfaceOzoneCanvas overrides:
  sk_sp<SkSurface> GetSurface() override;
  void ResizeCanvas(const gfx::Size& viewport_size) override;
  void PresentCanvas(const gfx::Rect& damage) override;
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;

 private:
  CacaWindow* window_;  // Not owned.

  ScopedCacaDither dither_;

  sk_sp<SkSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(CacaSurface);
};

CacaSurface::CacaSurface(CacaWindow* window) : window_(window) {
}

CacaSurface::~CacaSurface() {
}

bool CacaSurface::Initialize() {
  ResizeCanvas(window_->bitmap_size());
  return true;
}

sk_sp<SkSurface> CacaSurface::GetSurface() {
  return surface_;
}

void CacaSurface::ResizeCanvas(const gfx::Size& viewport_size) {
  TRACE_EVENT0("ozone", "CacaSurface::ResizeCanvas");

  VLOG(2) << "creating libcaca surface with from window " << (void*)window_;

  SkImageInfo info = SkImageInfo::Make(window_->bitmap_size().width(),
                                       window_->bitmap_size().height(),
                                       kN32_SkColorType,
                                       kPremul_SkAlphaType);

  surface_ = SkSurface::MakeRaster(info);
  if (!surface_)
    LOG(ERROR) << "Failed to create SkSurface";

  dither_.reset(caca_create_dither(info.bytesPerPixel() * 8,
                                   info.width(),
                                   info.height(),
                                   info.minRowBytes(),
                                   0x00ff0000,
                                   0x0000ff00,
                                   0x000000ff,
                                   0xff000000));
  if (!dither_)
    LOG(ERROR) << "failed to create dither";
}

void CacaSurface::PresentCanvas(const gfx::Rect& damage) {
  TRACE_EVENT0("ozone", "CacaSurface::PresentCanvas");

  SkPixmap pixmap;
  surface_->peekPixels(&pixmap);

  caca_canvas_t* canvas = caca_get_canvas(window_->display());
  caca_dither_bitmap(canvas, 0, 0, caca_get_canvas_width(canvas),
                     caca_get_canvas_height(canvas), dither_.get(),
                     static_cast<const uint8_t*>(pixmap.addr()));
  caca_refresh_display(window_->display());
}

std::unique_ptr<gfx::VSyncProvider> CacaSurface::CreateVSyncProvider() {
  return nullptr;
}

}  // namespace

CacaWindowManager::CacaWindowManager() {
}

int32_t CacaWindowManager::AddWindow(CacaWindow* window) {
  return windows_.Add(window);
}

void CacaWindowManager::RemoveWindow(int window_id, CacaWindow* window) {
  DCHECK_EQ(window, windows_.Lookup(window_id));
  windows_.Remove(window_id);
}

CacaWindowManager::~CacaWindowManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool CacaWindowManager::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return false;
}

std::unique_ptr<ui::SurfaceOzoneCanvas>
CacaWindowManager::CreateCanvasForWidget(gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CacaWindow* window = windows_.Lookup(widget);
  DCHECK(window);

  std::unique_ptr<CacaSurface> canvas(new CacaSurface(window));
  bool initialized = canvas->Initialize();
  DCHECK(initialized);
  return std::move(canvas);
}

}  // namespace ui
