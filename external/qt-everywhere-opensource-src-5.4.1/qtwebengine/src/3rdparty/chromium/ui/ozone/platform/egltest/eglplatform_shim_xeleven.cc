// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/egltest/eglplatform_shim.h"

#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#ifdef __cplusplus
extern "C" {
#endif

Display* g_display;

const int kDefaultX = 0;
const int kDefaultY = 0;
const int kDefaultWidth = 800;
const int kDefaultHeight = 600;
const int kDefaultBorderWidth = 0;

const char* ShimQueryString(int name) {
  switch (name) {
    case SHIM_EGL_LIBRARY:
      return "libEGL.so.1";
    case SHIM_GLES_LIBRARY:
      return "libGLESv2.so.2";
    default:
      return NULL;
  }
}

bool ShimInitialize(void) {
  g_display = XOpenDisplay(NULL);
  return g_display != NULL;
}

bool ShimTerminate(void) {
  XCloseDisplay(g_display);
  return true;
}

ShimNativeWindowId ShimCreateWindow(void) {
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.event_mask = 0;

  Window window = XCreateWindow(g_display,
                                DefaultRootWindow(g_display),
                                kDefaultX,
                                kDefaultY,
                                kDefaultWidth,
                                kDefaultHeight,
                                kDefaultBorderWidth,
                                CopyFromParent,
                                InputOutput,
                                CopyFromParent,
                                CWEventMask,
                                &swa);

  XMapWindow(g_display, window);
  XStoreName(g_display, window, "EGL test");
  XFlush(g_display);

  return window;
}

bool ShimQueryWindow(ShimNativeWindowId window_id, int attribute, int* value) {
  XWindowAttributes window_attributes;
  switch (attribute) {
    case SHIM_WINDOW_WIDTH:
      XGetWindowAttributes(g_display, window_id, &window_attributes);
      *value = window_attributes.width;
      return true;
    case SHIM_WINDOW_HEIGHT:
      XGetWindowAttributes(g_display, window_id, &window_attributes);
      *value = window_attributes.height;
      return true;
    default:
      return false;
  }
}

bool ShimDestroyWindow(ShimNativeWindowId window_id) {
  XDestroyWindow(g_display, window_id);
  return true;
}

ShimEGLNativeDisplayType ShimGetNativeDisplay(void) {
  return reinterpret_cast<ShimEGLNativeDisplayType>(g_display);
}

ShimEGLNativeWindowType ShimGetNativeWindow(
    ShimNativeWindowId native_window_id) {
  return native_window_id;
}

bool ShimReleaseNativeWindow(ShimEGLNativeWindowType native_window) {
  return true;
}

#ifdef __cplusplus
}
#endif
