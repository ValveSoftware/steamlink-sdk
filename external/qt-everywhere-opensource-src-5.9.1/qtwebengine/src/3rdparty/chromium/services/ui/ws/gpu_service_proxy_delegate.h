// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_GPU_SERVICE_PROXY_DELEGATEH_
#define SERVICES_UI_WS_GPU_SERVICE_PROXY_DELEGATEH_

#include "base/memory/ref_counted.h"

namespace gpu {
class GpuChannelHost;
}

namespace ui {
namespace ws {

class GpuServiceProxyDelegate {
 public:
  virtual ~GpuServiceProxyDelegate() {}

  virtual void OnGpuChannelEstablished(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel) = 0;
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_GPU_SERVICE_PROXY_DELEGATEH_
