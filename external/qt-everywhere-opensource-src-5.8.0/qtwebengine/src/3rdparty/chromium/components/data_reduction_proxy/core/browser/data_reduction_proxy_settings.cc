// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "net/base/network_change_notifier.h"

namespace {

// Key of the UMA DataReductionProxy.StartupState histogram.
const char kUMAProxyStartupStateHistogram[] =
    "DataReductionProxy.StartupState";

void RecordSettingsEnabledState(
    data_reduction_proxy::DataReductionSettingsEnabledAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "DataReductionProxy.EnabledState", action,
      data_reduction_proxy::DATA_REDUCTION_SETTINGS_ACTION_BOUNDARY);
}

}  // namespace

namespace data_reduction_proxy {

const char kDataReductionPassThroughHeader[] =
    "Chrome-Proxy: pass-through\nCache-Control: no-cache";

DataReductionProxySettings::DataReductionProxySettings()
    : unreachable_(false),
      deferred_initialization_(false),
      allowed_(false),
      promo_allowed_(false),
      lo_fi_mode_active_(false),
      lo_fi_load_image_requested_(false),
      data_reduction_proxy_enabled_pref_name_(),
      prefs_(NULL),
      config_(nullptr) {
  lo_fi_user_requests_for_images_per_session_ =
      params::GetFieldTrialParameterAsInteger(
          params::GetLoFiFieldTrialName(), "load_images_requests_per_session",
          3, 0);
  lo_fi_consecutive_session_disables_ = params::GetFieldTrialParameterAsInteger(
      params::GetLoFiFieldTrialName(), "consecutive_session_disables", 3, 0);
}

DataReductionProxySettings::~DataReductionProxySettings() {
  if (allowed_)
    spdy_proxy_auth_enabled_.Destroy();
}

void DataReductionProxySettings::InitPrefMembers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  spdy_proxy_auth_enabled_.Init(
      data_reduction_proxy_enabled_pref_name_, GetOriginalProfilePrefs(),
      base::Bind(&DataReductionProxySettings::OnProxyEnabledPrefChange,
                 base::Unretained(this)));
}

void DataReductionProxySettings::UpdateConfigValues() {
  DCHECK(config_);
  allowed_ = config_->allowed();
  promo_allowed_ = config_->promo_allowed();
}

void DataReductionProxySettings::InitDataReductionProxySettings(
    const std::string& data_reduction_proxy_enabled_pref_name,
    PrefService* prefs,
    DataReductionProxyIOData* io_data,
    std::unique_ptr<DataReductionProxyService> data_reduction_proxy_service) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!data_reduction_proxy_enabled_pref_name.empty());
  DCHECK(prefs);
  DCHECK(io_data);
  DCHECK(io_data->config());
  DCHECK(data_reduction_proxy_service.get());
  data_reduction_proxy_enabled_pref_name_ =
      data_reduction_proxy_enabled_pref_name;
  prefs_ = prefs;
  config_ = io_data->config();
  data_reduction_proxy_service_ = std::move(data_reduction_proxy_service);
  data_reduction_proxy_service_->AddObserver(this);
  InitPrefMembers();
  UpdateConfigValues();
  RecordDataReductionInit();
  data_reduction_proxy_service_->InitializeLoFiPrefs();
}

void DataReductionProxySettings::OnServiceInitialized() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!deferred_initialization_)
    return;
  deferred_initialization_ = false;
  // Technically, this is not "at startup", but this is the first chance that
  // IO data objects can be called.
  UpdateIOData(true);
}

void DataReductionProxySettings::SetCallbackToRegisterSyntheticFieldTrial(
    const SyntheticFieldTrialRegistrationCallback&
        on_data_reduction_proxy_enabled) {
  register_synthetic_field_trial_ = on_data_reduction_proxy_enabled;
  RegisterDataReductionProxyFieldTrial();
}

bool DataReductionProxySettings::IsDataReductionProxyEnabled() const {
  return spdy_proxy_auth_enabled_.GetValue() ||
         params::ShouldForceEnableDataReductionProxy();
}

