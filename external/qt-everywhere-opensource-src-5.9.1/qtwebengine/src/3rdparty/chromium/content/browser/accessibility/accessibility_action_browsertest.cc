// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class AccessibilityActionBrowserTest : public ContentBrowserTest {
 public:
  AccessibilityActionBrowserTest() {}
  ~AccessibilityActionBrowserTest() override {}

 protected:
  BrowserAccessibility* FindNode(ui::AXRole role,
                                 const std::string& name) {
    BrowserAccessibility* root = GetManager()->GetRoot();
    CHECK(root);
    return FindNodeInSubtree(*root, role, name);
  }

  BrowserAccessibilityManager* GetManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

 private:
  BrowserAccessibility* FindNodeInSubtree(
      BrowserAccessibility& node,
      ui::AXRole role,
      const std::string& name) {
    if (node.GetRole() == role &&
        node.GetStringAttribute(ui::AX_ATTR_NAME) == name)
      return &node;
    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      BrowserAccessibility* result = FindNodeInSubtree(
          *node.PlatformGetChild(i), role, name);
      if (result)
        return result;
    }
    return nullptr;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest, FocusAction) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         AccessibilityModeComplete,
                                         ui::AX_EVENT_LOAD_COMPLETE);
  GURL url("data:text/html,"
           "<button>One</button>"
           "<button>Two</button>"
           "<button>Three</button>");
  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  BrowserAccessibility* target = FindNode(ui::AX_ROLE_BUTTON, "One");
  ASSERT_NE(nullptr, target);

  AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                          AccessibilityModeComplete,
                                          ui::AX_EVENT_FOCUS);
  GetManager()->SetFocus(*target);
  waiter2.WaitForNotification();

  BrowserAccessibility* focus = GetManager()->GetFocus();
  EXPECT_EQ(focus->GetId(), target->GetId());
}

IN_PROC_BROWSER_TEST_F(AccessibilityActionBrowserTest,
                       IncrementDecrementActions) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         AccessibilityModeComplete,
                                         ui::AX_EVENT_LOAD_COMPLETE);
  GURL url("data:text/html,"
           "<input type=range min=2 value=8 max=10 step=2>");
  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  BrowserAccessibility* target = FindNode(ui::AX_ROLE_SLIDER, "");
  ASSERT_NE(nullptr, target);
  EXPECT_EQ(8.0, target->GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE));

  // Increment, should result in value changing from 8 to 10.
  {
    AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                            AccessibilityModeComplete,
                                            ui::AX_EVENT_VALUE_CHANGED);
    GetManager()->Increment(*target);
    waiter2.WaitForNotification();
  }
  EXPECT_EQ(10.0, target->GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE));

  // Increment, should result in value staying the same (max).
  {
    AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                            AccessibilityModeComplete,
                                            ui::AX_EVENT_VALUE_CHANGED);
    GetManager()->Increment(*target);
    waiter2.WaitForNotification();
  }
  EXPECT_EQ(10.0, target->GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE));

  // Decrement, should result in value changing from 10 to 8.
  {
    AccessibilityNotificationWaiter waiter2(shell()->web_contents(),
                                            AccessibilityModeComplete,
                                            ui::AX_EVENT_VALUE_CHANGED);
    GetManager()->Decrement(*target);
    waiter2.WaitForNotification();
  }
  EXPECT_EQ(8.0, target->GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE));
}

}  // namespace content
