// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/test/histogram_tester.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_delegate.h"
#include "net/base/request_priority.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::MockRead;
using net::MockWrite;
using testing::Return;

namespace data_reduction_proxy {

namespace {

const std::string kBody = "hello";
const std::string kNextBody = "hello again";
const std::string kErrorBody = "bad";

}  // namespace

class DataReductionProxyBypassStatsTest : public testing::Test {
 public:
  DataReductionProxyBypassStatsTest() : context_(true) {
    context_.Init();

    // The |test_job_factory_| takes ownership of the interceptor.
    test_job_interceptor_ = new net::TestJobInterceptor();
    EXPECT_TRUE(test_job_factory_.SetProtocolHandler(
        url::kHttpScheme, base::WrapUnique(test_job_interceptor_)));

    context_.set_job_factory(&test_job_factory_);

    test_context_ =
        DataReductionProxyTestContext::Builder().WithMockConfig().Build();
    mock_url_request_ = context_.CreateRequest(GURL(), net::IDLE, &delegate_);
  }

  std::unique_ptr<net::URLRequest> CreateURLRequestWithResponseHeaders(
      const GURL& url,
      const std::string& response_headers) {
    std::unique_ptr<net::URLRequest> fake_request =
        context_.CreateRequest(url, net::IDLE, &delegate_);

    // Create a test job that will fill in the given response headers for the
    // |fake_request|.
    std::unique_ptr<net::URLRequestTestJob> test_job(new net::URLRequestTestJob(
        fake_request.get(), context_.network_delegate(), response_headers,
        std::string(), true));

    // Configure the interceptor to use the test job to handle the next request.
    test_job_interceptor_->set_main_intercept_job(std::move(test_job));
    fake_request->Start();
    test_context_->RunUntilIdle();

    EXPECT_TRUE(fake_request->response_headers() != NULL);
    return fake_request;
  }

 protected:
  std::unique_ptr<DataReductionProxyBypassStats> BuildBypassStats() {
    return base::MakeUnique<DataReductionProxyBypassStats>(
        test_context_->config(), test_context_->unreachable_callback());
  }

  MockDataReductionProxyConfig* config() const {
    return test_context_->mock_config();
  }

 private:
  base::MessageLoopForIO message_loop_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  std::unique_ptr<net::URLRequest> mock_url_request_;
  // |test_job_interceptor_| is owned by |test_job_factory_|.
  net::TestJobInterceptor* test_job_interceptor_;
  net::URLRequestJobFactoryImpl test_job_factory_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
};


TEST_F(DataReductionProxyBypassStatsTest, RecordMissingViaHeaderBytes) {
  const std::string k4xxHistogramName =
      "DataReductionProxy.MissingViaHeader.Bytes.4xx";
  const std::string kOtherHistogramName =
      "DataReductionProxy.MissingViaHeader.Bytes.Other";
  const int64_t kResponseContentLength = 100;

  struct TestCase {
    bool was_proxy_used;
    const char* headers;
    bool is_4xx_sample_expected;
    bool is_other_sample_expected;
  };
  const TestCase test_cases[] = {
    // Nothing should be recorded for requests that don't use the proxy.
    {
      false,
      "HTTP/1.1 404 Not Found\n",
      false,
      false
    },
    {
      false,
      "HTTP/1.1 200 OK\n",
      false,
      false
    },
    // Nothing should be recorded for responses that have the via header.
    {
      true,
      "HTTP/1.1 404 Not Found\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      false,
      false
    },
    {
      true,
      "HTTP/1.1 200 OK\n"
      "Via: 1.1 Chrome-Compression-Proxy\n",
      false,
      false
    },
    // 4xx responses that used the proxy and don't have the via header should be
    // recorded.
    {
      true,
      "HTTP/1.1 404 Not Found\n",
      true,
      false
    },
    {
      true,
      "HTTP/1.1 400 Bad Request\n",
      true,
      false
    },
    {
      true,
      "HTTP/1.1 499 Big Client Error Response Code\n",
      true,
      false
    },
    // Non-4xx responses that used the proxy and don't have the via header
    // should be recorded.
    {
      true,
      "HTTP/1.1 200 OK\n",
      false,
      true
    },
    {
      true,
      "HTTP/1.1 399 Big Redirection Response Code\n",
      false,
      true
    },
    {
      true,
      "HTTP/1.1 500 Internal Server Error\n",
      false,
      true
    }
  };

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    base::HistogramTester histogram_tester;
    std::unique_ptr<DataReductionProxyBypassStats> bypass_stats =
        BuildBypassStats();

    std::unique_ptr<net::URLRequest> fake_request(
        CreateURLRequestWithResponseHeaders(GURL("http://www.google.com/"),
                                            test_cases[i].headers));
    fake_request->set_received_response_content_length(kResponseContentLength);

    EXPECT_CALL(*config(),
                WasDataReductionProxyUsed(fake_request.get(), testing::_))
        .WillRepeatedly(Return(test_cases[i].was_proxy_used));

    bypass_stats->RecordMissingViaHeaderBytes(*fake_request);

    if (test_cases[i].is_4xx_sample_expected) {
      histogram_tester.ExpectUniqueSample(k4xxHistogramName,
                                          kResponseContentLength, 1);
    } else {
      histogram_tester.ExpectTotalCount(k4xxHistogramName, 0);
    }

    if (test_cases[i].is_other_sample_expected) {
      histogram_tester.ExpectUniqueSample(kOtherHistogramName,
                                          kResponseContentLength, 1);
    } else {
      histogram_tester.ExpectTotalCount(kOtherHistogramName, 0);
    }
  }
}

