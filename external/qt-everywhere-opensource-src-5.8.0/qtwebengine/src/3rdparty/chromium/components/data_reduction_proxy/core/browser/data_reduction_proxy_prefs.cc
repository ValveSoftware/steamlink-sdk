// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"

#include <memory>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

void TransferPrefList(const char* pref_path,
                      PrefService* src,
                      PrefService* dest) {
  DCHECK(src->FindPreference(pref_path)->GetType() == base::Value::TYPE_LIST);
  ListPrefUpdate update_dest(dest, pref_path);
  std::unique_ptr<base::ListValue> src_list(
      src->GetList(pref_path)->DeepCopy());
  update_dest->Swap(src_list.get());
  ListPrefUpdate update_src(src, pref_path);
  src->ClearPref(pref_path);
}

}  // namespace

namespace data_reduction_proxy {

// Make sure any changes here that have the potential to impact android_webview
// are reflected in RegisterSimpleProfilePrefs.
void RegisterSyncableProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kDataReductionProxyWasEnabledBefore,
                                false);

  registry->RegisterBooleanPref(prefs::kDataUsageReportingEnabled, false);

  registry->RegisterInt64Pref(prefs::kHttpReceivedContentLength, 0);
  registry->RegisterInt64Pref(prefs::kHttpOriginalContentLength, 0);

  registry->RegisterBooleanPref(prefs::kStatisticsPrefsMigrated, false);
  registry->RegisterListPref(prefs::kDailyHttpOriginalContentLength);
  registry->RegisterInt64Pref(prefs::kDailyHttpOriginalContentLengthApplication,
                              0L);
  registry->RegisterInt64Pref(prefs::kDailyHttpOriginalContentLengthVideo, 0L);
  registry->RegisterInt64Pref(prefs::kDailyHttpOriginalContentLengthUnknown,
                              0L);

  registry->RegisterListPref(prefs::kDailyHttpReceivedContentLength);
  registry->RegisterInt64Pref(prefs::kDailyHttpReceivedContentLengthApplication,
                              0L);
  registry->RegisterInt64Pref(prefs::kDailyHttpReceivedContentLengthVideo, 0L);
  registry->RegisterInt64Pref(prefs::kDailyHttpReceivedContentLengthUnknown,
                              0L);

  registry->RegisterListPref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled);
  registry->RegisterInt64Pref(
      prefs::
          kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication,
      0L);
  registry->RegisterInt64Pref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown,
      0L);
  registry->RegisterListPref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabled);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledApplication, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledVideo, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledUnknown, 0L);
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
  registry->RegisterInt64Pref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyApplication, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyVideo, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyUnknown, 0L);
  registry->RegisterListPref(prefs::kDailyContentLengthViaDataReductionProxy);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthViaDataReductionProxyApplication, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthViaDataReductionProxyVideo, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthViaDataReductionProxyUnknown, 0L);

  registry->RegisterInt64Pref(prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
  registry->RegisterIntegerPref(prefs::kLoFiImplicitOptOutEpoch, 0);
  registry->RegisterIntegerPref(prefs::kLoFiSnackbarsShownPerSession, 0);
  registry->RegisterIntegerPref(prefs::kLoFiLoadImagesPerSession, 0);
  registry->RegisterIntegerPref(prefs::kLoFiConsecutiveSessionDisables, 0);
  registry->RegisterBooleanPref(prefs::kLoFiWasUsedThisSession, false);
  registry->RegisterInt64Pref(prefs::kSimulatedConfigRetrieveTime, 0L);
  registry->RegisterStringPref(prefs::kDataReductionProxyConfig, std::string());
}

void RegisterSimpleProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kDataReductionProxyWasEnabledBefore, false);

  registry->RegisterBooleanPref(prefs::kDataUsageReportingEnabled, false);

  registry->RegisterBooleanPref(
      prefs::kStatisticsPrefsMigrated, false);
  RegisterPrefs(registry);
}

