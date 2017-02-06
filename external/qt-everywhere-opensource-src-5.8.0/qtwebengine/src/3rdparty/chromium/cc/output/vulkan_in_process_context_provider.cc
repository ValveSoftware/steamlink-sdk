// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/vulkan_in_process_context_provider.h"

#if defined(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif  // defined(ENABLE_VULKAN)

namespace cc {

scoped_refptr<VulkanInProcessContextProvider>
VulkanInProcessContextProvider::Create() {
#if defined(ENABLE_VULKAN)
  if (!gpu::VulkanSupported())
    return nullptr;

  scoped_refptr<VulkanInProcessContextProvider> context_provider(
      new VulkanInProcessContextProvider);
  if (!context_provider->Initialize())
    return nullptr;
  return context_provider;
#else
  return nullptr;
#endif
}

bool VulkanInProcessContextProvider::Initialize() {
#if defined(ENABLE_VULKAN)
  DCHECK(!device_queue_);
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue(
      new gpu::VulkanDeviceQueue);
  if (!device_queue->Initialize(
          gpu::VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
          gpu::VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG)) {
    device_queue->Destroy();
    return false;
  }

  device_queue_ = std::move(device_queue);
  return true;
#else
  return false;
#endif
}

void VulkanInProcessContextProvider::Destroy() {
#if defined(ENABLE_VULKAN)
  if (device_queue_) {
    device_queue_->Destroy();
    device_queue_.reset();
  }
#endif
}

gpu::VulkanDeviceQueue* VulkanInProcessContextProvider::GetDeviceQueue() {
#if defined(ENABLE_VULKAN)
  return device_queue_.get();
#else
  return nullptr;
#endif
}

VulkanInProcessContextProvider::VulkanInProcessContextProvider() {}

VulkanInProcessContextProvider::~VulkanInProcessContextProvider() {
  Destroy();
}

}  // namespace cc
