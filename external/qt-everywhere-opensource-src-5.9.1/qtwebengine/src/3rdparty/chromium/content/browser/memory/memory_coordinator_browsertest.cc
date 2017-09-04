// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_coordinator.h"

#include "base/test/scoped_feature_list.h"
#include "content/browser/browser_main_loop.h"
#include "content/public/common/content_features.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

namespace content {

class MemoryCoordinatorTest : public ContentBrowserTest {
 public:
  MemoryCoordinatorTest() {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kMemoryCoordinator);
    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MemoryCoordinatorTest);
};

// TODO(bashi): Enable this test on macos when MemoryMonitorMac is implemented.
#if defined(OS_MACOSX)
#define MAYBE_HandleAdded DISABLED_HandleAdded
#else
#define MAYBE_HandleAdded HandleAdded
#endif
IN_PROC_BROWSER_TEST_F(MemoryCoordinatorTest, MAYBE_HandleAdded) {
  GURL url = GetTestUrl("", "simple_page.html");
  NavigateToURL(shell(), url);
  EXPECT_EQ(1u, MemoryCoordinator::GetInstance()->NumChildrenForTesting());
}

}  // namespace content
