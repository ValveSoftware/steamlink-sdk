// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_SURFACE_OZONE_EGL_H_
#define UI_OZONE_PUBLIC_SURFACE_OZONE_EGL_H_

#include <memory>

#include "base/callback.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/ozone_base_export.h"

namespace gfx {
class Size;
class VSyncProvider;
}

namespace ui {
class NativePixmap;

typedef base::Callback<void(gfx::SwapResult)> SwapCompletionCallback;

// Holds callbacks to functions for configuring EGL on platform.
struct OZONE_BASE_EXPORT EglConfigCallbacks {
  EglConfigCallbacks();
  EglConfigCallbacks(const EglConfigCallbacks& other);
  ~EglConfigCallbacks();
  base::Callback<bool(const int32_t* attribs,
                      void** /* EGLConfig* */ configs,
                      int32_t config_size,
                      int32_t* num_configs)>
      choose_config;
  base::Callback<
      bool(void* /* EGLConfig */ config, int32_t attribute, int32_t* value)>
      get_config_attribute;
  base::Callback<const char*()> get_last_error_string;
};

// The platform-specific part of an EGL surface.
//
// This class owns any bits that the ozone implementation needs freed when
// the EGL surface is destroyed.
class OZONE_BASE_EXPORT SurfaceOzoneEGL {
 public:
  virtual ~SurfaceOzoneEGL() {}

  // Returns the EGL native window for rendering onto this surface.
  // This can be used to to create a GLSurface.
  virtual intptr_t /* EGLNativeWindowType */ GetNativeWindow() = 0;

  // Attempts to resize the EGL native window to match the viewport
  // size.
  virtual bool ResizeNativeWindow(const gfx::Size& viewport_size) = 0;

  // Called after we swap buffers. This is usually a no-op but can
  // be used to present the new front buffer if the platform requires this.
  virtual bool OnSwapBuffers() = 0;

  // Called after we swap buffers. This is usually a no-op but can
  // be used to present the new front buffer if the platform requires this.
  // The callback should be run on the calling thread
  // (i.e. same thread SwapBuffersAsync is called).
  virtual void OnSwapBuffersAsync(const SwapCompletionCallback& callback) = 0;

  // Returns a gfx::VsyncProvider for this surface. Note that this may be
  // called after we have entered the sandbox so if there are operations (e.g.
  // opening a file descriptor providing vsync events) that must be done
  // outside of the sandbox, they must have been completed in
  // InitializeHardware. Returns an empty scoped_ptr on error.
  virtual std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() = 0;

  // Returns true if the surface is created on a UDL device.
  virtual bool IsUniversalDisplayLinkDevice();

  // Returns the EGL configuration to use for this surface. The default EGL
  // configuration will be used if this returns nullptr.
  virtual void* /* EGLConfig */ GetEGLSurfaceConfig(
      const EglConfigCallbacks& egl) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_SURFACE_OZONE_EGL_H_
