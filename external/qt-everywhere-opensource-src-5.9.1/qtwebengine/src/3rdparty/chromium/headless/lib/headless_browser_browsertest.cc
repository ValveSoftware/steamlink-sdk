// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/test_protocol_handler.h"
#include "headless/test/test_url_request_job.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_store.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

using testing::UnorderedElementsAre;

namespace headless {

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateAndDestroyBrowserContext) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));

  browser_context->Close();

  EXPECT_TRUE(browser()->GetAllBrowserContexts().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest,
                       CreateAndDoNotDestroyBrowserContext) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));

  // We check that HeadlessBrowser correctly handles non-closed BrowserContexts.
  // We can rely on Chromium DCHECKs to capture this.
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateAndDestroyWebContents) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents);

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));
  EXPECT_THAT(browser_context->GetAllWebContents(),
              UnorderedElementsAre(web_contents));

  // TODO(skyostil): Verify viewport dimensions once we can.

  web_contents->Close();

  EXPECT_TRUE(browser_context->GetAllWebContents().empty());

  browser_context->Close();

  EXPECT_TRUE(browser()->GetAllBrowserContexts().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest,
                       WebContentsAreDestroyedWithContext) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents);

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));
  EXPECT_THAT(browser_context->GetAllWebContents(),
              UnorderedElementsAre(web_contents));

  browser_context->Close();

  EXPECT_TRUE(browser()->GetAllBrowserContexts().empty());

  // If WebContents are not destroyed, Chromium DCHECKs will capture this.
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateAndDoNotDestroyWebContents) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents);

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));
  EXPECT_THAT(browser_context->GetAllWebContents(),
              UnorderedElementsAre(web_contents));

  // If WebContents are not destroyed, Chromium DCHECKs will capture this.
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, DestroyAndCreateTwoWebContents) {
  HeadlessBrowserContext* browser_context1 =
      browser()->CreateBrowserContextBuilder().Build();
  EXPECT_TRUE(browser_context1);
  HeadlessWebContents* web_contents1 =
      browser_context1->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents1);

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context1));
  EXPECT_THAT(browser_context1->GetAllWebContents(),
              UnorderedElementsAre(web_contents1));

  HeadlessBrowserContext* browser_context2 =
      browser()->CreateBrowserContextBuilder().Build();
  EXPECT_TRUE(browser_context2);
  HeadlessWebContents* web_contents2 =
      browser_context2->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents2);

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context1, browser_context2));
  EXPECT_THAT(browser_context1->GetAllWebContents(),
              UnorderedElementsAre(web_contents1));
  EXPECT_THAT(browser_context2->GetAllWebContents(),
              UnorderedElementsAre(web_contents2));

  browser_context1->Close();

  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context2));
  EXPECT_THAT(browser_context2->GetAllWebContents(),
              UnorderedElementsAre(web_contents2));

  browser_context2->Close();

  EXPECT_TRUE(browser()->GetAllBrowserContexts().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateWithBadURL) {
  GURL bad_url("not_valid");

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(bad_url)
          .Build();

  EXPECT_FALSE(web_contents);
  EXPECT_TRUE(browser_context->GetAllWebContents().empty());
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
  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetProxyServer(proxy_server()->host_port_pair())
          .Build();

  // Load a page which doesn't actually exist, but for which the our proxy
  // returns valid content anyway.
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));
  EXPECT_THAT(browser()->GetAllBrowserContexts(),
              UnorderedElementsAre(browser_context));
  EXPECT_THAT(browser_context->GetAllWebContents(),
              UnorderedElementsAre(web_contents));
  web_contents->Close();
  EXPECT_TRUE(browser_context->GetAllWebContents().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, SetHostResolverRules) {
  EXPECT_TRUE(embedded_test_server()->Start());

  std::string host_resolver_rules =
      base::StringPrintf("MAP not-an-actual-domain.tld 127.0.0.1:%d",
                         embedded_test_server()->host_port_pair().port());

  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetHostResolverRules(host_resolver_rules)
          .Build();

  // Load a page which doesn't actually exist, but which is turned into a valid
  // address by our host resolver rules.
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(web_contents);

  EXPECT_TRUE(WaitForLoad(web_contents));
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, HttpProtocolHandler) {
  const std::string kResponseBody = "<p>HTTP response body</p>";
  ProtocolHandlerMap protocol_handlers;
  protocol_handlers[url::kHttpScheme] =
      base::MakeUnique<TestProtocolHandler>(kResponseBody);

  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetProtocolHandlers(std::move(protocol_handlers))
          .Build();

  // Load a page which doesn't actually exist, but which is fetched by our
  // custom protocol handler.
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(web_contents);
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
      base::MakeUnique<TestProtocolHandler>(kResponseBody);

  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetProtocolHandlers(std::move(protocol_handlers))
          .Build();

  // Load a page which doesn't actually exist, but which is fetched by our
  // custom protocol handler.
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("https://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(web_contents);
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::string inner_html;
  EXPECT_TRUE(EvaluateScript(web_contents, "document.body.innerHTML")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsString(&inner_html));
  EXPECT_EQ(kResponseBody, inner_html);
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, WebGLSupported) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();

  bool webgl_supported;
  EXPECT_TRUE(
      EvaluateScript(web_contents,
                     "(document.createElement('canvas').getContext('webgl')"
                     "    instanceof WebGLRenderingContext)")
          ->GetResult()
          ->GetValue()
          ->GetAsBoolean(&webgl_supported));
  EXPECT_TRUE(webgl_supported);
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, DefaultSizes) {
  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().Build();

  HeadlessBrowser::Options::Builder builder;
  const HeadlessBrowser::Options kDefaultOptions = builder.Build();

  int screen_width;
  int screen_height;
  int window_width;
  int window_height;

  EXPECT_TRUE(EvaluateScript(web_contents, "screen.width")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsInteger(&screen_width));
  EXPECT_TRUE(EvaluateScript(web_contents, "screen.height")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsInteger(&screen_height));
  EXPECT_TRUE(EvaluateScript(web_contents, "window.innerWidth")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsInteger(&window_width));
  EXPECT_TRUE(EvaluateScript(web_contents, "window.innerHeight")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsInteger(&window_height));

  EXPECT_EQ(kDefaultOptions.window_size.width(), screen_width);
  EXPECT_EQ(kDefaultOptions.window_size.height(), screen_height);
  EXPECT_EQ(kDefaultOptions.window_size.width(), window_width);
  EXPECT_EQ(kDefaultOptions.window_size.height(), window_height);
}

