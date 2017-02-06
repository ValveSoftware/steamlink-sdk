// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/compositor_util.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/base/switches.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_feature_type.h"

namespace content {

namespace {

static bool IsGpuRasterizationBlacklisted() {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  return manager->IsFeatureBlacklisted(
        gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION);
}

const char kGpuCompositingFeatureName[] = "gpu_compositing";
const char kWebGLFeatureName[] = "webgl";
const char kRasterizationFeatureName[] = "rasterization";
const char kMultipleRasterThreadsFeatureName[] = "multiple_raster_threads";
const char kNativeGpuMemoryBuffersFeatureName[] = "native_gpu_memory_buffers";

const int kMinRasterThreads = 1;
const int kMaxRasterThreads = 4;

const int kMinMSAASampleCount = 0;

struct GpuFeatureInfo {
  std::string name;
  bool blocked;
  bool disabled;
  std::string disabled_description;
  bool fallback_to_software;
};

const GpuFeatureInfo GetGpuFeatureInfo(size_t index, bool* eof) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();

  const GpuFeatureInfo kGpuFeatureInfo[] = {
    {"2d_canvas",
     manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS),
     command_line.HasSwitch(switches::kDisableAccelerated2dCanvas) ||
         !GpuDataManagerImpl::GetInstance()
              ->GetGPUInfo()
              .SupportsAccelerated2dCanvas(),
     "Accelerated 2D canvas is unavailable: either disabled via blacklist or"
     " the command line.",
     true},
    {kGpuCompositingFeatureName,
     manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING),
     command_line.HasSwitch(switches::kDisableGpuCompositing),
     "Gpu compositing has been disabled, either via blacklist, about:flags"
     " or the command line. The browser will fall back to software compositing"
     " and hardware acceleration will be unavailable.",
     true},
    {kWebGLFeatureName,
     manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_WEBGL),
     command_line.HasSwitch(switches::kDisableExperimentalWebGL),
     "WebGL has been disabled via blacklist or the command line.", false},
    {"flash_3d", manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH3D),
     command_line.HasSwitch(switches::kDisableFlash3d),
     "Using 3d in flash has been disabled, either via blacklist, about:flags or"
     " the command line.",
     true},
    {"flash_stage3d",
     manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D),
     command_line.HasSwitch(switches::kDisableFlashStage3d),
     "Using Stage3d in Flash has been disabled, either via blacklist,"
     " about:flags or the command line.",
     true},
    {"flash_stage3d_baseline",
     manager->IsFeatureBlacklisted(
         gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE) ||
         manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D),
     command_line.HasSwitch(switches::kDisableFlashStage3d),
     "Using Stage3d Baseline profile in Flash has been disabled, either"
     " via blacklist, about:flags or the command line.",
     true},
    {"video_decode", manager->IsFeatureBlacklisted(
                         gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE),
     command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode),
     "Accelerated video decode has been disabled, either via blacklist,"
     " about:flags or the command line.",
     true},
#if defined(ENABLE_WEBRTC)
    {"video_encode", manager->IsFeatureBlacklisted(
                         gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE),
     command_line.HasSwitch(switches::kDisableWebRtcHWEncoding),
     "Accelerated video encode has been disabled, either via blacklist,"
     " about:flags or the command line.",
     true},
#endif
#if defined(OS_CHROMEOS)
    {"panel_fitting",
     manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_PANEL_FITTING),
     command_line.HasSwitch(switches::kDisablePanelFitting),
     "Panel fitting has been disabled, either via blacklist, about:flags or"
     " the command line.",
     false},
#endif
    {kRasterizationFeatureName,
     IsGpuRasterizationBlacklisted() && !IsGpuRasterizationEnabled() &&
         !IsForceGpuRasterizationEnabled(),
     !IsGpuRasterizationEnabled() && !IsForceGpuRasterizationEnabled() &&
         !IsGpuRasterizationBlacklisted(),
     "Accelerated rasterization has been disabled, either via blacklist,"
     " about:flags or the command line.",
     true},
    {kMultipleRasterThreadsFeatureName, false,
     NumberOfRendererRasterThreads() == 1, "Raster is using a single thread.",
     false},
    {kNativeGpuMemoryBuffersFeatureName, false,
     !BrowserGpuMemoryBufferManager::IsNativeGpuMemoryBuffersEnabled(),
     "Native GpuMemoryBuffers have been disabled, either via about:flags"
     " or command line.",
     true},
  };
  DCHECK(index < arraysize(kGpuFeatureInfo));
  *eof = (index == arraysize(kGpuFeatureInfo) - 1);
  return kGpuFeatureInfo[index];
}

}  // namespace

