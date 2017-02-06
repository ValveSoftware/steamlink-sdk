// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
#define GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_

#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_stream_constants.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gl/gpu_preference.h"
#include "url/ipc/url_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GPU_EXPORT

IPC_STRUCT_TRAITS_BEGIN(gpu::VideoDecodeAcceleratorSupportedProfile)
  IPC_STRUCT_TRAITS_MEMBER(profile)
  IPC_STRUCT_TRAITS_MEMBER(max_resolution)
  IPC_STRUCT_TRAITS_MEMBER(min_resolution)
  IPC_STRUCT_TRAITS_MEMBER(encrypted_only)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::VideoDecodeAcceleratorCapabilities)
  IPC_STRUCT_TRAITS_MEMBER(supported_profiles)
  IPC_STRUCT_TRAITS_MEMBER(flags)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::VideoEncodeAcceleratorSupportedProfile)
  IPC_STRUCT_TRAITS_MEMBER(profile)
  IPC_STRUCT_TRAITS_MEMBER(max_resolution)
  IPC_STRUCT_TRAITS_MEMBER(max_framerate_numerator)
  IPC_STRUCT_TRAITS_MEMBER(max_framerate_denominator)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(gl::GpuPreference, gl::GpuPreferenceLast)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::GpuStreamPriority, gpu::GpuStreamPriority::LAST)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::MemoryAllocation::PriorityCutoff,
                          gpu::MemoryAllocation::CUTOFF_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::error::ContextLostReason,
                          gpu::error::kContextLostReasonLast)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(gpu::CollectInfoResult,
                              gpu::kCollectInfoNone,
                              gpu::kCollectInfoFatalFailure)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(gpu::VideoCodecProfile,
                              gpu::VIDEO_CODEC_PROFILE_MIN,
                              gpu::VIDEO_CODEC_PROFILE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(gpu::gles2::ContextType,
                          gpu::gles2::CONTEXT_TYPE_LAST)

IPC_STRUCT_TRAITS_BEGIN(gpu::DxDiagNode)
  IPC_STRUCT_TRAITS_MEMBER(values)
  IPC_STRUCT_TRAITS_MEMBER(children)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::GPUInfo::GPUDevice)
  IPC_STRUCT_TRAITS_MEMBER(vendor_id)
  IPC_STRUCT_TRAITS_MEMBER(device_id)
  IPC_STRUCT_TRAITS_MEMBER(active)
  IPC_STRUCT_TRAITS_MEMBER(vendor_string)
  IPC_STRUCT_TRAITS_MEMBER(device_string)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::GPUInfo)
  IPC_STRUCT_TRAITS_MEMBER(initialization_time)
  IPC_STRUCT_TRAITS_MEMBER(optimus)
  IPC_STRUCT_TRAITS_MEMBER(amd_switchable)
  IPC_STRUCT_TRAITS_MEMBER(lenovo_dcute)
  IPC_STRUCT_TRAITS_MEMBER(gpu)
  IPC_STRUCT_TRAITS_MEMBER(secondary_gpus)
  IPC_STRUCT_TRAITS_MEMBER(adapter_luid)
  IPC_STRUCT_TRAITS_MEMBER(driver_vendor)
  IPC_STRUCT_TRAITS_MEMBER(driver_version)
  IPC_STRUCT_TRAITS_MEMBER(driver_date)
  IPC_STRUCT_TRAITS_MEMBER(pixel_shader_version)
  IPC_STRUCT_TRAITS_MEMBER(vertex_shader_version)
  IPC_STRUCT_TRAITS_MEMBER(max_msaa_samples)
  IPC_STRUCT_TRAITS_MEMBER(machine_model_name)
  IPC_STRUCT_TRAITS_MEMBER(machine_model_version)
  IPC_STRUCT_TRAITS_MEMBER(gl_version)
  IPC_STRUCT_TRAITS_MEMBER(gl_vendor)
  IPC_STRUCT_TRAITS_MEMBER(gl_renderer)
  IPC_STRUCT_TRAITS_MEMBER(gl_extensions)
  IPC_STRUCT_TRAITS_MEMBER(gl_ws_vendor)
  IPC_STRUCT_TRAITS_MEMBER(gl_ws_version)
  IPC_STRUCT_TRAITS_MEMBER(gl_ws_extensions)
  IPC_STRUCT_TRAITS_MEMBER(gl_reset_notification_strategy)
  IPC_STRUCT_TRAITS_MEMBER(can_lose_context)
  IPC_STRUCT_TRAITS_MEMBER(software_rendering)
  IPC_STRUCT_TRAITS_MEMBER(direct_rendering)
  IPC_STRUCT_TRAITS_MEMBER(sandboxed)
  IPC_STRUCT_TRAITS_MEMBER(process_crash_count)
  IPC_STRUCT_TRAITS_MEMBER(in_process_gpu)
  IPC_STRUCT_TRAITS_MEMBER(basic_info_state)
  IPC_STRUCT_TRAITS_MEMBER(context_info_state)
#if defined(OS_WIN)
  IPC_STRUCT_TRAITS_MEMBER(dx_diagnostics_info_state)
  IPC_STRUCT_TRAITS_MEMBER(dx_diagnostics)
#endif
  IPC_STRUCT_TRAITS_MEMBER(video_decode_accelerator_capabilities)
  IPC_STRUCT_TRAITS_MEMBER(video_encode_accelerator_supported_profiles)
  IPC_STRUCT_TRAITS_MEMBER(jpeg_decode_accelerator_supported)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::MemoryAllocation)
  IPC_STRUCT_TRAITS_MEMBER(bytes_limit_when_visible)
  IPC_STRUCT_TRAITS_MEMBER(priority_cutoff_when_visible)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::gles2::ContextCreationAttribHelper)
  IPC_STRUCT_TRAITS_MEMBER(offscreen_framebuffer_size)
  IPC_STRUCT_TRAITS_MEMBER(gpu_preference)
  IPC_STRUCT_TRAITS_MEMBER(alpha_size)
  IPC_STRUCT_TRAITS_MEMBER(blue_size)
  IPC_STRUCT_TRAITS_MEMBER(green_size)
  IPC_STRUCT_TRAITS_MEMBER(red_size)
  IPC_STRUCT_TRAITS_MEMBER(depth_size)
  IPC_STRUCT_TRAITS_MEMBER(stencil_size)
  IPC_STRUCT_TRAITS_MEMBER(samples)
  IPC_STRUCT_TRAITS_MEMBER(sample_buffers)
  IPC_STRUCT_TRAITS_MEMBER(buffer_preserved)
  IPC_STRUCT_TRAITS_MEMBER(bind_generates_resource)
  IPC_STRUCT_TRAITS_MEMBER(fail_if_major_perf_caveat)
  IPC_STRUCT_TRAITS_MEMBER(lose_context_when_out_of_memory)
  IPC_STRUCT_TRAITS_MEMBER(context_type)
IPC_STRUCT_TRAITS_END()


#endif  // GPU_IPC_COMMON_GPU_PARAM_TRAITS_MACROS_H_
