// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/sandbox_init_mac.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "content/common/sandbox_mac.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"

namespace content {

bool InitializeSandbox(int sandbox_type, const base::FilePath& allowed_dir) {
  // Warm up APIs before turning on the sandbox.
  Sandbox::SandboxWarmup(sandbox_type);

  // Actually sandbox the process.
  return Sandbox::EnableSandbox(sandbox_type, allowed_dir);
}

// Fill in |sandbox_type| and |allowed_dir| based on the command line,  returns
// false if the current process type doesn't need to be sandboxed or if the
// sandbox was disabled from the command line.
bool GetSandboxTypeFromCommandLine(int* sandbox_type,
                                   base::FilePath* allowed_dir) {
  DCHECK(sandbox_type);
  DCHECK(allowed_dir);

  *sandbox_type = -1;
  *allowed_dir = base::FilePath();  // Empty by default.

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kNoSandbox))
    return false;

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  if (process_type.empty()) {
    // Browser process isn't sandboxed.
    return false;
  } else if (process_type == switches::kRendererProcess) {
    *sandbox_type = SANDBOX_TYPE_RENDERER;
  } else if (process_type == switches::kUtilityProcess) {
    // Utility process sandbox.
    *sandbox_type = SANDBOX_TYPE_UTILITY;
    *allowed_dir =
        command_line.GetSwitchValuePath(switches::kUtilityProcessAllowedDir);
  } else if (process_type == switches::kGpuProcess) {
    if (command_line.HasSwitch(switches::kDisableGpuSandbox))
      return false;
    *sandbox_type = SANDBOX_TYPE_GPU;
  } else if (process_type == switches::kPpapiBrokerProcess) {
    return false;
  } else if (process_type == switches::kPpapiPluginProcess) {
    *sandbox_type = SANDBOX_TYPE_PPAPI;
  } else {
    // This is a process which we don't know about, i.e. an embedder-defined
    // process. If the embedder wants it sandboxed, they have a chance to return
    // the sandbox profile in ContentClient::GetSandboxProfileForSandboxType.
    return false;
  }
  return true;
}

bool InitializeSandbox() {
  int sandbox_type = 0;
  base::FilePath allowed_dir;
  if (!GetSandboxTypeFromCommandLine(&sandbox_type, &allowed_dir))
    return true;
  return InitializeSandbox(sandbox_type, allowed_dir);
}

}  // namespace content
