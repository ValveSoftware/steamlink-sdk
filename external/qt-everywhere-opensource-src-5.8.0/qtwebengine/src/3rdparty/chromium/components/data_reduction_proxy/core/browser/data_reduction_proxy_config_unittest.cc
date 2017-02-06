// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/log/test_net_log.h"
#include "net/nqe/external_estimate_provider.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_test_util.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace data_reduction_proxy {

namespace {

void SetProxiesForHttpOnCommandLine(
    const std::vector<net::ProxyServer>& proxies_for_http) {
  std::vector<std::string> proxy_strings;
  for (const net::ProxyServer& proxy : proxies_for_http)
    proxy_strings.push_back(proxy.ToURI());

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyHttpProxies,
      base::JoinString(proxy_strings, ";"));
}

std::string GetRetryMapKeyFromOrigin(const std::string& origin) {
  // The retry map has the scheme prefix for https but not for http.
  return origin;
}

}  // namespace

class DataReductionProxyConfigTest : public testing::Test {
 public:
  DataReductionProxyConfigTest() {}
  ~DataReductionProxyConfigTest() override {}

  void SetUp() override {
    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithMockConfig()
                        .WithMockDataReductionProxyService()
                        .Build();

    ResetSettings(true, true, true, false);

    expected_params_.reset(new TestDataReductionProxyParams(
        DataReductionProxyParams::kAllowed |
            DataReductionProxyParams::kFallbackAllowed |
            DataReductionProxyParams::kPromoAllowed,
        TestDataReductionProxyParams::HAS_EVERYTHING));
  }

  void ResetSettings(bool allowed,
                     bool fallback_allowed,
                     bool promo_allowed,
                     bool holdback) {
    int flags = 0;
    if (allowed)
      flags |= DataReductionProxyParams::kAllowed;
    if (fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    if (promo_allowed)
      flags |= DataReductionProxyParams::kPromoAllowed;
    if (holdback)
      flags |= DataReductionProxyParams::kHoldback;
    config()->ResetParamFlagsForTest(flags);
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() {
    return message_loop_.task_runner();
  }

  void ExpectSecureProxyCheckResult(SecureProxyCheckFetchResult result) {
    EXPECT_CALL(*config(), RecordSecureProxyCheckFetchResult(result)).Times(1);
  }

  class TestResponder {
   public:
    void ExecuteCallback(FetcherResponseCallback callback) {
      callback.Run(response, status, http_response_code);
    }

    std::string response;
    net::URLRequestStatus status;
    int http_response_code;
  };

  void CheckSecureProxyCheckOnIPChange(
      const std::string& response,
      int response_code,
      net::URLRequestStatus status,
      SecureProxyCheckFetchResult expected_fetch_result,
      const std::vector<net::ProxyServer>& expected_proxies_for_http) {
    base::HistogramTester histogram_tester;

    ExpectSecureProxyCheckResult(expected_fetch_result);
    TestResponder responder;
    responder.response = response;
    responder.status = status;
    responder.http_response_code = response_code;
    EXPECT_CALL(*config(), SecureProxyCheck(_, _))
        .Times(1)
        .WillRepeatedly(testing::WithArgs<1>(
            testing::Invoke(&responder, &TestResponder::ExecuteCallback)));
    config()->OnIPAddressChanged();
    test_context_->RunUntilIdle();
    EXPECT_EQ(expected_proxies_for_http, GetConfiguredProxiesForHttp());

    if (!status.is_success() &&
        status.error() != net::ERR_INTERNET_DISCONNECTED) {
      histogram_tester.ExpectUniqueSample("DataReductionProxy.ProbeURLNetError",
                                          std::abs(status.error()), 1);
    } else {
      histogram_tester.ExpectTotalCount("DataReductionProxy.ProbeURLNetError",
                                        0);
    }
  }

  void RunUntilIdle() {
    test_context_->RunUntilIdle();
  }

  std::unique_ptr<DataReductionProxyConfig> BuildConfig(
      std::unique_ptr<DataReductionProxyParams> params) {
    return base::WrapUnique(new DataReductionProxyConfig(
        task_runner(), test_context_->net_log(), std::move(params),
        test_context_->configurator(), test_context_->event_creator()));
  }

  MockDataReductionProxyConfig* config() {
    return test_context_->mock_config();
  }

  DataReductionProxyConfigurator* configurator() const {
    return test_context_->configurator();
  }

  TestDataReductionProxyParams* params() const {
    return expected_params_.get();
  }

  DataReductionProxyEventCreator* event_creator() const {
    return test_context_->event_creator();
  }

  net::NetLog* net_log() const { return test_context_->net_log(); }

  std::vector<net::ProxyServer> GetConfiguredProxiesForHttp() const {
    return test_context_->GetConfiguredProxiesForHttp();
  }

 private:
  base::MessageLoopForIO message_loop_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  std::unique_ptr<TestDataReductionProxyParams> expected_params_;
};

TEST_F(DataReductionProxyConfigTest, TestUpdateConfigurator) {
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);
  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});

  ResetSettings(true, true, true, false);

  // Expect both secure and insecure proxies to be available when the DRP is
  // enabled and secure proxies are allowed.
  config()->UpdateConfigurator(true, true);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());

  // Expect only insecure proxies to be available when the DRP is enabled and
  // secure proxies are not allowed.
  config()->UpdateConfigurator(true, false);
  EXPECT_EQ(std::vector<net::ProxyServer>(1, kHttpProxy),
            GetConfiguredProxiesForHttp());

  // Expect no proxies to be available when the DRP is disabled.
  config()->UpdateConfigurator(false, false);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
}

