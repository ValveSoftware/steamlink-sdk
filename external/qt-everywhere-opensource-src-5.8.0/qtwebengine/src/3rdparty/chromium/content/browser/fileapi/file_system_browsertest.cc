// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/test/thread_test_helper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "storage/browser/quota/quota_manager.h"

using storage::QuotaManager;

namespace content {

// This browser test is aimed towards exercising the FileAPI bindings and
// the actual implementation that lives in the browser side.
class FileSystemBrowserTest : public ContentBrowserTest {
 public:
  FileSystemBrowserTest() {}

  void SimpleTest(const GURL& test_url, bool incognito = false) {
    // The test page will perform tests on FileAPI, then navigate to either
    // a #pass or #fail ref.
    Shell* the_browser = incognito ? CreateOffTheRecordBrowser() : shell();

    VLOG(0) << "Navigating to URL and blocking.";
    NavigateToURLBlockUntilNavigationsComplete(the_browser, test_url, 2);
    VLOG(0) << "Navigation done.";
    std::string result =
        the_browser->web_contents()->GetLastCommittedURL().ref();
    if (result != "pass") {
      std::string js_result;
      ASSERT_TRUE(ExecuteScriptAndExtractString(
          the_browser, "window.domAutomationController.send(getLog())",
          &js_result));
      FAIL() << "Failed: " << js_result;
    }
  }
};

class FileSystemBrowserTestWithLowQuota : public FileSystemBrowserTest {
 public:
  void SetUpOnMainThread() override {
    const int kInitialQuotaKilobytes = 5000;
    const int kTemporaryStorageQuotaMaxSize =
        kInitialQuotaKilobytes * 1024 * QuotaManager::kPerHostTemporaryPortion;
    SetTempQuota(
        kTemporaryStorageQuotaMaxSize,
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext())->GetQuotaManager());
  }

  static void SetTempQuota(int64_t bytes, scoped_refptr<QuotaManager> qm) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&FileSystemBrowserTestWithLowQuota::SetTempQuota, bytes,
                     qm));
      return;
    }
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    qm->SetTemporaryGlobalOverrideQuota(bytes, storage::QuotaCallback());
    // Don't return until the quota has been set.
    scoped_refptr<base::ThreadTestHelper> helper(new base::ThreadTestHelper(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB).get()));
    ASSERT_TRUE(helper->Run());
  }
};

IN_PROC_BROWSER_TEST_F(FileSystemBrowserTest, RequestTest) {
  SimpleTest(GetTestUrl("fileapi", "request_test.html"));
}

IN_PROC_BROWSER_TEST_F(FileSystemBrowserTest, CreateTest) {
  SimpleTest(GetTestUrl("fileapi", "create_test.html"));
}

IN_PROC_BROWSER_TEST_F(FileSystemBrowserTestWithLowQuota, QuotaTest) {
  SimpleTest(GetTestUrl("fileapi", "quota_test.html"));
}

}  // namespace content
