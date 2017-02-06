// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/surface_factory_cast.h"

#include <EGL/egl.h>
#include <dlfcn.h>

#include <utility>

#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromecast/public/cast_egl_platform.h"
#include "chromecast/public/graphics_types.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/cast/surface_ozone_egl_cast.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

using chromecast::CastEglPlatform;

namespace ui {

namespace {

typedef EGLDisplay (*EGLGetDisplayFn)(NativeDisplayType);
typedef EGLBoolean (*EGLTerminateFn)(EGLDisplay);

chromecast::Size FromGfxSize(const gfx::Size& size) {
  return chromecast::Size(size.width(), size.height());
}

// Initial display size to create, needed before first window is created.
gfx::Size GetInitialDisplaySize() {
  return gfx::Size(1280, 720);
}

// Hard lower bound on display resolution
gfx::Size GetMinDisplaySize() {
  return gfx::Size(1280, 720);
}

class DummySurface : public SurfaceOzoneCanvas {
 public:
  DummySurface() {}
  ~DummySurface() override {}

  // SurfaceOzoneCanvas implementation:
  sk_sp<SkSurface> GetSurface() override { return surface_; }

  void ResizeCanvas(const gfx::Size& viewport_size) override {
    surface_ = SkSurface::MakeRaster(SkImageInfo::MakeN32Premul(
        viewport_size.width(), viewport_size.height()));
  }

  void PresentCanvas(const gfx::Rect& damage) override {}

  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    return nullptr;
  }

 private:
  sk_sp<SkSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(DummySurface);
};

}  // namespace

SurfaceFactoryCast::SurfaceFactoryCast() : SurfaceFactoryCast(nullptr) {}

SurfaceFactoryCast::SurfaceFactoryCast(
    std::unique_ptr<CastEglPlatform> egl_platform)
    : state_(kUninitialized),
      display_type_(0),
      have_display_type_(false),
      window_(0),
      display_size_(GetInitialDisplaySize()),
      new_display_size_(GetInitialDisplaySize()),
      egl_platform_(std::move(egl_platform)),
      overlay_count_(0),
      previous_frame_overlay_count_(0) {}

SurfaceFactoryCast::~SurfaceFactoryCast() {
  ShutdownHardware();
}

void SurfaceFactoryCast::InitializeHardware() {
  if (state_ == kInitialized) {
    return;
  }
  CHECK_EQ(state_, kUninitialized);

  if (egl_platform_->InitializeHardware()) {
    state_ = kInitialized;
  } else {
    ShutdownHardware();
    state_ = kFailed;
  }
}

void SurfaceFactoryCast::TerminateDisplay() {
  void* egl_lib_handle = egl_platform_->GetEglLibrary();
  if (!egl_lib_handle)
    return;

  EGLGetDisplayFn get_display =
      reinterpret_cast<EGLGetDisplayFn>(dlsym(egl_lib_handle, "eglGetDisplay"));
  EGLTerminateFn terminate =
      reinterpret_cast<EGLTerminateFn>(dlsym(egl_lib_handle, "eglTerminate"));
  DCHECK(get_display);
  DCHECK(terminate);

  EGLDisplay display = get_display(GetNativeDisplay());
  DCHECK_NE(display, EGL_NO_DISPLAY);

  EGLBoolean terminate_result = terminate(display);
  DCHECK_EQ(terminate_result, static_cast<EGLBoolean>(EGL_TRUE));
}

void SurfaceFactoryCast::ShutdownHardware() {
  if (state_ != kInitialized)
    return;

  DestroyDisplayTypeAndWindow();

  egl_platform_->ShutdownHardware();

  state_ = kUninitialized;
}

void SurfaceFactoryCast::OnSwapBuffers() {
  DCHECK(overlay_count_ == 0 || overlay_count_ == 1);

  // Logging for overlays to help diagnose bugs when nothing is visible on
  // screen.  Logging this every frame would be overwhelming, so we just
  // log on the transitions from 0 overlays -> 1 overlay and vice versa.
  if (overlay_count_ == 0 && previous_frame_overlay_count_ != 0) {
    LOG(INFO) << "Overlays deactivated";
  } else if (overlay_count_ != 0 && previous_frame_overlay_count_ == 0) {
    LOG(INFO) << "Overlays activated: " << overlay_bounds_.ToString();
  } else if (overlay_count_ == previous_frame_overlay_count_ &&
             overlay_bounds_ != previous_frame_overlay_bounds_) {
    LOG(INFO) << "Overlay geometry changed to " << overlay_bounds_.ToString();
  }

  previous_frame_overlay_count_ = overlay_count_;
  previous_frame_overlay_bounds_ = overlay_bounds_;
  overlay_count_ = 0;
}

void SurfaceFactoryCast::OnOverlayScheduled(const gfx::Rect& display_bounds) {
  ++overlay_count_;
  overlay_bounds_ = display_bounds;
}

