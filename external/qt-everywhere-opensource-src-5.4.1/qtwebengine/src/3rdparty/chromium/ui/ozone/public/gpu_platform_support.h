// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_GPU_PLATFORM_SUPPORT_H_
#define UI_OZONE_PUBLIC_GPU_PLATFORM_SUPPORT_H_

#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ui/ozone/ozone_base_export.h"

namespace ui {

// Platform-specific object to support a GPU process.
//
// See GpuPlatformSupportHost for more context.
class OZONE_BASE_EXPORT GpuPlatformSupport : public IPC::Listener {
 public:
  GpuPlatformSupport();
  virtual ~GpuPlatformSupport();

  // Called when the GPU process is spun up & channel established.
  virtual void OnChannelEstablished(IPC::Sender* sender) = 0;
};

// Create a stub implementation.
OZONE_BASE_EXPORT GpuPlatformSupport* CreateStubGpuPlatformSupport();

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_GPU_PLATFORM_SUPPORT_H_