bool DataReductionProxySettings::CanUseDataReductionProxy(
    const GURL& url) const {
  return url.is_valid() && url.scheme() == url::kHttpScheme &&
      IsDataReductionProxyEnabled();
}

bool DataReductionProxySettings::IsDataReductionProxyManaged() {
  return spdy_proxy_auth_enabled_.IsManaged();
}

void DataReductionProxySettings::SetDataReductionProxyEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!allowed_)
    return;

  if (spdy_proxy_auth_enabled_.GetValue() != enabled) {
    spdy_proxy_auth_enabled_.SetValue(enabled);
    OnProxyEnabledPrefChange();
  }
}

int64_t DataReductionProxySettings::GetDataReductionLastUpdateTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());
  return
      data_reduction_proxy_service_->compression_stats()->GetLastUpdateTime();
}

void DataReductionProxySettings::SetUnreachable(bool unreachable) {
  unreachable_ = unreachable;
}

bool DataReductionProxySettings::IsDataReductionProxyUnreachable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return unreachable_;
}

PrefService* DataReductionProxySettings::GetOriginalProfilePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return prefs_;
}

void DataReductionProxySettings::SetLoFiModeActiveOnMainFrame(
    bool lo_fi_mode_active) {
  if (prefs_ && lo_fi_mode_active)
    prefs_->SetBoolean(prefs::kLoFiWasUsedThisSession, true);
  lo_fi_load_image_requested_ = false;
  lo_fi_mode_active_ = lo_fi_mode_active;
}

bool DataReductionProxySettings::WasLoFiModeActiveOnMainFrame() const {
  return lo_fi_mode_active_;
}

bool DataReductionProxySettings::WasLoFiLoadImageRequestedBefore() {
  return lo_fi_load_image_requested_;
}

void DataReductionProxySettings::SetLoFiLoadImageRequested() {
  lo_fi_load_image_requested_ = true;
}

void DataReductionProxySettings::IncrementLoFiSnackbarShown() {
  prefs_->SetInteger(
      prefs::kLoFiSnackbarsShownPerSession,
      prefs_->GetInteger(prefs::kLoFiSnackbarsShownPerSession) + 1);
}

void DataReductionProxySettings::IncrementLoFiUserRequestsForImages() {
  if (!prefs_ || params::IsLoFiOnViaFlags())
    return;
  prefs_->SetInteger(prefs::kLoFiLoadImagesPerSession,
                     prefs_->GetInteger(prefs::kLoFiLoadImagesPerSession) + 1);
  if (prefs_->GetInteger(prefs::kLoFiLoadImagesPerSession) >=
      lo_fi_user_requests_for_images_per_session_) {
    data_reduction_proxy_service_->SetLoFiModeOff();
    prefs_->SetInteger(
        prefs::kLoFiConsecutiveSessionDisables,
        prefs_->GetInteger(prefs::kLoFiConsecutiveSessionDisables) + 1);
    RecordLoFiImplicitOptOutAction(LO_FI_OPT_OUT_ACTION_DISABLED_FOR_SESSION);
    if (prefs_->GetInteger(prefs::kLoFiConsecutiveSessionDisables) >=
        lo_fi_consecutive_session_disables_) {
      RecordLoFiImplicitOptOutAction(
          LO_FI_OPT_OUT_ACTION_DISABLED_UNTIL_NEXT_EPOCH);
    }
  }
}

void DataReductionProxySettings::RegisterDataReductionProxyFieldTrial() {
  register_synthetic_field_trial_.Run(
      "SyntheticDataReductionProxySetting",
      IsDataReductionProxyEnabled() ? "Enabled" : "Disabled");
}

void DataReductionProxySettings::OnProxyEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!register_synthetic_field_trial_.is_null()) {
    RegisterDataReductionProxyFieldTrial();
  }
  if (!allowed_)
    return;
  MaybeActivateDataReductionProxy(false);
}

void DataReductionProxySettings::ResetDataReductionStatistics() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());
  data_reduction_proxy_service_->compression_stats()->ResetStatistics();
}