// End-to-end tests for the DataReductionProxy.BypassedBytes histograms.
class DataReductionProxyBypassStatsEndToEndTest : public testing::Test {
 public:
  DataReductionProxyBypassStatsEndToEndTest()
      : context_(true), context_storage_(&context_) {}

  ~DataReductionProxyBypassStatsEndToEndTest() override {
    drp_test_context_->io_data()->ShutdownOnUIThread();
    drp_test_context_->RunUntilIdle();
  }

  void SetUp() override {
    // Only use the primary data reduction proxy in order to make it easier to
    // test bypassed bytes due to proxy fallbacks. This way, a test just needs
    // to cause one proxy fallback in order for the data reduction proxy to be
    // fully bypassed.
    drp_test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(DataReductionProxyParams::kAllowed)
            .WithURLRequestContext(&context_)
            .WithMockClientSocketFactory(&mock_socket_factory_)
            .Build();
    drp_test_context_->AttachToURLRequestContext(&context_storage_);
    context_.set_client_socket_factory(&mock_socket_factory_);
    proxy_delegate_ = drp_test_context_->io_data()->CreateProxyDelegate();
    context_.set_proxy_delegate(proxy_delegate_.get());
  }

  // Create and execute a fake request using the data reduction proxy stack.
  // Passing in nullptr for |retry_response_headers| indicates that the request
  // is not expected to be retried.
  std::unique_ptr<net::URLRequest> CreateAndExecuteRequest(
      const GURL& url,
      int load_flags,
      net::Error finish_code,
      const char* initial_response_headers,
      const char* initial_response_body,
      const char* retry_response_headers,
      const char* retry_response_body) {
    // Support HTTPS URLs, and fetches over HTTPS proxy.
    net::SSLSocketDataProvider ssl_socket_data_provider(net::ASYNC,
                                                        finish_code);
    if (url.SchemeIsCryptographic() ||
        (!config()->test_params()->proxies_for_http().empty() &&
         config()->test_params()->proxies_for_http().front().is_https())) {
      mock_socket_factory_.AddSSLSocketDataProvider(&ssl_socket_data_provider);
    }

    // Prepare for the initial response.
    MockRead initial_data_reads[] = {
        MockRead(initial_response_headers), MockRead(initial_response_body),
        MockRead(net::SYNCHRONOUS, finish_code),
    };
    net::StaticSocketDataProvider initial_socket_data_provider(
        initial_data_reads, arraysize(initial_data_reads), nullptr, 0);
    mock_socket_factory_.AddSocketDataProvider(&initial_socket_data_provider);

    // Prepare for the response from retrying the request, if applicable.
    // |retry_data_reads| and |retry_socket_data_provider| are out here so that
    // they stay in scope for when the request is executed.
    std::vector<MockRead> retry_data_reads;
    std::unique_ptr<net::StaticSocketDataProvider> retry_socket_data_provider;
    if (retry_response_headers) {
      retry_data_reads.push_back(MockRead(retry_response_headers));
      retry_data_reads.push_back(MockRead(retry_response_body));
      retry_data_reads.push_back(MockRead(net::SYNCHRONOUS, finish_code));

      retry_socket_data_provider.reset(new net::StaticSocketDataProvider(
          &retry_data_reads.front(), retry_data_reads.size(), nullptr, 0));
      mock_socket_factory_.AddSocketDataProvider(
          retry_socket_data_provider.get());
    }

    std::unique_ptr<net::URLRequest> request(
        context_.CreateRequest(url, net::IDLE, &delegate_));
    request->set_method("GET");
    request->SetLoadFlags(load_flags);
    request->Start();
    drp_test_context_->RunUntilIdle();
    return request;
  }

