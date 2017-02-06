// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_GPU_PLATFORM_SUPPORT_CAST_H_
#define UI_OZONE_PLATFORM_CAST_GPU_PLATFORM_SUPPORT_CAST_H_

#include "base/macros.h"
#include "ui/ozone/public/gpu_platform_support.h"

namespace ui {

class SurfaceFactoryCast;

// GpuPlatformSupport implementation for use with OzonePlatformCast.
class GpuPlatformSupportCast : public GpuPlatformSupport {
 public:
  explicit GpuPlatformSupportCast(SurfaceFactoryCast* parent);
  ~GpuPlatformSupportCast() override;

  // GpuPlatformSupport implementation:
  void OnChannelEstablished(IPC::Sender* sender) override {}
  bool OnMessageReceived(const IPC::Message& msg) override;
  IPC::MessageFilter* GetMessageFilter() override;

 private:
  SurfaceFactoryCast* parent_;

  DISALLOW_COPY_AND_ASSIGN(GpuPlatformSupportCast);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_GPU_PLATFORM_SUPPORT_CAST_H_
