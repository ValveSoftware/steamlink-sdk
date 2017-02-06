// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class RenderWidgetHostViewChildFrameTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewChildFrameTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    SetupCrossSiteRedirector(embedded_test_server());
  }

  void CheckScreenWidth(RenderFrameHost* render_frame_host) {
    int width;
    ExecuteScriptAndGetValue(render_frame_host, "window.screen.width")
        ->GetAsInteger(&width);
    EXPECT_EQ(expected_screen_width_, width);
  }

  void set_expected_screen_width(int width) {
    expected_screen_width_ = width;
  }

 private:
  int expected_screen_width_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewChildFrameTest);
};

// Tests that the screen is properly reflected for RWHVChildFrame.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewChildFrameTest, Screen) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()->root();

  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  int main_frame_screen_width = 0;
  ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
      "window.screen.width")->GetAsInteger(&main_frame_screen_width);
  set_expected_screen_width(main_frame_screen_width);
  EXPECT_FALSE(main_frame_screen_width == 0);

  shell()->web_contents()->ForEachFrame(
      base::Bind(&RenderWidgetHostViewChildFrameTest::CheckScreenWidth, this));
}

}  // namespace content