TEST_F(DataReductionProxyConfigTest, TestUpdateConfiguratorHoldback) {
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);
  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});

  ResetSettings(true, true, true, true);

  config()->UpdateConfigurator(true, false);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
}

TEST_F(DataReductionProxyConfigTest, TestOnIPAddressChanged) {
  const net::URLRequestStatus kSuccess(net::URLRequestStatus::SUCCESS, net::OK);
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  ResetSettings(true, true, true, false);

  // The proxy is enabled initially.
  config()->enabled_by_user_ = true;
  config()->secure_proxy_allowed_ = true;
  config()->UpdateConfigurator(true, true);

  // IP address change triggers a secure proxy check that succeeds. Proxy
  // remains unrestricted.
  CheckSecureProxyCheckOnIPChange("OK", net::HTTP_OK, kSuccess,
                                  SUCCEEDED_PROXY_ALREADY_ENABLED,
                                  {kHttpsProxy, kHttpProxy});

  // IP address change triggers a secure proxy check that fails. Proxy is
  // restricted.
  CheckSecureProxyCheckOnIPChange("Bad", net::HTTP_OK, kSuccess,
                                  FAILED_PROXY_DISABLED,
                                  std::vector<net::ProxyServer>(1, kHttpProxy));

  // IP address change triggers a secure proxy check that fails due to the
  // network changing again. This should be ignored, so the proxy should remain
  // unrestricted.
  CheckSecureProxyCheckOnIPChange(
      std::string(), net::URLFetcher::RESPONSE_CODE_INVALID,
      net::URLRequestStatus(net::URLRequestStatus::FAILED,
                            net::ERR_INTERNET_DISCONNECTED),
      INTERNET_DISCONNECTED, std::vector<net::ProxyServer>(1, kHttpProxy));

  // IP address change triggers a secure proxy check that fails. Proxy remains
  // restricted.
  CheckSecureProxyCheckOnIPChange("Bad", net::HTTP_OK, kSuccess,
                                  FAILED_PROXY_ALREADY_DISABLED,
                                  std::vector<net::ProxyServer>(1, kHttpProxy));

  // IP address change triggers a secure proxy check that succeeds. Proxy is
  // unrestricted.
  CheckSecureProxyCheckOnIPChange("OK", net::HTTP_OK, kSuccess,
                                  SUCCEEDED_PROXY_ENABLED,
                                  {kHttpsProxy, kHttpProxy});

  // IP address change triggers a secure proxy check that fails due to the
  // network changing again. This should be ignored, so the proxy should remain
  // unrestricted.
  CheckSecureProxyCheckOnIPChange(
      std::string(), net::URLFetcher::RESPONSE_CODE_INVALID,
      net::URLRequestStatus(net::URLRequestStatus::FAILED,
                            net::ERR_INTERNET_DISCONNECTED),
      INTERNET_DISCONNECTED, {kHttpsProxy, kHttpProxy});

  // IP address change triggers a secure proxy check that fails because of a
  // redirect response, e.g. by a captive portal. Proxy is restricted.
  CheckSecureProxyCheckOnIPChange(
      "Bad", net::HTTP_FOUND,
      net::URLRequestStatus(net::URLRequestStatus::CANCELED, net::ERR_ABORTED),
      FAILED_PROXY_DISABLED, std::vector<net::ProxyServer>(1, kHttpProxy));
}

