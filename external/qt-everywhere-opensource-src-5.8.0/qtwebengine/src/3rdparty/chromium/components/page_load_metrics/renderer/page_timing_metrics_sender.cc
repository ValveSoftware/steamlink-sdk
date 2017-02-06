// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metrics_sender.h"

#include <utility>

#include "base/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"
#include "ipc/ipc_sender.h"

namespace page_load_metrics {

namespace {
const int kInitialTimerDelayMillis = 50;
const int kTimerDelayMillis = 1000;
}  // namespace

PageTimingMetricsSender::PageTimingMetricsSender(
    IPC::Sender* ipc_sender,
    int routing_id,
    std::unique_ptr<base::Timer> timer,
    const PageLoadTiming& initial_timing)
    : ipc_sender_(ipc_sender),
      routing_id_(routing_id),
      timer_(std::move(timer)),
      last_timing_(initial_timing),
      metadata_(PageLoadMetadata()) {
  // Send an initial IPC relatively early to help track aborts.
  EnsureSendTimer(kInitialTimerDelayMillis);
}

// On destruction, we want to send any data we have if we have a timer
// currently running (and thus are talking to a browser process)
PageTimingMetricsSender::~PageTimingMetricsSender() {
  if (timer_->IsRunning()) {
    timer_->Stop();
    SendNow();
  }
}

void PageTimingMetricsSender::DidObserveLoadingBehavior(
    blink::WebLoadingBehaviorFlag behavior) {
  if (behavior & metadata_.behavior_flags)
    return;
  metadata_.behavior_flags |= behavior;
  EnsureSendTimer(kTimerDelayMillis);
}

void PageTimingMetricsSender::Send(const PageLoadTiming& timing) {
  if (timing == last_timing_)
    return;

  // We want to make sure that each PageTimingMetricsSender is associated
  // with a distinct page navigation. Because we reset the object on commit,
  // we can trash last_timing_ on a provisional load before SendNow() fires.
  if (!last_timing_.navigation_start.is_null() &&
      last_timing_.navigation_start != timing.navigation_start) {
    return;
  }

  last_timing_ = timing;
  EnsureSendTimer(kTimerDelayMillis);
}

void PageTimingMetricsSender::EnsureSendTimer(int delay) {
  if (!timer_->IsRunning())
    timer_->Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(delay),
        base::Bind(&PageTimingMetricsSender::SendNow, base::Unretained(this)));
}

void PageTimingMetricsSender::SendNow() {
  ipc_sender_->Send(new PageLoadMetricsMsg_TimingUpdated(
      routing_id_, last_timing_, metadata_));
}

}  // namespace page_load_metrics
