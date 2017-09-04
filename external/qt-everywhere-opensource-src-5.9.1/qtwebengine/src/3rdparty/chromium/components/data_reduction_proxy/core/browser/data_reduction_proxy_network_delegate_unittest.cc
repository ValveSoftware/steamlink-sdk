// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/lofi_decider.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace data_reduction_proxy {
namespace {

using TestNetworkDelegate = net::NetworkDelegateImpl;

const char kOtherProxy[] = "testproxy:17";

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

class TestLoFiDecider : public LoFiDecider {
 public:
  TestLoFiDecider()
      : should_request_lofi_resource_(false),
        ignore_is_using_data_reduction_proxy_check_(false) {}
  ~TestLoFiDecider() override {}

  bool IsUsingLoFiMode(const net::URLRequest& request) const override {
    return should_request_lofi_resource_;
  }

  void SetIsUsingLoFiMode(bool should_request_lofi_resource) {
    should_request_lofi_resource_ = should_request_lofi_resource;
  }

  void MaybeSetAcceptTransformHeader(
      const net::URLRequest& request,
      bool is_previews_disabled,
      net::HttpRequestHeaders* headers) const override {
    if (should_request_lofi_resource_) {
      headers->SetHeader(chrome_proxy_accept_transform_header(),
                         empty_image_directive());
    }
  }

  bool IsSlowPagePreviewRequested(
      const net::HttpRequestHeaders& headers) const override {
    std::string header_value;
    if (headers.GetHeader(chrome_proxy_accept_transform_header(),
                          &header_value)) {
      return header_value == empty_image_directive();
    }
    return false;
  }

  bool IsLitePagePreviewRequested(
      const net::HttpRequestHeaders& headers) const override {
    std::string header_value;
    if (headers.GetHeader(chrome_proxy_accept_transform_header(),
                          &header_value)) {
      return header_value == lite_page_directive();
    }
    return false;
  }

  void RemoveAcceptTransformHeader(
      net::HttpRequestHeaders* headers) const override {
    if (ignore_is_using_data_reduction_proxy_check_)
      return;
    headers->RemoveHeader(chrome_proxy_accept_transform_header());
  }

  void MaybeSetIgnorePreviewsBlacklistDirective(
      net::HttpRequestHeaders* headers) const override {}

  bool ShouldRecordLoFiUMA(const net::URLRequest& request) const override {
    return should_request_lofi_resource_;
  }

  void ignore_is_using_data_reduction_proxy_check() {
    ignore_is_using_data_reduction_proxy_check_ = true;
  }

 private:
  bool should_request_lofi_resource_;
  bool ignore_is_using_data_reduction_proxy_check_;
};

class TestLoFiUIService : public LoFiUIService {
 public:
  TestLoFiUIService() : on_lofi_response_(false) {}
  ~TestLoFiUIService() override {}

  bool DidNotifyLoFiResponse() const { return on_lofi_response_; }

  void OnLoFiReponseReceived(const net::URLRequest& request) override {
    on_lofi_response_ = true;
  }

 private:
  bool on_lofi_response_;
};

class DataReductionProxyNetworkDelegateTest : public testing::Test {
 public:
  DataReductionProxyNetworkDelegateTest()
      : context_(true),
        context_storage_(&context_),
        test_context_(DataReductionProxyTestContext::Builder()
                          .WithClient(kClient)
                          .WithMockClientSocketFactory(&mock_socket_factory_)
                          .WithURLRequestContext(&context_)
                          .Build()) {
    context_.set_client_socket_factory(&mock_socket_factory_);
    test_context_->AttachToURLRequestContext(&context_storage_);

    std::unique_ptr<TestLoFiDecider> lofi_decider(new TestLoFiDecider());
    lofi_decider_ = lofi_decider.get();
    test_context_->io_data()->set_lofi_decider(std::move(lofi_decider));

    std::unique_ptr<TestLoFiUIService> lofi_ui_service(new TestLoFiUIService());
    lofi_ui_service_ = lofi_ui_service.get();
    test_context_->io_data()->set_lofi_ui_service(std::move(lofi_ui_service));

    context_.Init();

    test_context_->EnableDataReductionProxyWithSecureProxyCheckSuccess();
  }

