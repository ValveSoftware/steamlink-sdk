// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/load_flags.h"
#include "net/base/network_delegate_impl.h"
#include "net/http/http_request_headers.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

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

}  // namespace

class ContentLoFiDeciderTest : public testing::Test {
 public:
  ContentLoFiDeciderTest() : context_(true) {
    context_.set_client_socket_factory(&mock_socket_factory_);
    context_.Init();

    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithClient(kClient)
                        .WithMockClientSocketFactory(&mock_socket_factory_)
                        .WithURLRequestContext(&context_)
                        .Build();

    data_reduction_proxy_network_delegate_.reset(
        new DataReductionProxyNetworkDelegate(
            std::unique_ptr<net::NetworkDelegate>(
                new net::NetworkDelegateImpl()),
            test_context_->config(),
            test_context_->io_data()->request_options(),
            test_context_->configurator()));

    data_reduction_proxy_network_delegate_->InitIODataAndUMA(
        test_context_->io_data(), test_context_->io_data()->bypass_stats());

    context_.set_network_delegate(data_reduction_proxy_network_delegate_.get());

    std::unique_ptr<data_reduction_proxy::ContentLoFiDecider>
        data_reduction_proxy_lofi_decider(
            new data_reduction_proxy::ContentLoFiDecider());
    test_context_->io_data()->set_lofi_decider(
        std::move(data_reduction_proxy_lofi_decider));
  }

  std::unique_ptr<net::URLRequest> CreateRequest(bool is_using_lofi) {
    std::unique_ptr<net::URLRequest> request = context_.CreateRequest(
        GURL("http://www.google.com/"), net::IDLE, &delegate_);

    content::ResourceRequestInfo::AllocateForTesting(
        request.get(), content::RESOURCE_TYPE_SUB_FRAME, NULL, -1, -1, -1,
        false,  // is_main_frame
        false,  // parent_is_main_frame
        false,  // allow_download
        false,  // is_async
        is_using_lofi);

    return request;
  }

  void NotifyBeforeSendHeaders(net::HttpRequestHeaders* headers,
                               net::URLRequest* request,
                               bool use_data_reduction_proxy) {
    net::ProxyInfo data_reduction_proxy_info;
    net::ProxyRetryInfoMap proxy_retry_info;

    if (use_data_reduction_proxy) {
      std::string data_reduction_proxy;
      base::TrimString(test_context_->config()->test_params()->DefaultOrigin(),
                       "/", &data_reduction_proxy);
      data_reduction_proxy_info.UseNamedProxy(data_reduction_proxy);
    } else {
      data_reduction_proxy_info.UseNamedProxy("proxy.com");
    }

    data_reduction_proxy_network_delegate_->NotifyBeforeSendHeaders(
        request, data_reduction_proxy_info, proxy_retry_info, headers);
  }

