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
#include "base/strings/stringprintf.h"
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

  void AllocateRequestInfoForTesting(net::URLRequest* request,
                                     content::ResourceType resource_type,
                                     bool is_using_lofi) {
    content::ResourceRequestInfo::AllocateForTesting(
        request, resource_type, NULL, -1, -1, -1,
        resource_type == content::RESOURCE_TYPE_MAIN_FRAME,
        false,  // parent_is_main_frame
        false,  // allow_download
        false,  // is_async
        is_using_lofi);
  }

  std::unique_ptr<net::URLRequest> CreateRequest(bool is_main_frame,
                                                 bool is_using_lofi) {
    std::unique_ptr<net::URLRequest> request = context_.CreateRequest(
        GURL("http://www.google.com/"), net::IDLE, &delegate_);
    AllocateRequestInfoForTesting(
        request.get(), (is_main_frame ? content::RESOURCE_TYPE_MAIN_FRAME
                                      : content::RESOURCE_TYPE_SUB_FRAME),
        is_using_lofi);
    return request;
  }

  std::unique_ptr<net::URLRequest> CreateRequestByType(
      content::ResourceType resource_type,
      bool scheme_is_https,
      bool is_using_lofi) {
    std::unique_ptr<net::URLRequest> request =
        context_.CreateRequest(GURL(scheme_is_https ? "https://www.google.com/"
                                                    : "http://www.google.com/"),
                               net::IDLE, &delegate_);
    AllocateRequestInfoForTesting(request.get(), resource_type, is_using_lofi);
    return request;
  }

  void DelegateStageDone(int result) {}

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

    data_reduction_proxy_network_delegate_->NotifyBeforeStartTransaction(
        request, base::Bind(&ContentLoFiDeciderTest::DelegateStageDone,
                            base::Unretained(this)),
        headers);
    data_reduction_proxy_network_delegate_->NotifyBeforeSendHeaders(
        request, data_reduction_proxy_info, proxy_retry_info, headers);
  }

  static void VerifyLitePageHeader(bool expected_lite_page_used,
                                   const net::HttpRequestHeaders& headers) {
    if (expected_lite_page_used)
      EXPECT_TRUE(headers.HasHeader(chrome_proxy_accept_transform_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    EXPECT_EQ(expected_lite_page_used,
              header_value.find(lite_page_directive()) != std::string::npos);
  }

  static void VerifyLoFiHeader(bool expected_lofi_used,
                               bool expected_if_heavy,
                               const net::HttpRequestHeaders& headers) {
    if (expected_lofi_used)
      EXPECT_TRUE(headers.HasHeader(chrome_proxy_accept_transform_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    if (expected_if_heavy) {
      std::string empty_image_if_heavy = base::StringPrintf(
          "%s;%s", empty_image_directive(), if_heavy_qualifier());
      EXPECT_EQ(expected_lofi_used, empty_image_if_heavy == header_value);
    } else {
      EXPECT_EQ(expected_lofi_used, header_value == empty_image_directive());
    }
  }

  static void VerifyLitePageHeader(bool expected_lofi_preview_used,
                                   bool expected_if_heavy,
                                   const net::HttpRequestHeaders& headers) {
    if (expected_lofi_preview_used)
      EXPECT_TRUE(headers.HasHeader(chrome_proxy_accept_transform_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    if (expected_if_heavy) {
      std::string lite_page_if_heavy = base::StringPrintf(
          "%s;%s", lite_page_directive(), if_heavy_qualifier());
      EXPECT_EQ(expected_lofi_preview_used, lite_page_if_heavy == header_value);
    } else {
      EXPECT_EQ(expected_lofi_preview_used,
                header_value == lite_page_directive());
    }
  }

  static void VerifyVideoHeader(bool expected_compressed_video_used,
                                const net::HttpRequestHeaders& headers) {
    EXPECT_EQ(expected_compressed_video_used,
              headers.HasHeader(chrome_proxy_accept_transform_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    EXPECT_EQ(
        expected_compressed_video_used,
        header_value.find(compressed_video_directive()) != std::string::npos);
  }

  static void VerifyLitePageIgnoreBlacklistHeader(
      bool expected_blacklist_directive_added,
      const net::HttpRequestHeaders& headers) {
    EXPECT_TRUE(headers.HasHeader(chrome_proxy_header()));
    std::string header_value;
    headers.GetHeader(chrome_proxy_header(), &header_value);
    EXPECT_EQ(expected_blacklist_directive_added,
              header_value.find(
                  chrome_proxy_lite_page_ignore_blacklist_directive()) !=
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
    bool is_using_lite_page;
    bool is_main_frame;
  } tests[] = {
      {false, false, false}, {false, false, true}, {true, false, true},
      {true, false, false},  {false, true, false}, {false, true, true},
      {true, true, true},    {true, true, false},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<net::URLRequest> request =
        CreateRequest(tests[i].is_main_frame, tests[i].is_using_lofi);
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->InitFromArgv(command_line->argv());
    if (tests[i].is_using_lite_page) {
      command_line->AppendSwitch(switches::kEnableDataReductionProxyLitePage);
    }

    // No flags or field trials. The Lo-Fi header should not be added.
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(false, false, headers);
    VerifyLitePageHeader(false, false, headers);
    VerifyLitePageIgnoreBlacklistHeader(false, headers);

    // The Lo-Fi flag is "always-on", Lo-Fi is being used, and it's a main frame
    // request. Lo-Fi or lite page header should be added.
    command_line->AppendSwitchASCII(
        switches::kDataReductionProxyLoFi,
        switches::kDataReductionProxyLoFiValueAlwaysOn);
    headers.Clear();
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(!tests[i].is_using_lite_page && !tests[i].is_main_frame,
                     !tests[i].is_using_lofi, headers);
    VerifyLitePageHeader(tests[i].is_using_lite_page && tests[i].is_main_frame,
                         !tests[i].is_using_lofi, headers);
    VerifyLitePageIgnoreBlacklistHeader(
        tests[i].is_using_lite_page && tests[i].is_main_frame, headers);
    DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
    // |lofi_requested| should be set to false when Lo-Fi is enabled using
    // flags.
    EXPECT_FALSE(data->lofi_requested());

    // The Lo-Fi flag is "always-on" and Lo-Fi is being used. Lo-Fi header
    // should be added.
    AllocateRequestInfoForTesting(request.get(),
                                  content::RESOURCE_TYPE_SUB_FRAME,
                                  tests[i].is_using_lofi);
    headers.Clear();
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(!tests[i].is_using_lite_page, !tests[i].is_using_lofi,
                     headers);
    VerifyLitePageHeader(false, false, headers);
    VerifyLitePageIgnoreBlacklistHeader(false, headers);

    // The Lo-Fi flag is "cellular-only" and Lo-Fi is being used. Lo-Fi header
    // should be added.
    command_line->AppendSwitchASCII(
        switches::kDataReductionProxyLoFi,
        switches::kDataReductionProxyLoFiValueCellularOnly);
    headers.Clear();
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(!tests[i].is_using_lite_page, !tests[i].is_using_lofi,
                     headers);
    VerifyLitePageHeader(false, false, headers);
    VerifyLitePageIgnoreBlacklistHeader(false, headers);
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
    VerifyLoFiHeader(!tests[i].is_using_lite_page, !tests[i].is_using_lofi,
                     headers);
    VerifyLitePageHeader(false, false, headers);
    VerifyLitePageIgnoreBlacklistHeader(false, headers);
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
    content::ResourceType resource_type;
  } tests[] = {{false, content::RESOURCE_TYPE_MAIN_FRAME},
               {false, content::RESOURCE_TYPE_SUB_FRAME},
               {false, content::RESOURCE_TYPE_STYLESHEET},
               {false, content::RESOURCE_TYPE_SCRIPT},
               {false, content::RESOURCE_TYPE_IMAGE},
               {false, content::RESOURCE_TYPE_FONT_RESOURCE},
               {false, content::RESOURCE_TYPE_SUB_RESOURCE},
               {false, content::RESOURCE_TYPE_OBJECT},
               {false, content::RESOURCE_TYPE_MEDIA},
               {false, content::RESOURCE_TYPE_WORKER},
               {false, content::RESOURCE_TYPE_SHARED_WORKER},
               {false, content::RESOURCE_TYPE_PREFETCH},
               {false, content::RESOURCE_TYPE_FAVICON},
               {false, content::RESOURCE_TYPE_XHR},
               {false, content::RESOURCE_TYPE_PING},
               {false, content::RESOURCE_TYPE_SERVICE_WORKER},
               {false, content::RESOURCE_TYPE_CSP_REPORT},
               {false, content::RESOURCE_TYPE_PLUGIN_RESOURCE},

               {true, content::RESOURCE_TYPE_MAIN_FRAME},
               {true, content::RESOURCE_TYPE_SUB_FRAME},
               {true, content::RESOURCE_TYPE_STYLESHEET},
               {true, content::RESOURCE_TYPE_SCRIPT},
               {true, content::RESOURCE_TYPE_IMAGE},
               {true, content::RESOURCE_TYPE_FONT_RESOURCE},
               {true, content::RESOURCE_TYPE_SUB_RESOURCE},
               {true, content::RESOURCE_TYPE_OBJECT},
               {true, content::RESOURCE_TYPE_MEDIA},
               {true, content::RESOURCE_TYPE_WORKER},
               {true, content::RESOURCE_TYPE_SHARED_WORKER},
               {true, content::RESOURCE_TYPE_PREFETCH},
               {true, content::RESOURCE_TYPE_FAVICON},
               {true, content::RESOURCE_TYPE_XHR},
               {true, content::RESOURCE_TYPE_PING},
               {true, content::RESOURCE_TYPE_SERVICE_WORKER},
               {true, content::RESOURCE_TYPE_CSP_REPORT},
               {true, content::RESOURCE_TYPE_PLUGIN_RESOURCE}};

  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::unique_ptr<net::URLRequest> request = CreateRequestByType(
        tests[i].resource_type, false, tests[i].is_using_lofi);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    bool is_lofi_resource_type =
        !(tests[i].resource_type == content::RESOURCE_TYPE_MAIN_FRAME ||
          tests[i].resource_type == content::RESOURCE_TYPE_STYLESHEET ||
          tests[i].resource_type == content::RESOURCE_TYPE_SCRIPT ||
          tests[i].resource_type == content::RESOURCE_TYPE_FONT_RESOURCE ||
          tests[i].resource_type == content::RESOURCE_TYPE_MEDIA ||
          tests[i].resource_type == content::RESOURCE_TYPE_CSP_REPORT);

    VerifyLoFiHeader(is_lofi_resource_type, !tests[i].is_using_lofi, headers);
    VerifyLitePageHeader(false, false, headers);
    VerifyLitePageIgnoreBlacklistHeader(false, headers);
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
        CreateRequest(tests[i].is_main_frame, tests[i].is_using_lofi);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(false, false, headers);
    VerifyLitePageHeader(false, false, headers);
    VerifyLitePageIgnoreBlacklistHeader(false, headers);
    DataReductionProxyData* data = DataReductionProxyData::GetData(*request);
    EXPECT_EQ(tests[i].is_using_lofi, data->lofi_requested()) << i;
  }
}

TEST_F(ContentLoFiDeciderTest, LitePageFieldTrial) {
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
        CreateRequest(tests[i].is_main_frame, tests[i].is_using_lofi);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);
    VerifyLoFiHeader(false, false, headers);
    VerifyLitePageHeader(tests[i].is_main_frame, !tests[i].is_using_lofi,
                         headers);
    VerifyLitePageIgnoreBlacklistHeader(false, headers);
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

    std::unique_ptr<net::URLRequest> request = CreateRequest(
        tests[i].is_main_frame, tests[i].network_prohibitively_slow);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);

    VerifyLoFiHeader(expect_lofi_header, !tests[i].network_prohibitively_slow,
                     headers);
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

    std::unique_ptr<net::URLRequest> request = CreateRequest(
        tests[i].is_main_frame, tests[i].network_prohibitively_slow);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), true);

    VerifyLoFiHeader(expect_lofi_header, false, headers);
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
        CreateRequest(false, tests[i].is_using_lofi);
    net::HttpRequestHeaders headers;
    NotifyBeforeSendHeaders(&headers, request.get(), false);
    std::string header_value;
    headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
    EXPECT_EQ(std::string::npos, header_value.find(empty_image_directive()));
  }
}

TEST_F(ContentLoFiDeciderTest, VideoDirectiveNotOverridden) {
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                         "Enabled");
  // Verify the directive gets added even when LoFi is triggered.
  test_context_->config()->SetNetworkProhibitivelySlow(true);
  std::unique_ptr<net::URLRequest> request =
      CreateRequestByType(content::RESOURCE_TYPE_MEDIA, false, true);
  net::HttpRequestHeaders headers;
  NotifyBeforeSendHeaders(&headers, request.get(), true);
  VerifyVideoHeader(true, headers);
}

TEST_F(ContentLoFiDeciderTest, VideoDirectiveNotAdded) {
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                         "Enabled");
  test_context_->config()->SetNetworkProhibitivelySlow(true);
  std::unique_ptr<net::URLRequest> request =
      CreateRequestByType(content::RESOURCE_TYPE_MEDIA, false, true);
  net::HttpRequestHeaders headers;
  // Verify the header isn't there when the data reduction proxy is disabled.
  NotifyBeforeSendHeaders(&headers, request.get(), false);
  VerifyVideoHeader(false, headers);
}

