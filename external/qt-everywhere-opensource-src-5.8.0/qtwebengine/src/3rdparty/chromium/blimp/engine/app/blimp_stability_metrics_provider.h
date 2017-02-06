// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_STABILITY_METRICS_PROVIDER_H_
#define BLIMP_ENGINE_APP_BLIMP_STABILITY_METRICS_PROVIDER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/process/kill.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;

// BlimpStabilityMetricsProvider gathers and logs Blimp-specific stability-
// related metrics.
class BlimpStabilityMetricsProvider
    : public metrics::MetricsProvider,
      public content::BrowserChildProcessObserver,
      public content::NotificationObserver {
 public:
  explicit BlimpStabilityMetricsProvider(PrefService* local_state);
  ~BlimpStabilityMetricsProvider() override;

  // metrics::MetricsDataProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ClearSavedStabilityMetrics() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BlimpStabilityMetricsProviderTest,
                           BrowserChildProcessObserver);
  FRIEND_TEST_ALL_PREFIXES(BlimpStabilityMetricsProviderTest,
                           NotificationObserver);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      int exit_code) override;

  metrics::StabilityMetricsHelper helper_;

  // Registrar for receiving stability-related notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BlimpStabilityMetricsProvider);
};

#endif  // BLIMP_ENGINE_APP_BLIMP_STABILITY_METRICS_PROVIDER_H_
