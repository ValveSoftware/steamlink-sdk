// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

namespace content {

// These tests simulate exploited renderer processes, which can fetch arbitrary
// resources from other websites, not constrained by the Same Origin Policy.  We
// are trying to verify that the renderer cannot fetch any cross-site document
// responses even when the Same Origin Policy is turned off inside the renderer.
class SiteIsolationPolicyBrowserTest : public ContentBrowserTest {
 public:
  SiteIsolationPolicyBrowserTest() {}
  virtual ~SiteIsolationPolicyBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ASSERT_TRUE(test_server()->Start());
    net::SpawnedTestServer https_server(
        net::SpawnedTestServer::TYPE_HTTPS,
        net::SpawnedTestServer::kLocalhost,
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    ASSERT_TRUE(https_server.Start());

    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames in URLs, which we can use to
    // create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        switches::kHostResolverRules,
        "MAP * " + test_server()->host_port_pair().ToString() +
            ",EXCLUDE localhost");

    // Since we assume exploited renderer process, it can bypass the same origin
    // policy at will. Simulate that by passing the disable-web-security flag.
    command_line->AppendSwitch(switches::kDisableWebSecurity);

    // We assume that we're using our cross-site document blocking logic which
    // is turned on even when the Same Origin Policy is turned off.
    command_line->AppendSwitch(switches::kBlockCrossSiteDocuments);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPolicyBrowserTest);
};

// TODO(dsjang): we cannot run these tests on Android since SetUpCommandLine()
// is executed before the I/O thread is created on Android. After this bug
// (crbug.com/278425) is resolved, we can enable this test case on Android.
#if defined(OS_ANDROID)
#define MAYBE_CrossSiteDocumentBlockingForMimeType \
  DISABLED_CrossSiteDocumentBlockingForMimeType
#else
#define MAYBE_CrossSiteDocumentBlockingForMimeType \
  CrossSiteDocumentBlockingForMimeType
#endif

IN_PROC_BROWSER_TEST_F(SiteIsolationPolicyBrowserTest,
                       MAYBE_CrossSiteDocumentBlockingForMimeType) {
  // Load a page that issues illegal cross-site document requests to bar.com.
  // The page uses XHR to request HTML/XML/JSON documents from bar.com, and
  // inspects if any of them were successfully received. The XHR requests will
  // get a one character string ' ' for a blocked response. This test is only
  // possible since we run the browser without the same origin policy.
  GURL foo("http://foo.com/files/cross_site_document_request.html");

  content::DOMMessageQueue msg_queue;

  NavigateToURL(shell(), foo);

  std::string status;
  // The page will return 1 from the DOMAutomationController if it succeeds,
  // otherwise it will return 0.
  std::string expected_status("1");
  EXPECT_TRUE(msg_queue.WaitForMessage(&status));
  EXPECT_STREQ(status.c_str(), expected_status.c_str());
}

// TODO(dsjang): we cannot run these tests on Android since SetUpCommandLine()
// is executed before the I/O thread is created on Android. After this bug
// (crbug.com/278425) is resolved, we can enable this test case on Android.
#if defined(OS_ANDROID)
#define MAYBE_CrossSiteDocumentBlockingForDifferentTargets \
  DISABLED_CrossSiteDocumentBlockingForDifferentTargets
#else
#define MAYBE_CrossSiteDocumentBlockingForDifferentTargets \
  CrossSiteDocumentBlockingForDifferentTargets
#endif

IN_PROC_BROWSER_TEST_F(SiteIsolationPolicyBrowserTest,
                       MAYBE_CrossSiteDocumentBlockingForDifferentTargets) {
  // This webpage loads a cross-site HTML page in different targets such as
  // <img>,<link>,<embed>, etc. Since the requested document is blocked, and one
  // character string (' ') is returned instead, this tests that the renderer
  // does not crash even when it receives a response body which is " ", whose
  // length is different from what's described in "content-length" for such
  // different targets.
  GURL foo("http://foo.com/files/cross_site_document_request_target.html");
  NavigateToURL(shell(), foo);
}

}
