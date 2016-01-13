// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/compositor_util.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_feature_type.h"

namespace content {

namespace {

static bool IsGpuRasterizationBlacklisted() {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  bool field_trial_enabled =
      (base::FieldTrialList::FindFullName(
           "GpuRasterizationExpandedDeviceWhitelist") == "Enabled");

  if (field_trial_enabled) {
    return manager->IsFeatureBlacklisted(
               gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION) &&
           manager->IsFeatureBlacklisted(
               gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION_FIELD_TRIAL);
  }

  return manager->IsFeatureBlacklisted(
        gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION);
}

const char* kGpuCompositingFeatureName = "gpu_compositing";
const char* kWebGLFeatureName = "webgl";
const char* kRasterizationFeatureName = "rasterization";
const char* kThreadedRasterizationFeatureName = "threaded_rasterization";

struct GpuFeatureInfo {
  std::string name;
  bool blocked;
  bool disabled;
  std::string disabled_description;
  bool fallback_to_software;
};

const GpuFeatureInfo GetGpuFeatureInfo(size_t index, bool* eof) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();

  const GpuFeatureInfo kGpuFeatureInfo[] = {
      {
          "2d_canvas",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS),
          command_line.HasSwitch(switches::kDisableAccelerated2dCanvas) ||
          !GpuDataManagerImpl::GetInstance()->
              GetGPUInfo().SupportsAccelerated2dCanvas(),
          "Accelerated 2D canvas is unavailable: either disabled at the command"
          " line or not supported by the current system.",
          true
      },
      {
          kGpuCompositingFeatureName,
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_GPU_COMPOSITING),
          false,
          "Gpu compositing has been disabled, either via about:flags or"
          " command line. The browser will fall back to software compositing"
          " and hardware acceleration will be unavailable.",
          true
      },
      {
          kWebGLFeatureName,
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_WEBGL),
          command_line.HasSwitch(switches::kDisableExperimentalWebGL),
          "WebGL has been disabled, either via about:flags or command line.",
          false
      },
      {
          "flash_3d",
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH3D),
          command_line.HasSwitch(switches::kDisableFlash3d),
          "Using 3d in flash has been disabled, either via about:flags or"
          " command line.",
          true
      },
      {
          "flash_stage3d",
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D),
          command_line.HasSwitch(switches::kDisableFlashStage3d),
          "Using Stage3d in Flash has been disabled, either via about:flags or"
          " command line.",
          true
      },
      {
          "flash_stage3d_baseline",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE) ||
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D),
          command_line.HasSwitch(switches::kDisableFlashStage3d),
          "Using Stage3d Baseline profile in Flash has been disabled, either"
          " via about:flags or command line.",
          true
      },
      {
          "video_decode",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE),
          command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode),
          "Accelerated video decode has been disabled, either via about:flags"
          " or command line.",
          true
      },
#if defined(ENABLE_WEBRTC)
      {
          "video_encode",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE),
          command_line.HasSwitch(switches::kDisableWebRtcHWEncoding),
          "Accelerated video encode has been disabled, either via about:flags"
          " or command line.",
          true
      },
#endif
#if defined(OS_CHROMEOS)
      {
          "panel_fitting",
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_PANEL_FITTING),
          command_line.HasSwitch(switches::kDisablePanelFitting),
          "Panel fitting has been disabled, either via about:flags or command"
          " line.",
          false
      },
#endif
      {
          kRasterizationFeatureName,
          IsGpuRasterizationBlacklisted() &&
          !IsGpuRasterizationEnabled() && !IsForceGpuRasterizationEnabled(),
          !IsGpuRasterizationEnabled() && !IsForceGpuRasterizationEnabled() &&
          !IsGpuRasterizationBlacklisted(),
          "Accelerated rasterization has been disabled, either via about:flags"
          " or command line.",
          true
      },
      {
          kThreadedRasterizationFeatureName,
          false,
          !IsImplSidePaintingEnabled(),
          "Threaded rasterization has not been enabled or"
          " is not supported by the current system.",
          false
      }

  };
  DCHECK(index < arraysize(kGpuFeatureInfo));
  *eof = (index == arraysize(kGpuFeatureInfo) - 1);
  return kGpuFeatureInfo[index];
}

}  // namespace

bool IsPinchVirtualViewportEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Command line switches take precedence over platform default.
  if (command_line.HasSwitch(cc::switches::kDisablePinchVirtualViewport))
    return false;
  if (command_line.HasSwitch(cc::switches::kEnablePinchVirtualViewport))
    return true;

#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool IsThreadedCompositingEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Command line switches take precedence over blacklist.
  if (command_line.HasSwitch(switches::kDisableThreadedCompositing))
    return false;
  if (command_line.HasSwitch(switches::kEnableThreadedCompositing))
    return true;

#if defined(USE_AURA) || defined(OS_MACOSX)
  // We always want threaded compositing on Aura and Mac (the fallback is a
  // threaded software compositor).
  return true;
#else
  return false;
#endif
}

bool IsDelegatedRendererEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool enabled = false;

#if defined(USE_AURA)
  // Enable on Aura.
  enabled = true;
#endif

  // Flags override.
  enabled |= command_line.HasSwitch(switches::kEnableDelegatedRenderer);
  enabled &= !command_line.HasSwitch(switches::kDisableDelegatedRenderer);

  // Needs compositing, and thread.
  if (enabled && !IsThreadedCompositingEnabled()) {
    enabled = false;
    LOG(ERROR) << "Disabling delegated-rendering because it needs "
               << "force-compositing-mode and threaded-compositing.";
  }

  return enabled;
}

bool IsImplSidePaintingEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableImplSidePainting))
    return false;
  else if (command_line.HasSwitch(switches::kEnableImplSidePainting))
    return true;
  else if (command_line.HasSwitch(
      switches::kEnableBleedingEdgeRenderingFastPaths))
    return true;

#if defined(OS_MACOSX) || defined(OS_WIN)
  return false;
#else
  return IsThreadedCompositingEnabled();
#endif
}

bool IsGpuRasterizationEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (!IsImplSidePaintingEnabled())
    return false;

  if (command_line.HasSwitch(switches::kDisableGpuRasterization))
    return false;
  else if (command_line.HasSwitch(switches::kEnableGpuRasterization))
    return true;

  if (IsGpuRasterizationBlacklisted()) {
    return false;
  }

  return true;
}

bool IsForceGpuRasterizationEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (!IsImplSidePaintingEnabled())
    return false;

  return command_line.HasSwitch(switches::kForceGpuRasterization);
}

base::Value* GetFeatureStatus() {
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
      if (gpu_feature_info.name == kThreadedRasterizationFeatureName)
        status += "_ok";
    } else if (gpu_feature_info.blocked ||
               gpu_access_blocked) {
      status = "unavailable";
      if (gpu_feature_info.fallback_to_software) {
        status += "_software";
      } else
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
      if (gpu_feature_info.name == kThreadedRasterizationFeatureName)
        status += "_on";
    }
    if (gpu_feature_info.name == kGpuCompositingFeatureName) {
      if (IsThreadedCompositingEnabled())
        status += "_threaded";
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
      base::DictionaryValue* problem = new base::DictionaryValue();
      problem->SetString(
          "description", gpu_feature_info.disabled_description);
      problem->Set("crBugs", new base::ListValue());
      problem->Set("webkitBugs", new base::ListValue());
      base::ListValue* disabled_features = new base::ListValue();
      disabled_features->AppendString(gpu_feature_info.name);
      problem->Set("affectedGpuSettings", disabled_features);
      problem->SetString("tag", "disabledFeatures");
      problem_list->Append(problem);
    }
  }
  return problem_list;
}

base::Value* GetDriverBugWorkarounds() {
  base::ListValue* workaround_list = new base::ListValue();
  GpuDataManagerImpl::GetInstance()->GetDriverBugWorkarounds(workaround_list);
  return workaround_list;
}

}  // namespace content