TEST_F(ContentLoFiDeciderTest, VideoDirectiveDoesNotOverride) {
  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                         "Enabled");
  // Verify the directive gets added even when LoFi is triggered.
  test_context_->config()->SetNetworkProhibitivelySlow(true);
  std::unique_ptr<net::URLRequest> request =
      CreateRequestByType(content::RESOURCE_TYPE_MEDIA, false, true);
  net::HttpRequestHeaders headers;
  headers.SetHeader(chrome_proxy_accept_transform_header(), "foo");
  NotifyBeforeSendHeaders(&headers, request.get(), true);
  std::string header_value;
  headers.GetHeader(chrome_proxy_accept_transform_header(), &header_value);
  EXPECT_EQ("foo", header_value);
}

TEST_F(ContentLoFiDeciderTest, IsSlowPagePreviewRequested) {
  std::unique_ptr<data_reduction_proxy::ContentLoFiDecider> lofi_decider(
      new data_reduction_proxy::ContentLoFiDecider());
  net::HttpRequestHeaders headers;
  EXPECT_FALSE(lofi_decider->IsSlowPagePreviewRequested(headers));
  headers.SetHeader(chrome_proxy_accept_transform_header(), "lite-page");
  EXPECT_TRUE(lofi_decider->IsSlowPagePreviewRequested(headers));
  headers.SetHeader(chrome_proxy_accept_transform_header(), "lite-page;foo");
  EXPECT_FALSE(lofi_decider->IsSlowPagePreviewRequested(headers));
  headers.SetHeader(chrome_proxy_accept_transform_header(), "empty-image");
  EXPECT_TRUE(lofi_decider->IsSlowPagePreviewRequested(headers));
  headers.SetHeader(chrome_proxy_accept_transform_header(), "empty-image;foo");
  EXPECT_FALSE(lofi_decider->IsSlowPagePreviewRequested(headers));
  headers.SetHeader("Another-Header", "empty-image");
  lofi_decider->RemoveAcceptTransformHeader(&headers);
  EXPECT_FALSE(lofi_decider->IsSlowPagePreviewRequested(headers));
}

