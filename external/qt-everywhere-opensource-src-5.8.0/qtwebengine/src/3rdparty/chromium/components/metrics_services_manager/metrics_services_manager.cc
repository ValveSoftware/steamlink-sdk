// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics_services_manager/metrics_services_manager.h"

#include <utility>

#include "base/logging.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"
#include "components/rappor/rappor_service.h"
#include "components/variations/service/variations_service.h"

namespace metrics_services_manager {

MetricsServicesManager::MetricsServicesManager(
    std::unique_ptr<MetricsServicesManagerClient> client)
    : client_(std::move(client)), may_upload_(false), may_record_(false) {
  DCHECK(client_);
}

MetricsServicesManager::~MetricsServicesManager() {}

metrics::MetricsService* MetricsServicesManager::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServiceClient()->GetMetricsService();
}

rappor::RapporService* MetricsServicesManager::GetRapporService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!rappor_service_) {
    rappor_service_ = client_->CreateRapporService();
    rappor_service_->Initialize(client_->GetURLRequestContext());
  }
  return rappor_service_.get();
}

variations::VariationsService* MetricsServicesManager::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!variations_service_)
    variations_service_ = client_->CreateVariationsService();
  return variations_service_.get();
}

void MetricsServicesManager::OnPluginLoadingError(
    const base::FilePath& plugin_path) {
  GetMetricsServiceClient()->OnPluginLoadingError(plugin_path);
}

void MetricsServicesManager::OnRendererProcessCrash() {
  GetMetricsServiceClient()->OnRendererProcessCrash();
}

metrics::MetricsServiceClient*
MetricsServicesManager::GetMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_service_client_)
    metrics_service_client_ = client_->CreateMetricsServiceClient();
  return metrics_service_client_.get();
}

void MetricsServicesManager::UpdatePermissions(bool may_record,
                                               bool may_upload) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Stash the current permissions so that we can update the RapporService
  // correctly when the Rappor preference changes.  The metrics recording
  // preference partially determines the initial rappor setting, and also
  // controls whether FINE metrics are sent.
  may_record_ = may_record;
  may_upload_ = may_upload;
  UpdateRunningServices();
}

void MetricsServicesManager::UpdateRunningServices() {
  DCHECK(thread_checker_.CalledOnValidThread());
  metrics::MetricsService* metrics = GetMetricsService();

  if (client_->OnlyDoMetricsRecording()) {
    metrics->StartRecordingForTests();
    GetRapporService()->Update(
        rappor::UMA_RAPPOR_GROUP | rappor::SAFEBROWSING_RAPPOR_GROUP, false);
    return;
  }

  if (may_record_) {
    if (!metrics->recording_active())
      metrics->Start();

    if (may_upload_)
      metrics->EnableReporting();
    else
      metrics->DisableReporting();
  } else {
    metrics->Stop();
  }

  int recording_groups = 0;
#if defined(GOOGLE_CHROME_BUILD)
  if (may_record_)
    recording_groups |= rappor::UMA_RAPPOR_GROUP;

  // NOTE: It is safe to use a raw pointer to |this| because this object owns
  // |client_|, and the contract of
  // MetricsServicesManagerClient::IsSafeBrowsingEnabled() states that the
  // callback passed in must not be used beyond the lifetime of the client
  // instance.
  base::Closure on_safe_browsing_update_callback = base::Bind(
      &MetricsServicesManager::UpdateRunningServices, base::Unretained(this));
  if (client_->IsSafeBrowsingEnabled(on_safe_browsing_update_callback))
    recording_groups |= rappor::SAFEBROWSING_RAPPOR_GROUP;
#endif  // defined(GOOGLE_CHROME_BUILD)
  GetRapporService()->Update(recording_groups, may_upload_);
}

void MetricsServicesManager::UpdateUploadPermissions(bool may_upload) {
  return UpdatePermissions(client_->IsMetricsReportingEnabled(), may_upload);
}

}  // namespace metrics_services_manager
