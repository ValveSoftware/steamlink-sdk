// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "net/proxy/proxy_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxyConfiguratorTest : public testing::Test {
 public:
  void SetUp() override {
    test_context_ = DataReductionProxyTestContext::Builder().Build();
    config_.reset(new DataReductionProxyConfigurator(
        test_context_->net_log(), test_context_->event_creator()));
  }

  std::vector<net::ProxyServer> BuildProxyList(const std::string& first,
                                               const std::string& second) {
    std::vector<net::ProxyServer> proxies;
    if (!first.empty()) {
      net::ProxyServer proxy =
          net::ProxyServer::FromURI(first, net::ProxyServer::SCHEME_HTTP);
      EXPECT_TRUE(proxy.is_valid()) << first;
      proxies.push_back(proxy);
    }
    if (!second.empty()) {
      net::ProxyServer proxy =
          net::ProxyServer::FromURI(second, net::ProxyServer::SCHEME_HTTP);
      EXPECT_TRUE(proxy.is_valid()) << second;
      proxies.push_back(proxy);
    }
    return proxies;
  }

  void CheckProxyConfig(
      const net::ProxyConfig::ProxyRules::Type& expected_rules_type,
      const std::string& expected_http_proxies,
      const std::string& expected_bypass_list) {
    test_context_->RunUntilIdle();
    const net::ProxyConfig::ProxyRules& rules =
        config_->GetProxyConfig().proxy_rules();
    ASSERT_EQ(expected_rules_type, rules.type);
    if (net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME ==
        expected_rules_type) {
      ASSERT_EQ(expected_http_proxies, rules.proxies_for_http.ToPacString());
      ASSERT_EQ(std::string(), rules.proxies_for_https.ToPacString());
      ASSERT_EQ(expected_bypass_list, rules.bypass_rules.ToString());
    }
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  std::unique_ptr<DataReductionProxyConfigurator> config_;
};

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestricted) {
  config_->Enable(false, BuildProxyList("https://www.foo.com:443",
                                        "http://www.bar.com:80"));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedQuic) {
  config_->Enable(
      false, BuildProxyList("quic://www.foo.com:443", "http://www.bar.com:80"));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "QUIC www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedWithBypassRule) {
  config_->AddHostPatternToBypass("<local>");
  config_->AddHostPatternToBypass("*.goo.com");
  config_->Enable(false, BuildProxyList("https://www.foo.com:443",
                                        "http://www.bar.com:80"));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   "<local>;*.goo.com;");
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedWithBypassRuleQuic) {
  config_->AddHostPatternToBypass("<local>");
  config_->AddHostPatternToBypass("*.goo.com");
  config_->Enable(
      false, BuildProxyList("quic://www.foo.com:443", "http://www.bar.com:80"));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "QUIC www.foo.com:443;PROXY www.bar.com:80;DIRECT",
                   "<local>;*.goo.com;");
}

TEST_F(DataReductionProxyConfiguratorTest, TestUnrestrictedWithoutFallback) {
  config_->Enable(false,
                  BuildProxyList("https://www.foo.com:443", std::string()));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "HTTPS www.foo.com:443;DIRECT", std::string());
}

TEST_F(DataReductionProxyConfiguratorTest,
       TestUnrestrictedWithoutFallbackQuic) {
  config_->Enable(false,
                  BuildProxyList("quic://www.foo.com:443", std::string()));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "QUIC www.foo.com:443;DIRECT", std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestRestricted) {
  config_->Enable(
      true, BuildProxyList("https://www.foo.com:443", "http://www.bar.com:80"));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "PROXY www.bar.com:80;DIRECT", std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestRestrictedQuic) {
  config_->Enable(
      true, BuildProxyList("quic://www.foo.com:443", "http://www.bar.com:80"));
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME,
                   "PROXY www.bar.com:80;DIRECT", std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestDisable) {
  config_->Enable(false, BuildProxyList("https://www.foo.com:443",
                                        "http://www.bar.com:80"));
  config_->Disable();
  CheckProxyConfig(net::ProxyConfig::ProxyRules::TYPE_NO_RULES, std::string(),
                   std::string());
}

TEST_F(DataReductionProxyConfiguratorTest, TestBypassList) {
  config_->AddHostPatternToBypass("http://www.google.com");
  config_->AddHostPatternToBypass("fefe:13::abc/33");

  std::string expected[] = {
    "http://www.google.com",
    "fefe:13::abc/33",
  };

  ASSERT_EQ(arraysize(expected), config_->bypass_rules_.size());
  int i = 0;
  for (const std::string& bypass_rule : config_->bypass_rules_)
    EXPECT_EQ(expected[i++], bypass_rule);
}

}  // namespace data_reduction_proxy
