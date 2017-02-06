// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/proxy/proxy_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {
class DataReductionProxyParamsTest : public testing::Test {
 public:
  void CheckParams(const TestDataReductionProxyParams& params,
                   bool expected_init_result,
                   bool expected_allowed,
                   bool expected_fallback_allowed,
                   bool expected_promo_allowed) {
    EXPECT_EQ(expected_init_result, params.init_result());
    EXPECT_EQ(expected_allowed, params.allowed());
    EXPECT_EQ(expected_fallback_allowed, params.fallback_allowed());
    EXPECT_EQ(expected_promo_allowed, params.promo_allowed());
  }
  void CheckValues(const TestDataReductionProxyParams& params,
                   const std::string& expected_origin,
                   const std::string& expected_fallback_origin,
                   const std::string& expected_secure_proxy_check_url) {
    std::vector<net::ProxyServer> proxies_for_http;
    if (!expected_origin.empty()) {
      proxies_for_http.push_back(net::ProxyServer::FromURI(
          expected_origin, net::ProxyServer::SCHEME_HTTP));
    }

    if (!expected_fallback_origin.empty()) {
      proxies_for_http.push_back(net::ProxyServer::FromURI(
          expected_fallback_origin, net::ProxyServer::SCHEME_HTTP));
    }

    EXPECT_THAT(proxies_for_http,
                testing::ContainerEq(params.proxies_for_http()));
    EXPECT_EQ(GURL(expected_secure_proxy_check_url),
              params.secure_proxy_check_url());
  }
};

TEST_F(DataReductionProxyParamsTest, EverythingDefined) {
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING);
  CheckParams(params, true, true, true, true);
  CheckValues(params, TestDataReductionProxyParams::DefaultOrigin(),
              TestDataReductionProxyParams::DefaultFallbackOrigin(),
              TestDataReductionProxyParams::DefaultSecureProxyCheckURL());
}

TEST_F(DataReductionProxyParamsTest, Flags) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxy,
      TestDataReductionProxyParams::FlagOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyFallback,
      TestDataReductionProxyParams::FlagFallbackOrigin());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxySecureProxyCheckURL,
      TestDataReductionProxyParams::FlagSecureProxyCheckURL());
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING);
  CheckParams(params, true, true, true, true);
  CheckValues(params, TestDataReductionProxyParams::FlagOrigin(),
              TestDataReductionProxyParams::FlagFallbackOrigin(),
              TestDataReductionProxyParams::FlagSecureProxyCheckURL());
}

TEST_F(DataReductionProxyParamsTest, CarrierTestFlag) {
  static const char kCarrierTestOrigin[] =
      "http://o-o.preferred.nttdocomodcp-hnd1.proxy-dev.googlezip.net:80";
  base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableDataReductionProxyCarrierTest, kCarrierTestOrigin);
  DataReductionProxyParams params(DataReductionProxyParams::kAllowed);
  std::vector<net::ProxyServer> proxies_for_http;
  proxies_for_http.push_back(net::ProxyServer::FromURI(
      kCarrierTestOrigin, net::ProxyServer::SCHEME_HTTP));
  EXPECT_THAT(params.proxies_for_http(),
              testing::ContainerEq(proxies_for_http));
}

TEST_F(DataReductionProxyParamsTest, InvalidConfigurations) {
  const struct {
    bool allowed;
    bool fallback_allowed;
    bool promo_allowed;
    unsigned int missing_definitions;
    bool expected_result;
  } tests[] = {
      {true, true, true, TestDataReductionProxyParams::HAS_NOTHING, true},
      {true, false, true, TestDataReductionProxyParams::HAS_NOTHING, true},
      {false, true, true, TestDataReductionProxyParams::HAS_NOTHING, false},
      {true, true, true, TestDataReductionProxyParams::HAS_ORIGIN, false},
      {true, false, true, TestDataReductionProxyParams::HAS_ORIGIN, false},
      {false, true, true, TestDataReductionProxyParams::HAS_ORIGIN, false},
      {true, true, true, TestDataReductionProxyParams::HAS_NOTHING, true},
      {true, false, true, TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
       true},
      {false, true, true, TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
       false},
      {true, true, true, TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
       false},
      {true, true, true,
       TestDataReductionProxyParams::HAS_SECURE_PROXY_CHECK_URL, false},
      {true, false, true,
       TestDataReductionProxyParams::HAS_SECURE_PROXY_CHECK_URL, false},
      {false, true, true,
       TestDataReductionProxyParams::HAS_SECURE_PROXY_CHECK_URL, false},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    int flags = 0;
    if (tests[i].allowed)
      flags |= DataReductionProxyParams::kAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    if (tests[i].promo_allowed)
      flags |= DataReductionProxyParams::kPromoAllowed;
    TestDataReductionProxyParams params(
        flags,
        TestDataReductionProxyParams::HAS_EVERYTHING &
            ~(tests[i].missing_definitions));
    EXPECT_EQ(tests[i].expected_result, params.init_result()) << i;
  }
}

