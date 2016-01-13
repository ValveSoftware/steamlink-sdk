// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_HARNESS_UTILS_H_
#define PPAPI_TESTS_TEST_HARNESS_UTILS_H_

#include <string>
#include "base/files/file_path.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

// This file specifies utility functions used in Pepper testing in
// browser_tests and content_browsertests.

namespace base {
class CommandLine;
}

namespace ppapi {

// Strips prefixes used to annotate tests from a test name.
std::string PPAPI_SHARED_EXPORT StripTestPrefixes(const std::string& test_name);

// Returns a platform-specific filename relative to the chrome executable.
base::FilePath::StringType PPAPI_SHARED_EXPORT GetTestLibraryName();

} // namespace ppapi

#endif  // PPAPI_TESTS_TEST_HARNESS_UTILS_H_