TEST_F(DataReductionProxyConfigTest,
       TestOnIPAddressChanged_SecureProxyDisabledByDefault) {
  const net::URLRequestStatus kSuccess(net::URLRequestStatus::SUCCESS, net::OK);
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      data_reduction_proxy::switches::kDataReductionProxyStartSecureDisabled);

  ResetSettings(true, true, true, false);

  // The proxy is enabled initially.
  config()->enabled_by_user_ = true;
  config()->secure_proxy_allowed_ = false;
  config()->UpdateConfigurator(true, false);

  // IP address change triggers a secure proxy check that succeeds. Proxy
  // becomes unrestricted.
  CheckSecureProxyCheckOnIPChange("OK", net::HTTP_OK, kSuccess,
                                  SUCCEEDED_PROXY_ENABLED,
                                  {kHttpsProxy, kHttpProxy});
  // IP address change triggers a secure proxy check that fails. Proxy is
  // restricted before the check starts, and remains disabled.
  ExpectSecureProxyCheckResult(PROXY_DISABLED_BEFORE_CHECK);
  CheckSecureProxyCheckOnIPChange("Bad", net::HTTP_OK, kSuccess,
                                  FAILED_PROXY_ALREADY_DISABLED,
                                  std::vector<net::ProxyServer>(1, kHttpProxy));
  // IP address change triggers a secure proxy check that fails. Proxy remains
  // restricted.
  CheckSecureProxyCheckOnIPChange("Bad", net::HTTP_OK, kSuccess,
                                  FAILED_PROXY_ALREADY_DISABLED,
                                  std::vector<net::ProxyServer>(1, kHttpProxy));
  // IP address change triggers a secure proxy check that succeeds. Proxy is
  // unrestricted.
  CheckSecureProxyCheckOnIPChange("OK", net::HTTP_OK, kSuccess,
                                  SUCCEEDED_PROXY_ENABLED,
                                  {kHttpsProxy, kHttpProxy});
}

TEST_F(DataReductionProxyConfigTest, AreProxiesBypassed) {
  const struct {
    // proxy flags
    bool allowed;
    bool fallback_allowed;
    // is https request
    bool is_https;
    // proxies in retry map
    bool origin;
    bool fallback_origin;

    bool expected_result;
  } tests[] = {
      {
          // proxy flags
          false, false,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          false, false,
          // is https request
          true,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          false, true,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, false,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, false,
          // is https request
          false,
          // proxies in retry map
          true, false,
          // expected result
          true,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          true, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          true, true,
          // expected result
          true,
      },
      {
          // proxy flags
          true, true,
          // is https request
          true,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          true,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          false, true,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          true,
          // proxies in retry map
          true, true,
          // expected result
          false,
      },
  };

  // The retry map has the scheme prefix for https but not for http.
  std::string origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultOrigin());
  std::string fallback_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultFallbackOrigin());

  for (size_t i = 0; i < arraysize(tests); ++i) {
    net::ProxyConfig::ProxyRules rules;
    std::vector<std::string> proxies;

    if (tests[i].allowed)
      proxies.push_back(origin);
    if (tests[i].allowed && tests[i].fallback_allowed)
      proxies.push_back(fallback_origin);

    std::string proxy_rules = "http=" + base::JoinString(proxies, ",") +
                              ",direct://;";

    rules.ParseFromString(proxy_rules);

    int flags = 0;
    if (tests[i].allowed)
      flags |= DataReductionProxyParams::kAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    unsigned int has_definitions = TestDataReductionProxyParams::HAS_EVERYTHING;
    std::unique_ptr<TestDataReductionProxyParams> params(
        new TestDataReductionProxyParams(flags, has_definitions));
    std::unique_ptr<DataReductionProxyConfig> config =
        BuildConfig(std::move(params));

    net::ProxyRetryInfoMap retry_map;
    net::ProxyRetryInfo retry_info;
    retry_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();

    if (tests[i].origin)
      retry_map[origin] = retry_info;
    if (tests[i].fallback_origin)
      retry_map[fallback_origin] = retry_info;

    bool was_bypassed = config->AreProxiesBypassed(retry_map,
                                                   rules,
                                                   tests[i].is_https,
                                                   NULL);

    EXPECT_EQ(tests[i].expected_result, was_bypassed) << i;
  }
}

