// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/extensions/extension_apitest.h"

class ResourcesPrivateApiTest : public ExtensionApiTest {
 public:
  ResourcesPrivateApiTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourcesPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(ResourcesPrivateApiTest, GetStrings) {
  ASSERT_TRUE(RunComponentExtensionTest("resources_private/get_strings"));
}
