// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MHTMLGenerationTest : public ContentBrowserTest {
 public:
  MHTMLGenerationTest() : mhtml_generated_(false), file_size_(0) {}

  void MHTMLGenerated(int64 size) {
    mhtml_generated_ = true;
    file_size_ = size;
    base::MessageLoopForUI::current()->Quit();
  }

 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ContentBrowserTest::SetUp();
  }

  bool mhtml_generated() const { return mhtml_generated_; }
  int64 file_size() const { return file_size_; }

  base::ScopedTempDir temp_dir_;

 private:
  bool mhtml_generated_;
  int64 file_size_;
};

// Tests that generating a MHTML does create contents.
// Note that the actual content of the file is not tested, the purpose of this
// test is to ensure we were successfull in creating the MHTML data from the
// renderer.
IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTML) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  base::FilePath path(temp_dir_.path());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  NavigateToURL(shell(), embedded_test_server()->GetURL("/simple_page.html"));

  shell()->web_contents()->GenerateMHTML(
      path, base::Bind(&MHTMLGenerationTest::MHTMLGenerated, this));

  // Block until the MHTML is generated.
  RunMessageLoop();

  EXPECT_TRUE(mhtml_generated());
  EXPECT_GT(file_size(), 0);

  // Make sure the actual generated file has some contents.
  int64 file_size;
  ASSERT_TRUE(base::GetFileSize(path, &file_size));
  EXPECT_GT(file_size, 100);
}

}  // namespace content
