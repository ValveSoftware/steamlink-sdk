// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl.h"

#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gl {

EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay() {
  return ui::OzonePlatform::GetInstance()
      ->GetSurfaceFactoryOzone()
      ->GetNativeDisplay();
}

}  // namespace gl
