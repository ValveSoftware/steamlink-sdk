// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_per_process_browsertest.h"

#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace content {

namespace {

// Helper class for TextInputClientMac.
class TextInputClientMacHelper {
 public:
  TextInputClientMacHelper() {}
  ~TextInputClientMacHelper() {}

  void WaitForStringFromRange(RenderWidgetHost* rwh, const gfx::Range& range) {
    GetStringFromRangeForRenderWidget(
        rwh, range, base::Bind(&TextInputClientMacHelper::OnResult,
                               base::Unretained(this)));
    loop_runner_ = new MessageLoopRunner();
    loop_runner_->Run();
  }

  void WaitForStringAtPoint(RenderWidgetHost* rwh, const gfx::Point& point) {
    GetStringAtPointForRenderWidget(
        rwh, point, base::Bind(&TextInputClientMacHelper::OnResult,
                               base::Unretained(this)));
    loop_runner_ = new MessageLoopRunner();
    loop_runner_->Run();
  }
  const std::string& word() const { return word_; }
  const gfx::Point& point() const { return point_; }

 private:
  void OnResult(const std::string& string, const gfx::Point& point) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&TextInputClientMacHelper::OnResult,
                     base::Unretained(this), string, point));
      return;
    }
    word_ = string;
    point_ = point;

    if (loop_runner_ && loop_runner_->loop_running())
      loop_runner_->Quit();
  }

  std::string word_;
  gfx::Point point_;
  scoped_refptr<MessageLoopRunner> loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TextInputClientMacHelper);
};

}  // namespace

// Site per process browser tests inside content which are specific to Mac OSX
// platform.
class SitePerProcessMacBrowserTest : public SitePerProcessBrowserTest {};

// This test will load a text only page inside a child frame and then queries
// the string range which includes the first word. Then it uses the returned
// point to query the text again and verifies that correct result is returned.
// Finally, the returned words are compared against the first word in the html
// file which is "This".
IN_PROC_BROWSER_TEST_F(SitePerProcessMacBrowserTest,
                       GetStringFromRangeAndPointChildFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  NavigateFrameToURL(child,
                     embedded_test_server()->GetURL("b.com", "/title1.html"));

  RenderWidgetHost* child_widget_host =
      child->current_frame_host()->GetRenderWidgetHost();
  TextInputClientMacHelper helper;

  // Get string from range.
  helper.WaitForStringFromRange(child_widget_host, gfx::Range(0, 4));
  gfx::Point point = helper.point();
  std::string word = helper.word();

  // Now get it at a given point.
  point.SetPoint(point.x(),
                 child_widget_host->GetView()->GetViewBounds().size().height() -
                     point.y());
  helper.WaitForStringAtPoint(child_widget_host, point);
  EXPECT_EQ(word, helper.word());
  EXPECT_EQ("This", word);
}

// This test will load a text only page and then queries the string range which
// includes the first word. Then it uses the returned point to query the text
// again and verifies that correct result is returned. Finally, the returned
// words are compared against the first word in the html file which is "This".
IN_PROC_BROWSER_TEST_F(SitePerProcessMacBrowserTest,
                       GetStringFromRangeAndPointMainFrame) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderWidgetHost* widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  TextInputClientMacHelper helper;

  // Get string from range.
  helper.WaitForStringFromRange(widget_host, gfx::Range(0, 4));
  gfx::Point point = helper.point();
  std::string word = helper.word();

  // Now get it at a given point.
  point.SetPoint(
      point.x(),
      widget_host->GetView()->GetViewBounds().size().height() - point.y());
  helper.WaitForStringAtPoint(widget_host, point);
  EXPECT_EQ(word, helper.word());
  EXPECT_EQ("This", word);
}
}  // namespace content
