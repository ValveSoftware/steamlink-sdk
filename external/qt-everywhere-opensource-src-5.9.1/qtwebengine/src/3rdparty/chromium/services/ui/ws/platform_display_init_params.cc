// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/platform_display_init_params.h"

#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/server_window.h"
#include "ui/display/display.h"

namespace ui {
namespace ws {

PlatformDisplayInitParams::PlatformDisplayInitParams()
    : display_id(display::Display::kInvalidDisplayID) {}

PlatformDisplayInitParams::PlatformDisplayInitParams(
    const PlatformDisplayInitParams& other) = default;

PlatformDisplayInitParams::~PlatformDisplayInitParams() {}

}  // namespace ws
}  // namespace ui
