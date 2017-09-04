// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_VULKAN_CONTEXT_PROVIDER_H_
#define CC_OUTPUT_VULKAN_CONTEXT_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"

namespace gpu {
class VulkanDeviceQueue;
}

namespace cc {

// The VulkanContextProvider groups sharing of vulkan objects synchronously.
class CC_EXPORT VulkanContextProvider
    : public base::RefCountedThreadSafe<VulkanContextProvider> {
 public:
  virtual gpu::VulkanDeviceQueue* GetDeviceQueue() = 0;

 protected:
  friend class base::RefCountedThreadSafe<VulkanContextProvider>;
  virtual ~VulkanContextProvider() {}
};

}  // namespace cc

#endif  // CC_OUTPUT_VULKAN_CONTEXT_PROVIDER_H_
