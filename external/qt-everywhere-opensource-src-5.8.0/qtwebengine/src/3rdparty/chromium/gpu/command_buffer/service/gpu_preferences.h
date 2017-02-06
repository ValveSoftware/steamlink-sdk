// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_PREFERENCES_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_PREFERENCES_H_

#include <stddef.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"

namespace gpu {

struct GPU_EXPORT GpuPreferences {
 public:
  GpuPreferences();

  GpuPreferences(const GpuPreferences& other);

  ~GpuPreferences();

  // ===================================
  // Settings from //content/public/common/content_switches.h

  // Runs the renderer and plugins in the same process as the browser.
  bool single_process = false;

  // Run the GPU process as a thread in the browser process.
  bool in_process_gpu = false;

  // Prioritizes the UI's command stream in the GPU process.
  bool ui_prioritize_in_gpu_process = false;

  // Disables hardware acceleration of video decode, where available.
  bool disable_accelerated_video_decode = false;

#if defined(OS_CHROMEOS)
  // Disables VA-API accelerated video encode.
  bool disable_vaapi_accelerated_video_encode = false;
#endif

#if defined(ENABLE_WEBRTC)
  // Disables HW encode acceleration for WebRTC.
  bool disable_web_rtc_hw_encoding = false;
#endif

#if defined(OS_WIN)
  // Enables experimental hardware acceleration for VP8/VP9 video decoding.
  bool enable_accelerated_vpx_decode = false;

  // Enables support for avoiding copying DXGI NV12 textures.
  bool enable_zero_copy_dxgi_video = false;

  // Enables support for outputting NV12 video frames.
  bool enable_nv12_dxgi_video = false;
#endif

  // ===================================
  // Settings from //gpu/command_buffer/service/gpu_switches.cc

  // Always return success when compiling a shader. Linking will still fail.
  bool compile_shader_always_succeeds = false;

  // Disable the GL error log limit.
  bool disable_gl_error_limit = false;

  // Disable the GLSL translator.
  bool disable_glsl_translator = false;

  // Disable workarounds for various GPU driver bugs.
  bool disable_gpu_driver_bug_workarounds = false;

  // Turn off user-defined name hashing in shaders.
  bool disable_shader_name_hashing = false;

  // Turn on Logging GPU commands.
  bool enable_gpu_command_logging = false;

  // Turn on Calling GL Error after every command.
  bool enable_gpu_debugging = false;

  // Enable GPU service logging.
  bool enable_gpu_service_logging_gpu = false;

  // Turn off gpu program caching
  bool disable_gpu_program_cache = false;

  // Enforce GL minimums.
  bool enforce_gl_minimums = false;

  // Sets the total amount of memory that may be allocated for GPU resources
  uint32_t force_gpu_mem_available = 0;

  // Sets the maximum size of the in-memory gpu program cache, in kb
  uint32_t gpu_program_cache_size = kDefaultMaxProgramCacheMemoryBytes;

  // Disables the GPU shader on disk cache.
  bool disable_gpu_shader_disk_cache = false;

  // Allows async texture uploads (off main thread) via GL context sharing.
  bool enable_share_group_async_texture_upload = false;

  // Simulates shared textures when share groups are not available.
  // Not available everywhere.
  bool enable_threaded_texture_mailboxes = false;

  // Include ANGLE's intermediate representation (AST) output in shader
  // compilation info logs.
  bool gl_shader_interm_output = false;

  // Emulate ESSL lowp and mediump float precisions by mutating the shaders to
  // round intermediate values in ANGLE.
  bool emulate_shader_precision = false;

  // ===================================
  // Settings from //ui/gl/gl_switches.h

  // Turns on GPU logging (debug build only).
  bool enable_gpu_service_logging = false;

  // Turns on calling TRACE for every GL call.
  bool enable_gpu_service_tracing = false;

  // Enable OpenGL ES 3 APIs without proper service side validation.
  bool enable_unsafe_es3_apis = false;

  // Use the Pass-through command decoder, skipping all validation and state
  // tracking.
  bool use_passthrough_cmd_decoder = false;
};

}  // namespace gpu

#endif // GPU_COMMAND_BUFFER_SERVICE_GPU_PREFERENCES_H_
