// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_SURFACE_FACTORY_CAST_H_
#define UI_OZONE_PLATFORM_CAST_SURFACE_FACTORY_CAST_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace chromecast {
class CastEglPlatform;
}

namespace ui {

// SurfaceFactoryOzone implementation for OzonePlatformCast.
class SurfaceFactoryCast : public SurfaceFactoryOzone {
 public:
  SurfaceFactoryCast();
  explicit SurfaceFactoryCast(
      std::unique_ptr<chromecast::CastEglPlatform> egl_platform);
  ~SurfaceFactoryCast() override;

  // SurfaceFactoryOzone implementation:
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  intptr_t GetNativeDisplay() override;
  std::unique_ptr<SurfaceOzoneEGL> CreateEGLSurfaceForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) override;

  intptr_t GetNativeWindow();
  bool ResizeDisplay(gfx::Size viewport_size);
  void ChildDestroyed();
  void TerminateDisplay();
  void ShutdownHardware();

  // API for keeping track of overlays per frame for logging purposes
  void OnSwapBuffers();
  void OnOverlayScheduled(const gfx::Rect& display_bounds);

 private:
  enum HardwareState { kUninitialized, kInitialized, kFailed };

  void CreateDisplayTypeAndWindowIfNeeded();
  void DestroyDisplayTypeAndWindow();
  void DestroyWindow();
  void InitializeHardware();

  HardwareState state_;
  void* display_type_;
  bool have_display_type_;
  void* window_;
  gfx::Size display_size_;
  gfx::Size new_display_size_;
  std::unique_ptr<chromecast::CastEglPlatform> egl_platform_;

  // Overlays scheduled in current and previous frames:
  int overlay_count_;
  gfx::Rect overlay_bounds_;
  int previous_frame_overlay_count_;
  gfx::Rect previous_frame_overlay_bounds_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceFactoryCast);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_SURFACE_FACTORY_CAST_H_
