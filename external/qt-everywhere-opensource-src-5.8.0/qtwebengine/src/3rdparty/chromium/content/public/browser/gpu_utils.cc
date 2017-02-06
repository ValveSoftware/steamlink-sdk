// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/gpu_utils.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gl/gl_switches.h"

namespace {

bool GetUintFromSwitch(const base::CommandLine* command_line,
                       const base::StringPiece& switch_string,
                       uint32_t* value) {
  if (!command_line->HasSwitch(switch_string))
    return false;
  std::string switch_value(command_line->GetSwitchValueASCII(switch_string));
  return base::StringToUint(switch_value, value);
}

}  // namespace

namespace content {

const gpu::GpuPreferences GetGpuPreferencesFromCommandLine() {
  DCHECK(base::CommandLine::InitializedForCurrentProcess());
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  gpu::GpuPreferences gpu_preferences;
  gpu_preferences.single_process =
      command_line->HasSwitch(switches::kSingleProcess);
  gpu_preferences.in_process_gpu =
      command_line->HasSwitch(switches::kInProcessGPU);
  gpu_preferences.ui_prioritize_in_gpu_process =
      command_line->HasSwitch(switches::kUIPrioritizeInGpuProcess);
  gpu_preferences.disable_accelerated_video_decode =
      command_line->HasSwitch(switches::kDisableAcceleratedVideoDecode);
#if defined(OS_CHROMEOS)
  gpu_preferences.disable_vaapi_accelerated_video_encode =
      command_line->HasSwitch(switches::kDisableVaapiAcceleratedVideoEncode);
#endif
#if defined(ENABLE_WEBRTC)
  gpu_preferences.disable_web_rtc_hw_encoding =
      command_line->HasSwitch(switches::kDisableWebRtcHWEncoding);
#endif
#if defined(OS_WIN)
  gpu_preferences.enable_accelerated_vpx_decode =
      command_line->HasSwitch(switches::kEnableAcceleratedVpxDecode);
  gpu_preferences.enable_zero_copy_dxgi_video =
      command_line->HasSwitch(switches::kEnableZeroCopyDxgiVideo);
  gpu_preferences.enable_nv12_dxgi_video =
      !command_line->HasSwitch(switches::kDisableNv12DxgiVideo);
#endif
  gpu_preferences.compile_shader_always_succeeds =
      command_line->HasSwitch(switches::kCompileShaderAlwaysSucceeds);
  gpu_preferences.disable_gl_error_limit =
      command_line->HasSwitch(switches::kDisableGLErrorLimit);
  gpu_preferences.disable_glsl_translator =
      command_line->HasSwitch(switches::kDisableGLSLTranslator);
  gpu_preferences.disable_gpu_driver_bug_workarounds =
      command_line->HasSwitch(switches::kDisableGpuDriverBugWorkarounds);
  gpu_preferences.disable_shader_name_hashing =
      command_line->HasSwitch(switches::kDisableShaderNameHashing);
  gpu_preferences.enable_gpu_command_logging =
      command_line->HasSwitch(switches::kEnableGPUCommandLogging);
  gpu_preferences.enable_gpu_debugging =
      command_line->HasSwitch(switches::kEnableGPUDebugging);
  gpu_preferences.enable_gpu_service_logging_gpu =
      command_line->HasSwitch(switches::kEnableGPUServiceLoggingGPU);
  gpu_preferences.disable_gpu_program_cache =
      command_line->HasSwitch(switches::kDisableGpuProgramCache);
  gpu_preferences.enforce_gl_minimums =
      command_line->HasSwitch(switches::kEnforceGLMinimums);
  if (GetUintFromSwitch(command_line, switches::kForceGpuMemAvailableMb,
                        &gpu_preferences.force_gpu_mem_available)) {
    gpu_preferences.force_gpu_mem_available *= 1024 * 1024;
  }
  if (GetUintFromSwitch(command_line, switches::kGpuProgramCacheSizeKb,
                        &gpu_preferences.gpu_program_cache_size)) {
    gpu_preferences.gpu_program_cache_size *= 1024;
  }
  gpu_preferences.disable_gpu_shader_disk_cache =
      command_line->HasSwitch(switches::kDisableGpuShaderDiskCache);
  gpu_preferences.enable_share_group_async_texture_upload =
      command_line->HasSwitch(switches::kEnableShareGroupAsyncTextureUpload);
  gpu_preferences.enable_threaded_texture_mailboxes =
      command_line->HasSwitch(switches::kEnableThreadedTextureMailboxes);
  gpu_preferences.gl_shader_interm_output =
      command_line->HasSwitch(switches::kGLShaderIntermOutput);
  gpu_preferences.emulate_shader_precision =
      command_line->HasSwitch(switches::kEmulateShaderPrecision);
  gpu_preferences.enable_gpu_service_logging =
      command_line->HasSwitch(switches::kEnableGPUServiceLogging);
  gpu_preferences.enable_gpu_service_tracing =
      command_line->HasSwitch(switches::kEnableGPUServiceTracing);
  gpu_preferences.enable_unsafe_es3_apis =
      command_line->HasSwitch(switches::kEnableUnsafeES3APIs);
  gpu_preferences.use_passthrough_cmd_decoder =
      command_line->HasSwitch(switches::kUsePassthroughCmdDecoder);
  return gpu_preferences;
}

}  // namespace content
