// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_EGL_UTIL_H_
#define UI_OZONE_COMMON_EGL_UTIL_H_

#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

bool LoadDefaultEGLGLES2Bindings(
    SurfaceFactoryOzone::AddGLLibraryCallback add_gl_library,
    SurfaceFactoryOzone::SetGLGetProcAddressProcCallback
        set_gl_get_proc_address);

bool LoadEGLGLES2Bindings(
    SurfaceFactoryOzone::AddGLLibraryCallback add_gl_library,
    SurfaceFactoryOzone::SetGLGetProcAddressProcCallback
        set_gl_get_proc_address,
    const char* egl_library_name,
    const char* gles_library_name);

void* /* EGLConfig */ ChooseEGLConfig(const EglConfigCallbacks& egl,
                                      const int32_t* attributes);

}  // namespace ui

#endif  // UI_OZONE_COMMON_EGL_UTIL_H_
