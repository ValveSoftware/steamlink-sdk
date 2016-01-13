// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lazy_instance.h"
#include "net/base/bandwidth_metrics.h"

static base::LazyInstance<net::BandwidthMetrics> g_bandwidth_metrics =
    LAZY_INSTANCE_INITIALIZER;

namespace net {

ScopedBandwidthMetrics::ScopedBandwidthMetrics()
    : started_(false) {
}

ScopedBandwidthMetrics::~ScopedBandwidthMetrics() {
  if (started_)
    g_bandwidth_metrics.Get().StopStream();
}

void ScopedBandwidthMetrics::StartStream() {
  started_ = true;
  g_bandwidth_metrics.Get().StartStream();
}

void ScopedBandwidthMetrics::StopStream() {
  started_ = false;
  g_bandwidth_metrics.Get().StopStream();
}

void ScopedBandwidthMetrics::RecordBytes(int bytes) {
  g_bandwidth_metrics.Get().RecordBytes(bytes);
}

}  // namespace net
