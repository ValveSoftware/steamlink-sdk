// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __eglplatform_shim_h_
#define __eglplatform_shim_h_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHIM_EXPORT __attribute__((visibility("default")))

// Simple integral native window identifier.
// NB: Unlike EGLNativeWindowType, this will be shipped between processes.
typedef int ShimNativeWindowId;
#define SHIM_NO_WINDOW_ID ((ShimNativeWindowId)0)

// Opaque versions of EGL types (as used by ozone).
typedef intptr_t ShimEGLNativeDisplayType;
typedef intptr_t ShimEGLNativeWindowType;

// QueryString targets
#define SHIM_EGL_LIBRARY 0x1001
#define SHIM_GLES_LIBRARY 0x1002

// CreateWindow / QueryWindow attributes
#define SHIM_WINDOW_WIDTH 0x0001
#define SHIM_WINDOW_HEIGHT 0x0002

// Query global implementation information.
SHIM_EXPORT const char* ShimQueryString(int name);

// Init/terminate library.
SHIM_EXPORT bool ShimInitialize(void);
SHIM_EXPORT bool ShimTerminate(void);

// Create window handle & query window properties (called from browser process).
SHIM_EXPORT ShimNativeWindowId ShimCreateWindow(void);
SHIM_EXPORT bool ShimQueryWindow(ShimNativeWindowId window_id,
                                 int attribute,
                                 int* value);
SHIM_EXPORT bool ShimDestroyWindow(ShimNativeWindowId window_id);

// Manage actual EGL platform objects (called from GPU process).
SHIM_EXPORT ShimEGLNativeDisplayType ShimGetNativeDisplay(void);
SHIM_EXPORT ShimEGLNativeWindowType
    ShimGetNativeWindow(ShimNativeWindowId native_window_id);
SHIM_EXPORT bool ShimReleaseNativeWindow(ShimEGLNativeWindowType native_window);

#ifdef __cplusplus
}
#endif

#endif /* __eglplatform_shim_h */
