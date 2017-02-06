// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_delegate.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace data_reduction_proxy {

namespace {

// Constructs and returns a proxy with the specified scheme.
net::ProxyServer GetProxyWithScheme(net::ProxyServer::Scheme scheme) {
  switch (scheme) {
    case net::ProxyServer::SCHEME_HTTP:
      return net::ProxyServer::FromURI("origin.net:443",
                                       net::ProxyServer::SCHEME_HTTP);
    case net::ProxyServer::SCHEME_HTTPS:
      return net::ProxyServer::FromURI("https://origin.net:443",
                                       net::ProxyServer::SCHEME_HTTP);
    case net::ProxyServer::SCHEME_QUIC:
      return net::ProxyServer::FromURI("quic://origin.net:443",
                                       net::ProxyServer::SCHEME_QUIC);
    default:
      NOTREACHED();
      return net::ProxyServer::FromURI("", net::ProxyServer::SCHEME_INVALID);
  }
}

// Tests that the trusted SPDY proxy is verified correctly.
TEST(DataReductionProxyDelegate, IsTrustedSpdyProxy) {
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<DataReductionProxyTestContext> test_context =
      DataReductionProxyTestContext::Builder()
          .WithConfigClient()
          .WithMockDataReductionProxyService()
          .Build();

  const struct {
    bool is_in_trusted_spdy_proxy_field_trial;
    net::ProxyServer::Scheme first_proxy_scheme;
    net::ProxyServer::Scheme second_proxy_scheme;
    bool expect_proxy_is_trusted;
  } tests[] = {
      {false, net::ProxyServer::SCHEME_HTTP, net::ProxyServer::SCHEME_INVALID,
       false},
      {true, net::ProxyServer::SCHEME_HTTP, net::ProxyServer::SCHEME_INVALID,
       false},
      {true, net::ProxyServer::SCHEME_QUIC, net::ProxyServer::SCHEME_INVALID,
       false},
      {true, net::ProxyServer::SCHEME_HTTP, net::ProxyServer::SCHEME_HTTP,
       false},
      {true, net::ProxyServer::SCHEME_INVALID, net::ProxyServer::SCHEME_INVALID,
       false},
      // First proxy is HTTPS, and second is invalid.
      {true, net::ProxyServer::SCHEME_HTTPS, net::ProxyServer::SCHEME_INVALID,
       true},
      // First proxy is invalid, and second proxy is HTTPS.
      {true, net::ProxyServer::SCHEME_INVALID, net::ProxyServer::SCHEME_HTTPS,
       true},
      // First proxy is HTTPS, and second is HTTP.
      {true, net::ProxyServer::SCHEME_HTTPS, net::ProxyServer::SCHEME_HTTPS,
       true},
      // Second proxy is HTTPS, and first is HTTP.
      {true, net::ProxyServer::SCHEME_HTTP, net::ProxyServer::SCHEME_HTTPS,
       true},
      {true, net::ProxyServer::SCHEME_QUIC, net::ProxyServer::SCHEME_INVALID,
       false},
      {true, net::ProxyServer::SCHEME_QUIC, net::ProxyServer::SCHEME_HTTP,
       false},
      {true, net::ProxyServer::SCHEME_QUIC, net::ProxyServer::SCHEME_HTTPS,
       true},
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    ASSERT_EQ(
        tests[i].expect_proxy_is_trusted,
        tests[i].is_in_trusted_spdy_proxy_field_trial &&
            (tests[i].first_proxy_scheme == net::ProxyServer::SCHEME_HTTPS ||
             tests[i].second_proxy_scheme == net::ProxyServer::SCHEME_HTTPS))
        << i;

    std::vector<net::ProxyServer> proxies_for_http;
    net::ProxyServer first_proxy;
    net::ProxyServer second_proxy;
    if (tests[i].first_proxy_scheme != net::ProxyServer::SCHEME_INVALID) {
      first_proxy = GetProxyWithScheme(tests[i].first_proxy_scheme);
      proxies_for_http.push_back(first_proxy);
    }
    if (tests[i].second_proxy_scheme != net::ProxyServer::SCHEME_INVALID) {
      second_proxy = GetProxyWithScheme(tests[i].second_proxy_scheme);
      proxies_for_http.push_back(second_proxy);
    }

    std::unique_ptr<DataReductionProxyMutableConfigValues> config_values =
        DataReductionProxyMutableConfigValues::CreateFromParams(
            test_context->test_params());
    config_values->UpdateValues(proxies_for_http);

    std::unique_ptr<DataReductionProxyConfig> config(
        new DataReductionProxyConfig(
            message_loop_.task_runner(), test_context->net_log(),
            std::move(config_values), test_context->configurator(),
            test_context->event_creator()));

    DataReductionProxyDelegate delegate(
        config.get(), test_context->io_data()->configurator(),
        test_context->io_data()->event_creator(),
        test_context->io_data()->bypass_stats(),
        test_context->io_data()->net_log());

    base::FieldTrialList field_trial_list(nullptr);
    base::FieldTrialList::CreateFieldTrial(
        params::GetTrustedSpdyProxyFieldTrialName(),
        tests[i].is_in_trusted_spdy_proxy_field_trial ? "Enabled" : "Control");

    EXPECT_EQ(tests[i].expect_proxy_is_trusted,
              delegate.IsTrustedSpdyProxy(first_proxy) ||
                  delegate.IsTrustedSpdyProxy(second_proxy))
        << i;
  }
}

#if defined(OS_ANDROID)
const Client kClient = Client::CHROME_ANDROID;
#elif defined(OS_IOS)
const Client kClient = Client::CHROME_IOS;
#elif defined(OS_MACOSX)
const Client kClient = Client::CHROME_MAC;
#elif defined(OS_CHROMEOS)
const Client kClient = Client::CHROME_CHROMEOS;
#elif defined(OS_LINUX)
const Client kClient = Client::CHROME_LINUX;
#elif defined(OS_WIN)
const Client kClient = Client::CHROME_WINDOWS;
#elif defined(OS_FREEBSD)
const Client kClient = Client::CHROME_FREEBSD;
#elif defined(OS_OPENBSD)
const Client kClient = Client::CHROME_OPENBSD;
#elif defined(OS_SOLARIS)
const Client kClient = Client::CHROME_SOLARIS;
#elif defined(OS_QNX)
const Client kClient = Client::CHROME_QNX;
#else
const Client kClient = Client::UNKNOWN;
#endif

class TestLoFiUIService : public LoFiUIService {
 public:
  TestLoFiUIService() {}
  ~TestLoFiUIService() override {}

