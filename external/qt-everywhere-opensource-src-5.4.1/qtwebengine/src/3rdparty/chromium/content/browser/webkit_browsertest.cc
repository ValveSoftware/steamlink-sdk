// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/net/url_request_abort_on_end_job.h"

namespace content {

typedef ContentBrowserTest WebKitBrowserTest;

const char kAsyncScriptThatAbortsOnEndPage[] =
    "files/webkit/async_script_abort_on_end.html";

// This is a browser test because it is hard to reproduce reliably in a
// layout test without races. http://crbug.com/75604 deals with a request
// for an async script which gets data in the response and immediately
// after aborts. This test creates that condition, and it is passed
// if chrome does not crash.

IN_PROC_BROWSER_TEST_F(WebKitBrowserTest, AbortOnEnd) {
  ASSERT_TRUE(test_server()->Start());
  URLRequestAbortOnEndJob::AddUrlHandler();
  GURL url = test_server()->GetURL(kAsyncScriptThatAbortsOnEndPage);

  NavigateToURL(shell(), url);

  // If you are seeing this test fail, please strongly investigate the
  // possibility that http://crbug.com/75604 and
  // https://bugs.webkit.org/show_bug.cgi?id=71122 have reverted before
  // marking this as flakey.
  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

// This is a browser test because the DumpRenderTree framework holds
// onto a Document* reference that blocks this reproduction from
// destroying the Document, so it is not a use after free unless
// you don't have DumpRenderTree loaded.

// TODO(gavinp): remove this browser_test if we can get good LayoutTest
// coverage of the same issue.
const char kXsltBadImportPage[] =
    "files/webkit/xslt-bad-import.html";
IN_PROC_BROWSER_TEST_F(WebKitBrowserTest, XsltBadImport) {
  ASSERT_TRUE(test_server()->Start());
  URLRequestAbortOnEndJob::AddUrlHandler();
  GURL url = test_server()->GetURL(kXsltBadImportPage);

  NavigateToURL(shell(), url);

  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

// This is a browser test because DumpRenderTree has a PrerendererClient
// implementation, and the purpose of this test is to ensure that content_shell
// does not crash when prerender elements are encountered with no Prererering
// implementation supplied to WebKit.

// TODO(gavinp,jochen): This browser_test depends on there not being a
// prerendering client and prerendering platform provided by the test_shell.
// But both will exist when we use content_shell to run layout tests. We must
// then add a mechanism to start content_shell without these, or else this
// test is not very interesting.
const char kPrerenderNoCrashPage[] =
    "files/prerender/prerender-no-crash.html";
IN_PROC_BROWSER_TEST_F(WebKitBrowserTest, PrerenderNoCrash) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL(kPrerenderNoCrashPage);

  NavigateToURL(shell(), url);

  EXPECT_FALSE(shell()->web_contents()->IsCrashed());
}

}  // namespace content
