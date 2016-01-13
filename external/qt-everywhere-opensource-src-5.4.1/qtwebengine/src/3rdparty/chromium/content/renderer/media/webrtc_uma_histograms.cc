// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_uma_histograms.h"

#include "base/metrics/histogram.h"

namespace content {

void UpdateWebRTCMethodCount(JavaScriptAPIName api_name) {
  DVLOG(3) << "Incrementing WebRTC.webkitApiCount for " << api_name;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.webkitApiCount", api_name, INVALID_NAME);
  PerSessionWebRTCAPIMetrics::GetInstance()->LogUsageOnlyOnce(api_name);
}

PerSessionWebRTCAPIMetrics::~PerSessionWebRTCAPIMetrics() {
}

// static
PerSessionWebRTCAPIMetrics* PerSessionWebRTCAPIMetrics::GetInstance() {
  return Singleton<PerSessionWebRTCAPIMetrics>::get();
}

void PerSessionWebRTCAPIMetrics::IncrementStreamCounter() {
  DCHECK(CalledOnValidThread());
  ++num_streams_;
}

void PerSessionWebRTCAPIMetrics::DecrementStreamCounter() {
  DCHECK(CalledOnValidThread());
  if (--num_streams_ == 0) {
    ResetUsage();
  }
}

PerSessionWebRTCAPIMetrics::PerSessionWebRTCAPIMetrics() : num_streams_(0) {
  ResetUsage();
}

void PerSessionWebRTCAPIMetrics::LogUsage(JavaScriptAPIName api_name) {
  DVLOG(3) << "Incrementing WebRTC.webkitApiCountPerSession for " << api_name;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.webkitApiCountPerSession",
                            api_name, INVALID_NAME);
}

void PerSessionWebRTCAPIMetrics::LogUsageOnlyOnce(JavaScriptAPIName api_name) {
  DCHECK(CalledOnValidThread());
  if (!has_used_api_[api_name]) {
    has_used_api_[api_name] = true;
    LogUsage(api_name);
  }
}

void PerSessionWebRTCAPIMetrics::ResetUsage() {
  for (size_t i = 0; i < arraysize(has_used_api_); ++i)
    has_used_api_[i] = false;
}

}  // namespace content