namespace {

// True if the request method is "safe" (per section 4.2.1 of RFC 7231).
bool IsMethodSafe(const std::string& method) {
  return method == "GET" || method == "HEAD" || method == "OPTIONS" ||
         method == "TRACE";
}

class ProtocolHandlerWithCookies
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ProtocolHandlerWithCookies(net::CookieList* sent_cookies);
  ~ProtocolHandlerWithCookies() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

 private:
  net::CookieList* sent_cookies_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerWithCookies);
};

class URLRequestJobWithCookies : public TestURLRequestJob {
 public:
  URLRequestJobWithCookies(net::URLRequest* request,
                           net::NetworkDelegate* network_delegate,
                           net::CookieList* sent_cookies);
  ~URLRequestJobWithCookies() override {}

  // net::URLRequestJob implementation:
  void Start() override;

 private:
  void SaveCookiesAndStart(const net::CookieList& cookie_list);

  net::CookieList* sent_cookies_;  // Not owned.
  base::WeakPtrFactory<URLRequestJobWithCookies> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(URLRequestJobWithCookies);
};

ProtocolHandlerWithCookies::ProtocolHandlerWithCookies(
    net::CookieList* sent_cookies)
    : sent_cookies_(sent_cookies) {}

net::URLRequestJob* ProtocolHandlerWithCookies::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return new URLRequestJobWithCookies(request, network_delegate, sent_cookies_);
}

