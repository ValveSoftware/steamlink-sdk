// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_metrics_service_client.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "blimp/engine/app/blimp_stability_metrics_provider.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/profiler/profiler_metrics_provider.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"

namespace blimp {
namespace engine {
namespace {

// How often after initial logging metrics results should be uploaded to the
// metrics service.
const int kStandardUploadIntervalMinutes = 30;

// Store/LoadClientInfo allows Windows Chrome to back up ClientInfo.
// Both are no-ops for Blimp.
// These callbacks are required by MetricsStateManager::Create.
void StoreClientInfo(const metrics::ClientInfo& client_info) {}

std::unique_ptr<metrics::ClientInfo> LoadClientInfo() {
  return nullptr;
}

}  // namespace

BlimpMetricsServiceClient::BlimpMetricsServiceClient(
    PrefService* pref_service,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter) {
  request_context_getter_ = request_context_getter;

  metrics_state_manager_ = metrics::MetricsStateManager::Create(
      pref_service, this, base::Bind(&StoreClientInfo),
      base::Bind(&LoadClientInfo));

  // Metrics state manager created while other class instances exist.
  // Sign of multiple initializations.
  DCHECK(metrics_state_manager_);

  metrics_service_.reset(new metrics::MetricsService(
      metrics_state_manager_.get(), this, pref_service));
  metrics_service_->RegisterMetricsProvider(
      base::WrapUnique<metrics::MetricsProvider>(
          new BlimpStabilityMetricsProvider(pref_service)));
  metrics_service_->RegisterMetricsProvider(
      base::WrapUnique<metrics::MetricsProvider>(
          new metrics::NetworkMetricsProvider(
              content::BrowserThread::GetBlockingPool())));
  metrics_service_->RegisterMetricsProvider(
      base::WrapUnique<metrics::MetricsProvider>(
          new metrics::GPUMetricsProvider));
  metrics_service_->RegisterMetricsProvider(
      base::WrapUnique<metrics::MetricsProvider>(
          new metrics::ScreenInfoMetricsProvider));
  metrics_service_->RegisterMetricsProvider(
      base::WrapUnique<metrics::MetricsProvider>(
          new metrics::ProfilerMetricsProvider()));
  metrics_service_->RegisterMetricsProvider(
      base::WrapUnique<metrics::MetricsProvider>(
          new metrics::CallStackProfileMetricsProvider));
  metrics_service_->InitializeMetricsRecordingState();
  if (IsReportingEnabled())
    metrics_service_->Start();
}

BlimpMetricsServiceClient::~BlimpMetricsServiceClient() {}

metrics::MetricsService* BlimpMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

// In Chrome, UMA and Breakpad are enabled/disabled together by the same
// checkbox and they share the same client ID (a.k.a. GUID).
// This is not required by Blimp, so these are no-ops.
void BlimpMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {}

// Recording can not be disabled in Blimp, so this function is a no-op.
void BlimpMetricsServiceClient::OnRecordingDisabled() {}

bool BlimpMetricsServiceClient::IsOffTheRecordSessionActive() {
  // Blimp does not have incognito mode.
  return false;
}

int32_t BlimpMetricsServiceClient::GetProduct() {
  // Indicates product family (e.g. Chrome v Android Webview), not reported
  // platform (e.g. Chrome_Linux, Chrome_Mac).
  return metrics::ChromeUserMetricsExtension::CHROME;
}

std::string BlimpMetricsServiceClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

bool BlimpMetricsServiceClient::GetBrand(std::string* brand_code) {
  // Blimp doesn't use brand codes.
  return false;
}

metrics::SystemProfileProto::Channel BlimpMetricsServiceClient::GetChannel() {
  // Blimp engine does not have channel info yet.
  return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
}

std::string BlimpMetricsServiceClient::GetVersionString() {
  return version_info::GetVersionNumber();
}

void BlimpMetricsServiceClient::OnLogUploadComplete() {}

void BlimpMetricsServiceClient::InitializeSystemProfileMetrics(
    const base::Closure& done_callback) {
  // Blimp requires no additional work to InitializeSystemProfileMetrics
  // and should proceed to the next call in the chain.
  done_callback.Run();
}

void BlimpMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  // Blimp requires no additional work to CollectFinalMetricsForLog
  // and should proceed to the next call in the chain
  done_callback.Run();
}

std::unique_ptr<metrics::MetricsLogUploader>
BlimpMetricsServiceClient::CreateUploader(
    const base::Callback<void(int)>& on_upload_complete) {
  return base::WrapUnique<metrics::MetricsLogUploader>(
      new metrics::NetMetricsLogUploader(
          request_context_getter_.get(), metrics::kDefaultMetricsServerUrl,
          metrics::kDefaultMetricsMimeType, on_upload_complete));
}

base::TimeDelta BlimpMetricsServiceClient::GetStandardUploadInterval() {
  return base::TimeDelta::FromMinutes(kStandardUploadIntervalMinutes);
}

metrics::EnableMetricsDefault
BlimpMetricsServiceClient::GetMetricsReportingDefaultState() {
  return metrics::EnableMetricsDefault::OPT_IN;
}

bool BlimpMetricsServiceClient::IsConsentGiven() {
  return true;
}

}  // namespace engine
}  // namespace blimp