int NumberOfRendererRasterThreads() {
  int num_processors = base::SysInfo::NumberOfProcessors();

#if defined(OS_ANDROID)
  // Android may report 6 to 8 CPUs for big.LITTLE configurations.
  // Limit the number of raster threads based on maximum of 4 big cores.
  num_processors = std::min(num_processors, 4);
#endif

  int num_raster_threads = num_processors / 2;

#if defined(OS_ANDROID)
  // Limit the number of raster threads to 1 on Android.
  // TODO(reveman): Remove this when we have a better mechanims to prevent
  // pre-paint raster work from slowing down non-raster work. crbug.com/504515
  num_raster_threads = 1;
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kNumRasterThreads)) {
    std::string string_value = command_line.GetSwitchValueASCII(
        switches::kNumRasterThreads);
    if (!base::StringToInt(string_value, &num_raster_threads)) {
      DLOG(WARNING) << "Failed to parse switch " <<
          switches::kNumRasterThreads  << ": " << string_value;
    }
  }

  return cc::MathUtil::ClampToRange(num_raster_threads, kMinRasterThreads,
                                    kMaxRasterThreads);
}

bool IsZeroCopyUploadEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if defined(OS_MACOSX)
  return !command_line.HasSwitch(switches::kDisableZeroCopy);
#else
  return command_line.HasSwitch(switches::kEnableZeroCopy);
#endif
}

bool IsPartialRasterEnabled() {
  // Zero copy currently doesn't take advantage of partial raster.
  if (IsZeroCopyUploadEnabled())
    return false;

  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  return !command_line.HasSwitch(switches::kDisablePartialRaster);
}

bool IsGpuMemoryBufferCompositorResourcesEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(
          switches::kEnableGpuMemoryBufferCompositorResources)) {
    return true;
  }
  if (command_line.HasSwitch(
          switches::kDisableGpuMemoryBufferCompositorResources)) {
    return false;
  }

  // Native GPU memory buffers are required.
  if (!BrowserGpuMemoryBufferManager::IsNativeGpuMemoryBuffersEnabled())
    return false;

#if defined(OS_MACOSX)
  return true;
#else
  return false;
#endif
}

bool IsGpuRasterizationEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableGpuRasterization))
    return false;
  else if (command_line.HasSwitch(switches::kEnableGpuRasterization))
    return true;

  if (IsGpuRasterizationBlacklisted()) {
    return false;
  }

#if defined(OS_ANDROID)
  return true;
#endif

  // Gpu Rasterization on platforms that are not fully enabled is controlled by
  // a finch experiment.
  return base::FeatureList::IsEnabled(features::kDefaultEnableGpuRasterization);
}

bool IsAsyncWorkerContextEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableGpuAsyncWorkerContext))
    return false;
  else if (command_line.HasSwitch(switches::kEnableGpuAsyncWorkerContext))
    return true;

  return false;
}

bool IsForceGpuRasterizationEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kForceGpuRasterization);
}

int GpuRasterizationMSAASampleCount() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch(switches::kGpuRasterizationMSAASampleCount))
#if defined(OS_ANDROID)
    return 4;
#else
    // Desktop platforms will compute this automatically based on DPI.
    return -1;
#endif
  std::string string_value = command_line.GetSwitchValueASCII(
      switches::kGpuRasterizationMSAASampleCount);
  int msaa_sample_count = 0;
  if (base::StringToInt(string_value, &msaa_sample_count) &&
      msaa_sample_count >= kMinMSAASampleCount) {
    return msaa_sample_count;
  } else {
    DLOG(WARNING) << "Failed to parse switch "
                  << switches::kGpuRasterizationMSAASampleCount << ": "
                  << string_value;
    return 0;
  }
}

