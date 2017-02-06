// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test.h"
#include "headless/public/domains/page.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/test_protocol_handler.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateAndDestroyWebContents) {
  HeadlessWebContents* web_contents =
      browser()->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents);

  EXPECT_EQ(static_cast<size_t>(1), browser()->GetAllWebContents().size());
  EXPECT_EQ(web_contents, browser()->GetAllWebContents()[0]);
  // TODO(skyostil): Verify viewport dimensions once we can.
  web_contents->Close();

  EXPECT_TRUE(browser()->GetAllWebContents().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateWithBadURL) {
  GURL bad_url("not_valid");
  HeadlessWebContents* web_contents =
      browser()->CreateWebContentsBuilder().SetInitialURL(bad_url).Build();
  EXPECT_FALSE(web_contents);
  EXPECT_TRUE(browser()->GetAllWebContents().empty());
}

class HeadlessBrowserTestWithProxy : public HeadlessBrowserTest {
 public:
  HeadlessBrowserTestWithProxy()
      : proxy_server_(net::SpawnedTestServer::TYPE_HTTP,
                      net::SpawnedTestServer::kLocalhost,
                      base::FilePath(FILE_PATH_LITERAL("headless/test/data"))) {
  }

  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    HeadlessBrowserTest::SetUp();
  }

  void TearDown() override {
    proxy_server_.Stop();
    HeadlessBrowserTest::TearDown();
  }

  net::SpawnedTestServer* proxy_server() { return &proxy_server_; }

 private:
  net::SpawnedTestServer proxy_server_;
};

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTestWithProxy, SetProxyServer) {
  HeadlessBrowser::Options::Builder builder;
  builder.SetProxyServer(proxy_server()->host_port_pair());
  SetBrowserOptions(builder.Build());

  // Load a page which doesn't actually exist, but for which the our proxy
  // returns valid content anyway.
  //
  // TODO(altimin): Currently this construction does not serve hello.html
  // from headless/test/data as expected. We should fix this.
  HeadlessWebContents* web_contents =
      browser()
          ->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));
  EXPECT_EQ(static_cast<size_t>(1), browser()->GetAllWebContents().size());
  EXPECT_EQ(web_contents, browser()->GetAllWebContents()[0]);
  web_contents->Close();
  EXPECT_TRUE(browser()->GetAllWebContents().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, SetHostResolverRules) {
  EXPECT_TRUE(embedded_test_server()->Start());
  HeadlessBrowser::Options::Builder builder;
  builder.SetHostResolverRules(
      base::StringPrintf("MAP not-an-actual-domain.tld 127.0.0.1:%d",
                         embedded_test_server()->host_port_pair().port()));
  SetBrowserOptions(builder.Build());

  // Load a page which doesn't actually exist, but which is turned into a valid
  // address by our host resolver rules.
  HeadlessWebContents* web_contents =
      browser()
          ->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, HttpProtocolHandler) {
  const std::string kResponseBody = "<p>HTTP response body</p>";
  ProtocolHandlerMap protocol_handlers;
  protocol_handlers[url::kHttpScheme] =
      base::WrapUnique(new TestProtocolHandler(kResponseBody));

  HeadlessBrowser::Options::Builder builder;
  builder.SetProtocolHandlers(std::move(protocol_handlers));
  SetBrowserOptions(builder.Build());

  // Load a page which doesn't actually exist, but which is fetched by our
  // custom protocol handler.
  HeadlessWebContents* web_contents =
      browser()
          ->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::string inner_html;
  EXPECT_TRUE(EvaluateScript(web_contents, "document.body.innerHTML")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsString(&inner_html));
  EXPECT_EQ(kResponseBody, inner_html);
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, HttpsProtocolHandler) {
  const std::string kResponseBody = "<p>HTTPS response body</p>";
  ProtocolHandlerMap protocol_handlers;
  protocol_handlers[url::kHttpsScheme] =
      base::WrapUnique(new TestProtocolHandler(kResponseBody));

  HeadlessBrowser::Options::Builder builder;
  builder.SetProtocolHandlers(std::move(protocol_handlers));
  SetBrowserOptions(builder.Build());

  // Load a page which doesn't actually exist, but which is fetched by our
  // custom protocol handler.
  HeadlessWebContents* web_contents =
      browser()
          ->CreateWebContentsBuilder()
          .SetInitialURL(GURL("https://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::string inner_html;
  EXPECT_TRUE(EvaluateScript(web_contents, "document.body.innerHTML")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsString(&inner_html));
  EXPECT_EQ(kResponseBody, inner_html);
}

}  // namespace headless
