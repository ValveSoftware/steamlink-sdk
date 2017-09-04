// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_surface.h"

#include <vulkan/vulkan.h>

#include "base/macros.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_platform.h"
#include "gpu/vulkan/vulkan_swap_chain.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"
#endif  // defined(USE_X11)

namespace gpu {

namespace {
const VkFormat kPreferredVkFormats32[] = {
    VK_FORMAT_B8G8R8A8_UNORM,  // FORMAT_BGRA8888,
    VK_FORMAT_R8G8B8A8_UNORM,  // FORMAT_RGBA8888,
};

const VkFormat kPreferredVkFormats16[] = {
    VK_FORMAT_R5G6B5_UNORM_PACK16,  // FORMAT_RGB565,
};

}  // namespace

class VulkanWSISurface : public VulkanSurface {
 public:
  explicit VulkanWSISurface(gfx::AcceleratedWidget window) : window_(window) {}

  ~VulkanWSISurface() override {
    DCHECK_EQ(static_cast<VkSurfaceKHR>(VK_NULL_HANDLE), surface_);
  }

  bool Initialize(VulkanDeviceQueue* device_queue,
                  VulkanSurface::Format format) override {
    DCHECK(format >= 0 && format < NUM_SURFACE_FORMATS);
    DCHECK_EQ(static_cast<VkSurfaceKHR>(VK_NULL_HANDLE), surface_);
    VkResult result = VK_SUCCESS;

#if defined(VK_USE_PLATFORM_XLIB_KHR)
    VkXlibSurfaceCreateInfoKHR surface_create_info = {};
    surface_create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surface_create_info.dpy = gfx::GetXDisplay();
    surface_create_info.window = window_;
    result = vkCreateXlibSurfaceKHR(GetVulkanInstance(), &surface_create_info,
                                    nullptr, &surface_);
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkCreateXlibSurfaceKHR() failed: " << result;
      return false;
    }
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    VkAndroidSurfaceCreateInfoKHR surface_create_info = {};
    surface_create_info.sType =
        VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surface_create_info.window = window_;
    result = vkCreateAndroidSurfaceKHR(
        GetVulkanInstance(), &surface_create_info, nullptr, &surface_);
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkCreateAndroidSurfaceKHR() failed: " << result;
      return false;
    }
#else
#error Unsupported Vulkan Platform.
#endif

    DCHECK_NE(static_cast<VkSurfaceKHR>(VK_NULL_HANDLE), surface_);
    DCHECK(device_queue);
    device_queue_ = device_queue;

    // Get list of supported formats.
    uint32_t format_count = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        device_queue_->GetVulkanPhysicalDevice(), surface_, &format_count,
        nullptr);
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceFormatsKHR() failed: "
                  << result;
      return false;
    }

    std::vector<VkSurfaceFormatKHR> formats(format_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        device_queue_->GetVulkanPhysicalDevice(), surface_, &format_count,
        formats.data());
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceFormatsKHR() failed: "
                  << result;
      return false;
    }

    const VkFormat* preferred_formats = (format == FORMAT_RGBA_32)
                                            ? kPreferredVkFormats32
                                            : kPreferredVkFormats16;
    unsigned int size = (format == FORMAT_RGBA_32)
                            ? arraysize(kPreferredVkFormats32)
                            : arraysize(kPreferredVkFormats16);

    if (formats.size() == 1 && VK_FORMAT_UNDEFINED == formats[0].format) {
      surface_format_.format = preferred_formats[0];
      surface_format_.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    } else {
      bool format_set = false;
      for (VkSurfaceFormatKHR supported_format : formats) {
        unsigned int counter = 0;
        while (counter < size && format_set == false) {
          if (supported_format.format == preferred_formats[counter]) {
            surface_format_ = supported_format;
            surface_format_.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
            format_set = true;
          }
          counter++;
        }
        if (format_set)
          break;
      }
      if (!format_set) {
        DLOG(ERROR) << "Format not supported.";
        return false;
      }
    }

    // Get Surface Information.
    VkSurfaceCapabilitiesKHR surface_caps;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device_queue_->GetVulkanPhysicalDevice(), surface_, &surface_caps);
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() failed: "
                  << result;
      return false;
    }

    // These are actual surfaces so the current extent should be defined.
    DCHECK_NE(UINT_MAX, surface_caps.currentExtent.width);
    DCHECK_NE(UINT_MAX, surface_caps.currentExtent.height);
    size_ = gfx::Size(surface_caps.currentExtent.width,
                      surface_caps.currentExtent.height);

    // Create Swapchain.
    if (!swap_chain_.Initialize(device_queue_, surface_, surface_caps,
                                surface_format_)) {
      return false;
    }

    return true;
  }

  void Destroy() override {
    swap_chain_.Destroy();
    vkDestroySurfaceKHR(GetVulkanInstance(), surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
  }

  gfx::SwapResult SwapBuffers() override { return swap_chain_.SwapBuffers(); }
  VulkanSwapChain* GetSwapChain() override { return &swap_chain_; }
  void Finish() override { vkQueueWaitIdle(device_queue_->GetVulkanQueue()); }

 protected:
  gfx::AcceleratedWidget window_;
  gfx::Size size_;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkSurfaceFormatKHR surface_format_ = {};
  VulkanDeviceQueue* device_queue_ = nullptr;
  VulkanSwapChain swap_chain_;
};

VulkanSurface::~VulkanSurface() {}

// static
std::unique_ptr<VulkanSurface> VulkanSurface::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  return std::unique_ptr<VulkanSurface>(new VulkanWSISurface(window));
}

VulkanSurface::VulkanSurface() {}

}  // namespace gpu
