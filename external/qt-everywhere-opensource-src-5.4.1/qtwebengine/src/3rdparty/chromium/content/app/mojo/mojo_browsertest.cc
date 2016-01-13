// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test.h"
#include "mojo/service_manager/service_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MojoTest : public ContentBrowserTest {
 public:
  MojoTest() {}

 protected:
  bool HasCreatedInstance() {
    return mojo::ServiceManager::TestAPI::HasCreatedInstance();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoTest);
};

// Placeholder test to confirm we are initializing Mojo.
IN_PROC_BROWSER_TEST_F(MojoTest, Init) {
  EXPECT_TRUE(HasCreatedInstance());
}

}  // namespace content
