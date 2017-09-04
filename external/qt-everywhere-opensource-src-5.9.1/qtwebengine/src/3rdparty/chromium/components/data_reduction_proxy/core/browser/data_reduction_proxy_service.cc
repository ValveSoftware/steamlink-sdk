// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service_observer.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/browser/data_use_group.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/prefs/pref_service.h"

namespace data_reduction_proxy {

DataReductionProxyService::DataReductionProxyService(
    DataReductionProxySettings* settings,
    PrefService* prefs,
    net::URLRequestContextGetter* request_context_getter,
    std::unique_ptr<DataStore> store,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::TimeDelta& commit_delay)
    : url_request_context_getter_(request_context_getter),
      pingback_client_(
          new DataReductionProxyPingbackClient(request_context_getter)),
      settings_(settings),
      prefs_(prefs),
      db_data_owner_(new DBDataOwner(std::move(store))),
      io_task_runner_(io_task_runner),
      db_task_runner_(db_task_runner),
      initialized_(false),
      weak_factory_(this) {
  DCHECK(settings);
  db_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&DBDataOwner::InitializeOnDBThread,
                                       db_data_owner_->GetWeakPtr()));
  if (prefs_) {
    compression_stats_.reset(
        new DataReductionProxyCompressionStats(this, prefs_, commit_delay));
  }
  event_store_.reset(new DataReductionProxyEventStore());
}

DataReductionProxyService::~DataReductionProxyService() {
  DCHECK(CalledOnValidThread());
  compression_stats_.reset();
  db_task_runner_->DeleteSoon(FROM_HERE, db_data_owner_.release());
}

void DataReductionProxyService::SetIOData(
    base::WeakPtr<DataReductionProxyIOData> io_data) {
  DCHECK(CalledOnValidThread());
  io_data_ = io_data;
  initialized_ = true;
  for (DataReductionProxyServiceObserver& observer : observer_list_)
    observer.OnServiceInitialized();

  // Load the Data Reduction Proxy configuration from |prefs_| and apply it.
  if (prefs_) {
    std::string config_value =
        prefs_->GetString(prefs::kDataReductionProxyConfig);
    if (!config_value.empty()) {
      io_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &DataReductionProxyIOData::SetDataReductionProxyConfiguration,
              io_data_, config_value));
    }
  }
}

void DataReductionProxyService::Shutdown() {
  DCHECK(CalledOnValidThread());
  weak_factory_.InvalidateWeakPtrs();
}

void DataReductionProxyService::UpdateContentLengths(
    int64_t data_used,
    int64_t original_size,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    scoped_refptr<DataUseGroup> data_use_group,
    const std::string& mime_type) {
  DCHECK(CalledOnValidThread());
  if (compression_stats_) {
    compression_stats_->UpdateContentLengths(
        data_used, original_size, data_reduction_proxy_enabled, request_type,
        data_use_group, mime_type);
  }
}

void DataReductionProxyService::AddEvent(std::unique_ptr<base::Value> event) {
  DCHECK(CalledOnValidThread());
  event_store_->AddEvent(std::move(event));
}

void DataReductionProxyService::AddEnabledEvent(
    std::unique_ptr<base::Value> event,
    bool enabled) {
  DCHECK(CalledOnValidThread());
  event_store_->AddEnabledEvent(std::move(event), enabled);
}

void DataReductionProxyService::AddEventAndSecureProxyCheckState(
    std::unique_ptr<base::Value> event,
    SecureProxyCheckState state) {
  DCHECK(CalledOnValidThread());
  event_store_->AddEventAndSecureProxyCheckState(std::move(event), state);
}

void DataReductionProxyService::AddAndSetLastBypassEvent(
    std::unique_ptr<base::Value> event,
    int64_t expiration_ticks) {
  DCHECK(CalledOnValidThread());
  event_store_->AddAndSetLastBypassEvent(std::move(event), expiration_ticks);
}

void DataReductionProxyService::SetUnreachable(bool unreachable) {
  DCHECK(CalledOnValidThread());
  settings_->SetUnreachable(unreachable);
}

