// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/test_harness_utils.h"

#include <string>
#include "base/macros.h"

namespace ppapi {

std::string StripTestPrefixes(const std::string& test_name) {
  const char* const kTestPrefixes[] = {
      "FAILS_", "FLAKY_", "DISABLED_", "SLOW_" };
  for (size_t i = 0; i < arraysize(kTestPrefixes); ++i)
    if (test_name.find(kTestPrefixes[i]) == 0)
      return test_name.substr(strlen(kTestPrefixes[i]));
  return test_name;
}

base::FilePath::StringType GetTestLibraryName() {
#if defined(OS_WIN)
  return L"ppapi_tests.dll";
#elif defined(OS_MACOSX)
  return "ppapi_tests.plugin";
#elif defined(OS_POSIX)
  return "libppapi_tests.so";
#endif
}

}  // namespace ppapi
