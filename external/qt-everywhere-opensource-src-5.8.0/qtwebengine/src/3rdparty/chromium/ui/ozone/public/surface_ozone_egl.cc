// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {

EglConfigCallbacks::EglConfigCallbacks() {}

EglConfigCallbacks::EglConfigCallbacks(const EglConfigCallbacks& other) =
    default;

EglConfigCallbacks::~EglConfigCallbacks() {}

bool SurfaceOzoneEGL::IsUniversalDisplayLinkDevice() {
  return false;
}

}  // namespace ui