TEST_F(DataReductionProxyConfigTest, AreProxiesBypassedRetryDelay) {
  std::string origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultOrigin());
  std::string fallback_origin = GetRetryMapKeyFromOrigin(
      TestDataReductionProxyParams::DefaultFallbackOrigin());

  net::ProxyConfig::ProxyRules rules;
  std::vector<std::string> proxies;

  proxies.push_back(origin);
  proxies.push_back(fallback_origin);

  std::string proxy_rules =
      "http=" + base::JoinString(proxies, ",") + ",direct://;";

  rules.ParseFromString(proxy_rules);

  int flags = 0;
  flags |= DataReductionProxyParams::kAllowed;
  flags |= DataReductionProxyParams::kFallbackAllowed;
  unsigned int has_definitions = TestDataReductionProxyParams::HAS_EVERYTHING;
  std::unique_ptr<TestDataReductionProxyParams> params(
      new TestDataReductionProxyParams(flags, has_definitions));
  std::unique_ptr<DataReductionProxyConfig> config =
      BuildConfig(std::move(params));

  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo retry_info;

  retry_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();
  retry_map[origin] = retry_info;

  retry_info.bad_until = base::TimeTicks();
  retry_map[fallback_origin] = retry_info;

  bool was_bypassed = config->AreProxiesBypassed(retry_map,
                                                 rules,
                                                 false,
                                                 NULL);

  EXPECT_FALSE(was_bypassed);

  base::TimeDelta delay = base::TimeDelta::FromHours(2);
  retry_info.bad_until = base::TimeTicks::Now() + delay;
  retry_info.current_delay = delay;
  retry_map[origin] = retry_info;

  delay = base::TimeDelta::FromHours(1);
  retry_info.bad_until = base::TimeTicks::Now() + delay;
  retry_info.current_delay = delay;
  retry_map[fallback_origin] = retry_info;

  base::TimeDelta min_retry_delay;
  was_bypassed = config->AreProxiesBypassed(retry_map,
                                            rules,
                                            false,
                                            &min_retry_delay);
  EXPECT_TRUE(was_bypassed);
  EXPECT_EQ(delay, min_retry_delay);
}

TEST_F(DataReductionProxyConfigTest, IsDataReductionProxyWithParams) {
  const struct {
    net::HostPortPair host_port_pair;
    bool fallback_allowed;
    bool expected_result;
    net::HostPortPair expected_first;
    net::HostPortPair expected_second;
    bool expected_is_fallback;
  } tests[] = {
      {net::ProxyServer::FromURI(TestDataReductionProxyParams::DefaultOrigin(),
                                 net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       true, true,
       net::ProxyServer::FromURI(TestDataReductionProxyParams::DefaultOrigin(),
                                 net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       false},
      {net::ProxyServer::FromURI(TestDataReductionProxyParams::DefaultOrigin(),
                                 net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       false, true,
       net::ProxyServer::FromURI(TestDataReductionProxyParams::DefaultOrigin(),
                                 net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       net::HostPortPair::FromURL(GURL()), false},
      {net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       true, true, net::ProxyServer::FromURI(
                       TestDataReductionProxyParams::DefaultFallbackOrigin(),
                       net::ProxyServer::SCHEME_HTTP)
                       .host_port_pair(),
       net::HostPortPair::FromURL(GURL()), true},
      {net::ProxyServer::FromURI(
           TestDataReductionProxyParams::DefaultFallbackOrigin(),
           net::ProxyServer::SCHEME_HTTP)
           .host_port_pair(),
       false, false, net::HostPortPair::FromURL(GURL()),
       net::HostPortPair::FromURL(GURL()), false},
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    int flags = DataReductionProxyParams::kAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    unsigned int has_definitions = TestDataReductionProxyParams::HAS_EVERYTHING;
    std::unique_ptr<TestDataReductionProxyParams> params(
        new TestDataReductionProxyParams(flags, has_definitions));
    DataReductionProxyTypeInfo proxy_type_info;
    std::unique_ptr<DataReductionProxyConfig> config(
        new DataReductionProxyConfig(task_runner(), net_log(),
                                     std::move(params), configurator(),
                                     event_creator()));
    EXPECT_EQ(
        tests[i].expected_result,
        config->IsDataReductionProxy(tests[i].host_port_pair, &proxy_type_info))
        << i;
    EXPECT_EQ(
        net::ProxyServer::FromURI(tests[i].expected_first.ToString(),
                                  net::ProxyServer::SCHEME_HTTP).is_valid(),
        proxy_type_info.proxy_servers.size() >= 1 &&
            proxy_type_info.proxy_servers[0].is_valid())
        << i;
    if (proxy_type_info.proxy_servers.size() >= 1 &&
        proxy_type_info.proxy_servers[0].is_valid()) {
      EXPECT_TRUE(tests[i].expected_first.Equals(
          proxy_type_info.proxy_servers[0].host_port_pair()))
          << i;
    }
    EXPECT_EQ(
        net::ProxyServer::FromURI(tests[i].expected_second.ToString(),
                                  net::ProxyServer::SCHEME_HTTP).is_valid(),
        proxy_type_info.proxy_servers.size() >= 2 &&
            proxy_type_info.proxy_servers[1].is_valid())
        << i;
    if (proxy_type_info.proxy_servers.size() >= 2 &&
        proxy_type_info.proxy_servers[1].is_valid()) {
      EXPECT_TRUE(tests[i].expected_second.Equals(
          proxy_type_info.proxy_servers[1].host_port_pair()))
          << i;
    }

    EXPECT_EQ(tests[i].expected_is_fallback, proxy_type_info.proxy_index != 0)
        << i;
  }
}

TEST_F(DataReductionProxyConfigTest, IsDataReductionProxyWithMutableConfig) {
  std::vector<net::ProxyServer> proxies_for_http;
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      "https://origin.net:443", net::ProxyServer::SCHEME_HTTP));
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      "http://origin.net:80", net::ProxyServer::SCHEME_HTTP));
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      "quic://anotherorigin.net:443", net::ProxyServer::SCHEME_HTTP));
  const struct {
    net::HostPortPair host_port_pair;
    bool expected_result;
    std::vector<net::ProxyServer> expected_proxies;
    size_t expected_proxy_index;
  } tests[] = {
      {
          proxies_for_http[0].host_port_pair(), true,
          std::vector<net::ProxyServer>(proxies_for_http.begin(),
                                        proxies_for_http.end()),
          0,
      },
      {
          proxies_for_http[1].host_port_pair(), true,
          std::vector<net::ProxyServer>(proxies_for_http.begin() + 1,
                                        proxies_for_http.end()),
          1,
      },
      {
          proxies_for_http[2].host_port_pair(), true,
          std::vector<net::ProxyServer>(proxies_for_http.begin() + 2,
                                        proxies_for_http.end()),
          2,
      },
      {
          net::HostPortPair(), false, std::vector<net::ProxyServer>(), 0,
      },
      {
          net::HostPortPair::FromString("https://otherorigin.net:443"), false,
          std::vector<net::ProxyServer>(), 0,
      },
  };

  std::unique_ptr<DataReductionProxyMutableConfigValues> config_values =
      DataReductionProxyMutableConfigValues::CreateFromParams(params());
  config_values->UpdateValues(proxies_for_http);
  std::unique_ptr<DataReductionProxyConfig> config(new DataReductionProxyConfig(
      task_runner(), net_log(), std::move(config_values), configurator(),
      event_creator()));
  for (const auto& test : tests) {
    DataReductionProxyTypeInfo proxy_type_info;
    EXPECT_EQ(test.expected_result, config->IsDataReductionProxy(
                                        test.host_port_pair, &proxy_type_info));
    EXPECT_THAT(proxy_type_info.proxy_servers,
                testing::ContainerEq(test.expected_proxies));
    EXPECT_EQ(test.expected_proxy_index, proxy_type_info.proxy_index);
  }
}