  static void VerifyHeaders(bool expected_data_reduction_proxy_used,
                            bool expected_lofi_used,
                            const net::HttpRequestHeaders& headers) {
    EXPECT_EQ(expected_data_reduction_proxy_used,
              headers.HasHeader(chrome_proxy_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    EXPECT_EQ(expected_data_reduction_proxy_used && expected_lofi_used,
              header_value.find("empty-image") != std::string::npos);
  }

  void VerifyWasLoFiModeActiveOnMainFrame(bool expected_value) {
    test_context_->RunUntilIdle();
    EXPECT_EQ(expected_value,
              test_context_->settings()->WasLoFiModeActiveOnMainFrame());
  }

  void VerifyDidNotifyLoFiResponse(bool lofi_response) const {
    EXPECT_EQ(lofi_response, lofi_ui_service_->DidNotifyLoFiResponse());
  }

  void VerifyDataReductionProxyData(const net::URLRequest& request,
                                    bool data_reduction_proxy_used,
                                    bool lofi_used) {
    DataReductionProxyData* data = DataReductionProxyData::GetData(request);
    if (!data_reduction_proxy_used) {
      EXPECT_FALSE(data);
    } else {
      EXPECT_TRUE(data->used_data_reduction_proxy());
      EXPECT_EQ(lofi_used, data->lofi_requested());
    }
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

  void DelegateStageDone(int result) {}

  void NotifyNetworkDelegate(net::URLRequest* request,
                             const net::ProxyInfo& data_reduction_proxy_info,
                             const net::ProxyRetryInfoMap& proxy_retry_info,
                             net::HttpRequestHeaders* headers) {
    network_delegate()->NotifyBeforeURLRequest(
        request,
        base::Bind(&DataReductionProxyNetworkDelegateTest::DelegateStageDone,
                   base::Unretained(this)),
        nullptr);
    network_delegate()->NotifyBeforeStartTransaction(
        request,
        base::Bind(&DataReductionProxyNetworkDelegateTest::DelegateStageDone,
                   base::Unretained(this)),
        headers);
    network_delegate()->NotifyBeforeSendHeaders(
        request, data_reduction_proxy_info, proxy_retry_info, headers);
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

  net::NetworkDelegate* network_delegate() const {
    return context_.network_delegate();
  }

  TestDataReductionProxyParams* params() const {
    return test_context_->config()->test_params();
  }

  TestDataReductionProxyConfig* config() const {
    return test_context_->config();
  }

  TestDataReductionProxyIOData* io_data() const {
    return test_context_->io_data();
  }

  TestLoFiDecider* lofi_decider() const { return lofi_decider_; }

 private:
  int64_t GetSessionNetworkStatsInfoInt64(const char* key) const {
    const DataReductionProxyNetworkDelegate* drp_network_delegate =
        reinterpret_cast<const DataReductionProxyNetworkDelegate*>(
            context_.network_delegate());

    std::unique_ptr<base::DictionaryValue> session_network_stats_info =
        base::DictionaryValue::From(
            drp_network_delegate->SessionNetworkStatsInfoToValue());
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

  TestLoFiDecider* lofi_decider_;
  TestLoFiUIService* lofi_ui_service_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyNetworkDelegateTest, AuthenticationTest) {
  std::unique_ptr<net::URLRequest> fake_request(FetchURLRequest(
      GURL("http://www.google.com/"), nullptr, std::string(), 0));

  net::ProxyInfo data_reduction_proxy_info;
  net::ProxyRetryInfoMap proxy_retry_info;
  std::string data_reduction_proxy;
  base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
  data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);

  net::HttpRequestHeaders headers;
  network_delegate()->NotifyBeforeSendHeaders(fake_request.get(),
                                              data_reduction_proxy_info,
                                              proxy_retry_info, &headers);

  EXPECT_TRUE(headers.HasHeader(chrome_proxy_header()));
  std::string header_value;
  headers.GetHeader(chrome_proxy_header(), &header_value);
  EXPECT_TRUE(header_value.find("ps=") != std::string::npos);
  EXPECT_TRUE(header_value.find("sid=") != std::string::npos);
}

TEST_F(DataReductionProxyNetworkDelegateTest, LoFiTransitions) {
  // Enable Lo-Fi.
  const struct {
    bool lofi_switch_enabled;
    bool auto_lofi_enabled;
    bool is_data_reduction_proxy;
  } tests[] = {
      {
          // Lo-Fi enabled through switch and not using a Data Reduction Proxy.
          true, false, false,
      },
      {
          // Lo-Fi enabled through switch and using a Data Reduction Proxy.
          true, false, true,
      },
      {
          // Lo-Fi enabled through field trial and not using a Data Reduction
          // Proxy.
          false, true, false,
      },
      {
          // Lo-Fi enabled through field trial and using a Data Reduction Proxy.
          false, true, true,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    if (tests[i].lofi_switch_enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    }
    base::FieldTrialList field_trial_list(nullptr);
    if (tests[i].auto_lofi_enabled) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Enabled");
    }
    config()->SetNetworkProhibitivelySlow(tests[i].auto_lofi_enabled);
    io_data()->SetLoFiModeActiveOnMainFrame(false);

    net::ProxyInfo data_reduction_proxy_info;
    std::string proxy;
    if (tests[i].is_data_reduction_proxy)
      base::TrimString(params()->DefaultOrigin(), "/", &proxy);
    else
      base::TrimString(kOtherProxy, "/", &proxy);
    data_reduction_proxy_info.UseNamedProxy(proxy);

    {
      // Main frame loaded. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;

      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL("http://www.google.com/"), net::IDLE, &delegate);
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
      lofi_decider()->SetIsUsingLoFiMode(
          config()->ShouldEnableLoFiMode(*fake_request.get()));
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);

      VerifyHeaders(tests[i].is_data_reduction_proxy, true, headers);
      VerifyWasLoFiModeActiveOnMainFrame(tests[i].is_data_reduction_proxy);
      VerifyDataReductionProxyData(
          *fake_request, tests[i].is_data_reduction_proxy,
          config()->ShouldEnableLoFiMode(*fake_request.get()));
    }

    {
      // Lo-Fi is already off. Lo-Fi should not be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL("http://www.google.com/"), net::IDLE, &delegate);
      lofi_decider()->SetIsUsingLoFiMode(false);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(tests[i].is_data_reduction_proxy, false, headers);
      // Not a mainframe request, WasLoFiModeActiveOnMainFrame should still be
      // true if the proxy is a Data Reduction Proxy.
      VerifyWasLoFiModeActiveOnMainFrame(tests[i].is_data_reduction_proxy);
      VerifyDataReductionProxyData(*fake_request,
                                   tests[i].is_data_reduction_proxy, false);
    }

    {
      // Lo-Fi is already on. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL("http://www.google.com/"), net::IDLE, &delegate);

      lofi_decider()->SetIsUsingLoFiMode(true);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(tests[i].is_data_reduction_proxy, true, headers);
      // Not a mainframe request, WasLoFiModeActiveOnMainFrame should still be
      // true if the proxy is a Data Reduction Proxy.
      VerifyWasLoFiModeActiveOnMainFrame(tests[i].is_data_reduction_proxy);
      VerifyDataReductionProxyData(*fake_request,
                                   tests[i].is_data_reduction_proxy, true);
    }

    {
      // Main frame request with Lo-Fi off. Lo-Fi should not be used.
      // State of Lo-Fi should persist until next page load.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL("http://www.google.com/"), net::IDLE, &delegate);
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
      lofi_decider()->SetIsUsingLoFiMode(false);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(tests[i].is_data_reduction_proxy, false, headers);
      VerifyWasLoFiModeActiveOnMainFrame(false);
      VerifyDataReductionProxyData(*fake_request,
                                   tests[i].is_data_reduction_proxy, false);
    }

    {
      // Lo-Fi is off. Lo-Fi is still not used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL("http://www.google.com/"), net::IDLE, &delegate);
      lofi_decider()->SetIsUsingLoFiMode(false);
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(tests[i].is_data_reduction_proxy, false, headers);
      // Not a mainframe request, WasLoFiModeActiveOnMainFrame should still be
      // false.
      VerifyWasLoFiModeActiveOnMainFrame(false);
      VerifyDataReductionProxyData(*fake_request,
                                   tests[i].is_data_reduction_proxy, false);
    }

    {
      // Main frame request. Lo-Fi should be used.
      net::HttpRequestHeaders headers;
      net::ProxyRetryInfoMap proxy_retry_info;
      net::TestDelegate delegate;
      std::unique_ptr<net::URLRequest> fake_request = context()->CreateRequest(
          GURL("http://www.google.com/"), net::IDLE, &delegate);
      fake_request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
      lofi_decider()->SetIsUsingLoFiMode(
          config()->ShouldEnableLoFiMode(*fake_request.get()));
      NotifyNetworkDelegate(fake_request.get(), data_reduction_proxy_info,
                            proxy_retry_info, &headers);
      VerifyHeaders(tests[i].is_data_reduction_proxy, true, headers);
      VerifyWasLoFiModeActiveOnMainFrame(tests[i].is_data_reduction_proxy);
      VerifyDataReductionProxyData(
          *fake_request, tests[i].is_data_reduction_proxy,
          config()->ShouldEnableLoFiMode(*fake_request.get()));
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest, RequestDataConfigurations) {
  const struct {
    bool lofi_on;
    bool used_data_reduction_proxy;
    bool main_frame;
  } tests[] = {
      // Lo-Fi off. Main Frame Request.
      {false, true, true},
      // Data reduction proxy not used. Main Frame Request.
      {false, false, true},
      // Data reduction proxy not used, Lo-Fi should not be used. Main Frame
      // Request.
      {true, false, true},
      // Lo-Fi on. Main Frame Request.
      {true, true, true},
      // Lo-Fi off. Not a Main Frame Request.
      {false, true, false},
      // Data reduction proxy not used. Not a Main Frame Request.
      {false, false, false},
      // Data reduction proxy not used, Lo-Fi should not be used. Not a Main
      // Frame Request.
      {true, false, false},
      // Lo-Fi on. Not a Main Frame Request.
      {true, true, false},
  };

  for (const auto& test : tests) {
    net::ProxyInfo data_reduction_proxy_info;
    std::string data_reduction_proxy;
    base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
    if (test.used_data_reduction_proxy)
      data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);
    else
      data_reduction_proxy_info.UseNamedProxy("port.of.other.proxy");
    // Main frame loaded. Lo-Fi should be used.
    net::HttpRequestHeaders headers;
    net::ProxyRetryInfoMap proxy_retry_info;

    std::map<std::string, std::string> network_quality_estimator_params;
    net::TestNetworkQualityEstimator test_network_quality_estimator(
        network_quality_estimator_params);
    test_network_quality_estimator.set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
    context()->set_network_quality_estimator(&test_network_quality_estimator);

    std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
        GURL("http://www.google.com/"), net::RequestPriority::IDLE, nullptr);
    request->SetLoadFlags(test.main_frame ? net::LOAD_MAIN_FRAME_DEPRECATED
                                          : 0);
    lofi_decider()->SetIsUsingLoFiMode(test.lofi_on);
    io_data()->request_options()->SetSecureSession("fake-session");
    network_delegate()->NotifyBeforeSendHeaders(
        request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
    DataReductionProxyData* data =
        DataReductionProxyData::GetData(*request.get());
    if (!test.used_data_reduction_proxy) {
      EXPECT_FALSE(data);
    } else {
      EXPECT_TRUE(data);
      EXPECT_EQ(test.main_frame ? net::EFFECTIVE_CONNECTION_TYPE_OFFLINE
                                : net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
                data->effective_connection_type());
      EXPECT_TRUE(data->used_data_reduction_proxy());
      EXPECT_EQ(GURL("http://www.google.com/"), data->request_url());
      EXPECT_EQ("fake-session", data->session_key());
      EXPECT_EQ(test.lofi_on, data->lofi_requested());
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       RequestDataHoldbackConfigurations) {
  const struct {
    bool data_reduction_proxy_enabled;
    bool used_direct;
  } tests[] = {
      {
          false, true,
      },
      {
          false, false,
      },
      {
          true, false,
      },
      {
          true, true,
      },
  };
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));
  for (const auto& test : tests) {
    net::ProxyInfo data_reduction_proxy_info;
    if (test.used_direct)
      data_reduction_proxy_info.UseDirect();
    else
      data_reduction_proxy_info.UseNamedProxy("some.other.proxy");
    config()->SetStateForTest(test.data_reduction_proxy_enabled, true);
    std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
        GURL("http://www.google.com/"), net::RequestPriority::IDLE, nullptr);
    request->set_method("GET");
    net::HttpRequestHeaders headers;
    net::ProxyRetryInfoMap proxy_retry_info;
    network_delegate()->NotifyBeforeSendHeaders(
        request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
    DataReductionProxyData* data =
        DataReductionProxyData::GetData(*request.get());
    if (!test.data_reduction_proxy_enabled || !test.used_direct) {
      EXPECT_FALSE(data);
    } else {
      EXPECT_TRUE(data);
      EXPECT_TRUE(data->used_data_reduction_proxy());
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest, RedirectRequestDataCleared) {
  net::ProxyInfo data_reduction_proxy_info;
  std::string data_reduction_proxy;
  base::TrimString(params()->DefaultOrigin(), "/", &data_reduction_proxy);
  data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);

  // Main frame loaded. Lo-Fi should be used.
  net::HttpRequestHeaders headers;
  net::ProxyRetryInfoMap proxy_retry_info;

  std::map<std::string, std::string> network_quality_estimator_params;
  net::TestNetworkQualityEstimator test_network_quality_estimator(
      network_quality_estimator_params);
  test_network_quality_estimator.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
  context()->set_network_quality_estimator(&test_network_quality_estimator);

  std::unique_ptr<net::URLRequest> request = context()->CreateRequest(
      GURL("http://www.google.com/"), net::RequestPriority::IDLE, nullptr);
  request->SetLoadFlags(net::LOAD_MAIN_FRAME_DEPRECATED);
  lofi_decider()->SetIsUsingLoFiMode(true);
  io_data()->request_options()->SetSecureSession("fake-session");
  network_delegate()->NotifyBeforeSendHeaders(
      request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
  DataReductionProxyData* data =
      DataReductionProxyData::GetData(*request.get());

  EXPECT_TRUE(data);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
            data->effective_connection_type());
  EXPECT_TRUE(data->used_data_reduction_proxy());
  EXPECT_EQ(GURL("http://www.google.com/"), data->request_url());
  EXPECT_EQ("fake-session", data->session_key());
  EXPECT_TRUE(data->lofi_requested());

  data_reduction_proxy_info.UseNamedProxy("port.of.other.proxy");

  // Simulate a redirect by calling NotifyBeforeSendHeaders again with different
  // proxy info.
  network_delegate()->NotifyBeforeSendHeaders(
      request.get(), data_reduction_proxy_info, proxy_retry_info, &headers);
  data = DataReductionProxyData::GetData(*request.get());
  EXPECT_FALSE(data);
}

TEST_F(DataReductionProxyNetworkDelegateTest, NetHistograms) {
  const std::string kReceivedValidOCLHistogramName =
      "Net.HttpContentLengthWithValidOCL";
  const std::string kOriginalValidOCLHistogramName =
      "Net.HttpOriginalContentLengthWithValidOCL";
  const std::string kDifferenceValidOCLHistogramName =
      "Net.HttpContentLengthDifferenceWithValidOCL";

  // Lo-Fi histograms.
  const std::string kReceivedValidOCLLoFiOnHistogramName =
      "Net.HttpContentLengthWithValidOCL.LoFiOn";
  const std::string kOriginalValidOCLLoFiOnHistogramName =
      "Net.HttpOriginalContentLengthWithValidOCL.LoFiOn";
  const std::string kDifferenceValidOCLLoFiOnHistogramName =
      "Net.HttpContentLengthDifferenceWithValidOCL.LoFiOn";

  const std::string kReceivedHistogramName = "Net.HttpContentLength";
  const std::string kOriginalHistogramName = "Net.HttpOriginalContentLength";
  const std::string kDifferenceHistogramName =
      "Net.HttpContentLengthDifference";
  const std::string kFreshnessLifetimeHistogramName =
      "Net.HttpContentFreshnessLifetime";
  const std::string kCacheableHistogramName = "Net.HttpContentLengthCacheable";
  const std::string kCacheable4HoursHistogramName =
      "Net.HttpContentLengthCacheable4Hours";
  const std::string kCacheable24HoursHistogramName =
      "Net.HttpContentLengthCacheable24Hours";
  const int64_t kResponseContentLength = 100;
  const int64_t kOriginalContentLength = 200;

  base::HistogramTester histogram_tester;

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "x-original-content-length: " +
      base::Int64ToString(kOriginalContentLength) + "\r\n\r\n";

  std::unique_ptr<net::URLRequest> fake_request(
      FetchURLRequest(GURL("http://www.google.com/"), nullptr, response_headers,
                      kResponseContentLength));
  fake_request->SetLoadFlags(fake_request->load_flags() |
                             net::LOAD_MAIN_FRAME_DEPRECATED);

  base::TimeDelta freshness_lifetime =
      fake_request->response_info().headers->GetFreshnessLifetimes(
          fake_request->response_info().response_time).freshness;

  histogram_tester.ExpectUniqueSample(kReceivedValidOCLHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kOriginalValidOCLHistogramName,
                                      kOriginalContentLength, 1);
  histogram_tester.ExpectUniqueSample(
      kDifferenceValidOCLHistogramName,
      kOriginalContentLength - kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kReceivedHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kOriginalHistogramName,
                                      kOriginalContentLength, 1);
  histogram_tester.ExpectUniqueSample(
      kDifferenceHistogramName,
      kOriginalContentLength - kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kFreshnessLifetimeHistogramName,
                                      freshness_lifetime.InSeconds(), 1);
  histogram_tester.ExpectUniqueSample(kCacheableHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kCacheable4HoursHistogramName,
                                      kResponseContentLength, 1);
  histogram_tester.ExpectUniqueSample(kCacheable24HoursHistogramName,
                                      kResponseContentLength, 1);

  // Check Lo-Fi histograms.
  const struct {
    bool lofi_enabled_through_switch;
    bool auto_lofi_enabled;
    int expected_count;
  } tests[] = {
      {
          // Lo-Fi disabled.
          false, false, 0,
      },
      {
          // Auto Lo-Fi enabled.
          // This should populate Lo-Fi content length histogram.
          false, true, 1,
      },
      {
          // Lo-Fi enabled through switch.
          // This should populate Lo-Fi content length histogram.
          true, false, 1,
      },
      {
          // Lo-Fi enabled through switch and Auto Lo-Fi also enabled.
          // This should populate Lo-Fi content length histogram.
          true, true, 1,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    config()->ResetLoFiStatusForTest();
    config()->SetNetworkProhibitivelySlow(tests[i].auto_lofi_enabled);
    base::FieldTrialList field_trial_list(nullptr);
    if (tests[i].auto_lofi_enabled) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Enabled");
    }

    if (tests[i].lofi_enabled_through_switch) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    }

    lofi_decider()->SetIsUsingLoFiMode(
        config()->ShouldEnableLoFiMode(*fake_request.get()));

    fake_request = (FetchURLRequest(GURL("http://www.example.com/"), nullptr,
                                    response_headers, kResponseContentLength));
    fake_request->SetLoadFlags(fake_request->load_flags() |
                               net::LOAD_MAIN_FRAME_DEPRECATED);

    // Histograms are accumulative, so get the sum of all the tests so far.
    int expected_count = 0;
    for (size_t j = 0; j <= i; ++j)
      expected_count += tests[j].expected_count;

    if (expected_count == 0) {
      histogram_tester.ExpectTotalCount(kReceivedValidOCLLoFiOnHistogramName,
                                        expected_count);
      histogram_tester.ExpectTotalCount(kOriginalValidOCLLoFiOnHistogramName,
                                        expected_count);
      histogram_tester.ExpectTotalCount(kDifferenceValidOCLLoFiOnHistogramName,
                                        expected_count);
    } else {
      histogram_tester.ExpectUniqueSample(kReceivedValidOCLLoFiOnHistogramName,
                                          kResponseContentLength,
                                          expected_count);
      histogram_tester.ExpectUniqueSample(kOriginalValidOCLLoFiOnHistogramName,
                                          kOriginalContentLength,
                                          expected_count);
      histogram_tester.ExpectUniqueSample(
          kDifferenceValidOCLLoFiOnHistogramName,
          kOriginalContentLength - kResponseContentLength, expected_count);
    }
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest, OnCompletedInternalLoFi) {
  // Enable Lo-Fi.
  const struct {
    bool lofi_response;
  } tests[] = {
      {false}, {true},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string response_headers =
        "HTTP/1.1 200 OK\r\n"
        "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
        "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
        "Via: 1.1 Chrome-Compression-Proxy\r\n"
        "x-original-content-length: 200\r\n";

    if (tests[i].lofi_response)
      response_headers += "Chrome-Proxy-Content-Transform: empty-image\r\n";

    response_headers += "\r\n";
    FetchURLRequest(GURL("http://www.google.com/"), nullptr, response_headers,
                    140);

    VerifyDidNotifyLoFiResponse(tests[i].lofi_response);
  }
}

TEST_F(DataReductionProxyNetworkDelegateTest,
       TestLoFiTransformationTypeHistogram) {
  const char kLoFiTransformationTypeHistogram[] =
      "DataReductionProxy.LoFi.TransformationType";
  base::HistogramTester histogram_tester;

  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader("chrome-proxy-accept-transform", "lite-page");
  lofi_decider()->ignore_is_using_data_reduction_proxy_check();
  FetchURLRequest(GURL("http://www.google.com/"), &request_headers,
                  std::string(), 140);
  histogram_tester.ExpectBucketCount(kLoFiTransformationTypeHistogram,
                                     NO_TRANSFORMATION_LITE_PAGE_REQUESTED, 1);

  std::string response_headers =
      "HTTP/1.1 200 OK\r\n"
      "Chrome-Proxy-Content-Transform: lite-page\r\n"
      "Date: Wed, 28 Nov 2007 09:40:09 GMT\r\n"
      "Expires: Mon, 24 Nov 2014 12:45:26 GMT\r\n"
      "Via: 1.1 Chrome-Compression-Proxy\r\n"
      "x-original-content-length: 200\r\n";

  response_headers += "\r\n";
  FetchURLRequest(GURL("http://www.google.com/"), nullptr, response_headers,
                  140);

  histogram_tester.ExpectBucketCount(kLoFiTransformationTypeHistogram,
                                     LITE_PAGE, 1);
}

}  // namespace

}  // namespace data_reduction_proxy
