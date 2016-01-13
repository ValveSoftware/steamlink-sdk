// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"

#ifdef USE_SECCOMP_BPF
#include "content/common/sandbox_linux/android/sandbox_bpf_base_policy_android.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"
#endif

#ifdef ENABLE_VTUNE_JIT_INTERFACE
#include "v8/src/third_party/vtune/v8-vtune.h"
#endif

namespace content {

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
    : parameters_(parameters) {
}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
#ifdef ENABLE_VTUNE_JIT_INTERFACE
  const CommandLine& command_line = parameters_.command_line;
  if (command_line.HasSwitch(switches::kEnableVtune))
    vTune::InitializeVtuneForV8();
#endif
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
}

bool RendererMainPlatformDelegate::EnableSandbox() {
#ifdef USE_SECCOMP_BPF
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSeccompFilterSandbox)) {
    return true;
  }

  sandbox::SandboxBPF sandbox;
  sandbox.SetSandboxPolicy(new SandboxBPFBasePolicyAndroid());
  CHECK(sandbox.StartSandbox(sandbox::SandboxBPF::PROCESS_MULTI_THREADED));
#endif
  return true;
}

}  // namespace content
