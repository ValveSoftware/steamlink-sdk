// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_GPU_STRUCT_TRAITS_H_
#define CC_IPC_GPU_STRUCT_TRAITS_H_

#include "base/time/time.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/dx_diag_node_struct_traits.h"
#include "gpu/ipc/common/gpu_info.mojom.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<gpu::mojom::GpuDeviceDataView, gpu::GPUInfo::GPUDevice> {
  static bool Read(gpu::mojom::GpuDeviceDataView data,
                   gpu::GPUInfo::GPUDevice* out);

  static uint32_t vendor_id(const gpu::GPUInfo::GPUDevice& input) {
    return input.vendor_id;
  }

  static uint32_t device_id(const gpu::GPUInfo::GPUDevice& input) {
    return input.device_id;
  }

  static bool active(const gpu::GPUInfo::GPUDevice& input) {
    return input.active;
  }

  static const std::string& vendor_string(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.vendor_string;
  }

  static const std::string& device_string(
      const gpu::GPUInfo::GPUDevice& input) {
    return input.device_string;
  }
};

template <>
struct EnumTraits<gpu::mojom::CollectInfoResult, gpu::CollectInfoResult> {
  static gpu::mojom::CollectInfoResult ToMojom(
      gpu::CollectInfoResult collect_info_result);

  static bool FromMojom(gpu::mojom::CollectInfoResult input,
                        gpu::CollectInfoResult* out);
};

template <>
struct EnumTraits<gpu::mojom::VideoCodecProfile, gpu::VideoCodecProfile> {
  static gpu::mojom::VideoCodecProfile ToMojom(
      gpu::VideoCodecProfile video_codec_profile);
  static bool FromMojom(gpu::mojom::VideoCodecProfile input,
                        gpu::VideoCodecProfile* out);
};

template <>
struct StructTraits<gpu::mojom::VideoDecodeAcceleratorSupportedProfileDataView,
                    gpu::VideoDecodeAcceleratorSupportedProfile> {
  static bool Read(
      gpu::mojom::VideoDecodeAcceleratorSupportedProfileDataView data,
      gpu::VideoDecodeAcceleratorSupportedProfile* out);

  static gpu::VideoCodecProfile profile(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.profile;
  }

  static const gfx::Size& max_resolution(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.max_resolution;
  }

  static const gfx::Size& min_resolution(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.min_resolution;
  }

  static bool encrypted_only(
      const gpu::VideoDecodeAcceleratorSupportedProfile& input) {
    return input.encrypted_only;
  }
};

template <>
struct StructTraits<gpu::mojom::VideoDecodeAcceleratorCapabilitiesDataView,
                    gpu::VideoDecodeAcceleratorCapabilities> {
  static bool Read(gpu::mojom::VideoDecodeAcceleratorCapabilitiesDataView data,
                   gpu::VideoDecodeAcceleratorCapabilities* out);

  static uint32_t flags(const gpu::VideoDecodeAcceleratorCapabilities& input) {
    return input.flags;
  }
};

template <>
struct StructTraits<gpu::mojom::VideoEncodeAcceleratorSupportedProfileDataView,
                    gpu::VideoEncodeAcceleratorSupportedProfile> {
  static bool Read(
      gpu::mojom::VideoEncodeAcceleratorSupportedProfileDataView data,
      gpu::VideoEncodeAcceleratorSupportedProfile* out);

  static gpu::VideoCodecProfile profile(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.profile;
  }

  static const gfx::Size& max_resolution(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.max_resolution;
  }

  static uint32_t max_framerate_numerator(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.max_framerate_numerator;
  }

  static uint32_t max_framerate_denominator(
      const gpu::VideoEncodeAcceleratorSupportedProfile& input) {
    return input.max_framerate_denominator;
  }
};

template <>
struct StructTraits<gpu::mojom::GpuInfoDataView, gpu::GPUInfo> {
  static bool Read(gpu::mojom::GpuInfoDataView data, gpu::GPUInfo* out);

  static base::TimeDelta initialization_time(const gpu::GPUInfo& input) {
    return input.initialization_time;
  }

  static bool optimus(const gpu::GPUInfo& input) { return input.optimus; }

  static bool amd_switchable(const gpu::GPUInfo& input) {
    return input.amd_switchable;
  }

  static bool lenovo_dcute(const gpu::GPUInfo& input) {
    return input.lenovo_dcute;
  }

  static const base::Version& display_link_version(const gpu::GPUInfo& input) {
    return input.display_link_version;
  }

  static const gpu::GPUInfo::GPUDevice& gpu(const gpu::GPUInfo& input) {
    return input.gpu;
  }

