// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

class LinuxZygoteBrowserTest : public ContentBrowserTest {
 public:
  LinuxZygoteBrowserTest() {}
  ~LinuxZygoteBrowserTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LinuxZygoteBrowserTest);
};

// https://crbug.com/638303
IN_PROC_BROWSER_TEST_F(LinuxZygoteBrowserTest, GetLocalTimeHasTimeZone) {
  const char kTestCommand[] =
      "window.domAutomationController.send(new Date().toString());";

  NavigateToURL(shell(), GURL("data:text/html,start page"));
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(shell(), kTestCommand, &result));
  std::vector<std::string> parts = base::SplitString(
      result, "()", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(3U, parts.size());
  EXPECT_FALSE(parts[0].empty());
  EXPECT_FALSE(parts[1].empty());
  EXPECT_TRUE(parts[2].empty());
}

}  // namespace content
