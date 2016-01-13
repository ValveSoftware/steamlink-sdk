// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_UTIL_H_
#define CONTENT_COMMON_SANDBOX_UTIL_H_

#include "base/process/process.h"
#include "ipc/ipc_platform_file.h"

// This file contains cross-platform sandbox code internal to content.

namespace content {

// Platform neutral wrapper for making an exact copy of a handle for use in
// the target process. On Windows this wraps BrokerDuplicateHandle() with the
// DUPLICATE_SAME_ACCESS flag. On posix it behaves essentially the same as
// IPC::GetFileHandleForProcess()
IPC::PlatformFileForTransit BrokerGetFileHandleForProcess(
    base::PlatformFile handle,
    base::ProcessId target_process_id,
    bool should_close_source);

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_UTIL_H_
