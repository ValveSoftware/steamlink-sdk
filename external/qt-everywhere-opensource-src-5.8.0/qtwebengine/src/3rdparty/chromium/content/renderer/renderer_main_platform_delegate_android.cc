// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"

#ifdef USE_SECCOMP_BPF
#include "content/common/sandbox_linux/android/sandbox_bpf_base_policy_android.h"
#include "content/public/common/content_features.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#endif

namespace content {

namespace {

// Scoper class to record a SeccompSandboxStatus UMA value.
class RecordSeccompStatus {
 public:
  enum SeccompSandboxStatus {
    NOT_SUPPORTED = 0,  // Seccomp is not supported.
    DETECTION_FAILED,   // Run-time detection of Seccomp+TSYNC failed.
    FEATURE_DISABLED,   // Sandbox was disabled by FeatureList.
    FEATURE_ENABLED,    // Sandbox was enabled by FeatureList.
    ENGAGED,            // Sandbox was enabled and successfully turned on.
    STATUS_MAX
    // This enum is used by an UMA histogram, so only append values.
  };

  RecordSeccompStatus() : status_(NOT_SUPPORTED) {}

  ~RecordSeccompStatus() {
    UMA_HISTOGRAM_ENUMERATION("Android.SeccompStatus.RendererSandbox", status_,
                              STATUS_MAX);
  }

  void set_status(SeccompSandboxStatus status) { status_ = status; }

 private:
  SeccompSandboxStatus status_;
  DISALLOW_COPY_AND_ASSIGN(RecordSeccompStatus);
};

#ifdef USE_SECCOMP_BPF
// Determines if the running device should support Seccomp, based on the Android
// SDK version.
bool IsSeccompBPFSupportedBySDK() {
  const auto info = base::android::BuildInfo::GetInstance();
  if (info->sdk_int() < 22) {
    // Seccomp was never available pre-Lollipop.
    return false;
  } else if (info->sdk_int() == 22) {
    // On Lollipop-MR1, only select Nexus devices have Seccomp available.
    const char* const kDevices[] = {
        "deb",   "flo",   "hammerhead", "mako",
        "manta", "shamu", "sprout",     "volantis",
    };

    for (const auto& device : kDevices) {
      if (strcmp(device, info->device()) == 0) {
        return true;
      }
    }
  } else {
    // On Marshmallow and higher, Seccomp is required by CTS.
    return true;
  }
  return false;
}
#endif  // USE_SECCOMP_BPF

}  // namespace

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters) {}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  RecordSeccompStatus status_uma;

#ifdef USE_SECCOMP_BPF
  // Determine if Seccomp is available via the Android SDK version.
  if (!IsSeccompBPFSupportedBySDK())
    return true;

  // Do run-time detection to ensure that support is present.
  if (!sandbox::SandboxBPF::SupportsSeccompSandbox(
          sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED)) {
    status_uma.set_status(RecordSeccompStatus::DETECTION_FAILED);
    LOG(WARNING) << "Seccomp support should be present, but detection "
        << "failed. Continuing without Seccomp-BPF.";
    return true;
  }

  // Seccomp has been detected, check if the field trial experiment should run.
  if (base::FeatureList::IsEnabled(features::kSeccompSandboxAndroid)) {
    status_uma.set_status(RecordSeccompStatus::FEATURE_ENABLED);

    sandbox::SandboxBPF sandbox(new SandboxBPFBasePolicyAndroid());
    CHECK(sandbox.StartSandbox(
        sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED));

    status_uma.set_status(RecordSeccompStatus::ENGAGED);
  } else {
    status_uma.set_status(RecordSeccompStatus::FEATURE_DISABLED);
  }
#endif
  return true;
}

}  // namespace content