URLRequestJobWithCookies::URLRequestJobWithCookies(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    net::CookieList* sent_cookies)
    // Return an empty response for every request.
    : TestURLRequestJob(request, network_delegate, ""),
      sent_cookies_(sent_cookies),
      weak_factory_(this) {}

void URLRequestJobWithCookies::Start() {
  net::CookieStore* cookie_store = request_->context()->cookie_store();
  net::CookieOptions options;
  options.set_include_httponly();

  // See net::URLRequestHttpJob::AddCookieHeaderAndStart().
  url::Origin requested_origin(request_->url());
  url::Origin site_for_cookies(request_->first_party_for_cookies());

  if (net::registry_controlled_domains::SameDomainOrHost(
          requested_origin, site_for_cookies,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    if (net::registry_controlled_domains::SameDomainOrHost(
            requested_origin, request_->initiator(),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      options.set_same_site_cookie_mode(
          net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
    } else if (IsMethodSafe(request_->method())) {
      options.set_same_site_cookie_mode(
          net::CookieOptions::SameSiteCookieMode::INCLUDE_LAX);
    }
  }
  cookie_store->GetCookieListWithOptionsAsync(
      request_->url(), options,
      base::Bind(&URLRequestJobWithCookies::SaveCookiesAndStart,
                 weak_factory_.GetWeakPtr()));
}

void URLRequestJobWithCookies::SaveCookiesAndStart(
    const net::CookieList& cookie_list) {
  *sent_cookies_ = cookie_list;
  NotifyHeadersComplete();
}

}  // namespace

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, ReadCookiesInProtocolHandler) {
  net::CookieList sent_cookies;
  ProtocolHandlerMap protocol_handlers;
  protocol_handlers[url::kHttpsScheme] =
      base::MakeUnique<ProtocolHandlerWithCookies>(&sent_cookies);

  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetProtocolHandlers(std::move(protocol_handlers))
          .Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("https://example.com/cookie.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  // The first load has no cookies.
  EXPECT_EQ(0u, sent_cookies.size());

  // Set a cookie and reload the page.
  EXPECT_FALSE(EvaluateScript(
                   web_contents,
                   "document.cookie = 'shape=oblong', window.location.reload()")
                   ->HasExceptionDetails());
  EXPECT_TRUE(WaitForLoad(web_contents));

  // We should have sent the cookie this time.
  EXPECT_EQ(1u, sent_cookies.size());
  EXPECT_EQ("shape", sent_cookies[0].Name());
  EXPECT_EQ("oblong", sent_cookies[0].Value());
}

namespace {

class CookieSetter {
 public:
  CookieSetter(HeadlessBrowserTest* browser_test,
               HeadlessWebContents* web_contents,
               std::unique_ptr<network::SetCookieParams> set_cookie_params)
      : browser_test_(browser_test),
        web_contents_(web_contents),
        devtools_client_(HeadlessDevToolsClient::Create()) {
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    devtools_client_->GetNetwork()->GetExperimental()->SetCookie(
        std::move(set_cookie_params),
        base::Bind(&CookieSetter::OnSetCookieResult, base::Unretained(this)));
  }

  ~CookieSetter() {
    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  }

  void OnSetCookieResult(std::unique_ptr<network::SetCookieResult> result) {
    result_ = std::move(result);
    browser_test_->FinishAsynchronousTest();
  }

  std::unique_ptr<network::SetCookieResult> TakeResult() {
    return std::move(result_);
  }

 private:
  HeadlessBrowserTest* browser_test_;  // Not owned.
  HeadlessWebContents* web_contents_;  // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;

  std::unique_ptr<network::SetCookieResult> result_;

  DISALLOW_COPY_AND_ASSIGN(CookieSetter);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, SetCookiesWithDevTools) {
  net::CookieList sent_cookies;
  ProtocolHandlerMap protocol_handlers;
  protocol_handlers[url::kHttpsScheme] =
      base::WrapUnique(new ProtocolHandlerWithCookies(&sent_cookies));

  HeadlessBrowserContext* browser_context =
      browser()
          ->CreateBrowserContextBuilder()
          .SetProtocolHandlers(std::move(protocol_handlers))
          .Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(GURL("https://example.com/cookie.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  // The first load has no cookies.
  EXPECT_EQ(0u, sent_cookies.size());

  // Set some cookies.
  {
    std::unique_ptr<network::SetCookieParams> set_cookie_params =
        network::SetCookieParams::Builder()
            .SetUrl("https://example.com")
            .SetName("shape")
            .SetValue("oblong")
            .Build();
    CookieSetter cookie_setter(this, web_contents,
                               std::move(set_cookie_params));
    RunAsynchronousTest();
    std::unique_ptr<network::SetCookieResult> result =
        cookie_setter.TakeResult();
    EXPECT_TRUE(result->GetSuccess());
  }
  {
    // Try setting all the fields so we notice if the protocol for any of them
    // changes.
    std::unique_ptr<network::SetCookieParams> set_cookie_params =
        network::SetCookieParams::Builder()
            .SetUrl("https://other.com")
            .SetName("shape")
            .SetValue("trapezoid")
            .SetDomain("other.com")
            .SetPath("")
            .SetSecure(true)
            .SetHttpOnly(true)
            .SetSameSite(network::CookieSameSite::STRICT)
            .SetExpirationDate(0)
            .Build();
    CookieSetter cookie_setter(this, web_contents,
                               std::move(set_cookie_params));
    RunAsynchronousTest();
    std::unique_ptr<network::SetCookieResult> result =
        cookie_setter.TakeResult();
    EXPECT_TRUE(result->GetSuccess());
  }

  // Reload the page.
  EXPECT_FALSE(EvaluateScript(web_contents, "window.location.reload();")
                   ->HasExceptionDetails());
  EXPECT_TRUE(WaitForLoad(web_contents));

  // We should have sent the matching cookies this time.
  EXPECT_EQ(1u, sent_cookies.size());
  EXPECT_EQ("shape", sent_cookies[0].Name());
  EXPECT_EQ("oblong", sent_cookies[0].Value());
}

// TODO(skyostil): This test currently relies on being able to run a shell
// script.
#if defined(OS_POSIX)
#define MAYBE_RendererCommandPrefixTest RendererCommandPrefixTest
#else
#define MAYBE_RendererCommandPrefixTest DISABLED_RendererCommandPrefixTest
#endif  // defined(OS_POSIX)
IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, MAYBE_RendererCommandPrefixTest) {
  base::ThreadRestrictions::SetIOAllowed(true);
  base::FilePath launcher_stamp;
  base::CreateTemporaryFile(&launcher_stamp);

  base::FilePath launcher_script;
  FILE* launcher_file = base::CreateAndOpenTemporaryFile(&launcher_script);
  fprintf(launcher_file, "#!/bin/sh\n");
  fprintf(launcher_file, "echo $@ > %s\n", launcher_stamp.value().c_str());
  fprintf(launcher_file, "exec $@\n");
  fclose(launcher_file);
  base::SetPosixFilePermissions(launcher_script,
                                base::FILE_PERMISSION_READ_BY_USER |
                                    base::FILE_PERMISSION_EXECUTE_BY_USER);

  base::CommandLine::ForCurrentProcess()->AppendSwitch("--no-sandbox");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "--renderer-cmd-prefix", launcher_script.value());

  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder()
          .SetInitialURL(embedded_test_server()->GetURL("/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  // Make sure the launcher was invoked when starting the renderer.
  std::string stamp;
  EXPECT_TRUE(base::ReadFileToString(launcher_stamp, &stamp));
  EXPECT_GE(stamp.find("--type=renderer"), 0u);

  base::DeleteFile(launcher_script, false);
  base::DeleteFile(launcher_stamp, false);
}

}  // namespace headless
