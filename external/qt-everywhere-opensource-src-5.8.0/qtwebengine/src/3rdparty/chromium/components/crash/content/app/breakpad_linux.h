// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Public interface for enabling Breakpad on Linux systems.

#ifndef COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_LINUX_H_
#define COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_LINUX_H_

#include <string>

#include "build/build_config.h"

namespace breakpad {

// Turns on the crash reporter in any process.
extern void InitCrashReporter(const std::string& process_type);

#if defined(OS_ANDROID)
// Enables the crash reporter in child processes.
extern void InitNonBrowserCrashReporterForAndroid(
    const std::string& process_type);

// Enables *micro*dump only. Can be called from any process.
extern void InitMicrodumpCrashHandlerIfNecessary(
    const std::string& process_type);

extern void AddGpuFingerprintToMicrodumpCrashHandler(
    const std::string& gpu_fingerprint);
#endif

// Checks if crash reporting is enabled. Note that this is not the same as
// being opted into metrics reporting (and crash reporting), which controls
// whether InitCrashReporter() is called.
bool IsCrashReporterEnabled();

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_LINUX_H_