bool IsMainFrameBeforeActivationEnabled() {
  if (base::SysInfo::NumberOfProcessors() < 4)
    return false;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(cc::switches::kDisableMainFrameBeforeActivation))
    return false;

  if (command_line.HasSwitch(cc::switches::kEnableMainFrameBeforeActivation))
    return true;

  return base::FeatureList::IsEnabled(features::kMainFrameBeforeActivation);
}

base::DictionaryValue* GetFeatureStatus() {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  std::string gpu_access_blocked_reason;
  bool gpu_access_blocked =
      !manager->GpuAccessAllowed(&gpu_access_blocked_reason);

  base::DictionaryValue* feature_status_dict = new base::DictionaryValue();

  bool eof = false;
  for (size_t i = 0; !eof; ++i) {
    const GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfo(i, &eof);
    std::string status;
    if (gpu_feature_info.disabled) {
      status = "disabled";
      if (gpu_feature_info.fallback_to_software)
        status += "_software";
      else
        status += "_off";
    } else if (gpu_feature_info.blocked ||
               gpu_access_blocked) {
      status = "unavailable";
      if (gpu_feature_info.fallback_to_software)
        status += "_software";
      else
        status += "_off";
    } else {
      status = "enabled";
      if (gpu_feature_info.name == kWebGLFeatureName &&
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING))
        status += "_readback";
      if (gpu_feature_info.name == kRasterizationFeatureName) {
        if (IsForceGpuRasterizationEnabled())
          status += "_force";
      }
      if (gpu_feature_info.name == kMultipleRasterThreadsFeatureName) {
        const base::CommandLine& command_line =
            *base::CommandLine::ForCurrentProcess();
        if (command_line.HasSwitch(switches::kNumRasterThreads))
          status += "_force";
      }
      if (gpu_feature_info.name == kMultipleRasterThreadsFeatureName)
        status += "_on";
    }
    if (gpu_feature_info.name == kWebGLFeatureName &&
        (gpu_feature_info.blocked || gpu_access_blocked) &&
        manager->ShouldUseSwiftShader()) {
      status = "unavailable_software";
    }

    feature_status_dict->SetString(
        gpu_feature_info.name.c_str(), status.c_str());
  }
  return feature_status_dict;
}

base::Value* GetProblems() {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  std::string gpu_access_blocked_reason;
  bool gpu_access_blocked =
      !manager->GpuAccessAllowed(&gpu_access_blocked_reason);

  base::ListValue* problem_list = new base::ListValue();
  manager->GetBlacklistReasons(problem_list);

  if (gpu_access_blocked) {
    base::DictionaryValue* problem = new base::DictionaryValue();
    problem->SetString("description",
        "GPU process was unable to boot: " + gpu_access_blocked_reason);
    problem->Set("crBugs", new base::ListValue());
    problem->Set("webkitBugs", new base::ListValue());
    base::ListValue* disabled_features = new base::ListValue();
    disabled_features->AppendString("all");
    problem->Set("affectedGpuSettings", disabled_features);
    problem->SetString("tag", "disabledFeatures");
    problem_list->Insert(0, problem);
  }

  bool eof = false;
  for (size_t i = 0; !eof; ++i) {
    const GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfo(i, &eof);
    if (gpu_feature_info.disabled) {
      std::unique_ptr<base::DictionaryValue> problem(
          new base::DictionaryValue());
      problem->SetString(
          "description", gpu_feature_info.disabled_description);
      problem->Set("crBugs", new base::ListValue());
      problem->Set("webkitBugs", new base::ListValue());
      base::ListValue* disabled_features = new base::ListValue();
      disabled_features->AppendString(gpu_feature_info.name);
      problem->Set("affectedGpuSettings", disabled_features);
      problem->SetString("tag", "disabledFeatures");
      problem_list->Append(std::move(problem));
    }
  }
  return problem_list;
}

std::vector<std::string> GetDriverBugWorkarounds() {
  return GpuDataManagerImpl::GetInstance()->GetDriverBugWorkarounds();
}

}  // namespace content
