// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_PLATFORM_SUPPORT_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_PLATFORM_SUPPORT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/ozone/public/gpu_platform_support.h"

namespace ui {

class DrmThread;
class DrmThreadMessageProxy;

class DrmGpuPlatformSupport : public GpuPlatformSupport {
 public:
  explicit DrmGpuPlatformSupport(
      const scoped_refptr<DrmThreadMessageProxy>& filter);
  ~DrmGpuPlatformSupport() override;

  // GpuPlatformSupport:
  void OnChannelEstablished(IPC::Sender* sender) override;
  IPC::MessageFilter* GetMessageFilter() override;

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  scoped_refptr<DrmThreadMessageProxy> filter_;

  DISALLOW_COPY_AND_ASSIGN(DrmGpuPlatformSupport);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_GPU_PLATFORM_SUPPORT_H_
