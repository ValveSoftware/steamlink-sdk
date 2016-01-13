// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/chromeos/display_mode_dri.h"

namespace ui {

DisplayModeDri::DisplayModeDri(const drmModeModeInfo& mode)
    : DisplayMode(gfx::Size(mode.hdisplay, mode.vdisplay),
                  mode.flags & DRM_MODE_FLAG_INTERLACE,
                  mode.vrefresh),
      mode_info_(mode) {}

DisplayModeDri::~DisplayModeDri() {}

}  // namespace ui
