// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_METRICS_SERVICE_CLIENT_H_
#define BLIMP_ENGINE_APP_BLIMP_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_service_client.h"

class PrefService;

namespace metrics {
class MetricsService;
class MetricsStateManager;
class SystemProfileProto;
}

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace blimp {
namespace engine {

// BlimpMetricsServiceClient provides an implementation of MetricsServiceClient
// tailored for the Blimp engine to support the upload of metrics information
// from the engine.
// Metrics are always turned on.
class BlimpMetricsServiceClient : public metrics::MetricsServiceClient,
                                  public metrics::EnabledStateProvider {
 public:
  // PrefService ownership is retained by the caller.
  // The request_context_getter is a system request context.
  // Both must remain valid for client lifetime.
  BlimpMetricsServiceClient(
      PrefService* pref_service,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);
  ~BlimpMetricsServiceClient() override;

  // metrics::MetricsServiceClient implementation.
  metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  void OnRecordingDisabled() override;
  bool IsOffTheRecordSessionActive() override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void OnLogUploadComplete() override;
  void InitializeSystemProfileMetrics(
      const base::Closure& done_callback) override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      const base::Callback<void(int)>& on_upload_complete) override;
  base::TimeDelta GetStandardUploadInterval() override;
  metrics::EnableMetricsDefault GetMetricsReportingDefaultState() override;

  // metrics::EnabledStateProvider implementation.
  // Returns if consent is given for the MetricsService to record metrics
  // information for the client. Always true.
  bool IsConsentGiven() override;

 private:
  // Used by NetMetricsLogUploader to create log-upload requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMetricsServiceClient);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_METRICS_SERVICE_CLIENT_H_