  static const std::vector<gpu::GPUInfo::GPUDevice>& secondary_gpus(
      const gpu::GPUInfo& input) {
    return input.secondary_gpus;
  }

  static uint64_t adapter_luid(const gpu::GPUInfo& input) {
    return input.adapter_luid;
  }

  static const std::string& driver_vendor(const gpu::GPUInfo& input) {
    return input.driver_vendor;
  }

  static const std::string& driver_version(const gpu::GPUInfo& input) {
    return input.driver_version;
  }

  static const std::string& driver_date(const gpu::GPUInfo& input) {
    return input.driver_date;
  }

  static const std::string& pixel_shader_version(const gpu::GPUInfo& input) {
    return input.pixel_shader_version;
  }

  static const std::string& vertex_shader_version(const gpu::GPUInfo& input) {
    return input.vertex_shader_version;
  }

  static const std::string& max_msaa_samples(const gpu::GPUInfo& input) {
    return input.max_msaa_samples;
  }

  static const std::string& machine_model_name(const gpu::GPUInfo& input) {
    return input.machine_model_name;
  }

  static const std::string& machine_model_version(const gpu::GPUInfo& input) {
    return input.machine_model_version;
  }

  static const std::string& gl_version(const gpu::GPUInfo& input) {
    return input.gl_version;
  }

  static const std::string& gl_vendor(const gpu::GPUInfo& input) {
    return input.gl_vendor;
  }

  static const std::string& gl_renderer(const gpu::GPUInfo& input) {
    return input.gl_renderer;
  }

  static const std::string& gl_extensions(const gpu::GPUInfo& input) {
    return input.gl_extensions;
  }

  static const std::string& gl_ws_vendor(const gpu::GPUInfo& input) {
    return input.gl_ws_vendor;
  }

  static const std::string& gl_ws_version(const gpu::GPUInfo& input) {
    return input.gl_ws_version;
  }

  static const std::string& gl_ws_extensions(const gpu::GPUInfo& input) {
    return input.gl_ws_extensions;
  }

  static uint32_t gl_reset_notification_strategy(const gpu::GPUInfo& input) {
    return input.gl_reset_notification_strategy;
  }

  static bool software_rendering(const gpu::GPUInfo& input) {
    return input.software_rendering;
  }

  static bool direct_rendering(const gpu::GPUInfo& input) {
    return input.direct_rendering;
  }

  static bool sandboxed(const gpu::GPUInfo& input) { return input.sandboxed; }

  static int process_crash_count(const gpu::GPUInfo& input) {
    return input.process_crash_count;
  }

  static bool in_process_gpu(const gpu::GPUInfo& input) {
    return input.in_process_gpu;
  }

  static gpu::CollectInfoResult basic_info_state(const gpu::GPUInfo& input) {
    return input.basic_info_state;
  }

  static gpu::CollectInfoResult context_info_state(const gpu::GPUInfo& input) {
    return input.context_info_state;
  }
#if defined(OS_WIN)
  static const gpu::DxDiagNode& dx_diagnostics(const gpu::GPUInfo& input) {
    return input.dx_diagnostics;
  }
#else
  static const base::Optional<gpu::DxDiagNode>& dx_diagnostics(
      const gpu::GPUInfo& input) {
    static const base::Optional<gpu::DxDiagNode> dx_diag_node(base::nullopt);
    return dx_diag_node;
  }
#endif

  static gpu::CollectInfoResult dx_diagnostics_info_state(
      const gpu::GPUInfo& input) {
#if defined(OS_WIN)
    return input.dx_diagnostics_info_state;
#else
    return gpu::CollectInfoResult::kCollectInfoNone;
#endif
  }

  static const gpu::VideoDecodeAcceleratorCapabilities&
  video_decode_accelerator_capabilities(const gpu::GPUInfo& input) {
    return input.video_decode_accelerator_capabilities;
  }

  static std::vector<gpu::VideoEncodeAcceleratorSupportedProfile>
  video_encode_accelerator_supported_profiles(const gpu::GPUInfo& input) {
    return input.video_encode_accelerator_supported_profiles;
  }

  static bool jpeg_decode_accelerator_supported(const gpu::GPUInfo& input) {
    return input.jpeg_decode_accelerator_supported;
  }

  static uint64_t system_visual(const gpu::GPUInfo& input) {
#if defined(USE_X11) && !defined(OS_CHROMEOS)
    return input.system_visual;
#endif
    return 0;
  }

  static uint64_t rgba_visual(const gpu::GPUInfo& input) {
#if defined(USE_X11) && !defined(OS_CHROMEOS)
    return input.rgba_visual;
#endif
    return 0;
  }
};

}  // namespace mojo
#endif  // CC_IPC_GPU_STRUCT_TRAITS_H_
