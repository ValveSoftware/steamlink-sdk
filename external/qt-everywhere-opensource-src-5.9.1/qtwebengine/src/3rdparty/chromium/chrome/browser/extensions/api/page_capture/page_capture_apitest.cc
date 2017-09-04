// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"

using extensions::PageCaptureSaveAsMHTMLFunction;

class ExtensionPageCaptureApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
  }
};

class PageCaptureSaveAsMHTMLDelegate
    : public PageCaptureSaveAsMHTMLFunction::TestDelegate {
 public:
  PageCaptureSaveAsMHTMLDelegate() {
    PageCaptureSaveAsMHTMLFunction::SetTestDelegate(this);
  }

  virtual ~PageCaptureSaveAsMHTMLDelegate() {
    PageCaptureSaveAsMHTMLFunction::SetTestDelegate(NULL);
  }

  void OnTemporaryFileCreated(const base::FilePath& temp_file) override {
    temp_file_ = temp_file;
  }

  base::FilePath temp_file_;
};

IN_PROC_BROWSER_TEST_F(ExtensionPageCaptureApiTest, SaveAsMHTML) {
  host_resolver()->AddRule("www.a.com", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunExtensionTest("page_capture")) << message_;
  ASSERT_FALSE(delegate.temp_file_.empty());
  // Flush the message loops to make sure the delete happens.
  content::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  ASSERT_FALSE(base::PathExists(delegate.temp_file_));
}