  void OnLoFiReponseReceived(const net::URLRequest& request,
                             bool is_preview) override {}
};

class DataReductionProxyDelegateTest : public testing::Test {
 public:
  DataReductionProxyDelegateTest()
      : context_(true),
        context_storage_(&context_),
        test_context_(DataReductionProxyTestContext::Builder()
                          .WithClient(kClient)
                          .WithMockClientSocketFactory(&mock_socket_factory_)
                          .WithURLRequestContext(&context_)
                          .Build()) {
    context_.set_client_socket_factory(&mock_socket_factory_);
    test_context_->AttachToURLRequestContext(&context_storage_);

    std::unique_ptr<TestLoFiUIService> lofi_ui_service(new TestLoFiUIService());
    lofi_ui_service_ = lofi_ui_service.get();
    test_context_->io_data()->set_lofi_ui_service(std::move(lofi_ui_service));

    proxy_delegate_ = test_context_->io_data()->CreateProxyDelegate();
    context_.set_proxy_delegate(proxy_delegate_.get());

    context_.Init();

    test_context_->EnableDataReductionProxyWithSecureProxyCheckSuccess();
  }

  // Each line in |response_headers| should end with "\r\n" and not '\0', and
  // the last line should have a second "\r\n".
  // An empty |response_headers| is allowed. It works by making this look like
  // an HTTP/0.9 response, since HTTP/0.9 responses don't have headers.
  std::unique_ptr<net::URLRequest> FetchURLRequest(
      const GURL& url,
      net::HttpRequestHeaders* request_headers,
      const std::string& response_headers,
      int64_t response_content_length) {
    const std::string response_body(
        base::checked_cast<size_t>(response_content_length), ' ');
    net::MockRead reads[] = {net::MockRead(response_headers.c_str()),
                             net::MockRead(response_body.c_str()),
                             net::MockRead(net::SYNCHRONOUS, net::OK)};
    net::StaticSocketDataProvider socket(reads, arraysize(reads), nullptr, 0);
    mock_socket_factory_.AddSocketDataProvider(&socket);

    net::TestDelegate delegate;
    std::unique_ptr<net::URLRequest> request =
        context_.CreateRequest(url, net::IDLE, &delegate);
    if (request_headers)
      request->SetExtraRequestHeaders(*request_headers);

    request->Start();
    base::RunLoop().RunUntilIdle();
    return request;
  }

  int64_t total_received_bytes() const {
    return GetSessionNetworkStatsInfoInt64("session_received_content_length");
  }

  int64_t total_original_received_bytes() const {
    return GetSessionNetworkStatsInfoInt64("session_original_content_length");
  }

