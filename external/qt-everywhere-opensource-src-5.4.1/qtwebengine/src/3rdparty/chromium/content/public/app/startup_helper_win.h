// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_STARTUP_HELPER_WIN_H_
#define CONTENT_PUBLIC_APP_STARTUP_HELPER_WIN_H_

namespace base {
class CommandLine;
}

namespace sandbox {
struct SandboxInterfaceInfo;
}

// This file contains functions that any embedder that's not using ContentMain
// will want to call at startup.
// NOTE: we never want to CONTENT_EXPORT these functions, they must run in the
// same module that calls them.
namespace content {

// Initializes the sandbox code and turns on DEP. Note: This function
// must be *statically* linked into the executable (along with the static
// sandbox library); it will not work correctly if it is exported from a
// DLL and linked in.
void InitializeSandboxInfo(sandbox::SandboxInterfaceInfo* sandbox_info);

// Register the invalid param handler and pure call handler to be able to
// notify breakpad when it happens.
void RegisterInvalidParamHandler();

// Sets up the CRT's debugging macros to output to stdout.
void SetupCRT(const base::CommandLine& command_line);

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_STARTUP_HELPER_WIN_H_