TEST_F(DataReductionProxyConfigTest, LoFiOn) {
  const struct {
    bool lofi_switch_enabled;
    const std::string lofi_field_trial_group_name;
    bool network_prohibitively_slow;
    bool expect_lofi_header;
    int bucket_to_check_for_auto_lofi_uma;
    int expect_bucket_count;
  } tests[] = {
      {
          // The Lo-Fi switch is off and the user is not in the enabled field
          // trial group. Lo-Fi should not be used.
          false, std::string(), false, false, 0,
          0,  // not in enabled field trial, UMA is not recorded
      },
      {
          // The Lo-Fi switch is off and the user is not in enabled field trial
          // group and the network quality is bad. Lo-Fi should not be used.
          false, std::string(), true, false, 0,
          0,  // not in enabled field trial, UMA is not recorded
      },
      {
          // Lo-Fi is enabled through command line switch. LoFi should be used.
          true, std::string(), false, true, 0,
          0,  // not in enabled field trial, UMA is not recorded
      },
      {
          // The user is in the enabled field trial group but the network
          // quality is not bad. Lo-Fi should not be used.
          false, "Enabled", false, false,
          0,  // Lo-Fi request header is not used (state change: empty to empty)
          1,
      },
      {
          // The user is in the enabled field trial group but the network
          // quality is not bad. Lo-Fi should not be used.
          false, "Enabled_Control", false, false,
          0,  // Lo-Fi request header is not used (state change: empty to empty)
          1,
      },
      {
          // The user is in the enabled field trial group and the network
          // quality is bad. Lo-Fi should be used.
          false, "Enabled", true, true,
          1,  // Lo-Fi request header is now used (state change: empty to low)
          1,
      },
      {
          // The user is in the enabled field trial group and the network
          // quality is bad. Lo-Fi should be used.
          false, "Enabled_Control", true, true,
          3,  // Lo-Fi request header is now used (state change: low to low)
          1,
      },
      {
          // The user is in the enabled field trial group and the network
          // quality is bad. Lo-Fi should be used again.
          false, "Enabled", true, true,
          3,  // Lo-Fi request header is now used (state change: low to low)
          1,
      },
      {
          // The user is in the enabled field trial group and the network
          // quality is bad. Lo-Fi should be used again.
          false, "Enabled_Control", true, true,
          3,  // Lo-Fi request header is now used (state change: low to low)
          1,
      },
      {
          // The user is in the enabled field trial group but the network
          // quality is not bad. Lo-Fi should not be used.
          false, "Enabled", false, false,
          2,  // Lo-Fi request header is not used (state change: low to empty)
          1,
      },
      {
          // The user is in the enabled field trial group but the network
          // quality is not bad. Lo-Fi should not be used.
          false, "Enabled_Control", false, false,
          0,  // Lo-Fi request header is not used (state change: empty to empty)
          1,
      },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    config()->ResetLoFiStatusForTest();
    if (tests[i].lofi_switch_enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueAlwaysOn);
    } else {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi, std::string());
    }

    base::FieldTrialList field_trial_list(nullptr);
    if (!tests[i].lofi_field_trial_group_name.empty()) {
      base::FieldTrialList::CreateFieldTrial(
          params::GetLoFiFieldTrialName(),
          tests[i].lofi_field_trial_group_name);
    }

    EXPECT_CALL(*config(), IsNetworkQualityProhibitivelySlow(_))
        .WillRepeatedly(testing::Return(tests[i].network_prohibitively_slow));

    base::HistogramTester histogram_tester;
    net::TestURLRequestContext context_;
    net::TestDelegate delegate_;
    std::unique_ptr<net::URLRequest> request =
        context_.CreateRequest(GURL(), net::IDLE, &delegate_);
    request->SetLoadFlags(request->load_flags() | net::LOAD_MAIN_FRAME);
    bool should_enable_lofi = config()->ShouldEnableLoFiMode(*request.get());
    if (tests[i].expect_bucket_count != 0) {
      histogram_tester.ExpectBucketCount(
          "DataReductionProxy.AutoLoFiRequestHeaderState.Unknown",
          tests[i].bucket_to_check_for_auto_lofi_uma,
          tests[i].expect_bucket_count);
    }

    EXPECT_EQ(tests[i].expect_lofi_header, should_enable_lofi) << i;
  }
}