  void set_proxy_service(net::ProxyService* proxy_service) {
    context_.set_proxy_service(proxy_service);
  }

  void set_host_resolver(net::HostResolver* host_resolver) {
    context_.set_host_resolver(host_resolver);
  }

  const DataReductionProxySettings* settings() const {
    return drp_test_context_->settings();
  }

  TestDataReductionProxyConfig* config() const {
    return drp_test_context_->config();
  }

  DataReductionProxyBypassStats* bypass_stats() const {
    return drp_test_context_->bypass_stats();
  }

  void ClearBadProxies() {
    context_.proxy_service()->ClearBadProxiesCache();
  }

  void InitializeContext() {
    context_.Init();
    drp_test_context_->EnableDataReductionProxyWithSecureProxyCheckSuccess();
  }

  bool IsUnreachable() const {
    return drp_test_context_->settings()->IsDataReductionProxyUnreachable();
  }

  DataReductionProxyTestContext* drp_test_context() const {
    return drp_test_context_.get();
  }

  void ExpectOtherBypassedBytesHistogramsEmpty(
      const base::HistogramTester& histogram_tester,
      const std::set<std::string>& excluded_histograms) const {
    const std::string kHistograms[] = {
        "DataReductionProxy.BypassedBytes.NotBypassed",
        "DataReductionProxy.BypassedBytes.SSL",
        "DataReductionProxy.BypassedBytes.LocalBypassRules",
        "DataReductionProxy.BypassedBytes.ProxyOverridden",
        "DataReductionProxy.BypassedBytes.Current",
        "DataReductionProxy.BypassedBytes.CurrentAudioVideo",
        "DataReductionProxy.BypassedBytes.CurrentApplicationOctetStream",
        "DataReductionProxy.BypassedBytes.ShortAll",
        "DataReductionProxy.BypassedBytes.ShortTriggeringRequest",
        "DataReductionProxy.BypassedBytes.ShortAudioVideo",
        "DataReductionProxy.BypassedBytes.MediumAll",
        "DataReductionProxy.BypassedBytes.MediumTriggeringRequest",
        "DataReductionProxy.BypassedBytes.LongAll",
        "DataReductionProxy.BypassedBytes.LongTriggeringRequest",
        "DataReductionProxy.BypassedBytes.MissingViaHeader4xx",
        "DataReductionProxy.BypassedBytes.MissingViaHeaderOther",
        "DataReductionProxy.BypassedBytes.Malformed407",
        "DataReductionProxy.BypassedBytes.Status500HttpInternalServerError",
        "DataReductionProxy.BypassedBytes.Status502HttpBadGateway",
        "DataReductionProxy.BypassedBytes.Status503HttpServiceUnavailable",
        "DataReductionProxy.BypassedBytes.NetworkErrorOther",
    };

    for (const std::string& histogram : kHistograms) {
      if (excluded_histograms.find(histogram) ==
          excluded_histograms.end()) {
        histogram_tester.ExpectTotalCount(histogram, 0);
      }
    }
  }