TEST_F(ContentLoFiDeciderTest, IsLitePagePreviewRequested) {
  std::unique_ptr<data_reduction_proxy::ContentLoFiDecider> lofi_decider(
      new data_reduction_proxy::ContentLoFiDecider());
  net::HttpRequestHeaders headers;
  EXPECT_FALSE(lofi_decider->IsLitePagePreviewRequested(headers));
  headers.SetHeader(chrome_proxy_accept_transform_header(), "empty-image");
  EXPECT_FALSE(lofi_decider->IsLitePagePreviewRequested(headers));
  headers.SetHeader(chrome_proxy_accept_transform_header(),
                    "empty-image;lite-page");
  EXPECT_FALSE(lofi_decider->IsLitePagePreviewRequested(headers));
  headers.SetHeader(chrome_proxy_accept_transform_header(), "lite-page");
  EXPECT_TRUE(lofi_decider->IsLitePagePreviewRequested(headers));
  headers.SetHeader(chrome_proxy_accept_transform_header(), "lite-page;foo");
  EXPECT_TRUE(lofi_decider->IsLitePagePreviewRequested(headers));
  headers.SetHeader("Another-Header", "lite-page");
  lofi_decider->RemoveAcceptTransformHeader(&headers);
  EXPECT_FALSE(lofi_decider->IsLitePagePreviewRequested(headers));
}

