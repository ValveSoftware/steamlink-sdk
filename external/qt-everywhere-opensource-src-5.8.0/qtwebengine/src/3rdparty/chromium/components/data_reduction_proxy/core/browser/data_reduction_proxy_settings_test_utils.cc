// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace {

const char kProxy[] = "proxy";

}  // namespace

namespace data_reduction_proxy {

DataReductionProxySettingsTestBase::DataReductionProxySettingsTestBase()
    : testing::Test() {
}

DataReductionProxySettingsTestBase::~DataReductionProxySettingsTestBase() {}

// testing::Test implementation:
void DataReductionProxySettingsTestBase::SetUp() {
  test_context_ =
      DataReductionProxyTestContext::Builder()
          .WithMockConfig()
          .WithMockDataReductionProxyService()
          .SkipSettingsInitialization()
          .Build();

  test_context_->SetDataReductionProxyEnabled(false);
  TestingPrefServiceSimple* pref_service = test_context_->pref_service();
  pref_service->SetInt64(prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
  pref_service->registry()->RegisterDictionaryPref(kProxy);
  pref_service->SetBoolean(prefs::kDataReductionProxyWasEnabledBefore, false);

  //AddProxyToCommandLine();
  ResetSettings(true, true, true, false);

  ListPrefUpdate original_update(test_context_->pref_service(),
                                 prefs::kDailyHttpOriginalContentLength);
  ListPrefUpdate received_update(test_context_->pref_service(),
                                 prefs::kDailyHttpReceivedContentLength);
  for (int64_t i = 0; i < kNumDaysInHistory; i++) {
    original_update->Insert(0,
                            new base::StringValue(base::Int64ToString(2 * i)));
    received_update->Insert(0, new base::StringValue(base::Int64ToString(i)));
  }
  last_update_time_ = base::Time::Now().LocalMidnight();
  pref_service->SetInt64(prefs::kDailyHttpContentLengthLastUpdateDate,
                         last_update_time_.ToInternalValue());
}

template <class C>
void DataReductionProxySettingsTestBase::ResetSettings(bool allowed,
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
  MockDataReductionProxySettings<C>* settings =
      new MockDataReductionProxySettings<C>();
  settings->config_ = test_context_->config();
  settings->prefs_ = test_context_->pref_service();
  settings->data_reduction_proxy_service_ =
      test_context_->CreateDataReductionProxyService(settings);
  test_context_->config()->ResetParamFlagsForTest(flags);
  settings->UpdateConfigValues();
  EXPECT_CALL(*settings, GetOriginalProfilePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(test_context_->pref_service()));
  EXPECT_CALL(*settings, GetLocalStatePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(test_context_->pref_service()));
  settings_.reset(settings);
}

// Explicitly generate required instantiations.
template void
DataReductionProxySettingsTestBase::ResetSettings<DataReductionProxySettings>(
    bool allowed,
    bool fallback_allowed,
    bool promo_allowed,
    bool holdback);

void DataReductionProxySettingsTestBase::ExpectSetProxyPrefs(
    bool expected_enabled,
    bool expected_at_startup) {
  MockDataReductionProxyService* mock_service =
      static_cast<MockDataReductionProxyService*>(
          settings_->data_reduction_proxy_service());
  EXPECT_CALL(*mock_service,
              SetProxyPrefs(expected_enabled, expected_at_startup));
}

void DataReductionProxySettingsTestBase::CheckOnPrefChange(
    bool enabled,
    bool expected_enabled,
    bool managed) {
  ExpectSetProxyPrefs(expected_enabled, false);
  if (managed) {
    test_context_->pref_service()->SetManagedPref(
        test_context_->GetDataReductionProxyEnabledPrefName(),
        new base::FundamentalValue(enabled));
  } else {
    test_context_->SetDataReductionProxyEnabled(enabled);
  }
  test_context_->RunUntilIdle();
  // Never expect the proxy to be restricted for pref change tests.
}

void DataReductionProxySettingsTestBase::InitDataReductionProxy(
    bool enabled_at_startup) {
  settings_->InitDataReductionProxySettings(
      test_context_->GetDataReductionProxyEnabledPrefName(),
      test_context_->pref_service(), test_context_->io_data(),
      test_context_->CreateDataReductionProxyService(settings_.get()));
  settings_->data_reduction_proxy_service()->SetIOData(
      test_context_->io_data()->GetWeakPtr());
  settings_->SetCallbackToRegisterSyntheticFieldTrial(
      base::Bind(&DataReductionProxySettingsTestBase::
                 SyntheticFieldTrialRegistrationCallback,
                 base::Unretained(this)));

  test_context_->RunUntilIdle();
}

void DataReductionProxySettingsTestBase::CheckDataReductionProxySyntheticTrial(
    bool enabled) {
  EXPECT_EQ(enabled ? "Enabled" : "Disabled",
      synthetic_field_trials_["SyntheticDataReductionProxySetting"]);
}

}  // namespace data_reduction_proxy
