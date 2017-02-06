// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/browser/startup_metric_host_impl.h"

#include "components/startup_metric_utils/browser/startup_metric_utils.h"

namespace startup_metric_utils {
// static
void StartupMetricHostImpl::Create(mojom::StartupMetricHostRequest request) {
  new StartupMetricHostImpl(std::move(request));
}

StartupMetricHostImpl::StartupMetricHostImpl(
    mojom::StartupMetricHostRequest request)
    : binding_(this, std::move(request)) {}

StartupMetricHostImpl::~StartupMetricHostImpl() = default;

void StartupMetricHostImpl::RecordRendererMainEntryTime(
    const base::TimeTicks& renderer_main_entry_time) {
  startup_metric_utils::RecordRendererMainEntryTime(renderer_main_entry_time);
}

}  // namespace startup_metric_utils
