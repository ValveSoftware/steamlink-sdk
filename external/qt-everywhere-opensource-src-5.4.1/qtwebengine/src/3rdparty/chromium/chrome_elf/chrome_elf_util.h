// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_CHROME_ELF_UTIL_H_
#define CHROME_ELF_CHROME_ELF_UTIL_H_

#include "base/strings/string16.h"

// Returns true if |exe_path| points to a Chrome installed in an SxS
// installation.
bool IsCanary(const wchar_t* exe_path);

// Returns true if |exe_path| points to a per-user level Chrome installation.
bool IsSystemInstall(const wchar_t* exe_path);

// Returns true if current installation of Chrome is a multi-install.
bool IsMultiInstall(bool is_system_install);

// Returns true if usage stats collecting is enabled for this user.
bool AreUsageStatsEnabled(const wchar_t* exe_path);

// Returns true if a policy is in effect. |breakpad_enabled| will be set to true
// if stats collecting is permitted by this policy and false if not.
bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled);

// Returns true if invoked in a Chrome process other than the main browser
// process. False otherwise.
bool IsNonBrowserProcess();

#endif  // CHROME_ELF_CHROME_ELF_UTIL_H_
