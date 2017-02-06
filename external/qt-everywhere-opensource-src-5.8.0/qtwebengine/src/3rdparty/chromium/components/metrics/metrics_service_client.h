// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_CLIENT_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/proto/system_profile.pb.h"

namespace base {
class FilePath;
}

namespace metrics {

class MetricsLogUploader;
class MetricsService;

// An abstraction of operations that depend on the embedder's (e.g. Chrome)
// environment.
class MetricsServiceClient {
 public:
  virtual ~MetricsServiceClient() {}

  // Returns the MetricsService instance that this client is associated with.
  // With the exception of testing contexts, the returned instance must be valid
  // for the lifetime of this object (typically, the embedder's client
  // implementation will own the MetricsService instance being returned).
  virtual MetricsService* GetMetricsService() = 0;

  // Registers the client id with other services (e.g. crash reporting), called
  // when metrics recording gets enabled.
  virtual void SetMetricsClientId(const std::string& client_id) = 0;

  // Notifies the client that recording is disabled, so that other services
  // (such as crash reporting) can clear any association with metrics.
  virtual void OnRecordingDisabled() = 0;

  // Whether there's an "off the record" (aka "Incognito") session active.
  virtual bool IsOffTheRecordSessionActive() = 0;

  // Returns the product value to use in uploaded reports, which will be used to
  // set the ChromeUserMetricsExtension.product field. See comments on that
  // field on why it's an int32_t rather than an enum.
  virtual int32_t GetProduct() = 0;

  // Returns the current application locale (e.g. "en-US").
  virtual std::string GetApplicationLocale() = 0;

  // Retrieves the brand code string associated with the install, returning
  // false if no brand code is available.
  virtual bool GetBrand(std::string* brand_code) = 0;

  // Returns the release channel (e.g. stable, beta, etc) of the application.
  virtual SystemProfileProto::Channel GetChannel() = 0;

  // Returns the version of the application as a string.
  virtual std::string GetVersionString() = 0;

  // Called by the metrics service when a log has been uploaded.
  virtual void OnLogUploadComplete() = 0;

  // Gathers metrics that will be filled into the system profile protobuf,
  // calling |done_callback| when complete.
  virtual void InitializeSystemProfileMetrics(
      const base::Closure& done_callback) = 0;

  // Called prior to a metrics log being closed, allowing the client to collect
  // extra histograms that will go in that log. Asynchronous API - the client
  // implementation should call |done_callback| when complete.
  virtual void CollectFinalMetricsForLog(
      const base::Closure& done_callback) = 0;

  // Creates a MetricsLogUploader with the specified parameters (see comments on
  // MetricsLogUploader for details).
  virtual std::unique_ptr<MetricsLogUploader> CreateUploader(
      const base::Callback<void(int)>& on_upload_complete) = 0;

  // Returns the standard interval between upload attempts.
  virtual base::TimeDelta GetStandardUploadInterval() = 0;

  // Returns the name of a key under HKEY_CURRENT_USER that can be used to store
  // backups of metrics data. Unused except on Windows.
  virtual base::string16 GetRegistryBackupKey();

  // Called on plugin loading errors.
  virtual void OnPluginLoadingError(const base::FilePath& plugin_path) {}

  // Called on renderer crashes in some embedders (e.g., those that do not use
  // //content and thus do not have //content's notification system available
  // as a mechanism for observing renderer crashes).
  virtual void OnRendererProcessCrash() {}

  // Returns whether metrics reporting is managed by policy.
  virtual bool IsReportingPolicyManaged();

  // Gets information about the default value for the metrics reporting checkbox
  // shown during first-run.
  virtual EnableMetricsDefault GetMetricsReportingDefaultState();

  // Returns whether cellular logic is enabled for metrics reporting.
  virtual bool IsUMACellularUploadLogicEnabled();
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_CLIENT_H_
