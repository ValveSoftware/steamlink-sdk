// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_stability_metrics_provider.h"

#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "build/build_config.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

BlimpStabilityMetricsProvider::BlimpStabilityMetricsProvider(
    PrefService* local_state)
    : helper_(local_state) {
  BrowserChildProcessObserver::Add(this);
}

BlimpStabilityMetricsProvider::~BlimpStabilityMetricsProvider() {
  BrowserChildProcessObserver::Remove(this);
}

void BlimpStabilityMetricsProvider::OnRecordingEnabled() {
  registrar_.Add(this,
                 content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());
}

void BlimpStabilityMetricsProvider::OnRecordingDisabled() {
  registrar_.RemoveAll();
}

void BlimpStabilityMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  helper_.ProvideStabilityMetrics(system_profile_proto);
}

void BlimpStabilityMetricsProvider::ClearSavedStabilityMetrics() {
  helper_.ClearSavedStabilityMetrics();
}

void BlimpStabilityMetricsProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_START: {
      helper_.LogLoadStarted();
      break;
    }

    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      content::RenderProcessHost::RendererClosedDetails* process_details =
          content::Details<content::RenderProcessHost::RendererClosedDetails>(
              details).ptr();
      bool was_extension_process = false;
      helper_.LogRendererCrash(was_extension_process, process_details->status,
                               process_details->exit_code);
      break;
    }

    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      helper_.LogRendererHang();
      break;

    default:
      NOTREACHED();
      break;
  }
}

void BlimpStabilityMetricsProvider::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    int exit_code) {
  helper_.BrowserChildProcessCrashed();
}
