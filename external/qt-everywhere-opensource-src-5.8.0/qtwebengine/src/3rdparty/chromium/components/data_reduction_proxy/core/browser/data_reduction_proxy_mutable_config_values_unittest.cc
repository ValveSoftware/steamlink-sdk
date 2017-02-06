// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"

#include <vector>

#include "base/command_line.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/proxy/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

class DataReductionProxyMutableConfigValuesTest : public testing::Test {
 public:
  DataReductionProxyMutableConfigValuesTest() {}
  ~DataReductionProxyMutableConfigValuesTest() override {}

  void Init() {
    params_.reset(new DataReductionProxyParams(
        DataReductionProxyParams::kAllowAllProxyConfigurations));
    mutable_config_values_ =
        DataReductionProxyMutableConfigValues::CreateFromParams(params_.get());
  }

  DataReductionProxyMutableConfigValues* mutable_config_values() const {
    return mutable_config_values_.get();
  }

 private:
  std::unique_ptr<DataReductionProxyParams> params_;
  std::unique_ptr<DataReductionProxyMutableConfigValues> mutable_config_values_;
};

TEST_F(DataReductionProxyMutableConfigValuesTest, UpdateValuesAndInvalidate) {
  Init();
  EXPECT_EQ(std::vector<net::ProxyServer>(),
            mutable_config_values()->proxies_for_http());

  std::vector<net::ProxyServer> proxies_for_http;
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      "http://first.net", net::ProxyServer::SCHEME_HTTP));
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      "http://second.net", net::ProxyServer::SCHEME_HTTP));

  mutable_config_values()->UpdateValues(proxies_for_http);
  EXPECT_EQ(proxies_for_http, mutable_config_values()->proxies_for_http());

  mutable_config_values()->Invalidate();
  EXPECT_EQ(std::vector<net::ProxyServer>(),
            mutable_config_values()->proxies_for_http());
}

// Tests if HTTP proxies are overridden when |kDataReductionProxyHttpProxies|
// switch is specified.
TEST_F(DataReductionProxyMutableConfigValuesTest, OverrideProxiesForHttp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyHttpProxies,
      "http://override-first.net;http://override-second.net");
  Init();

  EXPECT_EQ(std::vector<net::ProxyServer>(),
            mutable_config_values()->proxies_for_http());

  std::vector<net::ProxyServer> proxies_for_http;
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      "http://first.net", net::ProxyServer::SCHEME_HTTP));
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      "http://second.net", net::ProxyServer::SCHEME_HTTP));

  mutable_config_values()->UpdateValues(proxies_for_http);

  std::vector<net::ProxyServer> expected_override_proxies_for_http;
  expected_override_proxies_for_http.push_back(net::ProxyServer::FromURI(
      "http://override-first.net", net::ProxyServer::SCHEME_HTTP));
  expected_override_proxies_for_http.push_back(net::ProxyServer::FromURI(
      "http://override-second.net", net::ProxyServer::SCHEME_HTTP));

  EXPECT_EQ(expected_override_proxies_for_http,
            mutable_config_values()->proxies_for_http());

  mutable_config_values()->Invalidate();
  EXPECT_EQ(std::vector<net::ProxyServer>(),
            mutable_config_values()->proxies_for_http());
}

// Tests if HTTP proxies are overridden when |kDataReductionProxy| or
// |kDataReductionProxyFallback| switches are specified.
TEST_F(DataReductionProxyMutableConfigValuesTest, OverrideDataReductionProxy) {
  const struct {
    bool set_primary;
    bool set_fallback;
  } tests[] = {
      {false, false}, {true, false}, {false, true}, {true, true},
  };

  for (const auto& test : tests) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);
    if (test.set_primary) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxy, "http://override-first.net");
    }
    if (test.set_fallback) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyFallback, "http://override-second.net");
    }
    Init();

    EXPECT_EQ(std::vector<net::ProxyServer>(),
              mutable_config_values()->proxies_for_http());

    std::vector<net::ProxyServer> proxies_for_http;
    if (test.set_primary) {
      proxies_for_http.push_back(net::ProxyServer::FromURI(
          "http://first.net", net::ProxyServer::SCHEME_HTTP));
    }
    if (test.set_fallback) {
      proxies_for_http.push_back(net::ProxyServer::FromURI(
          "http://second.net", net::ProxyServer::SCHEME_HTTP));
    }

    mutable_config_values()->UpdateValues(proxies_for_http);

    std::vector<net::ProxyServer> expected_override_proxies_for_http;
    if (test.set_primary) {
      expected_override_proxies_for_http.push_back(net::ProxyServer::FromURI(
          "http://override-first.net", net::ProxyServer::SCHEME_HTTP));
    }
    if (test.set_fallback) {
      expected_override_proxies_for_http.push_back(net::ProxyServer::FromURI(
          "http://override-second.net", net::ProxyServer::SCHEME_HTTP));
    }

    EXPECT_EQ(expected_override_proxies_for_http,
              mutable_config_values()->proxies_for_http());

    mutable_config_values()->Invalidate();
    EXPECT_EQ(std::vector<net::ProxyServer>(),
              mutable_config_values()->proxies_for_http());
  }
}

}  // namespace

}  // namespace data_reduction_proxy
