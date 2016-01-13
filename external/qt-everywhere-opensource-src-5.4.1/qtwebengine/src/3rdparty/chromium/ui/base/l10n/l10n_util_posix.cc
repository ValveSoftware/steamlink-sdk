// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#if defined(OS_CHROMEOS)
#include "base/basictypes.h"
#include "base/strings/string_util.h"
#endif

namespace l10n_util {

// Return true blindly for now.
bool IsLocaleSupportedByOS(const std::string& locale) {
#if !defined(OS_CHROMEOS)
  return true;
#else
  // We don't have translations yet for am, and sw.
  // TODO(jungshik): Once the above issues are resolved, change this back
  // to return true.
  static const char* kUnsupportedLocales[] = {"am", "sw"};
  for (size_t i = 0; i < arraysize(kUnsupportedLocales); ++i) {
    if (LowerCaseEqualsASCII(locale, kUnsupportedLocales[i]))
      return false;
  }
  return true;
#endif
}

}  // namespace l10n_util
