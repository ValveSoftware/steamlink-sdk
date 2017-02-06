// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl.h"

#include <dwmapi.h>

namespace gl {

EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay() {
  return GetDC(NULL);
}

}  // namespace gl
