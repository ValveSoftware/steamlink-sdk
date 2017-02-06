// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_implementation.h"

#include <unordered_set>
#include <vector>
#include "base/logging.h"
#include "base/macros.h"
#include "gpu/vulkan/vulkan_platform.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include "ui/gfx/x/x11_types.h"
#endif  // defined(VK_USE_PLATFORM_XLIB_KHR)

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanErrorCallback(
    VkDebugReportFlagsEXT       flags,
    VkDebugReportObjectTypeEXT  objectType,
    uint64_t                    object,
    size_t                      location,
    int32_t                     messageCode,
    const char*                 pLayerPrefix,
    const char*                 pMessage,
    void*                       pUserData) {
  LOG(ERROR) << pMessage;
  return VK_TRUE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanWarningCallback(
    VkDebugReportFlagsEXT       flags,
    VkDebugReportObjectTypeEXT  objectType,
    uint64_t                    object,
    size_t                      location,
    int32_t                     messageCode,
    const char*                 pLayerPrefix,
    const char*                 pMessage,
    void*                       pUserData) {
  LOG(WARNING) << pMessage;
  return VK_TRUE;
}

namespace gpu {

struct VulkanInstance {
  VulkanInstance() {}

  void Initialize() { valid = InitializeVulkanInstance(); }

  bool InitializeVulkanInstance() {
    VkResult result = VK_SUCCESS;

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Chromium";
    app_info.apiVersion = VK_MAKE_VERSION(1, 0, 2);

    std::vector<const char*> enabled_ext_names;
    enabled_ext_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_XLIB_KHR)
    enabled_ext_names.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    enabled_ext_names.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif

    uint32_t num_instance_exts = 0;
    result = vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_exts,
                                                    nullptr);
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkEnumerateInstanceExtensionProperties(NULL) failed: "
                  << result;
      return false;
    }

    std::vector<VkExtensionProperties> instance_exts(num_instance_exts);
    result = vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_exts,
                                                    instance_exts.data());
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkEnumerateInstanceExtensionProperties() failed: "
                  << result;
      return false;
    }

    bool debug_report_enabled = false;
    for (const VkExtensionProperties& ext_property : instance_exts) {
      if (strcmp(ext_property.extensionName,
                 VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0) {
        debug_report_enabled = true;
        enabled_ext_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
      }
    }

    std::vector<const char*> enabled_layer_names;
#if DCHECK_IS_ON()
    uint32_t num_instance_layers = 0;
    result = vkEnumerateInstanceLayerProperties(&num_instance_layers, nullptr);
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkEnumerateInstanceLayerProperties(NULL) failed: "
                  << result;
      return false;
    }

    std::vector<VkLayerProperties> instance_layers(num_instance_layers);
    result = vkEnumerateInstanceLayerProperties(&num_instance_layers,
                                                instance_layers.data());
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkEnumerateInstanceLayerProperties() failed: " << result;
      return false;
    }

    std::unordered_set<std::string> desired_layers({
      "VK_LAYER_LUNARG_standard_validation",
    });

    for (const VkLayerProperties& layer_property : instance_layers) {
      if (desired_layers.find(layer_property.layerName) != desired_layers.end())
        enabled_layer_names.push_back(layer_property.layerName);
    }
#endif

    VkInstanceCreateInfo instance_create_info = {};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.enabledLayerCount = enabled_layer_names.size();
    instance_create_info.ppEnabledLayerNames = enabled_layer_names.data();
    instance_create_info.enabledExtensionCount = enabled_ext_names.size();
    instance_create_info.ppEnabledExtensionNames = enabled_ext_names.data();

    result = vkCreateInstance(&instance_create_info, nullptr, &vk_instance);
    if (VK_SUCCESS != result) {
      DLOG(ERROR) << "vkCreateInstance() failed: " << result;
      return false;
    }

#if DCHECK_IS_ON()
    // Register our error logging function.
    if (debug_report_enabled) {
      PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
          reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>
              (vkGetInstanceProcAddr(vk_instance,
                                     "vkCreateDebugReportCallbackEXT"));
      DCHECK(vkCreateDebugReportCallbackEXT);

      VkDebugReportCallbackCreateInfoEXT cb_create_info = {};
      cb_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;

      cb_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
      cb_create_info.pfnCallback = &VulkanErrorCallback;
      result = vkCreateDebugReportCallbackEXT(vk_instance, &cb_create_info,
                                              nullptr, &error_callback);
      if (VK_SUCCESS != result) {
        DLOG(ERROR) << "vkCreateDebugReportCallbackEXT(ERROR) failed: "
                    << result;
        return false;
      }

      cb_create_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                             VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
      cb_create_info.pfnCallback = &VulkanWarningCallback;
      result = vkCreateDebugReportCallbackEXT(vk_instance, &cb_create_info,
                                              nullptr, &warning_callback);
      if (VK_SUCCESS != result) {
        DLOG(ERROR) << "vkCreateDebugReportCallbackEXT(WARN) failed: "
                    << result;
        return false;
      }
    }
#endif

    return true;
  }

  bool valid = false;
  VkInstance vk_instance = VK_NULL_HANDLE;
#if DCHECK_IS_ON()
  VkDebugReportCallbackEXT error_callback = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT warning_callback = VK_NULL_HANDLE;
#endif
};

static VulkanInstance* vulkan_instance = nullptr;

bool InitializeVulkan() {
  DCHECK(!vulkan_instance);
  vulkan_instance = new VulkanInstance;
  vulkan_instance->Initialize();
  return vulkan_instance->valid;
}

bool VulkanSupported() {
  DCHECK(vulkan_instance);
  return vulkan_instance->valid;
}

VkInstance GetVulkanInstance() {
  DCHECK(vulkan_instance);
  DCHECK(vulkan_instance->valid);
  return vulkan_instance->vk_instance;
}

}  // namespace gpu
