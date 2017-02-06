// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_H_
#define COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"

namespace base {
class FilePath;
}

namespace metrics {
class MetricsService;
class MetricsServiceClient;
class MetricsStateManager;
}

namespace rappor {
class RapporService;
}

namespace variations {
class VariationsService;
}

namespace metrics_services_manager {

class MetricsServicesManagerClient;

// MetricsServicesManager is a helper class for embedders that use the various
// metrics-related services in a Chrome-like fashion: MetricsService (via its
// client), RapporService and VariationsService.
class MetricsServicesManager {
 public:
  // Creates the MetricsServicesManager with the given client.
  explicit MetricsServicesManager(
      std::unique_ptr<MetricsServicesManagerClient> client);
  virtual ~MetricsServicesManager();

  // Returns the MetricsService, creating it if it hasn't been created yet (and
  // additionally creating the MetricsServiceClient in that case).
  metrics::MetricsService* GetMetricsService();

  // Returns the RapporService, creating it if it hasn't been created yet.
  rappor::RapporService* GetRapporService();

  // Returns the VariationsService, creating it if it hasn't been created yet.
  variations::VariationsService* GetVariationsService();

  // Should be called when a plugin loading error occurs.
  void OnPluginLoadingError(const base::FilePath& plugin_path);

  // Some embedders use this method to notify the metrics system when a
  // renderer process exits unexpectedly.
  void OnRendererProcessCrash();

  // Update the managed services when permissions for recording/uploading
  // metrics change.
  void UpdatePermissions(bool may_record, bool may_upload);

  // Update the managed services when permissions for uploading metrics change.
  void UpdateUploadPermissions(bool may_upload);

 private:
  // Update the managed services when permissions for recording/uploading
  // metrics change.
  void UpdateRapporService();

  // Returns the MetricsServiceClient, creating it if it hasn't been
  // created yet (and additionally creating the MetricsService in that case).
  metrics::MetricsServiceClient* GetMetricsServiceClient();

  metrics::MetricsStateManager* GetMetricsStateManager();

  // Update which services are running to match current permissions.
  void UpdateRunningServices();

  // The client passed in from the embedder.
  std::unique_ptr<MetricsServicesManagerClient> client_;

  // Ensures that all functions are called from the same thread.
  base::ThreadChecker thread_checker_;

  // The current metrics reporting setting.
  bool may_upload_;

  // The current metrics recording setting.
  bool may_record_;

  // The MetricsServiceClient. Owns the MetricsService.
  std::unique_ptr<metrics::MetricsServiceClient> metrics_service_client_;

  // The RapporService, for RAPPOR metric uploads.
  std::unique_ptr<rappor::RapporService> rappor_service_;

  // The VariationsService, for server-side experiments infrastructure.
  std::unique_ptr<variations::VariationsService> variations_service_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServicesManager);
};

}  // namespace metrics_services_manager

#endif  // COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_H_