TEST_F(ContentLoFiDeciderTest, RemoveAcceptTransformHeader) {
  std::unique_ptr<data_reduction_proxy::ContentLoFiDecider> lofi_decider(
      new data_reduction_proxy::ContentLoFiDecider());
  net::HttpRequestHeaders headers;
  headers.SetHeader(chrome_proxy_accept_transform_header(), "Foo");
  EXPECT_TRUE(headers.HasHeader(chrome_proxy_accept_transform_header()));
  lofi_decider->RemoveAcceptTransformHeader(&headers);
  EXPECT_FALSE(headers.HasHeader(chrome_proxy_accept_transform_header()));
}

TEST_F(ContentLoFiDeciderTest, MaybeIgnoreBlacklist) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->InitFromArgv(command_line->argv());
  std::unique_ptr<data_reduction_proxy::ContentLoFiDecider> lofi_decider(
      new data_reduction_proxy::ContentLoFiDecider());
  net::HttpRequestHeaders headers;
  lofi_decider->MaybeSetIgnorePreviewsBlacklistDirective(&headers);
  EXPECT_FALSE(headers.HasHeader(chrome_proxy_header()));

  headers.SetHeader(chrome_proxy_header(), "Foo");
  lofi_decider->MaybeSetIgnorePreviewsBlacklistDirective(&headers);
  std::string header_value;
  headers.GetHeader(chrome_proxy_header(), &header_value);
  EXPECT_EQ("Foo", header_value);

  headers.RemoveHeader(chrome_proxy_header());
  command_line->AppendSwitch(switches::kEnableDataReductionProxyLitePage);
  headers.SetHeader(chrome_proxy_accept_transform_header(), "empty-image");
  lofi_decider->MaybeSetIgnorePreviewsBlacklistDirective(&headers);
  EXPECT_FALSE(headers.HasHeader(chrome_proxy_header()));

  headers.SetHeader(chrome_proxy_accept_transform_header(), "lite-page");
  lofi_decider->MaybeSetIgnorePreviewsBlacklistDirective(&headers);
  EXPECT_TRUE(headers.HasHeader(chrome_proxy_header()));
  headers.GetHeader(chrome_proxy_header(), &header_value);
  EXPECT_EQ("exp=ignore_preview_blacklist", header_value);

  headers.SetHeader(chrome_proxy_header(), "Foo");
  lofi_decider->MaybeSetIgnorePreviewsBlacklistDirective(&headers);
  EXPECT_TRUE(headers.HasHeader(chrome_proxy_header()));
  headers.GetHeader(chrome_proxy_header(), &header_value);
  EXPECT_EQ("Foo, exp=ignore_preview_blacklist", header_value);
}

}  // namespace data_reduction_roxy
