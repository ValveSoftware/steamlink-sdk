// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_WIN_H_
#define CONTENT_COMMON_SANDBOX_WIN_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/security_level.h"

namespace base {
class CommandLine;
}

namespace sandbox {
class BrokerServices;
class TargetPolicy;
class TargetServices;
}

namespace content {

// Wrapper around sandbox::TargetPolicy::SetJobLevel that checks if the sandbox
// should be let to run without a job object assigned.
sandbox::ResultCode SetJobLevel(const base::CommandLine& cmd_line,
                                sandbox::JobLevel job_level,
                                uint32_t ui_exceptions,
                                sandbox::TargetPolicy* policy);

// Closes handles that are opened at process creation and initialization.
sandbox::ResultCode AddBaseHandleClosePolicy(sandbox::TargetPolicy* policy);

// Add AppContainer policy for |sid| on supported OS.
sandbox::ResultCode AddAppContainerPolicy(sandbox::TargetPolicy* policy,
                                          const wchar_t* sid);

// Add the win32k lockdown policy on supported OS.
sandbox::ResultCode AddWin32kLockdownPolicy(sandbox::TargetPolicy* policy,
                                            bool enable_opm);

bool InitBrokerServices(sandbox::BrokerServices* broker_services);

bool InitTargetServices(sandbox::TargetServices* target_services);

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_WIN_H_
