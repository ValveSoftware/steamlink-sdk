// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_MAC_H_
#define COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_MAC_H_

#include <string>

// This header defines the entry points for Breakpad integration.

namespace breakpad {

// Initializes Breakpad.
void InitCrashReporter(const std::string& process_type);

// Give Breakpad a chance to store information about the current process.
// Extra information requires a parsed command line, so call this after
// CommandLine::Init has been called.
void InitCrashProcessInfo(const std::string& process_type_switch);

// Is Breakpad enabled?
bool IsCrashReporterEnabled();

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_APP_BREAKPAD_MAC_H_