// Overrides net::NetworkQualityEstimator for testing.
class TestNetworkQualityEstimator : public net::NetworkQualityEstimator {
 public:
  explicit TestNetworkQualityEstimator(
      const std::map<std::string, std::string>& variation_params)
      : NetworkQualityEstimator(
            std::unique_ptr<net::ExternalEstimateProvider>(),
            variation_params),
        effective_connection_type_(
            net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
        recent_effective_connection_type_(
            net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {}

  ~TestNetworkQualityEstimator() override {}

  EffectiveConnectionType GetEffectiveConnectionType() const override {
    return effective_connection_type_;
  }

  EffectiveConnectionType GetRecentEffectiveConnectionType(
      const base::TimeTicks& start_time) const override {
    return recent_effective_connection_type_;
  }

  void SetEffectiveConnectionType(
      net::NetworkQualityEstimator::EffectiveConnectionType
          effective_connection_type) {
    effective_connection_type_ = effective_connection_type;
  }

  void SetRecentEffectiveConnectionType(
      net::NetworkQualityEstimator::EffectiveConnectionType
          recent_effective_connection_type) {
    recent_effective_connection_type_ = recent_effective_connection_type;
  }

 private:
  // Estimate of the quality of the network.
  net::NetworkQualityEstimator::EffectiveConnectionType
      effective_connection_type_;
  net::NetworkQualityEstimator::EffectiveConnectionType
      recent_effective_connection_type_;

  base::TimeDelta rtt_since_;
};

TEST_F(DataReductionProxyConfigTest, AutoLoFiParams) {
  DataReductionProxyConfig config(task_runner(), nullptr, nullptr,
                                  configurator(), event_creator());
  variations::testing::ClearAllVariationParams();
  std::map<std::string, std::string> variation_params;
  std::map<std::string, std::string> variation_params_flag;

  variation_params["effective_connection_type"] = "Slow2G";
  variation_params_flag["effective_connection_type"] = "2G";

  variation_params["hysteresis_period_seconds"] = "360";
  variation_params_flag["hysteresis_period_seconds"] = "361";

  variation_params["spurious_field"] = "480";
  variation_params_flag["spurious_field"] = "481";

  ASSERT_TRUE(variations::AssociateVariationParams(
      params::GetLoFiFieldTrialName(), "Enabled", variation_params));

  ASSERT_TRUE(variations::AssociateVariationParams(
      params::GetLoFiFlagFieldTrialName(), "Enabled", variation_params_flag));

  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                         "Enabled");
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFlagFieldTrialName(),
                                         "Enabled");

