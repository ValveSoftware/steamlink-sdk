// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {
namespace {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, UncaughtExceptionLogging) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("uncaught_exception_logging")) << message_;
}

}  // namespace
}  // namespace extensions