  net::MockClientSocketFactory* mock_socket_factory() {
    return &mock_socket_factory_;
  }

  net::TestURLRequestContext* context() { return &context_; }

  TestDataReductionProxyParams* params() const {
    return test_context_->config()->test_params();
  }

  TestDataReductionProxyConfig* config() const {
    return test_context_->config();
  }

 private:
  int64_t GetSessionNetworkStatsInfoInt64(const char* key) const {
    const DataReductionProxyNetworkDelegate* drp_network_delegate =
        reinterpret_cast<const DataReductionProxyNetworkDelegate*>(
            context_.network_delegate());

    std::unique_ptr<base::DictionaryValue> session_network_stats_info =
        base::DictionaryValue::From(base::WrapUnique(
            drp_network_delegate->SessionNetworkStatsInfoToValue()));
    EXPECT_TRUE(session_network_stats_info);

    std::string string_value;
    EXPECT_TRUE(session_network_stats_info->GetString(key, &string_value));
    int64_t value = 0;
    EXPECT_TRUE(base::StringToInt64(string_value, &value));
    return value;
  }

  base::MessageLoopForIO message_loop_;
  net::MockClientSocketFactory mock_socket_factory_;
  net::TestURLRequestContext context_;
  net::URLRequestContextStorage context_storage_;