  void ExpectOtherBypassedBytesHistogramsEmpty(
      const base::HistogramTester& histogram_tester,
      const std::string& excluded_histogram) const {
    std::set<std::string> excluded_histograms;
    excluded_histograms.insert(excluded_histogram);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            excluded_histograms);
  }

  void ExpectOtherBypassedBytesHistogramsEmpty(
      const base::HistogramTester& histogram_tester,
      const std::string& first_excluded_histogram,
      const std::string& second_excluded_histogram) const {
    std::set<std::string> excluded_histograms;
    excluded_histograms.insert(first_excluded_histogram);
    excluded_histograms.insert(second_excluded_histogram);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            excluded_histograms);
  }

  net::TestDelegate* delegate() { return &delegate_; }

  // Marks a data reduction proxy as bypassed if
  // |bypassed_proxy_server_is_data_reduction_proxy| is true. Then, runs a
  // request via data reduction proxy if |is_data_reduction_proxy| is true,
  // and verifies that proxy is unreachable only if |is_unreachable| is true.
  void VerifyProxyReachablity(
      bool bypassed_proxy_server_is_data_reduction_proxy,
      bool is_data_reduction_proxy,
      bool is_unreachable) {
    InitializeContext();

    std::string proxy = bypassed_proxy_server_is_data_reduction_proxy
                            ? "origin.net:80"
                            : "foo.net:80";
    net::ProxyServer fallback_proxy_server =
        net::ProxyServer::FromURI(proxy, net::ProxyServer::SCHEME_HTTP);

    bypass_stats()->OnProxyFallback(fallback_proxy_server,
                                    net::ERR_PROXY_CONNECTION_FAILED);
    drp_test_context()->RunUntilIdle();

    if (!is_data_reduction_proxy)
      config()->SetWasDataReductionProxyNotUsed();

    CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                            "HTTP/1.1 200 OK\r\n"
                            "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                            kNextBody.c_str(), nullptr, nullptr);

    drp_test_context()->RunUntilIdle();
    EXPECT_EQ(is_unreachable, IsUnreachable());
  }

 private:
  base::MessageLoopForIO message_loop_;
  net::TestDelegate delegate_;
  net::MockClientSocketFactory mock_socket_factory_;
  net::TestURLRequestContext context_;
  net::URLRequestContextStorage context_storage_;
  std::unique_ptr<net::ProxyDelegate> proxy_delegate_;
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context_;
};

