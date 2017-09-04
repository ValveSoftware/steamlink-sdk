// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_DEBUG_HANDLING_LINUX_H_
#define CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_DEBUG_HANDLING_LINUX_H_

#include "base/macros.h"

namespace content {

class SandboxDebugHandling {
 public:
  // Depending on the command line, set the current process as
  // non dumpable. Also set any signal handlers for sandbox
  // debugging.
  static bool SetDumpableStatusAndHandlers();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxDebugHandling);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_LINUX_SANDBOX_DEBUG_HANDLING_LINUX_H_
