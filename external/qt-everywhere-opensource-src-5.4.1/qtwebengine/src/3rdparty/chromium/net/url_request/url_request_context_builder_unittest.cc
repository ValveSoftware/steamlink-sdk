// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context_builder.h"

#include "build/build_config.h"
#include "net/base/request_priority.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

namespace net {

namespace {

// A subclass of SpawnedTestServer that uses a statically-configured hostname.
// This is to work around mysterious failures in chrome_frame_net_tests. See:
// http://crbug.com/114369
class LocalHttpTestServer : public SpawnedTestServer {
 public:
  explicit LocalHttpTestServer(const base::FilePath& document_root)
      : SpawnedTestServer(SpawnedTestServer::TYPE_HTTP,
                          ScopedCustomUrlRequestTestHttpHost::value(),
                          document_root) {}
  LocalHttpTestServer()
      : SpawnedTestServer(SpawnedTestServer::TYPE_HTTP,
                          ScopedCustomUrlRequestTestHttpHost::value(),
                          base::FilePath()) {}
};

class MockHttpAuthHandlerFactory : public HttpAuthHandlerFactory {
 public:
  explicit MockHttpAuthHandlerFactory(int return_code) :
      return_code_(return_code) {}
  virtual ~MockHttpAuthHandlerFactory() {}

  virtual int CreateAuthHandler(HttpAuthChallengeTokenizer* challenge,
                                HttpAuth::Target target,
                                const GURL& origin,
                                CreateReason reason,
                                int nonce_count,
                                const BoundNetLog& net_log,
                                scoped_ptr<HttpAuthHandler>* handler) OVERRIDE {
    handler->reset();
    return return_code_;
  }

 private:
  int return_code_;
};

class URLRequestContextBuilderTest : public PlatformTest {
 protected:
  URLRequestContextBuilderTest()
      : test_server_(
          base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest"))) {
#if defined(OS_LINUX) || defined(OS_ANDROID)
    builder_.set_proxy_config_service(
        new ProxyConfigServiceFixed(ProxyConfig::CreateDirect()));
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
  }

  LocalHttpTestServer test_server_;
  URLRequestContextBuilder builder_;
};

TEST_F(URLRequestContextBuilderTest, DefaultSettings) {
  ASSERT_TRUE(test_server_.Start());

  scoped_ptr<URLRequestContext> context(builder_.Build());
  TestDelegate delegate;
  URLRequest request(test_server_.GetURL("echoheader?Foo"),
                     DEFAULT_PRIORITY,
                     &delegate,
                     context.get());
  request.set_method("GET");
  request.SetExtraRequestHeaderByName("Foo", "Bar", false);
  request.Start();
  base::MessageLoop::current()->Run();
  EXPECT_EQ("Bar", delegate.data_received());
}

TEST_F(URLRequestContextBuilderTest, UserAgent) {
  ASSERT_TRUE(test_server_.Start());

  builder_.set_user_agent("Bar");
  scoped_ptr<URLRequestContext> context(builder_.Build());
  TestDelegate delegate;
  URLRequest request(test_server_.GetURL("echoheader?User-Agent"),
                     DEFAULT_PRIORITY,
                     &delegate,
                     context.get());
  request.set_method("GET");
  request.Start();
  base::MessageLoop::current()->Run();
  EXPECT_EQ("Bar", delegate.data_received());
}

TEST_F(URLRequestContextBuilderTest, ExtraHttpAuthHandlerFactory) {
  GURL gurl("www.google.com");
  const int kBasicReturnCode = net::OK;
  MockHttpAuthHandlerFactory* mock_factory_basic =
      new MockHttpAuthHandlerFactory(kBasicReturnCode);
  scoped_ptr<HttpAuthHandler> handler;
  builder_.add_http_auth_handler_factory("ExtraScheme", mock_factory_basic);
  scoped_ptr<URLRequestContext> context(builder_.Build());
  // Verify that a handler is returned for and added scheme.
  EXPECT_EQ(kBasicReturnCode,
            context->http_auth_handler_factory()->CreateAuthHandlerFromString(
                "ExtraScheme",
                HttpAuth::AUTH_SERVER,
                gurl,
                BoundNetLog(),
                &handler));
  // Verify that a handler isn't returned for a bogus scheme.
  EXPECT_EQ(ERR_UNSUPPORTED_AUTH_SCHEME,
            context->http_auth_handler_factory()->CreateAuthHandlerFromString(
                "Bogus", HttpAuth::AUTH_SERVER, gurl, BoundNetLog(), &handler));
}

}  // namespace

}  // namespace net
