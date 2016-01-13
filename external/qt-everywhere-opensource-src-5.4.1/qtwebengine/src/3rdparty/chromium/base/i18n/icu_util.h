// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICU_UTIL_H_
#define BASE_I18N_ICU_UTIL_H_

#include "build/build_config.h"
#include "base/i18n/base_i18n_export.h"

namespace base {
namespace i18n {

// Call this function to load ICU's data tables for the current process.  This
// function should be called before ICU is used.
BASE_I18N_EXPORT bool InitializeICU();

#if defined(OS_ANDROID)
// Android uses a file descriptor passed by browser process to initialize ICU
// in render processes.
BASE_I18N_EXPORT bool InitializeICUWithFileDescriptor(int data_fd);
#endif

// In a test binary, the call above might occur twice.
BASE_I18N_EXPORT void AllowMultipleInitializeCallsForTesting();

}  // namespace i18n
}  // namespace base

#endif  // BASE_I18N_ICU_UTIL_H_
