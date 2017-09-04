// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "content/browser/download/save_package.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

const char kTestFile[] = "/simple_page.html";

class SavePackageBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(save_dir_.CreateUniqueTempDir());
    ContentBrowserTest::SetUp();
  }

  // Returns full paths of destination file and directory.
  void GetDestinationPaths(const std::string& prefix,
                           base::FilePath* full_file_name,
                           base::FilePath* dir) {
    *full_file_name = save_dir_.GetPath().AppendASCII(prefix + ".htm");
    *dir = save_dir_.GetPath().AppendASCII(prefix + "_files");
  }

  // Temporary directory we will save pages to.
  base::ScopedTempDir save_dir_;
};

// Create a SavePackage and delete it without calling Init.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ImplicitCancel) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kTestFile);
  NavigateToURL(shell(), url);
  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(new SavePackage(
      shell()->web_contents(), SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name,
      dir));
}

// Create a SavePackage, call Cancel, then delete it.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ExplicitCancel) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kTestFile);
  NavigateToURL(shell(), url);
  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(new SavePackage(
      shell()->web_contents(), SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name,
      dir));
  save_package->Cancel(true);
}

}  // namespace content
