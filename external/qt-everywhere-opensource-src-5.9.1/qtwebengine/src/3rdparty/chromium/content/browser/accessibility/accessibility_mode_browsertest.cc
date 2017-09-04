// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
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
  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void ExpectBrowserAccessibilityManager(bool expect_bam,
                                         std::string message = "") {
    if (expect_bam) {
      EXPECT_NE(
          (BrowserAccessibilityManager*)NULL,
          web_contents()->GetRootBrowserAccessibilityManager()) << message;
    } else {
      EXPECT_EQ(
          (BrowserAccessibilityManager*)NULL,
          web_contents()->GetRootBrowserAccessibilityManager()) << message;
    }
  }

  AccessibilityMode CorrectedAccessibility(AccessibilityMode mode) {
    return AddAccessibilityModeTo(GetBaseAccessibilityMode(), mode);
  }

  bool ShouldBeBrowserAccessibilityManager(AccessibilityMode mode) {
    mode = CorrectedAccessibility(mode);
    switch (mode) {
      case AccessibilityModeOff:
      case AccessibilityModeTreeOnly:
        return false;
      case AccessibilityModeComplete:
        return true;
      default:
        NOTREACHED();
    }
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeOff) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));

  EXPECT_EQ(CorrectedAccessibility(AccessibilityModeOff),
            web_contents()->GetAccessibilityMode());
  ExpectBrowserAccessibilityManager(
      ShouldBeBrowserAccessibilityManager(AccessibilityModeOff));
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeComplete) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));
  ASSERT_EQ(CorrectedAccessibility(AccessibilityModeOff),
            web_contents()->GetAccessibilityMode());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(AccessibilityModeComplete);
  EXPECT_EQ(AccessibilityModeComplete, web_contents()->GetAccessibilityMode());
  waiter.WaitForNotification();
  ExpectBrowserAccessibilityManager(
      ShouldBeBrowserAccessibilityManager(AccessibilityModeComplete));
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AccessibilityModeTreeOnly) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));
  ASSERT_EQ(CorrectedAccessibility(AccessibilityModeOff),
            web_contents()->GetAccessibilityMode());

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(AccessibilityModeTreeOnly);
  EXPECT_EQ(CorrectedAccessibility(AccessibilityModeTreeOnly),
            web_contents()->GetAccessibilityMode());
  waiter.WaitForNotification();
  // No BrowserAccessibilityManager expected for AccessibilityModeTreeOnly
  ExpectBrowserAccessibilityManager(
      ShouldBeBrowserAccessibilityManager(AccessibilityModeTreeOnly));
}

IN_PROC_BROWSER_TEST_F(AccessibilityModeTest, AddingModes) {
  NavigateToURL(shell(), GURL(kMinimalPageDataURL));

  AccessibilityNotificationWaiter waiter(shell()->web_contents());
  web_contents()->AddAccessibilityMode(AccessibilityModeTreeOnly);
  EXPECT_EQ(CorrectedAccessibility(AccessibilityModeTreeOnly),
            web_contents()->GetAccessibilityMode());
  waiter.WaitForNotification();
  ExpectBrowserAccessibilityManager(ShouldBeBrowserAccessibilityManager(
                                        AccessibilityModeTreeOnly),
                                    "Should be no BrowserAccessibilityManager "
                                    "for AccessibilityModeTreeOnly");

  AccessibilityNotificationWaiter waiter2(shell()->web_contents());
  web_contents()->AddAccessibilityMode(AccessibilityModeComplete);
  EXPECT_EQ(AccessibilityModeComplete, web_contents()->GetAccessibilityMode());
  waiter2.WaitForNotification();
  ExpectBrowserAccessibilityManager(ShouldBeBrowserAccessibilityManager(
                                          AccessibilityModeComplete),
                                    "Should be a BrowserAccessibilityManager "
                                    "for AccessibilityModeComplete");
}

}  // namespace content