  const struct {
    bool lofi_flag_group;

  } tests[] = {
      {
          false,
      },
      {
          true,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    net::NetworkQualityEstimator::EffectiveConnectionType
        expected_effective_connection_type =
            net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
    int expected_hysteresis_sec = 360;

    if (tests[i].lofi_flag_group) {
      // LoFi flag field trial has higher priority than LoFi field trial.
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueSlowConnectionsOnly);
      expected_effective_connection_type =
          net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_2G;
      expected_hysteresis_sec = 361;
    }

  config.PopulateAutoLoFiParams();

  EXPECT_EQ(expected_effective_connection_type,
            config.lofi_effective_connection_type_threshold_);
  EXPECT_EQ(base::TimeDelta::FromSeconds(expected_hysteresis_sec),
            config.auto_lofi_hysteresis_);

  std::map<std::string, std::string> network_quality_estimator_params;
  TestNetworkQualityEstimator test_network_quality_estimator(
      network_quality_estimator_params);

  // Network is slow.
  test_network_quality_estimator.SetEffectiveConnectionType(
      expected_effective_connection_type);
  EXPECT_TRUE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Network quality improved. However, network should still be marked as slow
  // because of hysteresis.
  test_network_quality_estimator.SetEffectiveConnectionType(
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_BROADBAND);
  EXPECT_TRUE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Change the last update time to be older than the hysteresis duration.
  // Checking network quality afterwards should show that network is no longer
  // slow.
  config.network_quality_last_checked_ =
      base::TimeTicks::Now() -
      base::TimeDelta::FromSeconds(expected_hysteresis_sec + 1);
  EXPECT_FALSE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Changing the network quality has no effect because of hysteresis.
  test_network_quality_estimator.SetEffectiveConnectionType(
      expected_effective_connection_type);
  EXPECT_FALSE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Change in connection type changes the network quality despite hysteresis.
  config.connection_type_ = net::NetworkChangeNotifier::CONNECTION_WIFI;
  EXPECT_TRUE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));
  }
}

// Tests that default parameters for Lo-Fi are used when the parameters from
// field trial are missing.
TEST_F(DataReductionProxyConfigTest, AutoLoFiMissingParams) {
  DataReductionProxyConfig config(task_runner(), nullptr, nullptr,
                                  configurator(), event_creator());
  variations::testing::ClearAllVariationParams();
  std::map<std::string, std::string> variation_params;
  variation_params["spurious_field"] = "480";

  ASSERT_TRUE(variations::AssociateVariationParams(
      params::GetLoFiFieldTrialName(), "Enabled", variation_params));

  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                         "Enabled");

  config.PopulateAutoLoFiParams();

  EXPECT_EQ(net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            config.lofi_effective_connection_type_threshold_);
  EXPECT_EQ(base::TimeDelta::FromSeconds(60), config.auto_lofi_hysteresis_);
}

TEST_F(DataReductionProxyConfigTest, AutoLoFiParamsSlowConnectionsFlag) {
  DataReductionProxyConfig config(task_runner(), nullptr, nullptr,
                                  configurator(), event_creator());
  variations::testing::ClearAllVariationParams();

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyLoFi,
      switches::kDataReductionProxyLoFiValueSlowConnectionsOnly);

  config.PopulateAutoLoFiParams();

  net::NetworkQualityEstimator::EffectiveConnectionType
      expected_effective_connection_type =
          net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G;
  int hysteresis_sec = 60;
  EXPECT_EQ(expected_effective_connection_type,
            config.lofi_effective_connection_type_threshold_);
  EXPECT_EQ(base::TimeDelta::FromSeconds(hysteresis_sec),
            config.auto_lofi_hysteresis_);

  std::map<std::string, std::string> network_quality_estimator_params;
  TestNetworkQualityEstimator test_network_quality_estimator(
      network_quality_estimator_params);

  // Network is slow.
  test_network_quality_estimator.SetEffectiveConnectionType(
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  EXPECT_TRUE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Network quality improved. However, network should still be marked as slow
  // because of hysteresis.
  test_network_quality_estimator.SetEffectiveConnectionType(
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_TRUE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Change the last update time to be older than the hysteresis duration.
  // Checking network quality afterwards should show that network is no longer
  // slow.
  config.network_quality_last_checked_ =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(hysteresis_sec + 1);
  EXPECT_FALSE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Changing the network quality has no effect because of hysteresis.
  test_network_quality_estimator.SetEffectiveConnectionType(
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  EXPECT_FALSE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));

  // Change in connection type changes the network quality despite hysteresis.
  config.connection_type_ = net::NetworkChangeNotifier::CONNECTION_WIFI;
  EXPECT_TRUE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));
}

