// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_IMPLEMENTATION_H_
#define GPU_VULKAN_VULKAN_IMPLEMENTATION_H_

#include <vulkan/vulkan.h>

#include "gpu/vulkan/vulkan_export.h"
namespace gpu {

class VulkanCommandPool;

VULKAN_EXPORT bool InitializeVulkan();
VULKAN_EXPORT bool VulkanSupported();

VkInstance GetVulkanInstance();

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_WSI_API_IMPLEMENTATION_H_
