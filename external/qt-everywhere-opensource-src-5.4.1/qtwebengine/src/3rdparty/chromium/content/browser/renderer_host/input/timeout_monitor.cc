// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/timeout_monitor.h"

using base::Time;
using base::TimeDelta;

namespace content {

TimeoutMonitor::TimeoutMonitor(const TimeoutHandler& timeout_handler)
    : timeout_handler_(timeout_handler) {
  DCHECK(!timeout_handler_.is_null());
}

TimeoutMonitor::~TimeoutMonitor() {}

void TimeoutMonitor::Start(TimeDelta delay) {
  // Set time_when_considered_timed_out_ if it's null. Also, update
  // time_when_considered_timed_out_ if the caller's request is sooner than the
  // existing one. This will have the side effect that the existing timeout will
  // be forgotten.
  Time requested_end_time = Time::Now() + delay;
  if (time_when_considered_timed_out_.is_null() ||
      time_when_considered_timed_out_ > requested_end_time)
    time_when_considered_timed_out_ = requested_end_time;

  // If we already have a timer with the same or shorter duration, then we can
  // wait for it to finish.
  if (timeout_timer_.IsRunning() && timeout_timer_.GetCurrentDelay() <= delay) {
    // If time_when_considered_timed_out_ was null, this timer may fire early.
    // CheckTimedOut handles that that by calling Start with the remaining time.
    // If time_when_considered_timed_out_ was non-null, it means we still
    // haven't been stopped, so we leave time_when_considered_timed_out_ as is.
    return;
  }

  // Either the timer is not yet running, or we need to adjust the timer to
  // fire sooner.
  time_when_considered_timed_out_ = requested_end_time;
  timeout_timer_.Stop();
  timeout_timer_.Start(FROM_HERE, delay, this, &TimeoutMonitor::CheckTimedOut);
}

void TimeoutMonitor::Restart(TimeDelta delay) {
  // Setting to null will cause StartTimeoutMonitor to restart the timer.
  time_when_considered_timed_out_ = Time();
  Start(delay);
}

void TimeoutMonitor::Stop() {
  // We do not bother to stop the timeout_timer_ here in case it will be
  // started again shortly, which happens to be the common use case.
  time_when_considered_timed_out_ = Time();
}

void TimeoutMonitor::CheckTimedOut() {
  // If we received a call to |Stop()|.
  if (time_when_considered_timed_out_.is_null())
    return;

  // If we have not waited long enough, then wait some more.
  Time now = Time::Now();
  if (now < time_when_considered_timed_out_) {
    Start(time_when_considered_timed_out_ - now);
    return;
  }

  timeout_handler_.Run();
}

bool TimeoutMonitor::IsRunning() const {
  return timeout_timer_.IsRunning() &&
         !time_when_considered_timed_out_.is_null();
}

}  // namespace content
