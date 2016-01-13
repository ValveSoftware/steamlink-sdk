// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/chrome_elf_constants.h"

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kAppDataDirName[] = L"Google\\Chrome";
#else
const wchar_t kAppDataDirName[] = L"Chromium";
#endif
const wchar_t kCanaryAppDataDirName[] = L"Google\\Chrome SxS";
const wchar_t kLocalStateFilename[] = L"Local State";
const wchar_t kPreferencesFilename[] = L"Preferences";
const wchar_t kUserDataDirName[] = L"User Data";

namespace blacklist {

const wchar_t kRegistryBeaconPath[] = L"SOFTWARE\\Google\\Chrome\\BLBeacon";
const wchar_t kRegistryFinchListPath[] =
    L"SOFTWARE\\Google\\Chrome\\BLFinchList";
const wchar_t kBeaconVersion[] = L"version";
const wchar_t kBeaconState[] = L"state";
const wchar_t kBeaconAttemptCount[] = L"failed_count";

const DWORD kBeaconMaxAttempts = 2;

}  // namespace blacklist
