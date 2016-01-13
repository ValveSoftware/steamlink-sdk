// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_switches_internal.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

bool IsPinchToZoomEnabled() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // --disable-pinch should always disable pinch
  if (command_line.HasSwitch(switches::kDisablePinch))
    return false;

#if defined(OS_WIN)
  return base::win::GetVersion() >= base::win::VERSION_WIN8;
#elif defined(OS_CHROMEOS)
  return true;
#else
  return command_line.HasSwitch(switches::kEnableViewport) ||
      command_line.HasSwitch(switches::kEnablePinch);
#endif
}

} // namespace content
