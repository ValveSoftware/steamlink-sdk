// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/history_controller.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"

namespace content {

class HistoryControllerTest : public RenderViewTest {
 public:
  ~HistoryControllerTest() override {}

  // Create a main frame with a single child frame.
  void SetUp() override {
    RenderViewTest::SetUp();
    LoadHTML("Parent frame <iframe name='frame'></iframe>");
  }

  void TearDown() override { RenderViewTest::TearDown(); }

  HistoryController* history_controller() {
    return static_cast<RenderViewImpl*>(view_)->history_controller();
  }
};

#if defined(OS_ANDROID)
// See https://crbug.com/472717
#define MAYBE_InertCommitRemovesChildren DISABLED_InertCommitRemovesChildren
#else
#define MAYBE_InertCommitRemovesChildren InertCommitRemovesChildren
#endif

TEST_F(HistoryControllerTest, MAYBE_InertCommitRemovesChildren) {
  // This test is moot when subframe FrameNavigationEntries are enabled, since
  // we don't use HistoryController in that case.
  if (SiteIsolationPolicy::UseSubframeNavigationEntries())
    return;

  HistoryEntry* entry = history_controller()->GetCurrentEntry();
  ASSERT_TRUE(entry);
  ASSERT_EQ(1ul, entry->root_history_node()->children().size());

  blink::WebHistoryItem item;
  item.initialize();
  RenderFrameImpl* main_frame =
      static_cast<RenderFrameImpl*>(view_->GetMainRenderFrame());

  // Don't clear children for in-page navigations.
  history_controller()->UpdateForCommit(main_frame, item,
                                        blink::WebHistoryInertCommit, true);
  EXPECT_EQ(1ul, entry->root_history_node()->children().size());

  // Clear children for cross-page navigations.
  history_controller()->UpdateForCommit(main_frame, item,
                                        blink::WebHistoryInertCommit, false);
  EXPECT_EQ(0ul, entry->root_history_node()->children().size());
}

}  // namespace
