// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/accessibility_tree_formatter.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class AndroidHitTestingBrowserTest : public ContentBrowserTest {
 public:
  AndroidHitTestingBrowserTest() {}
  virtual ~AndroidHitTestingBrowserTest() {}
};

IN_PROC_BROWSER_TEST_F(AndroidHitTestingBrowserTest,
                       HitTestOutsideDocumentBoundsReturnsRoot) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Load the page.
  AccessibilityNotificationWaiter waiter(
      shell(), AccessibilityModeComplete,
      ui::AX_EVENT_LOAD_COMPLETE);
  const char url_str[] =
      "data:text/html,"
      "<!doctype html>"
      "<html><head><title>Accessibility Test</title></head>"
      "<body>"
      "<a href='#'>"
      "This is some text in a link"
      "</a>"
      "</body></html>";
  GURL url(url_str);
  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  // Get the BrowserAccessibilityManager.
  RenderWidgetHostViewBase* host_view = static_cast<RenderWidgetHostViewBase*>(
      shell()->web_contents()->GetRenderWidgetHostView());
  BrowserAccessibilityManager* manager =
      host_view->GetBrowserAccessibilityManager();

  // Send a hit test request, and wait for the hover event in response.
  AccessibilityNotificationWaiter hover_waiter(
      shell(), AccessibilityModeComplete,
      ui::AX_EVENT_HOVER);
  manager->delegate()->AccessibilityHitTest(gfx::Point(-1, -1));
  hover_waiter.WaitForNotification();

  // Assert that the hover event was fired on the root of the tree.
  int hover_target_id = hover_waiter.event_target_id();
  BrowserAccessibility* hovered_node = manager->GetFromID(hover_target_id);
  ASSERT_TRUE(hovered_node != NULL);
  ASSERT_EQ(ui::AX_ROLE_ROOT_WEB_AREA, hovered_node->GetRole());
}

}  // namespace content