TEST_F(DataReductionProxyBypassStatsEndToEndTest, BypassedBytesNoRetry) {
  struct TestCase {
    GURL url;
    const char* histogram_name;
    const char* initial_response_headers;
  };
  const TestCase test_cases[] = {
    { GURL("http://foo.com"),
      "DataReductionProxy.BypassedBytes.NotBypassed",
      "HTTP/1.1 200 OK\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
    },
    { GURL("https://foo.com"),
      "DataReductionProxy.BypassedBytes.SSL",
      "HTTP/1.1 200 OK\r\n\r\n",
    },
    { GURL("http://localhost"),
      "DataReductionProxy.BypassedBytes.LocalBypassRules",
      "HTTP/1.1 200 OK\r\n\r\n",
    },
  };

  InitializeContext();
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;
    CreateAndExecuteRequest(test_case.url, net::LOAD_NORMAL, net::OK,
                            test_case.initial_response_headers, kBody.c_str(),
                            nullptr, nullptr);

    histogram_tester.ExpectUniqueSample(test_case.histogram_name, kBody.size(),
                                        1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.histogram_name);
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       BypassedBytesProxyOverridden) {
  std::unique_ptr<net::ProxyService> proxy_service(
      net::ProxyService::CreateFixed("http://test.com:80"));
  set_proxy_service(proxy_service.get());
  InitializeContext();

  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n\r\n", kBody.c_str(), nullptr,
                          nullptr);

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.ProxyOverridden", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.ProxyOverridden");
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest, BypassedBytesCurrent) {
  InitializeContext();
  struct TestCase {
    const char* histogram_name;
    const char* retry_response_headers;
  };
  const TestCase test_cases[] = {
      {"DataReductionProxy.BypassedBytes.Current", "HTTP/1.1 200 OK\r\n\r\n"},
      {"DataReductionProxy.BypassedBytes.CurrentAudioVideo",
       "HTTP/1.1 200 OK\r\n"
       "Content-Type: video/mp4\r\n\r\n"},
      {"DataReductionProxy.BypassedBytes.CurrentApplicationOctetStream",
       "HTTP/1.1 200 OK\r\n"
       "Content-Type: application/octet-stream\r\n\r\n"},
  };
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;
    CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                            "HTTP/1.1 502 Bad Gateway\r\n"
                            "Via: 1.1 Chrome-Compression-Proxy\r\n"
                            "Chrome-Proxy: block-once\r\n\r\n",
                            kErrorBody.c_str(),
                            test_case.retry_response_headers, kBody.c_str());

    histogram_tester.ExpectUniqueSample(test_case.histogram_name, kBody.size(),
                                        1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.histogram_name);
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       BypassedBytesShortAudioVideo) {
  InitializeContext();
  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 502 Bad Gateway\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n"
                          "Chrome-Proxy: block=1\r\n\r\n",
                          kErrorBody.c_str(),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: video/mp4\r\n\r\n",
                          kBody.c_str());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.ShortAudioVideo", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.ShortAudioVideo");
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       BypassedBytesShortAudioVideoCancelled) {
  InitializeContext();
  base::HistogramTester histogram_tester;

  delegate()->set_cancel_in_received_data(true);
  CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 502 Bad Gateway\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n"
                          "Chrome-Proxy: block=1\r\n\r\n",
                          kErrorBody.c_str(),
                          "HTTP/1.1 200 OK\r\n"
                          "Content-Type: video/mp4\r\n\r\n",
                          kBody.c_str());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.ShortAudioVideo", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.ShortAudioVideo");
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest, BypassedBytesExplicitBypass) {
  struct TestCase {
    const char* triggering_histogram_name;
    const char* all_histogram_name;
    const char* initial_response_headers;
  };
  const TestCase test_cases[] = {
    { "DataReductionProxy.BypassedBytes.ShortTriggeringRequest",
      "DataReductionProxy.BypassedBytes.ShortAll",
      "HTTP/1.1 502 Bad Gateway\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: block=1\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.MediumTriggeringRequest",
      "DataReductionProxy.BypassedBytes.MediumAll",
      "HTTP/1.1 502 Bad Gateway\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: block=0\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.LongTriggeringRequest",
      "DataReductionProxy.BypassedBytes.LongAll",
      "HTTP/1.1 502 Bad Gateway\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "Chrome-Proxy: block=3600\r\n\r\n",
    },
  };

  InitializeContext();
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;

    CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                            test_case.initial_response_headers,
                            kErrorBody.c_str(), "HTTP/1.1 200 OK\r\n\r\n",
                            kBody.c_str());
    // The first request caused the proxy to be marked as bad, so this second
    // request should not come through the proxy.
    CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                            "HTTP/1.1 200 OK\r\n\r\n", kNextBody.c_str(),
                            nullptr, nullptr);

    histogram_tester.ExpectUniqueSample(test_case.triggering_histogram_name,
                                        kBody.size(), 1);
    histogram_tester.ExpectUniqueSample(test_case.all_histogram_name,
                                        kNextBody.size(), 1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.triggering_histogram_name,
                                            test_case.all_histogram_name);
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       BypassedBytesClientSideFallback) {
  struct TestCase {
    const char* histogram_name;
    const char* initial_response_headers;
  };
  const TestCase test_cases[] = {
    { "DataReductionProxy.BypassedBytes.MissingViaHeaderOther",
      "HTTP/1.1 200 OK\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Malformed407",
      "HTTP/1.1 407 Proxy Authentication Required\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Status500HttpInternalServerError",
      "HTTP/1.1 500 Internal Server Error\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Status502HttpBadGateway",
      "HTTP/1.1 502 Bad Gateway\r\n\r\n",
    },
    { "DataReductionProxy.BypassedBytes.Status503HttpServiceUnavailable",
      "HTTP/1.1 503 Service Unavailable\r\n\r\n",
    },
  };

  InitializeContext();
  for (const TestCase& test_case : test_cases) {
    ClearBadProxies();
    base::HistogramTester histogram_tester;

    CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                            test_case.initial_response_headers,
                            kErrorBody.c_str(), "HTTP/1.1 200 OK\r\n\r\n",
                            kBody.c_str());

    histogram_tester.ExpectUniqueSample(
        "DataReductionProxy.ConfigService.HTTPRequests", 1, 1);

    // The first request caused the proxy to be marked as bad, so this second
    // request should not come through the proxy.
    CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                            "HTTP/1.1 200 OK\r\n\r\n", kNextBody.c_str(),
                            nullptr, nullptr);

    histogram_tester.ExpectTotalCount(test_case.histogram_name, 2);
    histogram_tester.ExpectBucketCount(test_case.histogram_name, kBody.size(),
                                       1);
    histogram_tester.ExpectBucketCount(test_case.histogram_name,
                                       kNextBody.size(), 1);
    ExpectOtherBypassedBytesHistogramsEmpty(histogram_tester,
                                            test_case.histogram_name);

    // "DataReductionProxy.ConfigService.HTTPRequests" should not be recorded
    // for bypassed requests.
    histogram_tester.ExpectUniqueSample(
        "DataReductionProxy.ConfigService.HTTPRequests", 1, 1);
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest, BypassedBytesNetErrorOther) {
  // Make the data reduction proxy host fail to resolve.
  net::ProxyServer origin = config()->test_params()->proxies_for_http().front();
  std::unique_ptr<net::MockHostResolver> host_resolver(
      new net::MockHostResolver());
  host_resolver->rules()->AddSimulatedFailure(origin.host_port_pair().host());
  set_host_resolver(host_resolver.get());
  InitializeContext();

  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n\r\n", kBody.c_str(), nullptr,
                          nullptr);

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassedBytes.NetworkErrorOther", kBody.size(), 1);
  ExpectOtherBypassedBytesHistogramsEmpty(
      histogram_tester, "DataReductionProxy.BypassedBytes.NetworkErrorOther");
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.BypassOnNetworkErrorPrimary",
      -net::ERR_PROXY_CONNECTION_FAILED, 1);
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       BypassedBytesMissingViaHeader4xx) {
  InitializeContext();
  const char* test_case_response_headers[] = {
      "HTTP/1.1 414 Request-URI Too Long\r\n\r\n",
      "HTTP/1.1 404 Not Found\r\n\r\n",
  };
  for (const char* test_case : test_case_response_headers) {
    ClearBadProxies();
    // The first request should be bypassed for missing Via header.
    {
      base::HistogramTester histogram_tester;
      CreateAndExecuteRequest(GURL("http://foo.com"), net::LOAD_NORMAL, net::OK,
                              test_case, kErrorBody.c_str(),
                              "HTTP/1.1 200 OK\r\n\r\n", kBody.c_str());

      histogram_tester.ExpectUniqueSample(
          "DataReductionProxy.BypassedBytes.MissingViaHeader4xx", kBody.size(),
          1);
      ExpectOtherBypassedBytesHistogramsEmpty(
          histogram_tester,
          "DataReductionProxy.BypassedBytes.MissingViaHeader4xx");
    }
    // The second request should be sent via the proxy.
    {
      base::HistogramTester histogram_tester;
      CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                              "HTTP/1.1 200 OK\r\n"
                              "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                              kNextBody.c_str(), nullptr, nullptr);
      histogram_tester.ExpectUniqueSample(
          "DataReductionProxy.BypassedBytes.NotBypassed", kNextBody.size(), 1);
      ExpectOtherBypassedBytesHistogramsEmpty(
          histogram_tester, "DataReductionProxy.BypassedBytes.NotBypassed");
    }
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       IsDataReductionProxyUnreachable_Unreachable) {
  VerifyProxyReachablity(true, false, true);
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       IsDataReductionProxyUnreachable_Unreachable_Then_Reachable) {
  VerifyProxyReachablity(true, true, false);
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       IsDataReductionProxyUnreachable_Not_A_data_reduction_proxy_1) {
  VerifyProxyReachablity(false, true, false);
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       IsDataReductionProxyUnreachableNot_A_data_reduction_proxy_2) {
  VerifyProxyReachablity(false, false, false);
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       ProxyUnreachableThenReachable) {
  net::ProxyServer fallback_proxy_server =
      net::ProxyServer::FromURI("origin.net:80", net::ProxyServer::SCHEME_HTTP);

  InitializeContext();

  // Proxy falls back.
  bypass_stats()->OnProxyFallback(fallback_proxy_server,
                                  net::ERR_PROXY_CONNECTION_FAILED);
  drp_test_context()->RunUntilIdle();
  EXPECT_TRUE(IsUnreachable());

  // Proxy succeeds.
  CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                          kNextBody.c_str(), nullptr, nullptr);
  drp_test_context()->RunUntilIdle();
  EXPECT_FALSE(IsUnreachable());
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       ProxyReachableThenUnreachable) {
  InitializeContext();
  net::ProxyServer fallback_proxy_server =
      net::ProxyServer::FromURI("origin.net:80", net::ProxyServer::SCHEME_HTTP);

  // Proxy succeeds.
  CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                          kNextBody.c_str(), nullptr, nullptr);
  drp_test_context()->RunUntilIdle();
  EXPECT_FALSE(IsUnreachable());

  // Then proxy falls back indefinitely after |kMaxFailedRequestsBeforeReset|
  // failures.
  for (size_t i = 0; i < 4; ++i) {
    bypass_stats()->OnProxyFallback(fallback_proxy_server,
                                    net::ERR_PROXY_CONNECTION_FAILED);
  }

  drp_test_context()->RunUntilIdle();
  EXPECT_TRUE(IsUnreachable());
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest,
       DetectAndRecordMissingViaHeaderResponseCode) {
  const std::string kPrimaryHistogramName =
      "DataReductionProxy.MissingViaHeader.ResponseCode.Primary";
  const std::string kFallbackHistogramName =
      "DataReductionProxy.MissingViaHeader.ResponseCode.Fallback";
  InitializeContext();
  struct TestCase {
    bool is_primary;
    const char* headers;
    int expected_primary_sample;   // -1 indicates no expected sample.
    int expected_fallback_sample;  // -1 indicates no expected sample.
  };
  const TestCase test_cases[] = {
      {true,
       "HTTP/1.1 200 OK\n"
       "Via: 1.1 Chrome-Compression-Proxy\n",
       -1, -1},
      {false,
       "HTTP/1.1 200 OK\n"
       "Via: 1.1 Chrome-Compression-Proxy\n",
       -1, -1},
      {true, "HTTP/1.1 200 OK\n", 200, -1},
      {false, "HTTP/1.1 200 OK\n", -1, 200},
      {true, "HTTP/1.1 304 Not Modified\n", 304, -1},
      {false, "HTTP/1.1 304 Not Modified\n", -1, 304},
      {true, "HTTP/1.1 404 Not Found\n", 404, -1},
      {false, "HTTP/1.1 404 Not Found\n", -1, 404}};

  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    base::HistogramTester histogram_tester;
    std::string raw_headers(test_cases[i].headers);
    HeadersToRaw(&raw_headers);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(raw_headers));

    DataReductionProxyBypassStats::DetectAndRecordMissingViaHeaderResponseCode(
        test_cases[i].is_primary, headers.get());

    if (test_cases[i].expected_primary_sample == -1) {
      histogram_tester.ExpectTotalCount(kPrimaryHistogramName, 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          kPrimaryHistogramName, test_cases[i].expected_primary_sample, 1);
    }

    if (test_cases[i].expected_fallback_sample == -1) {
      histogram_tester.ExpectTotalCount(kFallbackHistogramName, 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          kFallbackHistogramName, test_cases[i].expected_fallback_sample, 1);
    }
  }
}

