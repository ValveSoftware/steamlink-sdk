// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_CLIENT_H_
#define COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_CLIENT_H_

#include <memory>

#include "base/callback_forward.h"

namespace metrics {
class MetricsServiceClient;
}

namespace net {
class URLRequestContextGetter;
}

namespace rappor {
class RapporService;
}

namespace variations {
class VariationsService;
}

namespace metrics_services_manager {

// MetricsServicesManagerClient is an interface that allows
// MetricsServicesManager to interact with its embedder.
class MetricsServicesManagerClient {
 public:
  virtual ~MetricsServicesManagerClient() {}

  // Methods that create the various services in the context of the embedder.
  virtual std::unique_ptr<rappor::RapporService> CreateRapporService() = 0;
  virtual std::unique_ptr<variations::VariationsService>
  CreateVariationsService() = 0;
  virtual std::unique_ptr<metrics::MetricsServiceClient>
  CreateMetricsServiceClient() = 0;

  // Returns the URL request context in which the metrics services should
  // operate.
  virtual net::URLRequestContextGetter* GetURLRequestContext() = 0;

  // Returns whether safe browsing is enabled. If relevant in the embedder's
  // context, |on_update_callback| will be set up to be called when the state of
  // safe browsing changes. |on_update_callback| is guaranteed to be valid for
  // the lifetime of this client instance, but should not be used beyond this
  // instance being destroyed.
  virtual bool IsSafeBrowsingEnabled(
      const base::Closure& on_update_callback) = 0;

  // Returns whether metrics reporting is enabled.
  virtual bool IsMetricsReportingEnabled() = 0;

  // Whether the metrics services should record but not report metrics.
  virtual bool OnlyDoMetricsRecording() = 0;
};

}  // namespace metrics_services_manager

#endif  // COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_CLIENT_H_
