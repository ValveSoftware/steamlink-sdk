// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_
#define COMPONENTS_MUS_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/rect.h"

namespace shell {
class Connector;
}

namespace mus {

class GpuState;
class SurfacesState;

namespace ws {

struct PlatformDisplayInitParams {
  PlatformDisplayInitParams();
  PlatformDisplayInitParams(const PlatformDisplayInitParams& other);
  ~PlatformDisplayInitParams();

  scoped_refptr<GpuState> gpu_state;
  scoped_refptr<SurfacesState> surfaces_state;

  gfx::Rect display_bounds;
  int64_t display_id;
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_PLATFORM_DISPLAY_INIT_PARAMS_H_
