// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_gpu_platform_support.h"

#include "ui/ozone/platform/drm/gpu/drm_thread_message_proxy.h"

namespace ui {

DrmGpuPlatformSupport::DrmGpuPlatformSupport(
    const scoped_refptr<DrmThreadMessageProxy>& filter)
    : filter_(filter) {}

DrmGpuPlatformSupport::~DrmGpuPlatformSupport() {}

void DrmGpuPlatformSupport::OnChannelEstablished(IPC::Sender* sender) {}

IPC::MessageFilter* DrmGpuPlatformSupport::GetMessageFilter() {
  return filter_.get();
}

bool DrmGpuPlatformSupport::OnMessageReceived(const IPC::Message& message) {
  return false;
}

}  // namespace ui