TEST_F(DataReductionProxyParamsTest, AndroidOnePromoFieldTrial) {
  EXPECT_TRUE(params::IsIncludedInAndroidOnePromoFieldTrial(
      "google/sprout/sprout:4.4.4/KPW53/1379542:user/release-keys"));
  EXPECT_FALSE(params::IsIncludedInAndroidOnePromoFieldTrial(
      "google/hammerhead/hammerhead:5.0/LRX210/1570415:user/release-keys"));
}

TEST_F(DataReductionProxyParamsTest, IsClientConfigEnabled) {
  const struct {
    std::string test_case;
    std::string trial_group_value;
    bool expected;
  } tests[] = {
      {
          "Nothing set", "", true,
      },
      {
          "Enabled in experiment", "Enabled", true,
      },
      {
          "Alternate enabled in experiment", "Enabled_Other", true,
      },
      {
          "Control in experiment", "Control", true,
      },
      {
          "Disabled in experiment", "Disabled", false,
      },
      {
          "Disabled in experiment", "Disabled_Other", false,
      },
      {
          "disabled in experiment lower case", "disabled", true,
      },
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);
    if (!test.trial_group_value.empty()) {
      ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
          "DataReductionProxyConfigService", test.trial_group_value));
    }
    EXPECT_EQ(test.expected, params::IsConfigClientEnabled()) << test.test_case;
  }
}

TEST_F(DataReductionProxyParamsTest, AreServerExperimentsEnabled) {
  const struct {
    std::string test_case;
    std::string trial_group_value;
    bool disable_flag_set;
    bool expected;
  } tests[] = {
      {
          "Field trial not set", "", false, true,
      },
      {
          "Field trial not set, flag set", "", true, false,
      },
      {
          "Enabled", "Enabled", false, true,
      },
      {
          "Enabled via field trial but disabled via flag", "Enabled", true,
          false,
      },
      {
          "Disabled via field trial", "Disabled", false, false,
      },
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);
    if (!test.trial_group_value.empty()) {
      ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
          "DataReductionProxyServerExperiments", test.trial_group_value));
    }

    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);
    if (test.disable_flag_set) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyServerExperimentsDisabled, "");
    }
    EXPECT_EQ(test.expected, params::IsIncludedInServerExperimentsFieldTrial())
        << test.test_case;
  }
}

TEST_F(DataReductionProxyParamsTest, IsTamperDetectionEnabled) {
  const struct {
    std::string test_case;
    std::string trial_group_value;
    bool disable_flag_set;
    bool expected;
  } tests[] = {
      {
          "Field trial not set", "", false, false,
      },
      {
          "Field trial not set, flag set", "", true, false,
      },
      {
          "Enabled", "Enabled", false, false,
      },
      {
          "TamperDetection_Enabled but disabled via flag",
          "TamperDetection_Enabled", true, false,
      },
      {
          "TamperDetection_Enabled", "TamperDetection_Enabled", false, true,
      },
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);
    if (!test.trial_group_value.empty()) {
      ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
          "DataReductionProxyServerExperiments", test.trial_group_value));
    }

    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);
    if (test.disable_flag_set) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyServerExperimentsDisabled, "");
    }
    EXPECT_EQ(test.expected, params::IsIncludedInTamperDetectionExperiment())
        << test.test_case;
  }
}

TEST_F(DataReductionProxyParamsTest, SecureProxyCheckDefault) {
  struct {
    bool command_line_set;
    bool experiment_enabled;
    bool in_trial_group;
    bool expected_use_by_default;
  } test_cases[]{
      {
       false, false, false, true,
      },
      {
       true, false, false, false,
      },
      {
       true, true, false, false,
      },
      {
       true, true, true, false,
      },
      {
       false, true, true, false,
      },
      {
       false, true, false, true,
      },
  };

  int test_index = 0;
  for (const auto& test_case : test_cases) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);

    base::FieldTrialList trial_list(nullptr);
    if (test_case.command_line_set) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyStartSecureDisabled, "");
    }

    if (test_case.experiment_enabled) {
      ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
          "DataReductionProxySecureProxyAfterCheck",
          test_case.in_trial_group ? "Enabled" : "Disabled"));
    }

    EXPECT_EQ(test_case.expected_use_by_default,
              params::ShouldUseSecureProxyByDefault())
        << test_index;
    test_index++;
  }
}

