// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/platform_display_init_params.h"

#include "components/mus/gles2/gpu_state.h"
#include "components/mus/surfaces/surfaces_state.h"

namespace mus {
namespace ws {

PlatformDisplayInitParams::PlatformDisplayInitParams()
    : display_bounds(gfx::Rect(0, 0, 1024, 768)), display_id(1) {}

PlatformDisplayInitParams::PlatformDisplayInitParams(
    const PlatformDisplayInitParams& other) = default;
PlatformDisplayInitParams::~PlatformDisplayInitParams() {}

}  // namespace ws
}  // namespace mus