TEST_F(DataReductionProxyBypassStatsEndToEndTest, SuccessfulRequestCompletion) {
  const std::string kPrimaryHistogramName =
      "DataReductionProxy.SuccessfulRequestCompletionCounts";
  const std::string kPrimaryMainFrameHistogramName =
      "DataReductionProxy.SuccessfulRequestCompletionCounts.MainFrame";

  InitializeContext();
  const struct {
    bool was_proxy_used;
    bool is_load_bypass_proxy;
    size_t proxy_index;
    bool is_main_frame;
    net::Error net_error;
  } tests[] = {{false, true, 0, true, net::OK},
               {false, true, 0, false, net::ERR_TOO_MANY_REDIRECTS},
               {false, false, 0, true, net::OK},
               {false, false, 0, false, net::ERR_TOO_MANY_REDIRECTS},
               {true, false, 0, true, net::OK},
               {true, false, 0, true, net::ERR_TOO_MANY_REDIRECTS},
               {true, false, 0, false, net::OK},
               {true, false, 0, false, net::ERR_TOO_MANY_REDIRECTS},
               {true, false, 1, true, net::OK},
               {true, false, 1, true, net::ERR_TOO_MANY_REDIRECTS},
               {true, false, 1, false, net::OK},
               {true, false, 1, false, net::ERR_TOO_MANY_REDIRECTS}};

  for (const auto& test : tests) {
    config()->ResetWasDataReductionProxyUsed();
    base::HistogramTester histogram_tester;

    // Proxy succeeds.
    int load_flags = net::LOAD_NORMAL;
    if (test.is_load_bypass_proxy) {
      load_flags |= net::LOAD_BYPASS_PROXY;
    }
    if (test.is_main_frame) {
      load_flags |= net::LOAD_MAIN_FRAME_DEPRECATED;
    }

    if (!test.was_proxy_used)
      config()->SetWasDataReductionProxyNotUsed();
    else {
      config()->SetWasDataReductionProxyUsedProxyIndex(test.proxy_index);
    }

    CreateAndExecuteRequest(GURL("http://bar.com"), load_flags, test.net_error,
                            "HTTP/1.1 200 OK\r\n"
                            "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                            kNextBody.c_str(), nullptr, nullptr);
    drp_test_context()->RunUntilIdle();

    if (test.was_proxy_used && !test.is_load_bypass_proxy &&
        test.net_error == net::OK) {
      histogram_tester.ExpectUniqueSample(kPrimaryHistogramName,
                                          test.proxy_index, 1);
    } else {
      histogram_tester.ExpectTotalCount(kPrimaryHistogramName, 0);
    }

    if (test.was_proxy_used && !test.is_load_bypass_proxy &&
        test.is_main_frame && test.net_error == net::OK) {
      histogram_tester.ExpectUniqueSample(kPrimaryMainFrameHistogramName,
                                          test.proxy_index, 1);
    } else {
      histogram_tester.ExpectTotalCount(kPrimaryMainFrameHistogramName, 0);
    }
  }
}

// Verifies that the scheme of the HTTP data reduction proxy used is recorded
// correctly.
TEST_F(DataReductionProxyBypassStatsEndToEndTest, HttpProxyScheme) {
  InitializeContext();

  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                          kNextBody.c_str(), nullptr, nullptr);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.ProxySchemeUsed",
                                      1 /*PROXY_SCHEME_HTTP */, 1);
}

// Verifies that the scheme of the HTTPS data reduction proxy used is recorded
// correctly.
TEST_F(DataReductionProxyBypassStatsEndToEndTest, HttpsProxyScheme) {
  net::ProxyServer origin =
      net::ProxyServer::FromURI("test.com:443", net::ProxyServer::SCHEME_HTTPS);
  std::vector<net::ProxyServer> proxies_for_http;
  proxies_for_http.push_back(origin);
  config()->test_params()->SetProxiesForHttp(proxies_for_http);

  InitializeContext();

  base::HistogramTester histogram_tester;
  CreateAndExecuteRequest(GURL("http://bar.com"), net::LOAD_NORMAL, net::OK,
                          "HTTP/1.1 200 OK\r\n"
                          "Via: 1.1 Chrome-Compression-Proxy\r\n\r\n",
                          kNextBody.c_str(), nullptr, nullptr);
  histogram_tester.ExpectUniqueSample("DataReductionProxy.ProxySchemeUsed",
                                      2 /*PROXY_SCHEME_HTTPS */, 1);
}

}  // namespace data_reduction_proxy
