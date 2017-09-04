// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/clear_site_data_throttle.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"
#include "url/url_constants.h"

using testing::_;

namespace content {

namespace {

class MockContentBrowserClient : public ContentBrowserClient {
 public:
  MOCK_METHOD6(ClearSiteData,
               void(content::BrowserContext* browser_context,
                    const url::Origin& origin,
                    bool remove_cookies,
                    bool remove_storage,
                    bool remove_cache,
                    const base::Closure& callback));
};

class TestContentBrowserClient : public MockContentBrowserClient {
 public:
  void ClearSiteData(content::BrowserContext* browser_context,
                     const url::Origin& origin,
                     bool remove_cookies,
                     bool remove_storage,
                     bool remove_cache,
                     const base::Closure& callback) override {
    // Record the method call and run the |callback|.
    MockContentBrowserClient::ClearSiteData(browser_context, origin,
                                            remove_cookies, remove_storage,
                                            remove_cache, callback);
    callback.Run();
  }
};

// Adds a key=value pair to the url's query.
void AddQuery(GURL* url, const std::string& key, const std::string& value) {
  *url = GURL(url->spec() + (url->has_query() ? "&" : "?") + key + "=" +
              net::EscapeQueryParamValue(value, false));
}

// A value of the Clear-Site-Data header that requests cookie deletion. Reused
// in tests that need a valid header but do not depend on its value.
static const char* kClearCookiesHeader = "{ \"types\": [ \"cookies\" ] }";

}  // namespace

class ClearSiteDataThrottleBrowserTest : public ContentBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);

    // We're redirecting all hosts to localhost even on HTTPS, so we'll get
    // certificate errors.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    SetBrowserClientForTesting(&test_client_);

    // Set up HTTP and HTTPS test servers that handle all hosts.
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&ClearSiteDataThrottleBrowserTest::HandleRequest,
                   base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    https_server_.reset(new net::EmbeddedTestServer(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS));
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_->RegisterRequestHandler(
        base::Bind(&ClearSiteDataThrottleBrowserTest::HandleRequest,
                   base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());
  }

  TestContentBrowserClient* GetContentBrowserClient() { return &test_client_; }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  // Handles all requests. If the request url query contains a "header" key,
  // responds with the "Clear-Site-Data" header of the corresponding value.
  // If the query contains a "redirect" key, responds with a redirect to a url
  // given by the corresponding value.
  //
  // Example: "https://localhost/?header={}&redirect=example.com" will respond
  // with headers
  // Clear-Site-Data: {}
  // Location: example.com
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse());

    std::string value;
    if (net::GetValueForKeyInQuery(request.GetURL(), "header", &value))
      response->AddCustomHeader("Clear-Site-Data", value);

    if (net::GetValueForKeyInQuery(request.GetURL(), "redirect", &value)) {
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", value);
    } else {
      response->set_code(net::HTTP_OK);
    }

    return std::move(response);
  }

  TestContentBrowserClient test_client_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// Tests that the header is recognized on the beginning, in the middle, and on
// the end of a redirect chain. Each of the three parts of the chain may or
// may not send the header, so there are 8 configurations to test.
IN_PROC_BROWSER_TEST_F(ClearSiteDataThrottleBrowserTest, Redirect) {
  GURL base_urls[3] = {
      https_server()->GetURL("origin1.com", "/"),
      https_server()->GetURL("origin2.com", "/foo/bar"),
      https_server()->GetURL("origin3.com", "/index.html"),
  };

  // Iterate through the configurations. URLs whose index is matched by the mask
  // will send the header, the others won't.
  for (int mask = 0; mask < (1 << 3); ++mask) {
    GURL urls[3];

    // Set up the expectations.
    for (int i = 0; i < 3; ++i) {
      urls[i] = base_urls[i];
      if (mask & (1 << i))
        AddQuery(&urls[i], "header", kClearCookiesHeader);

      EXPECT_CALL(*GetContentBrowserClient(),
                  ClearSiteData(shell()->web_contents()->GetBrowserContext(),
                                url::Origin(urls[i]), _, _, _, _))
          .Times((mask & (1 << i)) ? 1 : 0);
    }

    // Set up redirects between urls 0 --> 1 --> 2.
    AddQuery(&urls[1], "redirect", urls[2].spec());
    AddQuery(&urls[0], "redirect", urls[1].spec());

    // Navigate to the first url of the redirect chain.
    NavigateToURL(shell(), urls[0]);

    // We reached the end of the redirect chain.
    EXPECT_EQ(urls[2], shell()->web_contents()->GetURL());

    testing::Mock::VerifyAndClearExpectations(GetContentBrowserClient());
  }
}

// Tests that the Clear-Site-Data header is ignored for insecure origins.
IN_PROC_BROWSER_TEST_F(ClearSiteDataThrottleBrowserTest, Insecure) {
  // ClearSiteData() should not be called on HTTP.
  GURL url = embedded_test_server()->GetURL("example.com", "/");
  AddQuery(&url, "header", kClearCookiesHeader);
  ASSERT_FALSE(url.SchemeIsCryptographic());

  EXPECT_CALL(*GetContentBrowserClient(), ClearSiteData(_, _, _, _, _, _))
      .Times(0);

  NavigateToURL(shell(), url);
}

// Tests that ClearSiteData() is called for the correct datatypes.
IN_PROC_BROWSER_TEST_F(ClearSiteDataThrottleBrowserTest, Types) {
  GURL base_url = https_server()->GetURL("example.com", "/");

  struct TestCase {
    const char* value;
    bool remove_cookies;
    bool remove_storage;
    bool remove_cache;
  } test_cases[] = {
      {"{ \"types\": [ \"cookies\" ] }", true, false, false},
      {"{ \"types\": [ \"storage\" ] }", false, true, false},
      {"{ \"types\": [ \"cache\" ] }", false, false, true},
      {"{ \"types\": [ \"cookies\", \"storage\" ] }", true, true, false},
      {"{ \"types\": [ \"cookies\", \"cache\" ] }", true, false, true},
      {"{ \"types\": [ \"storage\", \"cache\" ] }", false, true, true},
      {"{ \"types\": [ \"cookies\", \"storage\", \"cache\" ] }", true, true,
       true},
  };

  for (const TestCase& test_case : test_cases) {
    GURL url = base_url;
    AddQuery(&url, "header", test_case.value);

    EXPECT_CALL(
        *GetContentBrowserClient(),
        ClearSiteData(shell()->web_contents()->GetBrowserContext(),
                      url::Origin(url), test_case.remove_cookies,
                      test_case.remove_storage, test_case.remove_cache, _));

    NavigateToURL(shell(), url);

    testing::Mock::VerifyAndClearExpectations(GetContentBrowserClient());
  }
}

}  // namespace content
