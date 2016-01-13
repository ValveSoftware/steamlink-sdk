// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the ChromeELF.

#ifndef CHROME_ELF_CHROME_ELF_CONSTANTS_H_
#define CHROME_ELF_CHROME_ELF_CONSTANTS_H_

#include <windows.h>

// directory names
extern const wchar_t kAppDataDirName[];
extern const wchar_t kCanaryAppDataDirName[];
extern const wchar_t kLocalStateFilename[];
extern const wchar_t kPreferencesFilename[];
extern const wchar_t kUserDataDirName[];

namespace blacklist {

// The registry path of the blacklist beacon.
extern const wchar_t kRegistryBeaconPath[];

// The registry path of the finch blacklist dlls.
extern const wchar_t kRegistryFinchListPath[];

// The properties for the blacklist beacon.
extern const wchar_t kBeaconVersion[];
extern const wchar_t kBeaconState[];
extern const wchar_t kBeaconAttemptCount[];

// The number of failures that can occur on startup with the beacon enabled
// before we give up and turn off the blacklist.
extern const DWORD kBeaconMaxAttempts;

// The states for the blacklist setup code.
enum BlacklistState {
  BLACKLIST_DISABLED = 0,
  BLACKLIST_ENABLED,
  // The blacklist setup code is running. If this is the state at startup, it
  // means the last setup crashed.
  BLACKLIST_SETUP_RUNNING,
  // If the last setup crashed, we reassign the state to failed.
  BLACKLIST_SETUP_FAILED,
  // Always keep this at the end.
  BLACKLIST_STATE_MAX,
};

}  // namespace blacklist

#endif  // CHROME_ELF_CHROME_ELF_CONSTANTS_H_
