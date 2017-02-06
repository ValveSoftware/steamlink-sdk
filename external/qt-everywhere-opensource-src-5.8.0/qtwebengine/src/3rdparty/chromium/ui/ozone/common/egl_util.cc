// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/egl_util.h"

#include "base/files/file_path.h"

namespace ui {
namespace {

const char kDefaultEglSoname[] = "libEGL.so.1";
const char kDefaultGlesSoname[] = "libGLESv2.so.2";

}  // namespace

bool LoadDefaultEGLGLES2Bindings(
    SurfaceFactoryOzone::AddGLLibraryCallback add_gl_library,
    SurfaceFactoryOzone::SetGLGetProcAddressProcCallback
        set_gl_get_proc_address) {
  return LoadEGLGLES2Bindings(add_gl_library, set_gl_get_proc_address,
                              kDefaultEglSoname, kDefaultGlesSoname);
}

bool LoadEGLGLES2Bindings(
    SurfaceFactoryOzone::AddGLLibraryCallback add_gl_library,
    SurfaceFactoryOzone::SetGLGetProcAddressProcCallback
        set_gl_get_proc_address,
    const char* egl_library_name,
    const char* gles_library_name) {
  base::NativeLibraryLoadError error;
  base::NativeLibrary gles_library =
      base::LoadNativeLibrary(base::FilePath(gles_library_name), &error);
  if (!gles_library) {
    LOG(WARNING) << "Failed to load GLES library: " << error.ToString();
    return false;
  }

  base::NativeLibrary egl_library =
      base::LoadNativeLibrary(base::FilePath(egl_library_name), &error);
  if (!egl_library) {
    LOG(WARNING) << "Failed to load EGL library: " << error.ToString();
    base::UnloadNativeLibrary(gles_library);
    return false;
  }

  SurfaceFactoryOzone::GLGetProcAddressProc get_proc_address =
      reinterpret_cast<SurfaceFactoryOzone::GLGetProcAddressProc>(
          base::GetFunctionPointerFromNativeLibrary(egl_library,
                                                    "eglGetProcAddress"));
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

void* /* EGLConfig */ ChooseEGLConfig(const EglConfigCallbacks& egl,
                                      const int32_t* attributes) {
  void* config;
  int32_t num_configs;
  if (!egl.choose_config.Run(attributes, nullptr, 0, &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << egl.get_last_error_string.Run();
    return nullptr;
  }

  if (num_configs == 0) {
    LOG(ERROR) << "No suitable EGL configs found.";
    return nullptr;
  }

  if (!egl.choose_config.Run(attributes, &config, 1, &num_configs)) {
    LOG(ERROR) << "eglChooseConfig failed with error "
               << egl.get_last_error_string.Run();
    return nullptr;
  }
  return config;
}

}  // namespace ui
