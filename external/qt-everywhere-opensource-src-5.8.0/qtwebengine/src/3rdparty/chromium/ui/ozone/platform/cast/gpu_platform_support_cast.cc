// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/gpu_platform_support_cast.h"

#include "ui/ozone/platform/cast/surface_factory_cast.h"

namespace ui {

GpuPlatformSupportCast::GpuPlatformSupportCast(SurfaceFactoryCast* parent)
    : parent_(parent) {
  DCHECK(parent_);
}

GpuPlatformSupportCast::~GpuPlatformSupportCast() {
  // eglTerminate must be called first on display before releasing resources
  // and shutting down hardware
  parent_->TerminateDisplay();
  parent_->ShutdownHardware();
}

bool GpuPlatformSupportCast::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

IPC::MessageFilter* GpuPlatformSupportCast::GetMessageFilter() {
  return nullptr;
}

}  // namespace ui
