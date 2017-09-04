// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/chrome_elf_constants.h"

#if defined(GOOGLE_CHROME_BUILD)
// Using PRODUCT_STRING_PATH will make Google Chrome and Canary configurations
// collide. This was deemed to be the best option due to ELF's inability to
// depend on BrowserDistribution. http://crbug.com/577820
#define PRODUCT_STRING_PATH L"Google\\Chrome"
#elif defined(CHROMIUM_BUILD)
#define PRODUCT_STRING_PATH L"Chromium"
#else
#error Unknown branding
#endif

namespace blacklist {

const wchar_t kRegistryBeaconPath[] =
    L"SOFTWARE\\" PRODUCT_STRING_PATH L"\\BLBeacon";
const wchar_t kBeaconVersion[] = L"version";
const wchar_t kBeaconState[] = L"state";
const wchar_t kBeaconAttemptCount[] = L"failed_count";

const DWORD kBeaconMaxAttempts = 2;

}  // namespace blacklist

namespace elf_sec {

const wchar_t kRegSecurityFinchPath[] =
    L"SOFTWARE\\" PRODUCT_STRING_PATH L"\\BrowserSboxFinch";
const wchar_t kRegSecurityPath[] =
    L"SOFTWARE\\" PRODUCT_STRING_PATH L"\\BrowserSec";

}  // namespace elf_sec