void DataReductionProxyService::SetLoFiModeActiveOnMainFrame(
    bool lo_fi_mode_active) {
  DCHECK(CalledOnValidThread());
  settings_->SetLoFiModeActiveOnMainFrame(lo_fi_mode_active);
}

void DataReductionProxyService::SetLoFiModeOff() {
  DCHECK(CalledOnValidThread());
  if (io_task_runner_->BelongsToCurrentThread()) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DataReductionProxyIOData::SetLoFiModeOff, io_data_));
    return;
  }
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyIOData::SetLoFiModeOff, io_data_));
}

void DataReductionProxyService::InitializeLoFiPrefs() {
  DCHECK(CalledOnValidThread());
  if (!prefs_)
    return;

  int lo_fi_user_requests_for_images_per_session =
      params::GetFieldTrialParameterAsInteger(
          params::GetLoFiFieldTrialName(), "load_images_requests_per_session",
          3, 0);

  int lo_fi_implicit_opt_out_epoch = params::GetFieldTrialParameterAsInteger(
      params::GetLoFiFieldTrialName(), "implicit_opt_out_epoch", 0, 0);

  int lo_fi_consecutive_session_disables =
      params::GetFieldTrialParameterAsInteger(params::GetLoFiFieldTrialName(),
                                              "consecutive_session_disables", 3,
                                              0);

  // Record if Lo-Fi was used during the last session. Do not record if the
  // Data Reduction Proxy is disabled.
  if (!settings_->IsDataReductionProxyEnabled()) {
    // Don't record the session state.
  } else if (prefs_->GetInteger(prefs::kLoFiConsecutiveSessionDisables) >=
             lo_fi_consecutive_session_disables) {
    RecordLoFiSessionState(LO_FI_SESSION_STATE_OPTED_OUT);
  } else if (prefs_->GetInteger(prefs::kLoFiLoadImagesPerSession) >=
             lo_fi_user_requests_for_images_per_session) {
    DCHECK(prefs_->GetBoolean(prefs::kLoFiWasUsedThisSession));
    DCHECK_GT(prefs_->GetInteger(prefs::kLoFiConsecutiveSessionDisables), 0);
    // Consider the session as temporary opt out only if the number of
    // requests for images were more than the threshold.
    RecordLoFiSessionState(LO_FI_SESSION_STATE_TEMPORARILY_OPTED_OUT);
  } else if (prefs_->GetBoolean(prefs::kLoFiWasUsedThisSession)) {
    RecordLoFiSessionState(LO_FI_SESSION_STATE_USED);
  } else {
    DCHECK(!prefs_->GetBoolean(prefs::kLoFiWasUsedThisSession));
    RecordLoFiSessionState(LO_FI_SESSION_STATE_NOT_USED);
  }

  if (prefs_->GetInteger(prefs::kLoFiImplicitOptOutEpoch) <
      lo_fi_implicit_opt_out_epoch) {
    // We have a new implicit opt out epoch, reset the consecutive session
    // disables count so that Lo-Fi can be enabled again.
    prefs_->SetInteger(prefs::kLoFiConsecutiveSessionDisables, 0);
    prefs_->SetInteger(prefs::kLoFiImplicitOptOutEpoch,
                       lo_fi_implicit_opt_out_epoch);
    settings_->RecordLoFiImplicitOptOutAction(LO_FI_OPT_OUT_ACTION_NEXT_EPOCH);
  } else if (!params::IsLoFiOnViaFlags() &&
             (prefs_->GetInteger(prefs::kLoFiConsecutiveSessionDisables) >=
              lo_fi_consecutive_session_disables)) {
    // If Lo-Fi isn't always on and and the number of
    // |consecutive_session_disables| has been met, turn Lo-Fi off for this
    // session.
    SetLoFiModeOff();
  } else if (prefs_->GetInteger(prefs::kLoFiLoadImagesPerSession) <
                 lo_fi_user_requests_for_images_per_session &&
             prefs_->GetInteger(prefs::kLoFiUIShownPerSession) >=
                 lo_fi_user_requests_for_images_per_session) {
    // If the last session didn't have
    // |lo_fi_user_requests_for_images_per_session|, but the user saw at least
    // that many "Load image" UI notifications, reset the consecutive sessions
    // count.
    prefs_->SetInteger(prefs::kLoFiConsecutiveSessionDisables, 0);
  }
  prefs_->SetInteger(prefs::kLoFiLoadImagesPerSession, 0);
  prefs_->SetInteger(prefs::kLoFiUIShownPerSession, 0);
  prefs_->SetBoolean(prefs::kLoFiWasUsedThisSession, false);
}

