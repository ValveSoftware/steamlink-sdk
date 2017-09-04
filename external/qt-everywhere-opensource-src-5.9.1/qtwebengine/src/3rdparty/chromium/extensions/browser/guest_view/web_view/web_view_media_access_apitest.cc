// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/guest_view/web_view/web_view_apitest.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

// This class intercepts media access request from the embedder. The request
// should be triggered only if the embedder API (from tests) allows the request
// in Javascript.
// We do not issue the actual media request; the fact that the request reached
// embedder's WebContents is good enough for our tests. This is also to make
// the test run successfully on trybots.
class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() : requested_(false), checked_(false) {}
  ~MockWebContentsDelegate() override {}

  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override {
    requested_ = true;
    if (request_message_loop_runner_.get())
      request_message_loop_runner_->Quit();
  }

  bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override {
    checked_ = true;
    if (check_message_loop_runner_.get())
      check_message_loop_runner_->Quit();
    return true;
  }

  void WaitForRequestMediaPermission() {
    if (requested_)
      return;
    request_message_loop_runner_ = new content::MessageLoopRunner;
    request_message_loop_runner_->Run();
  }

  void WaitForCheckMediaPermission() {
    if (checked_)
      return;
    check_message_loop_runner_ = new content::MessageLoopRunner;
    check_message_loop_runner_->Run();
  }

 private:
  bool requested_;
  bool checked_;
  scoped_refptr<content::MessageLoopRunner> request_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> check_message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockWebContentsDelegate);
};

}  // namespace

namespace extensions {

class WebViewMediaAccessAPITest : public WebViewAPITest {
 protected:
  WebViewMediaAccessAPITest() {}

  // Runs media_access tests.
  void RunTest(const std::string& test_name) {
    ExtensionTestMessageListener test_run_listener("TEST_PASSED", false);
    test_run_listener.set_failure_message("TEST_FAILED");
    EXPECT_TRUE(content::ExecuteScript(
        embedder_web_contents_,
        base::StringPrintf("runTest('%s');", test_name.c_str())));
    ASSERT_TRUE(test_run_listener.WaitUntilSatisfied());
  }

  // content::BrowserTestBase implementation
  void SetUpOnMainThread() override {
    WebViewAPITest::SetUpOnMainThread();
    StartTestServer();
  }

  void TearDownOnMainThread() override {
    WebViewAPITest::TearDownOnMainThread();
    StopTestServer();
  }
};

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllow) {
  LaunchApp("web_view/media_access/allow");
  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllow");

  mock->WaitForRequestMediaPermission();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllowAndThenDeny) {
  LaunchApp("web_view/media_access/allow");
  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllowAndThenDeny");

  mock->WaitForRequestMediaPermission();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllowAsync) {
  LaunchApp("web_view/media_access/allow");
  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllowAsync");

  mock->WaitForRequestMediaPermission();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestAllowTwice) {
  LaunchApp("web_view/media_access/allow");
  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testAllowTwice");

  mock->WaitForRequestMediaPermission();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestCheck) {
  LaunchApp("web_view/media_access/check");
  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents_->SetDelegate(mock.get());

  RunTest("testCheck");

  mock->WaitForCheckMediaPermission();
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestDeny) {
  LaunchApp("web_view/media_access/deny");
  RunTest("testDeny");
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestDenyThenAllowThrows) {
  LaunchApp("web_view/media_access/deny");
  RunTest("testDenyThenAllowThrows");
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestDenyWithPreventDefault) {
  LaunchApp("web_view/media_access/deny");
  RunTest("testDenyWithPreventDefault");
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest, TestNoListenersImplyDeny) {
  LaunchApp("web_view/media_access/deny");
  RunTest("testNoListenersImplyDeny");
}

IN_PROC_BROWSER_TEST_F(WebViewMediaAccessAPITest,
                       TestNoPreventDefaultImpliesDeny) {
  LaunchApp("web_view/media_access/deny");
  RunTest("testNoPreventDefaultImpliesDeny");
}

}  // namespace extensions
