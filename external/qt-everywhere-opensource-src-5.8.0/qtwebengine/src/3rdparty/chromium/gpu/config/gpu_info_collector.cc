// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

namespace {

scoped_refptr<gl::GLSurface> InitializeGLSurface() {
  scoped_refptr<gl::GLSurface> surface(
      gl::init::CreateOffscreenGLSurface(gfx::Size()));
  if (!surface.get()) {
    LOG(ERROR) << "gl::GLContext::CreateOffscreenGLSurface failed";
    return NULL;
  }

  return surface;
}

scoped_refptr<gl::GLContext> InitializeGLContext(gl::GLSurface* surface) {
  scoped_refptr<gl::GLContext> context(
      gl::init::CreateGLContext(nullptr, surface, gl::PreferIntegratedGpu));
  if (!context.get()) {
    LOG(ERROR) << "gl::init::CreateGLContext failed";
    return NULL;
  }

  if (!context->MakeCurrent(surface)) {
    LOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return NULL;
  }

  return context;
}

std::string GetGLString(unsigned int pname) {
  const char* gl_string =
      reinterpret_cast<const char*>(glGetString(pname));
  if (gl_string)
    return std::string(gl_string);
  return std::string();
}

// Return a version string in the format of "major.minor".
std::string GetVersionFromString(const std::string& version_string) {
  size_t begin = version_string.find_first_of("0123456789");
  if (begin != std::string::npos) {
    size_t end = version_string.find_first_not_of("01234567890.", begin);
    std::string sub_string;
    if (end != std::string::npos)
      sub_string = version_string.substr(begin, end - begin);
    else
      sub_string = version_string.substr(begin);
    std::vector<std::string> pieces = base::SplitString(
        sub_string, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (pieces.size() >= 2)
      return pieces[0] + "." + pieces[1];
  }
  return std::string();
}

// Return the array index of the found name, or return -1.
int StringContainsName(
    const std::string& str, const std::string* names, size_t num_names) {
  std::vector<std::string> tokens = base::SplitString(
      str, " .,()-_", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t ii = 0; ii < tokens.size(); ++ii) {
    for (size_t name_index = 0; name_index < num_names; ++name_index) {
      if (tokens[ii] == names[name_index])
        return name_index;
    }
  }
  return -1;
}

}  // namespace anonymous

namespace gpu {

CollectInfoResult CollectGraphicsInfoGL(GPUInfo* gpu_info) {
  TRACE_EVENT0("startup", "gpu_info_collector::CollectGraphicsInfoGL");
  DCHECK_NE(gl::GetGLImplementation(), gl::kGLImplementationNone);

  scoped_refptr<gl::GLSurface> surface(InitializeGLSurface());
  if (!surface.get()) {
    LOG(ERROR) << "Could not create surface for info collection.";
    return kCollectInfoFatalFailure;
  }

  scoped_refptr<gl::GLContext> context(InitializeGLContext(surface.get()));
  if (!context.get()) {
    LOG(ERROR) << "Could not create context for info collection.";
    return kCollectInfoFatalFailure;
  }

  gpu_info->gl_renderer = GetGLString(GL_RENDERER);
  gpu_info->gl_vendor = GetGLString(GL_VENDOR);
  gpu_info->gl_version = GetGLString(GL_VERSION);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGpuTestingGLVendor)) {
    gpu_info->gl_vendor =
        command_line->GetSwitchValueASCII(switches::kGpuTestingGLVendor);
  }
  if (command_line->HasSwitch(switches::kGpuTestingGLRenderer)) {
    gpu_info->gl_renderer =
        command_line->GetSwitchValueASCII(switches::kGpuTestingGLRenderer);
  }
  if (command_line->HasSwitch(switches::kGpuTestingGLVersion)) {
    gpu_info->gl_version =
        command_line->GetSwitchValueASCII(switches::kGpuTestingGLVersion);
  }