  TestLoFiUIService* lofi_ui_service_;
  std::unique_ptr<net::ProxyDelegate> proxy_delegate_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyDelegateTest, OnResolveProxyHandler) {
  int load_flags = net::LOAD_NORMAL;
  GURL url("http://www.google.com/");

  // Data reduction proxy info
  net::ProxyInfo data_reduction_proxy_info;
  std::string data_reduction_proxy;
  base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
  data_reduction_proxy_info.UsePacString(
      "PROXY " +
      net::ProxyServer::FromURI(params()->DefaultOrigin(),
                                net::ProxyServer::SCHEME_HTTP)
          .host_port_pair()
          .ToString() +
      "; DIRECT");
  EXPECT_FALSE(data_reduction_proxy_info.is_empty());

  // Data reduction proxy config
  net::ProxyConfig data_reduction_proxy_config;
  data_reduction_proxy_config.proxy_rules().ParseFromString(
      "http=" + data_reduction_proxy + ",direct://;");
  data_reduction_proxy_config.set_id(1);

  // Other proxy info
  net::ProxyInfo other_proxy_info;
  other_proxy_info.UseNamedProxy("proxy.com");
  EXPECT_FALSE(other_proxy_info.is_empty());

  // Direct
  net::ProxyInfo direct_proxy_info;
  direct_proxy_info.UseDirect();
  EXPECT_TRUE(direct_proxy_info.is_direct());

  // Empty retry info map
  net::ProxyRetryInfoMap empty_proxy_retry_info;

  // Retry info map with the data reduction proxy;
  net::ProxyRetryInfoMap data_reduction_proxy_retry_info;
  net::ProxyRetryInfo retry_info;
  retry_info.current_delay = base::TimeDelta::FromSeconds(1000);
  retry_info.bad_until = base::TimeTicks().Now() + retry_info.current_delay;
  retry_info.try_while_bad = false;
  data_reduction_proxy_retry_info[data_reduction_proxy_info.proxy_server()
                                      .ToURI()] = retry_info;

  net::ProxyInfo result;
  // Another proxy is used. It should be used afterwards.
  result.Use(other_proxy_info);
  OnResolveProxyHandler(url, "GET", load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(), &result);
  EXPECT_EQ(other_proxy_info.proxy_server(), result.proxy_server());

  // A direct connection is used. The data reduction proxy should be used
  // afterwards.
  // Another proxy is used. It should be used afterwards.
  result.Use(direct_proxy_info);
  net::ProxyConfig::ID prev_id = result.config_id();
  OnResolveProxyHandler(url, "GET", load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(), &result);
  EXPECT_EQ(data_reduction_proxy_info.proxy_server(), result.proxy_server());
  // Only the proxy list should be updated, not the proxy info.
  EXPECT_EQ(result.config_id(), prev_id);

  // A direct connection is used, but the data reduction proxy is on the retry
  // list. A direct connection should be used afterwards.
  result.Use(direct_proxy_info);
  prev_id = result.config_id();
  OnResolveProxyHandler(GURL("ws://echo.websocket.org/"), "GET", load_flags,
                        data_reduction_proxy_config,
                        data_reduction_proxy_retry_info, config(), &result);
  EXPECT_TRUE(result.proxy_server().is_direct());
  EXPECT_EQ(result.config_id(), prev_id);

  // Test that ws:// and wss:// URLs bypass the data reduction proxy.
  result.UseDirect();
  OnResolveProxyHandler(GURL("wss://echo.websocket.org/"), "GET", load_flags,
                        data_reduction_proxy_config, empty_proxy_retry_info,
                        config(), &result);
  EXPECT_TRUE(result.is_direct());

  result.UseDirect();
  OnResolveProxyHandler(GURL("wss://echo.websocket.org/"), "GET", load_flags,
                        data_reduction_proxy_config, empty_proxy_retry_info,
                        config(), &result);
  EXPECT_TRUE(result.is_direct());

  // POST methods go direct.
  result.UseDirect();
  OnResolveProxyHandler(url, "POST", load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(), &result);
  EXPECT_TRUE(result.is_direct());

  // Without DataCompressionProxyCriticalBypass Finch trial set, the
  // BYPASS_DATA_REDUCTION_PROXY load flag should be ignored.
  result.UseDirect();
  OnResolveProxyHandler(url, "GET", load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(), &result);
  EXPECT_FALSE(result.is_direct());

  OnResolveProxyHandler(url, "GET", load_flags, data_reduction_proxy_config,
                        empty_proxy_retry_info, config(), &other_proxy_info);
  EXPECT_FALSE(other_proxy_info.is_direct());
}

// Verifies that requests that were not proxied through data saver proxy due to
// missing config are recorded properly.
TEST_F(DataReductionProxyDelegateTest, HTTPRequests) {
  const struct {
    std::string url;
    bool enabled_by_user;
    bool use_direct_proxy;
    bool expect_histogram;
  } tests[] = {
      {
          // Request should not be logged because data saver is disabled.
          "http://www.example.com/", false, true, false,
      },
      {
          "http://www.example.com/", true, true, true,
      },
      {
          "http://www.example.com/", true, false, true,
      },
      {
          "https://www.example.com/", false, true, false,
      },
      {
          // Request should not be logged because request is HTTPS.
          "https://www.example.com/", true, true, false,
      },
      {
          // Request to localhost should not be logged.
          "http://127.0.0.1/", true, true, false,
      },
      {
          // Special use IPv4 address for testing purposes (RFC 5735).
          "http://198.51.100.1/", true, true, true,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    ASSERT_TRUE(tests[i].use_direct_proxy || tests[i].enabled_by_user);
    ASSERT_TRUE(tests[i].enabled_by_user || !tests[i].expect_histogram);
    base::HistogramTester histogram_tester;
    GURL url(tests[i].url);

    net::ProxyInfo data_reduction_proxy_info;

    std::string data_reduction_proxy;
    if (!tests[i].use_direct_proxy) {
      base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
      data_reduction_proxy_info.UsePacString(
          "PROXY " +
          net::ProxyServer::FromURI(params()->DefaultOrigin(),
                                    net::ProxyServer::SCHEME_HTTP)
              .host_port_pair()
              .ToString() +
          "; DIRECT");
    }
    EXPECT_EQ(tests[i].use_direct_proxy, data_reduction_proxy_info.is_empty());

    net::ProxyConfig data_reduction_proxy_config;
    if (tests[i].use_direct_proxy) {
      data_reduction_proxy_config = net::ProxyConfig::CreateDirect();

    } else {
      data_reduction_proxy_config.proxy_rules().ParseFromString(
          "http=" + data_reduction_proxy + ",direct://;");
      data_reduction_proxy_config.set_id(1);
    }
    config()->SetStateForTest(tests[i].enabled_by_user /* enabled */,
                              false /* at_startup */);

    net::ProxyRetryInfoMap empty_proxy_retry_info;

    net::ProxyInfo direct_proxy_info;
    direct_proxy_info.UseDirect();
    EXPECT_TRUE(direct_proxy_info.is_direct());

    net::ProxyInfo result;
    result.Use(direct_proxy_info);
    OnResolveProxyHandler(url, "GET", net::LOAD_NORMAL,
                          data_reduction_proxy_config, empty_proxy_retry_info,
                          config(), &result);
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.ConfigService.HTTPRequests",
        tests[i].expect_histogram ? 1 : 0);

    if (tests[i].expect_histogram) {
      histogram_tester.ExpectUniqueSample(
          "DataReductionProxy.ConfigService.HTTPRequests",
          tests[i].use_direct_proxy ? 0 : 1, 1);
    }
  }
}

#if defined(OS_ANDROID)
#define MAYBE_OnCompletedInternalLoFi DISABLED_OnCompletedInternalLoFi
#else
#define MAYBE_OnCompletedInternalLoFi OnCompletedInternalLoFi
#endif

#if defined(OS_ANDROID)
#define MAYBE_OnCompletedInternalLoFiPreview \
  DISABLED_OnCompletedInternalLoFiPreview
#else
#define MAYBE_OnCompletedInternalLoFiPreview OnCompletedInternalLoFiPreview
#endif

TEST_F(DataReductionProxyDelegateTest, OnCompletedSizeFor200) {
  int64_t baseline_received_bytes = total_received_bytes();
  int64_t baseline_original_received_bytes = total_original_received_bytes();

  const char kDrpResponseHeaders[] =
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Warning: 199 Misc-Agent \"some warning text\"\r\n"
      "Via:\r\n"
      "Via: 1.1 Chrome-Compression-Proxy-Suffix, 9.9 other-proxy\r\n"
      "Via: 2.2 Chrome-Compression-Proxy\r\n"
      "Warning: 214 Chrome-Compression-Proxy \"Transformation Applied\"\r\n"
      "X-Original-Content-Length: 10000\r\n"
      "Chrome-Proxy: q=low\r\n"
      "Content-Length: 1000\r\n\r\n";

  std::unique_ptr<net::URLRequest> request = FetchURLRequest(
      GURL("http://example.com/path/"), nullptr, kDrpResponseHeaders, 1000);

  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_received_bytes() - baseline_received_bytes);

