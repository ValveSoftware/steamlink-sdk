// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_surface_factory.h"

#include <EGL/egl.h>
#include <gbm.h>

#include "base/files/file_path.h"
#include "ui/ozone/platform/dri/buffer_data.h"
#include "ui/ozone/platform/dri/dri_vsync_provider.h"
#include "ui/ozone/platform/dri/gbm_surface.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/scanout_surface.h"
#include "ui/ozone/platform/dri/screen_manager.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

namespace {

class GbmSurfaceAdapter : public ui::SurfaceOzoneEGL {
 public:
  GbmSurfaceAdapter(const base::WeakPtr<HardwareDisplayController>& controller);
  virtual ~GbmSurfaceAdapter();

  // SurfaceOzoneEGL:
  virtual intptr_t GetNativeWindow() OVERRIDE;
  virtual bool ResizeNativeWindow(const gfx::Size& viewport_size) OVERRIDE;
  virtual bool OnSwapBuffers() OVERRIDE;
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE;

 private:
  base::WeakPtr<HardwareDisplayController> controller_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceAdapter);
};

GbmSurfaceAdapter::GbmSurfaceAdapter(
    const base::WeakPtr<HardwareDisplayController>& controller)
    : controller_(controller) {}

GbmSurfaceAdapter::~GbmSurfaceAdapter() {}

intptr_t GbmSurfaceAdapter::GetNativeWindow() {
  if (!controller_)
    return 0;

  return reinterpret_cast<intptr_t>(
      static_cast<GbmSurface*>(controller_->surface())->native_surface());
}

bool GbmSurfaceAdapter::ResizeNativeWindow(const gfx::Size& viewport_size) {
  NOTIMPLEMENTED();
  return false;
}

bool GbmSurfaceAdapter::OnSwapBuffers() {
  if (!controller_)
    return false;

  static_cast<GbmSurface*>(controller_->surface())->LockCurrentDrawable();
  if (controller_->SchedulePageFlip()) {
    controller_->WaitForPageFlipEvent();
    return true;
  }

  return false;
}

scoped_ptr<gfx::VSyncProvider> GbmSurfaceAdapter::CreateVSyncProvider() {
  return scoped_ptr<gfx::VSyncProvider>(new DriVSyncProvider(controller_));
}

}  // namespace

GbmSurfaceFactory::GbmSurfaceFactory(DriWrapper* dri,
                                     gbm_device* device,
                                     ScreenManager* screen_manager)
    : DriSurfaceFactory(dri, screen_manager),
      device_(device) {}

GbmSurfaceFactory::~GbmSurfaceFactory() {}

intptr_t GbmSurfaceFactory::GetNativeDisplay() {
  CHECK(state_ == INITIALIZED);
  return reinterpret_cast<intptr_t>(device_);
}

const int32* GbmSurfaceFactory::GetEGLSurfaceProperties(
    const int32* desired_list) {
  static const int32 kConfigAttribs[] = {
    EGL_BUFFER_SIZE, 32,
    EGL_ALPHA_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_NONE
  };

  return kConfigAttribs;
}

bool GbmSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  base::NativeLibraryLoadError error;
  base::NativeLibrary gles_library = base::LoadNativeLibrary(
      base::FilePath("libGLESv2.so.2"),
      &error);
  if (!gles_library) {
    LOG(WARNING) << "Failed to load GLES library: " << error.ToString();
    return false;
  }

  base::NativeLibrary egl_library = base::LoadNativeLibrary(
      base::FilePath("libEGL.so.1"),
      &error);
  if (!egl_library) {
    LOG(WARNING) << "Failed to load EGL library: " << error.ToString();
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  GLGetProcAddressProc get_proc_address =
      reinterpret_cast<GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(
              egl_library, "eglGetProcAddress"));
  if (!get_proc_address) {
    LOG(ERROR) << "eglGetProcAddress not found.";
    base::UnloadNativeLibrary(egl_library);
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  set_gl_get_proc_address.Run(get_proc_address);
  add_gl_library.Run(egl_library);
  add_gl_library.Run(gles_library);

  return true;
}

scoped_ptr<ui::SurfaceOzoneEGL> GbmSurfaceFactory::CreateEGLSurfaceForWidget(
    gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  ResetCursor(w);

  return scoped_ptr<ui::SurfaceOzoneEGL>(
      new GbmSurfaceAdapter(screen_manager_->GetDisplayController(w)));
}

gfx::NativeBufferOzone GbmSurfaceFactory::CreateNativeBuffer(
    gfx::Size size,
    BufferFormat format) {
  uint32_t gbm_format = 0;
  switch (format) {
    case SurfaceFactoryOzone::UNKNOWN:
      return 0;
    // TODO(alexst): Setting this to XRGB for now to allow presentation
    // as a primary plane but disallowing overlay transparency. Address this
    // to allow both use cases.
    case SurfaceFactoryOzone::RGBA_8888:
      gbm_format = GBM_FORMAT_XRGB8888;
      break;
    case SurfaceFactoryOzone::RGB_888:
      gbm_format = GBM_FORMAT_RGB888;
      break;
  }
  gbm_bo* buffer_object =
      gbm_bo_create(device_,
                    size.width(),
                    size.height(),
                    gbm_format,
                    GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
  if (!buffer_object)
    return 0;

  BufferData* data = BufferData::CreateData(drm_, buffer_object);
  DCHECK(data) << "Failed to associate the buffer with the controller";

  return reinterpret_cast<gfx::NativeBufferOzone>(buffer_object);
}

}  // namespace ui