  gpu_info->gl_extensions = gl::GetGLExtensionsFromCurrentContext();
  std::string glsl_version_string = GetGLString(GL_SHADING_LANGUAGE_VERSION);

  gl::GLVersionInfo gl_info(gpu_info->gl_version.c_str(),
                            gpu_info->gl_renderer.c_str(),
                            gpu_info->gl_extensions.c_str());
  GLint max_samples = 0;
  if (gl_info.IsAtLeastGL(3, 0) || gl_info.IsAtLeastGLES(3, 0) ||
      gpu_info->gl_extensions.find("GL_ANGLE_framebuffer_multisample") !=
          std::string::npos ||
      gpu_info->gl_extensions.find("GL_APPLE_framebuffer_multisample") !=
          std::string::npos ||
      gpu_info->gl_extensions.find("GL_EXT_framebuffer_multisample") !=
          std::string::npos ||
      gpu_info->gl_extensions.find("GL_EXT_multisampled_render_to_texture") !=
          std::string::npos ||
      gpu_info->gl_extensions.find("GL_NV_framebuffer_multisample") !=
          std::string::npos) {
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
  }
  gpu_info->max_msaa_samples = base::IntToString(max_samples);
  UMA_HISTOGRAM_SPARSE_SLOWLY("GPU.MaxMSAASampleCount", max_samples);

  gl::GLWindowSystemBindingInfo window_system_binding_info;
  if (GetGLWindowSystemBindingInfo(&window_system_binding_info)) {
    gpu_info->gl_ws_vendor = window_system_binding_info.vendor;
    gpu_info->gl_ws_version = window_system_binding_info.version;
    gpu_info->gl_ws_extensions = window_system_binding_info.extensions;
    gpu_info->direct_rendering = window_system_binding_info.direct_rendering;
  }

  bool supports_robustness =
      gpu_info->gl_extensions.find("GL_EXT_robustness") != std::string::npos ||
      gpu_info->gl_extensions.find("GL_KHR_robustness") != std::string::npos ||
      gpu_info->gl_extensions.find("GL_ARB_robustness") != std::string::npos;
  if (supports_robustness) {
    glGetIntegerv(GL_RESET_NOTIFICATION_STRATEGY_ARB,
        reinterpret_cast<GLint*>(&gpu_info->gl_reset_notification_strategy));
  }

  // TODO(kbr): remove once the destruction of a current context automatically
  // clears the current context.
  context->ReleaseCurrent(surface.get());

  std::string glsl_version = GetVersionFromString(glsl_version_string);
  gpu_info->pixel_shader_version = glsl_version;
  gpu_info->vertex_shader_version = glsl_version;

  IdentifyActiveGPU(gpu_info);
  return CollectDriverInfoGL(gpu_info);
}