  const std::string raw_headers = net::HttpUtil::AssembleRawHeaders(
      kDrpResponseHeaders, arraysize(kDrpResponseHeaders) - 1);
  EXPECT_EQ(static_cast<int64_t>(raw_headers.size() +
                                 10000 /* original_response_body */),
            total_original_received_bytes() - baseline_original_received_bytes);
}

TEST_F(DataReductionProxyDelegateTest, OnCompletedSizeFor304) {
  int64_t baseline_received_bytes = total_received_bytes();
  int64_t baseline_original_received_bytes = total_original_received_bytes();

  const char kDrpResponseHeaders[] =
      "HTTP/1.1 304 Not Modified\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "X-Original-Content-Length: 10000\r\n\r\n";

  std::unique_ptr<net::URLRequest> request = FetchURLRequest(
      GURL("http://example.com/path/"), nullptr, kDrpResponseHeaders, 0);

  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_received_bytes() - baseline_received_bytes);

  const std::string raw_headers = net::HttpUtil::AssembleRawHeaders(
      kDrpResponseHeaders, arraysize(kDrpResponseHeaders) - 1);
  EXPECT_EQ(static_cast<int64_t>(raw_headers.size() +
                                 10000 /* original_response_body */),
            total_original_received_bytes() - baseline_original_received_bytes);
}

TEST_F(DataReductionProxyDelegateTest, OnCompletedSizeForWriteError) {
  int64_t baseline_received_bytes = total_received_bytes();
  int64_t baseline_original_received_bytes = total_original_received_bytes();

  net::MockWrite writes[] = {
      net::MockWrite("GET http://example.com/path/ HTTP/1.1\r\n"
                     "Host: example.com\r\n"),
      net::MockWrite(net::ASYNC, net::ERR_ABORTED)};
  net::StaticSocketDataProvider socket(nullptr, 0, writes, arraysize(writes));
  mock_socket_factory()->AddSocketDataProvider(&socket);

  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
      GURL("http://example.com/path/"), net::IDLE, &delegate);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_received_bytes() - baseline_received_bytes);
  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_original_received_bytes() - baseline_original_received_bytes);
}

TEST_F(DataReductionProxyDelegateTest, OnCompletedSizeForReadError) {
  int64_t baseline_received_bytes = total_received_bytes();
  int64_t baseline_original_received_bytes = total_original_received_bytes();

  net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 OK\r\n"
                                         "Via: 1.1 Chrome-Compression-Proxy\r\n"
                                         "X-Original-Content-Length: 10000\r\n"
                                         "Content-Length: 1000\r\n\r\n"),
                           net::MockRead(net::ASYNC, net::ERR_ABORTED)};
  net::StaticSocketDataProvider socket(reads, arraysize(reads), nullptr, 0);
  mock_socket_factory()->AddSocketDataProvider(&socket);

  net::TestDelegate delegate;
  std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
      GURL("http://example.com/path/"), net::IDLE, &delegate);
  request->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_received_bytes() - baseline_received_bytes);
  EXPECT_EQ(request->GetTotalReceivedBytes(),
            total_original_received_bytes() - baseline_original_received_bytes);
}

}  // namespace

}  // namespace data_reduction_proxy
