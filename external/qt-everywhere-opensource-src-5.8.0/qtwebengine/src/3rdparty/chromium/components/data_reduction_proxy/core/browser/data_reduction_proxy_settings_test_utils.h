// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_

#include <map>
#include <memory>
#include <string>

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/prefs/testing_pref_service.h"
#include "net/log/test_net_log.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefService;
class TestingPrefServiceSimple;

namespace data_reduction_proxy {

class DataReductionProxyTestContext;
class MockDataReductionProxyConfig;

template <class C>
class MockDataReductionProxySettings : public C {
 public:
  MockDataReductionProxySettings<C>() : C() {
  }
  MOCK_METHOD0(GetOriginalProfilePrefs, PrefService*());
  MOCK_METHOD0(GetLocalStatePrefs, PrefService*());
  MOCK_METHOD1(RecordStartupState,
               void(ProxyStartupState state));
};

class DataReductionProxySettingsTestBase : public testing::Test {
 public:
  static void AddTestProxyToCommandLine();

  DataReductionProxySettingsTestBase();
  DataReductionProxySettingsTestBase(bool allowed,
                                     bool fallback_allowed,
                                     bool promo_allowed);
  ~DataReductionProxySettingsTestBase() override;

  void AddProxyToCommandLine();

  void SetUp() override;

  template <class C> void ResetSettings(bool allowed,
                                        bool fallback_allowed,
                                        bool promo_allowed,
                                        bool holdback);
  virtual void ResetSettings(bool allowed,
                             bool fallback_allowed,
                             bool promo_allowed,
                             bool holdback) = 0;

  void ExpectSetProxyPrefs(bool expected_enabled,
                           bool expected_at_startup);

  void CheckMaybeActivateDataReductionProxy(bool initially_enabled,
                                            bool request_succeeded,
                                            bool expected_enabled,
                                            bool expected_restricted,
                                            bool expected_fallback_restricted);
  void CheckOnPrefChange(bool enabled, bool expected_enabled, bool managed);
  void InitWithStatisticsPrefs();
  void InitDataReductionProxy(bool enabled_at_startup);
  void CheckDataReductionProxySyntheticTrial(bool enabled);
  bool SyntheticFieldTrialRegistrationCallback(const std::string& trial_name,
                                               const std::string& group_name) {
    synthetic_field_trials_[trial_name] = group_name;
    return true;
  }

  base::MessageLoopForIO message_loop_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  std::unique_ptr<DataReductionProxySettings> settings_;
  base::Time last_update_time_;
  std::map<std::string, std::string> synthetic_field_trials_;
};

// Test implementations should be subclasses of an instantiation of this
// class parameterized for whatever DataReductionProxySettings class
// is being tested.
template <class C>
class ConcreteDataReductionProxySettingsTest
    : public DataReductionProxySettingsTestBase {
 public:
  typedef MockDataReductionProxySettings<C> MockSettings;
  void ResetSettings(bool allowed,
                     bool fallback_allowed,
                     bool promo_allowed,
                     bool holdback) override {
    return DataReductionProxySettingsTestBase::ResetSettings<C>(
        allowed, fallback_allowed, promo_allowed, holdback);
  }
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_
