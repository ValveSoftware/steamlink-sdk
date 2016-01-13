// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_WIN_H_
#define CONTENT_COMMON_SANDBOX_WIN_H_

#include "content/common/content_export.h"
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
void SetJobLevel(const base::CommandLine& cmd_line,
                 sandbox::JobLevel job_level,
                 uint32 ui_exceptions,
                 sandbox::TargetPolicy* policy);

// Closes handles that are opened at process creation and initialization.
void AddBaseHandleClosePolicy(sandbox::TargetPolicy* policy);

bool InitBrokerServices(sandbox::BrokerServices* broker_services);

bool InitTargetServices(sandbox::TargetServices* target_services);

// Returns whether DirectWrite font rendering should be used.
CONTENT_EXPORT bool ShouldUseDirectWrite();

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_WIN_H_
