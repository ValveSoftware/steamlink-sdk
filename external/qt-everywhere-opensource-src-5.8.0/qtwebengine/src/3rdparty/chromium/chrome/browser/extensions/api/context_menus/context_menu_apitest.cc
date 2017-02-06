// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

// Times out on win syzyasan, http://crbug.com/166026
#if defined(SYZYASAN)
#define MAYBE_ContextMenus DISABLED_ContextMenus
#else
#define MAYBE_ContextMenus ContextMenus
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_ContextMenus) {
  ASSERT_TRUE(RunExtensionTest("context_menus/basics")) << message_;
  ASSERT_TRUE(RunExtensionTest("context_menus/no_perms")) << message_;
  ASSERT_TRUE(RunExtensionTest("context_menus/item_ids")) << message_;
  ASSERT_TRUE(RunExtensionTest("context_menus/event_page")) << message_;
}

// crbug.com/51436 -- creating context menus from multiple script contexts
// should work.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ContextMenusFromMultipleContexts) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(RunExtensionTest("context_menus/add_from_multiple_contexts"))
      << message_;
  const extensions::Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension) << message_;

  {
    // Tell the extension to update the page action state.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        extension->GetResourceURL("popup.html"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  {
    // Tell the extension to update the page action state again.
    ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(),
        extension->GetResourceURL("popup2.html"));
    ASSERT_TRUE(catcher.GetNextResult());
  }
}

}  // namespace extensions
