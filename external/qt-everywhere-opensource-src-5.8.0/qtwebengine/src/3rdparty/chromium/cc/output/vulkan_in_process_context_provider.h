// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
#define CC_OUTPUT_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <memory>

#include "cc/base/cc_export.h"
#include "cc/output/vulkan_context_provider.h"

namespace gpu {
class VulkanDeviceQueue;
}

namespace cc {

class CC_EXPORT VulkanInProcessContextProvider : public VulkanContextProvider {
 public:
  static scoped_refptr<VulkanInProcessContextProvider> Create();

  bool Initialize();
  void Destroy();

  // VulkanContextProvider implementation
  gpu::VulkanDeviceQueue* GetDeviceQueue() override;

 protected:
  VulkanInProcessContextProvider();
  ~VulkanInProcessContextProvider() override;

 private:
#if defined(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue_;
#endif

  DISALLOW_COPY_AND_ASSIGN(VulkanInProcessContextProvider);
};

}  // namespace cc

#endif  // CC_OUTPUT_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
