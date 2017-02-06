// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"

#include <stdint.h>

#include "base/strings/string_number_conversions.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxyPrefsTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterPrefs(local_state_prefs_.registry());
    PrefRegistrySimple* profile_registry = profile_prefs_.registry();
    RegisterPrefs(profile_registry);
    profile_registry->RegisterBooleanPref(
        prefs::kStatisticsPrefsMigrated, false);
  }

  PrefService* local_state_prefs() {
    return &local_state_prefs_;
  }

  PrefService* profile_prefs() {
    return &profile_prefs_;
  }

  // Initializes a list with ten string representations of successive int64_t
  // values, starting with |starting_value|.
  void InitializeList(const char* pref_name,
                      int64_t starting_value,
                      PrefService* pref_service) {
    ListPrefUpdate list(local_state_prefs(), pref_name);
    for (int64_t i = 0; i < 10L; ++i) {
      list->Set(i, new base::StringValue(
          base::Int64ToString(i + starting_value)));
    }
  }

  // Verifies that ten string repreentations of successive int64_t values
  // starting with |starting_value| are found in the |ListValue| with the
  // associated |pref_name|.
  void VerifyList(const char* pref_name,
                  int64_t starting_value,
                  PrefService* pref_service) {
    const base::ListValue* list_value = pref_service->GetList(pref_name);
    for (int64_t i = 0; i < 10L; ++i) {
      std::string string_value;
      int64_t value;
      list_value->GetString(i, &string_value);
      base::StringToInt64(string_value, &value);
      EXPECT_EQ(i + starting_value, value);
    }
  }

 private:
  void RegisterPrefs(PrefRegistrySimple* registry) {
    registry->RegisterInt64Pref(prefs::kHttpReceivedContentLength, 0);
    registry->RegisterInt64Pref(prefs::kHttpOriginalContentLength, 0);

    registry->RegisterListPref(prefs::kDailyHttpOriginalContentLength);
    registry->RegisterListPref(prefs::kDailyHttpReceivedContentLength);
    registry->RegisterListPref(
        prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        prefs::kDailyContentLengthWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled);
    registry->RegisterListPref(
        prefs::kDailyOriginalContentLengthViaDataReductionProxy);
    registry->RegisterListPref(prefs::kDailyContentLengthViaDataReductionProxy);
    registry->RegisterInt64Pref(
        prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
  }

  TestingPrefServiceSimple local_state_prefs_;
  TestingPrefServiceSimple profile_prefs_;
};

TEST_F(DataReductionProxyPrefsTest, TestMigration) {
  local_state_prefs()->SetInt64(prefs::kHttpReceivedContentLength, 123L);
  local_state_prefs()->SetInt64(prefs::kHttpOriginalContentLength, 234L);
  local_state_prefs()->SetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate, 345L);
  InitializeList(
      prefs::kDailyHttpOriginalContentLength, 0L, local_state_prefs());
  InitializeList(
      prefs::kDailyHttpReceivedContentLength, 1L, local_state_prefs());
  InitializeList(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled,
      2L,
      local_state_prefs());
  InitializeList(
      prefs::kDailyContentLengthWithDataReductionProxyEnabled,
      3L,
      local_state_prefs());
  InitializeList(
      prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled,
      4L,
      local_state_prefs());
  InitializeList(
      prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled,
      5L,
      local_state_prefs());
  InitializeList(
      prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled,
      6L,
      local_state_prefs());
  InitializeList(
      prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled,
      7L,
      local_state_prefs());
  InitializeList(
      prefs::kDailyOriginalContentLengthViaDataReductionProxy,
      8L,
      local_state_prefs());
  InitializeList(
      prefs::kDailyContentLengthViaDataReductionProxy,
      9L,
      local_state_prefs());

  EXPECT_EQ(0L, profile_prefs()->GetInt64(prefs::kHttpReceivedContentLength));
  EXPECT_EQ(0L, profile_prefs()->GetInt64(prefs::kHttpOriginalContentLength));
  EXPECT_EQ(0u, profile_prefs()->GetList(
      prefs::kDailyHttpOriginalContentLength)->GetSize());
  EXPECT_EQ(0u, profile_prefs()->GetList(
      prefs::kDailyHttpReceivedContentLength)->GetSize());
  EXPECT_EQ(0u, profile_prefs()->GetList(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled)->
          GetSize());
  EXPECT_EQ(0u, profile_prefs()->GetList(
      prefs::kDailyContentLengthWithDataReductionProxyEnabled)->GetSize());
  EXPECT_EQ(0u, profile_prefs()->GetList(
      prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled)->GetSize());
  EXPECT_EQ(0u, profile_prefs()->GetList(
      prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled)->
          GetSize());
  EXPECT_EQ(0u, profile_prefs()->GetList(
      prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled)->
          GetSize());
  EXPECT_EQ(0u, profile_prefs()->GetList(
      prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled)->
          GetSize());
  EXPECT_EQ(0u, profile_prefs()->GetList(
      prefs::kDailyOriginalContentLengthViaDataReductionProxy)->GetSize());
  EXPECT_EQ(0u, profile_prefs()->GetList(
      prefs::kDailyContentLengthViaDataReductionProxy)->GetSize());
  EXPECT_EQ(0L, profile_prefs()->GetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate));
  EXPECT_FALSE(profile_prefs()->GetBoolean(prefs::kStatisticsPrefsMigrated));

  data_reduction_proxy::MigrateStatisticsPrefs(local_state_prefs(),
                                               profile_prefs());

  EXPECT_EQ(123L, profile_prefs()->GetInt64(prefs::kHttpReceivedContentLength));
  EXPECT_EQ(234L, profile_prefs()->GetInt64(prefs::kHttpOriginalContentLength));
  VerifyList(prefs::kDailyHttpOriginalContentLength, 0L, profile_prefs());
  VerifyList(prefs::kDailyHttpReceivedContentLength, 1L, profile_prefs());
  VerifyList(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled,
      2L,
      profile_prefs());
  VerifyList(
      prefs::kDailyContentLengthWithDataReductionProxyEnabled,
      3L,
      profile_prefs());
  VerifyList(
      prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled,
      4L,
      profile_prefs());
  VerifyList(
      prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled,
      5L,
      profile_prefs());
  VerifyList(
      prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled,
      6L,
      profile_prefs());
  VerifyList(
      prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled,
      7L,
      profile_prefs());
  VerifyList(
      prefs::kDailyOriginalContentLengthViaDataReductionProxy,
      8L,
      profile_prefs());
  VerifyList(
      prefs::kDailyContentLengthViaDataReductionProxy,
      9L,
      profile_prefs());
  EXPECT_EQ(345L, profile_prefs()->GetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate));
  EXPECT_TRUE(profile_prefs()->GetBoolean(prefs::kStatisticsPrefsMigrated));

  // Migration should only happen once.
  local_state_prefs()->SetInt64(prefs::kHttpReceivedContentLength, 456L);
  data_reduction_proxy::MigrateStatisticsPrefs(local_state_prefs(),
                                               profile_prefs());
  EXPECT_EQ(123L, profile_prefs()->GetInt64(prefs::kHttpReceivedContentLength));
}

}  // namespace data_reduction_proxy