void MergeGPUInfoGL(GPUInfo* basic_gpu_info,
                    const GPUInfo& context_gpu_info) {
  DCHECK(basic_gpu_info);
  // Copy over GPUs because which one is active could change.
  basic_gpu_info->gpu = context_gpu_info.gpu;
  basic_gpu_info->secondary_gpus = context_gpu_info.secondary_gpus;

  basic_gpu_info->gl_renderer = context_gpu_info.gl_renderer;
  basic_gpu_info->gl_vendor = context_gpu_info.gl_vendor;
  basic_gpu_info->gl_version = context_gpu_info.gl_version;
  basic_gpu_info->gl_extensions = context_gpu_info.gl_extensions;
  basic_gpu_info->pixel_shader_version =
      context_gpu_info.pixel_shader_version;
  basic_gpu_info->vertex_shader_version =
      context_gpu_info.vertex_shader_version;
  basic_gpu_info->max_msaa_samples =
      context_gpu_info.max_msaa_samples;
  basic_gpu_info->gl_ws_vendor = context_gpu_info.gl_ws_vendor;
  basic_gpu_info->gl_ws_version = context_gpu_info.gl_ws_version;
  basic_gpu_info->gl_ws_extensions = context_gpu_info.gl_ws_extensions;
  basic_gpu_info->gl_reset_notification_strategy =
      context_gpu_info.gl_reset_notification_strategy;

  if (!context_gpu_info.driver_vendor.empty())
    basic_gpu_info->driver_vendor = context_gpu_info.driver_vendor;
  if (!context_gpu_info.driver_version.empty())
    basic_gpu_info->driver_version = context_gpu_info.driver_version;

  basic_gpu_info->can_lose_context = context_gpu_info.can_lose_context;
  basic_gpu_info->sandboxed = context_gpu_info.sandboxed;
  basic_gpu_info->direct_rendering = context_gpu_info.direct_rendering;
  basic_gpu_info->in_process_gpu = context_gpu_info.in_process_gpu;
  basic_gpu_info->context_info_state = context_gpu_info.context_info_state;
  basic_gpu_info->initialization_time = context_gpu_info.initialization_time;
  basic_gpu_info->video_decode_accelerator_capabilities =
      context_gpu_info.video_decode_accelerator_capabilities;
  basic_gpu_info->video_encode_accelerator_supported_profiles =
      context_gpu_info.video_encode_accelerator_supported_profiles;
  basic_gpu_info->jpeg_decode_accelerator_supported =
      context_gpu_info.jpeg_decode_accelerator_supported;
}

void IdentifyActiveGPU(GPUInfo* gpu_info) {
  const std::string kNVidiaName = "nvidia";
  const std::string kNouveauName = "nouveau";
  const std::string kIntelName = "intel";
  const std::string kAMDName = "amd";
  const std::string kATIName = "ati";
  const std::string kVendorNames[] = {kNVidiaName, kNouveauName, kIntelName,
                                      kAMDName, kATIName};

  const uint32_t kNVidiaID = 0x10de;
  const uint32_t kIntelID = 0x8086;
  const uint32_t kAMDID = 0x1002;
  const uint32_t kATIID = 0x1002;
  const uint32_t kVendorIDs[] = {kNVidiaID, kNVidiaID, kIntelID, kAMDID,
                                 kATIID};

  DCHECK(gpu_info);
  if (gpu_info->secondary_gpus.size() == 0)
    return;

  uint32_t active_vendor_id = 0;
  if (!gpu_info->gl_vendor.empty()) {
    std::string gl_vendor_lower = base::ToLowerASCII(gpu_info->gl_vendor);
    int index = StringContainsName(
        gl_vendor_lower, kVendorNames, arraysize(kVendorNames));
    if (index >= 0) {
      active_vendor_id = kVendorIDs[index];
    }
  }
  if (active_vendor_id == 0 && !gpu_info->gl_renderer.empty()) {
    std::string gl_renderer_lower = base::ToLowerASCII(gpu_info->gl_renderer);
    int index = StringContainsName(
        gl_renderer_lower, kVendorNames, arraysize(kVendorNames));
    if (index >= 0) {
      active_vendor_id = kVendorIDs[index];
    }
  }
  if (active_vendor_id == 0) {
    // We fail to identify the GPU vendor through GL_VENDOR/GL_RENDERER.
    return;
  }
  gpu_info->gpu.active = false;
  for (size_t ii = 0; ii < gpu_info->secondary_gpus.size(); ++ii)
    gpu_info->secondary_gpus[ii].active = false;

  // TODO(zmo): if two GPUs are from the same vendor, this code will always
  // set the first GPU as active, which could be wrong.
  if (active_vendor_id == gpu_info->gpu.vendor_id) {
    gpu_info->gpu.active = true;
    return;
  }
  for (size_t ii = 0; ii < gpu_info->secondary_gpus.size(); ++ii) {
    if (active_vendor_id == gpu_info->secondary_gpus[ii].vendor_id) {
      gpu_info->secondary_gpus[ii].active = true;
      return;
    }
  }
}

}  // namespace gpu