// Add any new data reduction proxy prefs to the |pref_map_| or the
// |list_pref_map_| in Init() of DataReductionProxyCompressionStats.
void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDataReductionProxy, std::string());
  registry->RegisterInt64Pref(
      prefs::kHttpReceivedContentLength, 0);
  registry->RegisterInt64Pref(
      prefs::kHttpOriginalContentLength, 0);
  registry->RegisterListPref(
      prefs::kDailyHttpOriginalContentLength);
  registry->RegisterInt64Pref(prefs::kDailyHttpOriginalContentLengthApplication,
                              0L);
  registry->RegisterInt64Pref(prefs::kDailyHttpOriginalContentLengthVideo, 0L);
  registry->RegisterInt64Pref(prefs::kDailyHttpOriginalContentLengthUnknown,
                              0L);
  registry->RegisterListPref(prefs::kDailyHttpReceivedContentLength);
  registry->RegisterInt64Pref(prefs::kDailyHttpReceivedContentLengthApplication,
                              0L);
  registry->RegisterInt64Pref(prefs::kDailyHttpReceivedContentLengthVideo, 0L);
  registry->RegisterInt64Pref(prefs::kDailyHttpReceivedContentLengthUnknown,
                              0L);
  registry->RegisterListPref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled);
  registry->RegisterInt64Pref(
      prefs::
          kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication,
      0L);
  registry->RegisterInt64Pref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown,
      0L);
  registry->RegisterListPref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabled);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledApplication, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledVideo, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledUnknown, 0L);
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
  registry->RegisterInt64Pref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyApplication, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyVideo, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyUnknown, 0L);
  registry->RegisterListPref(prefs::kDailyContentLengthViaDataReductionProxy);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthViaDataReductionProxyApplication, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthViaDataReductionProxyVideo, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyContentLengthViaDataReductionProxyUnknown, 0L);
  registry->RegisterInt64Pref(
      prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
  registry->RegisterIntegerPref(prefs::kLoFiImplicitOptOutEpoch, 0);
  registry->RegisterIntegerPref(prefs::kLoFiSnackbarsShownPerSession, 0);
  registry->RegisterIntegerPref(prefs::kLoFiLoadImagesPerSession, 0);
  registry->RegisterIntegerPref(prefs::kLoFiConsecutiveSessionDisables, 0);
  registry->RegisterBooleanPref(prefs::kLoFiWasUsedThisSession, false);
  registry->RegisterInt64Pref(prefs::kSimulatedConfigRetrieveTime, 0L);
  registry->RegisterStringPref(prefs::kDataReductionProxyConfig, std::string());
}

void MigrateStatisticsPrefs(PrefService* local_state_prefs,
                            PrefService* profile_prefs) {
  if (profile_prefs->GetBoolean(prefs::kStatisticsPrefsMigrated))
    return;
  if (local_state_prefs == profile_prefs) {
    profile_prefs->SetBoolean(prefs::kStatisticsPrefsMigrated, true);
    return;
  }
  profile_prefs->SetInt64(
      prefs::kHttpReceivedContentLength,
      local_state_prefs->GetInt64(prefs::kHttpReceivedContentLength));
  local_state_prefs->ClearPref(prefs::kHttpReceivedContentLength);
  profile_prefs->SetInt64(
      prefs::kHttpOriginalContentLength,
      local_state_prefs->GetInt64(prefs::kHttpOriginalContentLength));
  local_state_prefs->ClearPref(prefs::kHttpOriginalContentLength);
  TransferPrefList(
      prefs::kDailyHttpOriginalContentLength, local_state_prefs, profile_prefs);
  TransferPrefList(
      prefs::kDailyHttpReceivedContentLength, local_state_prefs, profile_prefs);
  TransferPrefList(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyOriginalContentLengthViaDataReductionProxy,
      local_state_prefs,
      profile_prefs);
  TransferPrefList(
      prefs::kDailyContentLengthViaDataReductionProxy,
      local_state_prefs,
      profile_prefs);
  profile_prefs->SetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate,
      local_state_prefs->GetInt64(
          prefs::kDailyHttpContentLengthLastUpdateDate));
  local_state_prefs->ClearPref(prefs::kDailyHttpContentLengthLastUpdateDate);
  profile_prefs->SetBoolean(prefs::kStatisticsPrefsMigrated, true);
}

}  // namespace data_reduction_proxy