void DataReductionProxyService::RecordLoFiSessionState(LoFiSessionState state) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.LoFi.SessionState", state,
                            LO_FI_SESSION_STATE_INDEX_BOUNDARY);
}

void DataReductionProxyService::SetInt64Pref(const std::string& pref_path,
                                             int64_t value) {
  if (prefs_)
    prefs_->SetInt64(pref_path, value);
}

void DataReductionProxyService::SetStringPref(const std::string& pref_path,
                                              const std::string& value) {
  if (prefs_)
    prefs_->SetString(pref_path, value);
}

void DataReductionProxyService::SetProxyPrefs(bool enabled, bool at_startup) {
  DCHECK(CalledOnValidThread());
  if (io_task_runner_->BelongsToCurrentThread()) {
    io_data_->SetProxyPrefs(enabled, at_startup);
    return;
  }
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DataReductionProxyIOData::SetProxyPrefs, io_data_,
                            enabled, at_startup));
}

void DataReductionProxyService::SetPingbackReportingFraction(
    float pingback_reporting_fraction) {
  DCHECK(CalledOnValidThread());
  pingback_client_->SetPingbackReportingFraction(pingback_reporting_fraction);
}

void DataReductionProxyService::LoadHistoricalDataUsage(
    const HistoricalDataUsageCallback& load_data_usage_callback) {
  std::unique_ptr<std::vector<DataUsageBucket>> data_usage(
      new std::vector<DataUsageBucket>());
  std::vector<DataUsageBucket>* data_usage_ptr = data_usage.get();
  db_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&DBDataOwner::LoadHistoricalDataUsage,
                            db_data_owner_->GetWeakPtr(),
                            base::Unretained(data_usage_ptr)),
      base::Bind(load_data_usage_callback, base::Passed(&data_usage)));
}

void DataReductionProxyService::LoadCurrentDataUsageBucket(
    const LoadCurrentDataUsageCallback& load_current_data_usage_callback) {
  std::unique_ptr<DataUsageBucket> bucket(new DataUsageBucket());
  DataUsageBucket* bucket_ptr = bucket.get();
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DBDataOwner::LoadCurrentDataUsageBucket,
                 db_data_owner_->GetWeakPtr(), base::Unretained(bucket_ptr)),
      base::Bind(load_current_data_usage_callback, base::Passed(&bucket)));
}

void DataReductionProxyService::StoreCurrentDataUsageBucket(
    std::unique_ptr<DataUsageBucket> current) {
  db_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DBDataOwner::StoreCurrentDataUsageBucket,
                 db_data_owner_->GetWeakPtr(), base::Passed(&current)));
}

void DataReductionProxyService::DeleteHistoricalDataUsage() {
  db_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&DBDataOwner::DeleteHistoricalDataUsage,
                                       db_data_owner_->GetWeakPtr()));
}

void DataReductionProxyService::DeleteBrowsingHistory(const base::Time& start,
                                                      const base::Time& end) {
  DCHECK_LE(start, end);
  db_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DBDataOwner::DeleteBrowsingHistory,
                            db_data_owner_->GetWeakPtr(), start, end));
}

void DataReductionProxyService::AddObserver(
    DataReductionProxyServiceObserver* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void DataReductionProxyService::RemoveObserver(
    DataReductionProxyServiceObserver* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

bool DataReductionProxyService::Initialized() const {
  DCHECK(CalledOnValidThread());
  return initialized_;
}

base::WeakPtr<DataReductionProxyService>
DataReductionProxyService::GetWeakPtr() {
  DCHECK(CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

}  // namespace data_reduction_proxy
