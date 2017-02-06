// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares a raw symbol and should be included only once in a
// certain binary target. This needs to be run before we raise the sandbox,
// which means that it can't use mojo. Our runners will dig around in the
// symbol table and run this before the mojo system is initialized.

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/i18n/icu_util.h"
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "mojo/public/c/system/types.h"

#if !defined(OS_ANDROID)
#include "third_party/icu/source/i18n/unicode/timezone.h"
#endif

extern "C" {
#if defined(WIN32)
__declspec(dllexport) void __cdecl
#else
void __attribute__((visibility("default")))
#endif
InitializeBase(const uint8_t* icu_data) {
  base::RandUint64();
  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::NumberOfProcessors();

#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
  // Initialize core ICU. We must perform the full initialization before we
  // initialize icu::TimeZone subsystem because otherwise ICU gets in a state
  // where the timezone data is disconnected from the locale data which can
  // cause crashes.
  CHECK(base::i18n::InitializeICUFromRawMemory(icu_data));
#endif  // ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE

#if !defined(OS_ANDROID)
  // ICU DateFormat class (used in base/time_format.cc) needs to get the
  // Olson timezone ID by accessing the zoneinfo files on disk. After
  // TimeZone::createDefault is called once here, the timezone ID is
  // cached and there's no more need to access the file system.
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
#endif
}
}