  static void VerifyLoFiHeader(bool expected_lofi_used,
                               const net::HttpRequestHeaders& headers) {
    EXPECT_TRUE(headers.HasHeader(chrome_proxy_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_header(), &header_value);
    EXPECT_EQ(
        expected_lofi_used,
        header_value.find(chrome_proxy_lo_fi_directive()) != std::string::npos);
  }

  static void VerifyLoFiPreviewHeader(bool expected_lofi_preview_used,
                                      const net::HttpRequestHeaders& headers) {
    EXPECT_TRUE(headers.HasHeader(chrome_proxy_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_header(), &header_value);
    EXPECT_EQ(expected_lofi_preview_used,
              header_value.find(chrome_proxy_lo_fi_preview_directive()) !=
                  std::string::npos);
  }

  static void VerifyLoFiIgnorePreviewBlacklistHeader(
      bool expected_lofi_preview_used,
      const net::HttpRequestHeaders& headers) {
    EXPECT_TRUE(headers.HasHeader(chrome_proxy_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_header(), &header_value);
    EXPECT_EQ(expected_lofi_preview_used,
              header_value.find(
                  chrome_proxy_lo_fi_ignore_preview_blacklist_directive()) !=
                  std::string::npos);
  }

 protected:
  base::MessageLoopForIO message_loop_;
  net::MockClientSocketFactory mock_socket_factory_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  std::unique_ptr<DataReductionProxyNetworkDelegate>
      data_reduction_proxy_network_delegate_;
};

TEST_F(ContentLoFiDeciderTest, LoFiFlags) {
  // Enable Lo-Fi.
  const struct {
    bool is_using_lofi;
    bool is_using_previews;
    bool is_main_frame;
  } tests[] = {
      {false, false, false}, {false, false, true}, {true, false, true},
      {true, false, false},  {false, true, false}, {false, true, true},
      {true, true, true},    {true, true, false},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<net::URLRequest> request =
        CreateRequest(tests[i].is_using_lofi);
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->InitFromArgv(command_line->argv());
    if (tests[i].is_using_previews) {
      command_line->AppendSwitch(
          switches::kEnableDataReductionProxyLoFiPreview);
    }

    // No flags or field trials. The Lo-Fi header should not be added.
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(false, headers);
    VerifyLoFiPreviewHeader(false, headers);
    VerifyLoFiIgnorePreviewBlacklistHeader(false, headers);

    // The Lo-Fi flag is "always-on", Lo-Fi is being used, and it's a main frame
    // request. Lo-Fi preview header should be added.
    command_line->AppendSwitchASCII(
        switches::kDataReductionProxyLoFi,
        switches::kDataReductionProxyLoFiValueAlwaysOn);
    if (tests[i].is_main_frame)
      request->SetLoadFlags(request->load_flags() | net::LOAD_MAIN_FRAME);
    headers.Clear();
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(tests[i].is_using_lofi && !tests[i].is_using_previews &&
                         !tests[i].is_main_frame,
                     headers);
    VerifyLoFiPreviewHeader(tests[i].is_using_lofi &&
                                tests[i].is_using_previews &&
                                tests[i].is_main_frame,
                            headers);
    VerifyLoFiIgnorePreviewBlacklistHeader(tests[i].is_using_lofi &&
                                               tests[i].is_using_previews &&
                                               tests[i].is_main_frame,
                                           headers);
    DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
    // |lofi_requested| should be set to false when Lo-Fi is enabled using
    // flags.
    EXPECT_FALSE(data->lofi_requested());

    // The Lo-Fi flag is "always-on" and Lo-Fi is being used. Lo-Fi header
    // should be added.
    request->SetLoadFlags(0);
    headers.Clear();
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(tests[i].is_using_lofi && !tests[i].is_using_previews,
                     headers);
    VerifyLoFiPreviewHeader(false, headers);
    VerifyLoFiIgnorePreviewBlacklistHeader(false, headers);

    // The Lo-Fi flag is "cellular-only" and Lo-Fi is being used. Lo-Fi header
    // should be added.
    command_line->AppendSwitchASCII(
        switches::kDataReductionProxyLoFi,
        switches::kDataReductionProxyLoFiValueCellularOnly);
    headers.Clear();
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(tests[i].is_using_lofi && !tests[i].is_using_previews,
                     headers);
    VerifyLoFiPreviewHeader(false, headers);
    VerifyLoFiIgnorePreviewBlacklistHeader(false, headers);
    data = DataReductionProxyData::GetData(*request);
    // |lofi_requested| should be set to false when Lo-Fi is enabled using
    // flags.
    EXPECT_FALSE(data->lofi_requested());

    // The Lo-Fi flag is "slow-connections-only" and Lo-Fi is being used. Lo-Fi
    // header should be added.
    command_line->AppendSwitchASCII(
        switches::kDataReductionProxyLoFi,
        switches::kDataReductionProxyLoFiValueSlowConnectionsOnly);
    headers.Clear();
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(tests[i].is_using_lofi && !tests[i].is_using_previews,
                     headers);
    VerifyLoFiPreviewHeader(false, headers);
    VerifyLoFiIgnorePreviewBlacklistHeader(false, headers);
    data = DataReductionProxyData::GetData(*request);
    // |lofi_requested| should be set to false when Lo-Fi is enabled using
    // flags.
    EXPECT_FALSE(data->lofi_requested());
  }
}

TEST_F(ContentLoFiDeciderTest, LoFiEnabledFieldTrial) {
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                         "Enabled");
  // Enable Lo-Fi.
  const struct {
    bool is_using_lofi;
    bool is_main_frame;
  } tests[] = {{false, false}, {false, true}, {true, false}, {true, true}};

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<net::URLRequest> request =
        CreateRequest(tests[i].is_using_lofi);
    if (tests[i].is_main_frame)
      request->SetLoadFlags(request->load_flags() | net::LOAD_MAIN_FRAME);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(tests[i].is_using_lofi && !tests[i].is_main_frame,
                     headers);
    VerifyLoFiPreviewHeader(false, headers);
    VerifyLoFiIgnorePreviewBlacklistHeader(false, headers);
    DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
    EXPECT_EQ(tests[i].is_using_lofi, data->lofi_requested()) << i;
  }
}

TEST_F(ContentLoFiDeciderTest, LoFiControlFieldTrial) {
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                         "Control");
  // Enable Lo-Fi.
  const struct {
    bool is_using_lofi;
    bool is_main_frame;
  } tests[] = {{false, false}, {false, true}, {true, false}, {true, true}};

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<net::URLRequest> request =
        CreateRequest(tests[i].is_using_lofi);
    if (tests[i].is_main_frame)
      request->SetLoadFlags(request->load_flags() | net::LOAD_MAIN_FRAME);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(false, headers);
    VerifyLoFiPreviewHeader(false, headers);
    VerifyLoFiIgnorePreviewBlacklistHeader(false, headers);
    DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
    EXPECT_EQ(tests[i].is_using_lofi, data->lofi_requested()) << i;
  }
}