std::unique_ptr<SurfaceOzoneCanvas> SurfaceFactoryCast::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  // Software canvas support only in headless mode
  if (egl_platform_)
    return nullptr;
  return base::WrapUnique<SurfaceOzoneCanvas>(new DummySurface());
}

intptr_t SurfaceFactoryCast::GetNativeDisplay() {
  CreateDisplayTypeAndWindowIfNeeded();
  return reinterpret_cast<intptr_t>(display_type_);
}

void SurfaceFactoryCast::CreateDisplayTypeAndWindowIfNeeded() {
  if (state_ == kUninitialized) {
    InitializeHardware();
  }
  if (new_display_size_ != display_size_) {
    DestroyDisplayTypeAndWindow();
    display_size_ = new_display_size_;
  }
  DCHECK_EQ(state_, kInitialized);
  if (!have_display_type_) {
    chromecast::Size create_size = FromGfxSize(display_size_);
    display_type_ = egl_platform_->CreateDisplayType(create_size);
    have_display_type_ = true;
  }
  if (!window_) {
    chromecast::Size create_size = FromGfxSize(display_size_);
    window_ = egl_platform_->CreateWindow(display_type_, create_size);
    if (!window_) {
      DestroyDisplayTypeAndWindow();
      state_ = kFailed;
      LOG(FATAL) << "Create EGLNativeWindowType(" << display_size_.ToString()
                 << ") failed.";
    }
  }
}

intptr_t SurfaceFactoryCast::GetNativeWindow() {
  CreateDisplayTypeAndWindowIfNeeded();
  return reinterpret_cast<intptr_t>(window_);
}

bool SurfaceFactoryCast::ResizeDisplay(gfx::Size size) {
  // set size to at least 1280x720 even if passed 1x1
  size.SetToMax(GetMinDisplaySize());
  if (have_display_type_ && size != display_size_) {
    DestroyDisplayTypeAndWindow();
  }
  display_size_ = size;
  return true;
}

void SurfaceFactoryCast::DestroyWindow() {
  if (window_) {
    egl_platform_->DestroyWindow(window_);
    window_ = 0;
  }
}

void SurfaceFactoryCast::DestroyDisplayTypeAndWindow() {
  DestroyWindow();
  if (have_display_type_) {
    egl_platform_->DestroyDisplayType(display_type_);
    display_type_ = 0;
    have_display_type_ = false;
  }
}

std::unique_ptr<SurfaceOzoneEGL> SurfaceFactoryCast::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget widget) {
  new_display_size_ = gfx::Size(widget >> 16, widget & 0xFFFF);
  new_display_size_.SetToMax(GetMinDisplaySize());
  return base::WrapUnique<SurfaceOzoneEGL>(new SurfaceOzoneEglCast(this));
}

void SurfaceFactoryCast::ChildDestroyed() {
  if (egl_platform_->MultipleSurfaceUnsupported())
    DestroyWindow();
}

scoped_refptr<NativePixmap> SurfaceFactoryCast::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  class CastPixmap : public NativePixmap {
   public:
    CastPixmap(SurfaceFactoryCast* parent) : parent_(parent) {}

    void* GetEGLClientBuffer() const override {
      // TODO(halliwell): try to implement this through CastEglPlatform.
      return nullptr;
    }
    bool AreDmaBufFdsValid() const override { return false; }
    size_t GetDmaBufFdCount() const override { return 0; }
    int GetDmaBufFd(size_t plane) const override { return -1; }
    int GetDmaBufPitch(size_t plane) const override { return 0; }
    int GetDmaBufOffset(size_t plane) const override { return 0; }
    gfx::BufferFormat GetBufferFormat() const override {
      return gfx::BufferFormat::BGRA_8888;
    }
    gfx::Size GetBufferSize() const override { return gfx::Size(); }

    bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                              int plane_z_order,
                              gfx::OverlayTransform plane_transform,
                              const gfx::Rect& display_bounds,
                              const gfx::RectF& crop_rect) override {
      parent_->OnOverlayScheduled(display_bounds);
      return true;
    }
    void SetProcessingCallback(
        const ProcessingCallback& processing_callback) override {}
    gfx::NativePixmapHandle ExportHandle() override {
      return gfx::NativePixmapHandle();
    }

   private:
    ~CastPixmap() override {}

    SurfaceFactoryCast* parent_;

    DISALLOW_COPY_AND_ASSIGN(CastPixmap);
  };
  return make_scoped_refptr(new CastPixmap(this));
}

bool SurfaceFactoryCast::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  if (state_ != kInitialized) {
    InitializeHardware();
    if (state_ != kInitialized) {
      return false;
    }
  }

  void* lib_egl = egl_platform_->GetEglLibrary();
  void* lib_gles2 = egl_platform_->GetGles2Library();
  GLGetProcAddressProc gl_proc = egl_platform_->GetGLProcAddressProc();
  if (!lib_egl || !lib_gles2 || !gl_proc) {
    return false;
  }

  set_gl_get_proc_address.Run(gl_proc);
  add_gl_library.Run(lib_egl);
  add_gl_library.Run(lib_gles2);
  return true;
}

}  // namespace ui
