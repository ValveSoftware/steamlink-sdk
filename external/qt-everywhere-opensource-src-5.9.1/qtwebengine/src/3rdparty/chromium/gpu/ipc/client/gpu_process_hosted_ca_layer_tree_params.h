// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <IOSurface/IOSurface.h>

#include "base/mac/scoped_cftyperef.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "gpu/gpu_export.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

struct GPU_EXPORT GpuProcessHostedCALayerTreeParamsMac {
  GpuProcessHostedCALayerTreeParamsMac();
  ~GpuProcessHostedCALayerTreeParamsMac();

  CAContextID ca_context_id = 0;
  bool fullscreen_low_power_ca_context_valid = false;
  CAContextID fullscreen_low_power_ca_context_id = 0;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  gfx::Size pixel_size;
  float scale_factor = 1;
  TextureInUseResponses responses;
};

}  // namespace gpu
