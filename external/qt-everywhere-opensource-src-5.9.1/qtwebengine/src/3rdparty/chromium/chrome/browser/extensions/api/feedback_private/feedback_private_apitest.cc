// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

class FeedbackApiTest: public ExtensionApiTest {
 public:
  FeedbackApiTest() {}
  ~FeedbackApiTest() override {}
};

// Fails on Linux/Win.  http://crbug.com/408917
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif

IN_PROC_BROWSER_TEST_F(FeedbackApiTest, MAYBE_Basic) {
  EXPECT_TRUE(RunExtensionTest("feedback_private/basic")) << message_;
}

} // namespace extensions
