// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "content/browser/download/save_package.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

const char kTestFile[] = "files/simple_page.html";

class SavePackageBrowserTest : public ContentBrowserTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(save_dir_.CreateUniqueTempDir());
    ContentBrowserTest::SetUp();
  }

  // Returns full paths of destination file and directory.
  void GetDestinationPaths(const std::string& prefix,
                           base::FilePath* full_file_name,
                           base::FilePath* dir) {
    *full_file_name = save_dir_.path().AppendASCII(prefix + ".htm");
    *dir = save_dir_.path().AppendASCII(prefix + "_files");
  }

  // Temporary directory we will save pages to.
  base::ScopedTempDir save_dir_;
};

// Create a SavePackage and delete it without calling Init.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ImplicitCancel) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL(kTestFile);
  NavigateToURL(shell(), url);
  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(new SavePackage(
      shell()->web_contents(), SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name,
      dir));
  ASSERT_TRUE(test_server()->Stop());
}

// Create a SavePackage, call Cancel, then delete it.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ExplicitCancel) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL(kTestFile);
  NavigateToURL(shell(), url);
  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(new SavePackage(
      shell()->web_contents(), SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name,
      dir));
  save_package->Cancel(true);
  ASSERT_TRUE(test_server()->Stop());
}

}  // namespace content