// Tests if metrics for Lo-Fi accuracy are recorded properly.
TEST_F(DataReductionProxyConfigTest, LoFiAccuracy) {
  std::unique_ptr<base::SimpleTestTickClock> tick_clock(
      new base::SimpleTestTickClock());

  std::vector<base::TimeDelta> lofi_accuracy_recording_intervals;
  lofi_accuracy_recording_intervals.push_back(base::TimeDelta::FromSeconds(0));

  TestDataReductionProxyConfig config(
      DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING, task_runner(), nullptr,
      configurator(), event_creator());
  config.SetLofiAccuracyRecordingIntervals(lofi_accuracy_recording_intervals);
  config.SetTickClock(tick_clock.get());

  variations::testing::ClearAllVariationParams();
  std::map<std::string, std::string> variation_params;

  int expected_hysteresis_sec = 360;

  variation_params["effective_connection_type"] = "Slow2G";
  variation_params["hysteresis_period_seconds"] =
      base::IntToString(expected_hysteresis_sec);

  const struct {
    std::string description;
    std::string field_trial_group;
    net::NetworkQualityEstimator::EffectiveConnectionType
        effective_connection_type;
    net::NetworkQualityEstimator::EffectiveConnectionType
        recent_effective_connection_type;
    bool expect_network_quality_slow;
    uint32_t bucket_to_check;
    uint32_t expected_bucket_count;
  } tests[] = {
      {"Predicted slow, actually slow, Enabled group", "Enabled",
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G, true, 0,
       1},
      {"Predicted slow, actually slow, Enabled_NoControl group",
       "Enabled_NoControl",
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G, true, 0,
       1},
      {"Predicted slow, actually slow, Control group", "Control",
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G, true, 0,
       1},
      {"Predicted slow, actually not slow", "Enabled",
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_2G, true, 1, 1},
      {"Predicted not slow, actually slow", "Enabled",
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_2G,
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G, false,
       2, 1},
      {"Predicted not slow, actually not slow", "Enabled",
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_2G,
       net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_2G, false, 3, 1},
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();
    ASSERT_TRUE(variations::AssociateVariationParams(
        params::GetLoFiFieldTrialName(), test.field_trial_group,
        variation_params))
        << test.description;

    ASSERT_NE(nullptr,
              base::FieldTrialList::CreateFieldTrial(
                  params::GetLoFiFieldTrialName(), test.field_trial_group))
        << test.description;
    config.PopulateAutoLoFiParams();

    std::map<std::string, std::string> network_quality_estimator_params;
    TestNetworkQualityEstimator test_network_quality_estimator(
        network_quality_estimator_params);

    base::HistogramTester histogram_tester;
    test_network_quality_estimator.SetEffectiveConnectionType(
        test.effective_connection_type);
    test_network_quality_estimator.SetRecentEffectiveConnectionType(
        test.recent_effective_connection_type);
    ASSERT_EQ(test.expect_network_quality_slow,
              config.IsNetworkQualityProhibitivelySlow(
                  &test_network_quality_estimator))
        << test.description;
    RunUntilIdle();
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.LoFi.Accuracy.0.Unknown", 1);
    histogram_tester.ExpectBucketCount(
        "DataReductionProxy.LoFi.Accuracy.0.Unknown", test.bucket_to_check,
        test.expected_bucket_count);
  }
}

// Tests if metrics for Lo-Fi accuracy are recorded properly at the specified
// interval.
TEST_F(DataReductionProxyConfigTest, LoFiAccuracyNonZeroDelay) {
  std::unique_ptr<base::SimpleTestTickClock> tick_clock(
      new base::SimpleTestTickClock());

  std::vector<base::TimeDelta> lofi_accuracy_recording_intervals;
  lofi_accuracy_recording_intervals.push_back(base::TimeDelta::FromSeconds(1));

  TestDataReductionProxyConfig config(
      DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING, task_runner(), nullptr,
      configurator(), event_creator());
  config.SetLofiAccuracyRecordingIntervals(lofi_accuracy_recording_intervals);
  config.SetTickClock(tick_clock.get());

  variations::testing::ClearAllVariationParams();
  std::map<std::string, std::string> variation_params;

  variation_params["effective_connection_type"] = "Slow2G";

  ASSERT_TRUE(variations::AssociateVariationParams(
      params::GetLoFiFieldTrialName(), "Enabled", variation_params));

  base::FieldTrialList field_trial_list(nullptr);
  base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                         "Enabled");
  config.PopulateAutoLoFiParams();

  std::map<std::string, std::string> network_quality_estimator_params;
  TestNetworkQualityEstimator test_network_quality_estimator(
      network_quality_estimator_params);

  base::HistogramTester histogram_tester;
  // Network was predicted to be slow and actually was slow.
  test_network_quality_estimator.SetEffectiveConnectionType(
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  test_network_quality_estimator.SetRecentEffectiveConnectionType(
      net::NetworkQualityEstimator::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  ASSERT_TRUE(config.IsNetworkQualityProhibitivelySlow(
      &test_network_quality_estimator));
  tick_clock->Advance(base::TimeDelta::FromSeconds(1));

  // Sleep to ensure that the delayed task is posted.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.LoFi.Accuracy.1.Unknown", 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.LoFi.Accuracy.1.Unknown", 0, 1);
}

}  // namespace data_reduction_proxy