// Tests if Lo-Fi field trial is set correctly.
TEST_F(DataReductionProxyParamsTest, LoFiEnabledFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
    bool expected_control;
    bool expected_preview_enabled;
  } tests[] = {
      {"Enabled", true, false, false},
      {"Enabled_Control", true, false, false},
      {"Disabled", false, false, false},
      {"enabled", false, false, false},
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        params::GetLoFiFieldTrialName(), test.trial_group_name));
    EXPECT_EQ(test.expected_enabled,
              params::IsIncludedInLoFiEnabledFieldTrial())
        << test.trial_group_name;
    EXPECT_EQ(test.expected_control,
              params::IsIncludedInLoFiControlFieldTrial())
        << test.trial_group_name;
    EXPECT_EQ(test.expected_preview_enabled,
              params::IsIncludedInLoFiPreviewFieldTrial())
        << test.trial_group_name;
  }
}

// Tests if Lo-Fi field trial is set correctly.
TEST_F(DataReductionProxyParamsTest, LoFiControlFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
    bool expected_control;
    bool expected_preview_enabled;
  } tests[] = {
      {"Control", false, true, false},
      {"Control_Enabled", false, true, false},
      {"Disabled", false, false, false},
      {"control", false, false, false},
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        params::GetLoFiFieldTrialName(), test.trial_group_name));
    EXPECT_EQ(test.expected_enabled,
              params::IsIncludedInLoFiEnabledFieldTrial())
        << test.trial_group_name;
    EXPECT_EQ(test.expected_control,
              params::IsIncludedInLoFiControlFieldTrial())
        << test.trial_group_name;
    EXPECT_EQ(test.expected_preview_enabled,
              params::IsIncludedInLoFiPreviewFieldTrial())
        << test.trial_group_name;
  }
}

// Tests if Lo-Fi field trial is set correctly.
TEST_F(DataReductionProxyParamsTest, LoFiPreviewFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
    bool expected_control;
    bool expected_preview_enabled;
  } tests[] = {
      {"Enabled_Preview", true, false, true},
      {"Enabled_Preview_Control", true, false, true},
      {"Disabled", false, false, false},
      {"enabled_Preview", false, false, false},
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        params::GetLoFiFieldTrialName(), test.trial_group_name));
    EXPECT_EQ(test.expected_enabled,
              params::IsIncludedInLoFiEnabledFieldTrial())
        << test.trial_group_name;
    EXPECT_EQ(test.expected_control,
              params::IsIncludedInLoFiControlFieldTrial())
        << test.trial_group_name;
    EXPECT_EQ(test.expected_preview_enabled,
              params::IsIncludedInLoFiPreviewFieldTrial())
        << test.trial_group_name;
  }
}

TEST_F(DataReductionProxyParamsTest, HoldbackEnabledFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
  } tests[] = {
      {"Enabled", true},
      {"Enabled_Control", true},
      {"Disabled", false},
      {"enabled", false},
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "DataCompressionProxyHoldback", test.trial_group_name));
    EXPECT_EQ(test.expected_enabled, params::IsIncludedInHoldbackFieldTrial())
        << test.trial_group_name;
  }
}

TEST_F(DataReductionProxyParamsTest, PromoFieldTrial) {
  const struct {
    std::string trial_group_name;
    bool expected_enabled;
  } tests[] = {
      {"Enabled", true},
      {"Enabled_Control", true},
      {"Disabled", false},
      {"enabled", false},
  };

  for (const auto& test : tests) {
    base::FieldTrialList field_trial_list(nullptr);

    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "DataCompressionProxyPromoVisibility", test.trial_group_name));
    EXPECT_EQ(test.expected_enabled, params::IsIncludedInPromoFieldTrial())
        << test.trial_group_name;
  }
}

TEST_F(DataReductionProxyParamsTest, GetConfigServiceURL) {
  const struct {
    std::string test_case;
    std::string flag_value;
    GURL expected;
  } tests[] = {
      {
          "Nothing set", "",
          GURL("https://datasaver.googleapis.com/v1/clientConfigs"),
      },
      {
          "Only command line set", "http://commandline.config-service/",
          GURL("http://commandline.config-service/"),
      },
  };

  for (const auto& test : tests) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);
    if (!test.flag_value.empty()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyConfigURL, test.flag_value);
    }
    EXPECT_EQ(test.expected, params::GetConfigServiceURL()) << test.test_case;
  }
}

TEST(DataReductionProxyParamsStandaloneTest, OverrideProxiesForHttp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyHttpProxies,
      "http://override-first.net;http://override-second.net");
  DataReductionProxyParams params(
      DataReductionProxyParams::kAllowAllProxyConfigurations);

  std::vector<net::ProxyServer> expected_override_proxies_for_http;
  expected_override_proxies_for_http.push_back(net::ProxyServer::FromURI(
      "http://override-first.net", net::ProxyServer::SCHEME_HTTP));
  expected_override_proxies_for_http.push_back(net::ProxyServer::FromURI(
      "http://override-second.net", net::ProxyServer::SCHEME_HTTP));

  EXPECT_EQ(expected_override_proxies_for_http, params.proxies_for_http());
}

}  // namespace data_reduction_proxy