TEST_F(ContentLoFiDeciderTest, LoFiPreviewFieldTrial) {
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                         "Enabled_Preview");
  // Enable Lo-Fi.
  const struct {
    bool is_using_lofi;
    bool is_main_frame;
  } tests[] = {
      {false, false}, {true, false}, {false, true}, {true, true},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<net::URLRequest> request =
        CreateRequest(tests[i].is_using_lofi);
    if (tests[i].is_main_frame)
      request->SetLoadFlags(request->load_flags() | net::LOAD_MAIN_FRAME);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(false, headers);
    VerifyLoFiPreviewHeader(tests[i].is_using_lofi && tests[i].is_main_frame,
                            headers);
    VerifyLoFiIgnorePreviewBlacklistHeader(false, headers);
    DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
    EXPECT_EQ(tests[i].is_using_lofi, data->lofi_requested()) << i;
  }
}

TEST_F(ContentLoFiDeciderTest, AutoLoFi) {
  const struct {
    bool auto_lofi_enabled_group;
    bool auto_lofi_control_group;
    bool network_prohibitively_slow;
    bool is_main_frame;
  } tests[] = {
      {false, false, false, false},
      {false, false, true, false},
      {true, false, false, false},
      {true, false, true, false},
      {true, false, true, true},
      {false, true, false, false},
      {false, true, true, false},
      // Repeat this test data to simulate user moving out of Lo-Fi control
      // experiment.
      {false, true, false, false},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    test_context_->config()->ResetLoFiStatusForTest();
    const bool expect_lofi_header = tests[i].auto_lofi_enabled_group &&
                                    tests[i].network_prohibitively_slow &&
                                    !tests[i].is_main_frame;

    base::FieldTrialList field_trial_list(nullptr);
    if (tests[i].auto_lofi_enabled_group) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Enabled");
    }

    if (tests[i].auto_lofi_control_group) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Control");
    }

    test_context_->config()->SetNetworkProhibitivelySlow(
        tests[i].network_prohibitively_slow);

    std::unique_ptr<net::URLRequest> request =
        CreateRequest(tests[i].network_prohibitively_slow);
    if (tests[i].is_main_frame)
      request->SetLoadFlags(request->load_flags() | net::LOAD_MAIN_FRAME);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);

    VerifyLoFiHeader(expect_lofi_header, headers);
  }
}

TEST_F(ContentLoFiDeciderTest, SlowConnectionsFlag) {
  const struct {
    bool slow_connections_flag_enabled;
    bool network_prohibitively_slow;
    bool auto_lofi_enabled_group;
    bool is_main_frame;
  } tests[] = {
      {false, false, false, false}, {false, true, false, false},
      {true, false, false, false},  {true, true, false, false},
      {true, true, false, true},    {false, false, true, false},
      {false, false, true, true},   {false, true, true, false},
      {true, false, true, false},   {true, true, true, false},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    test_context_->config()->ResetLoFiStatusForTest();
    // For the purpose of this test, Lo-Fi header is expected only if LoFi Slow
    // Connection Flag is enabled or session is part of Lo-Fi enabled field
    // trial. For both cases, an additional condition is that network must be
    // prohibitively slow.
    const bool expect_lofi_header = ((tests[i].slow_connections_flag_enabled &&
                                      tests[i].network_prohibitively_slow) ||
                                     (!tests[i].slow_connections_flag_enabled &&
                                      tests[i].auto_lofi_enabled_group &&
                                      tests[i].network_prohibitively_slow)) &&
                                    !tests[i].is_main_frame;

    std::string expected_header;

    if (tests[i].slow_connections_flag_enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueSlowConnectionsOnly);
    }

    base::FieldTrialList field_trial_list(nullptr);
    if (tests[i].auto_lofi_enabled_group) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Enabled");
    }

    test_context_->config()->SetNetworkProhibitivelySlow(
        tests[i].network_prohibitively_slow);

    std::unique_ptr<net::URLRequest> request =
        CreateRequest(tests[i].network_prohibitively_slow);
    if (tests[i].is_main_frame)
      request->SetLoadFlags(request->load_flags() | net::LOAD_MAIN_FRAME);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);

    VerifyLoFiHeader(expect_lofi_header, headers);
  }
}

TEST_F(ContentLoFiDeciderTest, ProxyIsNotDataReductionProxy) {
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                         "Enabled");
  // Enable Lo-Fi.
  const struct {
    bool is_using_lofi;
  } tests[] = {
      {false}, {true},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<net::URLRequest> request =
        CreateRequest(tests[i].is_using_lofi);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), false);
    std::string header_value;
    headers.GetHeader(chrome_proxy_header(), &header_value);
    EXPECT_EQ(std::string::npos,
              header_value.find(chrome_proxy_lo_fi_directive()));
  }
}

}  // namespace data_reduction_roxy