void DataReductionProxySettings::UpdateIOData(bool at_startup) {
  data_reduction_proxy_service_->SetProxyPrefs(IsDataReductionProxyEnabled(),
                                               at_startup);
}

void DataReductionProxySettings::MaybeActivateDataReductionProxy(
    bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PrefService* prefs = GetOriginalProfilePrefs();
  // Do nothing if prefs have not been initialized. This allows unit testing
  // of profile related code without having to initialize data reduction proxy
  // related prefs.
  if (!prefs)
    return;
  if (spdy_proxy_auth_enabled_.GetValue() &&
      !prefs->GetBoolean(prefs::kDataReductionProxyWasEnabledBefore)) {
    prefs->SetBoolean(prefs::kDataReductionProxyWasEnabledBefore, true);
    ResetDataReductionStatistics();
  }
  if (!at_startup) {
    if (IsDataReductionProxyEnabled())
      RecordSettingsEnabledState(DATA_REDUCTION_SETTINGS_ACTION_OFF_TO_ON);
    else
      RecordSettingsEnabledState(DATA_REDUCTION_SETTINGS_ACTION_ON_TO_OFF);
  }
  // Configure use of the data reduction proxy if it is enabled.
  if (at_startup && !data_reduction_proxy_service_->Initialized())
    deferred_initialization_ = true;
  else
    UpdateIOData(at_startup);
}

DataReductionProxyEventStore* DataReductionProxySettings::GetEventStore()
    const {
  if (data_reduction_proxy_service_)
    return data_reduction_proxy_service_->event_store();

  return nullptr;
}

// Metrics methods
void DataReductionProxySettings::RecordDataReductionInit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProxyStartupState state = PROXY_NOT_AVAILABLE;
  if (allowed_) {
    if (IsDataReductionProxyEnabled())
      state = PROXY_ENABLED;
    else
      state = PROXY_DISABLED;
  }

  RecordStartupState(state);
}

void DataReductionProxySettings::RecordStartupState(ProxyStartupState state) {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyStartupStateHistogram,
                            state,
                            PROXY_STARTUP_STATE_COUNT);
}

void DataReductionProxySettings::RecordLoFiImplicitOptOutAction(
    LoFiImplicitOptOutAction action) const {
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();

  switch (connection_type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.LoFi.ImplicitOptOutAction.Unknown", action,
          LO_FI_OPT_OUT_ACTION_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.LoFi.ImplicitOptOutAction.Ethernet", action,
          LO_FI_OPT_OUT_ACTION_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.LoFi.ImplicitOptOutAction.WiFi", action,
          LO_FI_OPT_OUT_ACTION_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_2G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.LoFi.ImplicitOptOutAction.2G", action,
          LO_FI_OPT_OUT_ACTION_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_3G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.LoFi.ImplicitOptOutAction.3G", action,
          LO_FI_OPT_OUT_ACTION_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_4G:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.LoFi.ImplicitOptOutAction.4G", action,
          LO_FI_OPT_OUT_ACTION_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.LoFi.ImplicitOptOutAction.None", action,
          LO_FI_OPT_OUT_ACTION_INDEX_BOUNDARY);
      break;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      UMA_HISTOGRAM_ENUMERATION(
          "DataReductionProxy.LoFi.ImplicitOptOutAction.Bluetooth", action,
          LO_FI_OPT_OUT_ACTION_INDEX_BOUNDARY);
      break;
    default:
      NOTREACHED();
      break;
  }
}

ContentLengthList
DataReductionProxySettings::GetDailyContentLengths(const char* pref_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());
  return data_reduction_proxy_service_->compression_stats()->
      GetDailyContentLengths(pref_name);
}

void DataReductionProxySettings::GetContentLengths(
    unsigned int days,
    int64_t* original_content_length,
    int64_t* received_content_length,
    int64_t* last_update_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());

  data_reduction_proxy_service_->compression_stats()->GetContentLengths(
      days, original_content_length, received_content_length, last_update_time);
}

}  // namespace data_reduction_proxy
