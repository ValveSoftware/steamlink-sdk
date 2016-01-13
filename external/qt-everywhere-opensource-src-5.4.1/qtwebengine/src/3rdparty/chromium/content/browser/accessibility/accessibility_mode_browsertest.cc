// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const char kMinimalPageDataURL[] =
    "data:text/html,<html><head></head><body>Hello, world</body></html>";

class AccessibilityModeTest : public ContentBrowserTest {
 protected:
  WebContents* web_contents() {
    return shell()->web_contents();
  }

  RenderWidgetHostImpl* rwhi() {
    RenderWidgetHost* rwh =
        web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost();
    return RenderWidgetHostImpl::From(rwh);
  }

  RenderWidgetHostViewBase* host_view() {
    return static_cast<RenderWidgetHostViewBase*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

  void ExpectBrowserAccessibilityManager(bool expect_bam,
                                         std::string message = "") {
    if (expect_bam) {
      EXPECT_NE((BrowserAccessibilityManager*)NULL,
                host_view()->GetBrowserAccessibilityManager()) << message;
    } else {
      EXPECT_EQ((BrowserAccessibilityManager*)NULL,
                host_view()->GetBrowserAccessibilityManager()) << message;
    }
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeOff) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));

  EXPECT_EQ(AccessibilityModeOff, rwhi()->accessibility_mode());
  ExpectBrowserAccessibilityManager(false);
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeComplete) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));
  ASSERT_EQ(AccessibilityModeOff, rwhi()->accessibility_mode());

  AccessibilityNotificationWaiter waiter(shell());
  rwhi()->AddAccessibilityMode(AccessibilityModeComplete);
  EXPECT_EQ(AccessibilityModeComplete, rwhi()->accessibility_mode());
  waiter.WaitForNotification();
  ExpectBrowserAccessibilityManager(true);
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeTreeOnly) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));
  ASSERT_EQ(AccessibilityModeOff, rwhi()->accessibility_mode());

  AccessibilityNotificationWaiter waiter(shell());
  rwhi()->AddAccessibilityMode(AccessibilityModeTreeOnly);
  EXPECT_EQ(AccessibilityModeTreeOnly, rwhi()->accessibility_mode());
  waiter.WaitForNotification();
  // No BrowserAccessibilityManager expected for AccessibilityModeTreeOnly
  ExpectBrowserAccessibilityManager(false);
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AddingModes) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));

  AccessibilityNotificationWaiter waiter(shell());
  rwhi()->AddAccessibilityMode(AccessibilityModeTreeOnly);
  EXPECT_EQ(AccessibilityModeTreeOnly, rwhi()->accessibility_mode());
  waiter.WaitForNotification();
  ExpectBrowserAccessibilityManager(false,
                                    "Should be no BrowserAccessibilityManager "
                                    "for AccessibilityModeTreeOnly");

  AccessibilityNotificationWaiter waiter2(shell());
  rwhi()->AddAccessibilityMode(AccessibilityModeComplete);
  EXPECT_EQ(AccessibilityModeComplete, rwhi()->accessibility_mode());
  waiter2.WaitForNotification();
  ExpectBrowserAccessibilityManager(true,
                                    "Should be a BrowserAccessibilityManager "
                                    "for AccessibilityModeComplete");
}

}  // namespace content
