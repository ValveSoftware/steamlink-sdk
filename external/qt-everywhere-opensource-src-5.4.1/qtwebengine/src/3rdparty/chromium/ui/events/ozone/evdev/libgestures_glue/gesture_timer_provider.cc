// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/gesture_timer_provider.h"

#include <gestures/gestures.h>

#include "base/timer/timer.h"

// libgestures requires that this be in the top level namespace.
class GesturesTimer {
 public:
  GesturesTimer() : callback_(NULL), callback_data_(NULL) {}
  ~GesturesTimer() {}

  void Set(stime_t delay, GesturesTimerCallback callback, void* callback_data) {
    callback_ = callback;
    callback_data_ = callback_data;
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMicroseconds(
                     delay * base::Time::kMicrosecondsPerSecond),
                 this,
                 &GesturesTimer::OnTimerExpired);
  }

  void Cancel() { timer_.Stop(); }

 private:
  void OnTimerExpired() {
    struct timespec ts;
    CHECK(!clock_gettime(CLOCK_MONOTONIC, &ts));
    stime_t next_delay = callback_(StimeFromTimespec(&ts), callback_data_);
    if (next_delay >= 0) {
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMicroseconds(
                       next_delay * base::Time::kMicrosecondsPerSecond),
                   this,
                   &GesturesTimer::OnTimerExpired);
    }
  }

  GesturesTimerCallback callback_;
  void* callback_data_;
  base::OneShotTimer<GesturesTimer> timer_;
};

namespace ui {

namespace {

GesturesTimer* GesturesTimerCreate(void* data) { return new GesturesTimer; }

void GesturesTimerSet(void* data,
                      GesturesTimer* timer,
                      stime_t delay,
                      GesturesTimerCallback callback,
                      void* callback_data) {
  timer->Set(delay, callback, callback_data);
}

void GesturesTimerCancel(void* data, GesturesTimer* timer) { timer->Cancel(); }

void GesturesTimerFree(void* data, GesturesTimer* timer) { delete timer; }

}  // namespace

const GesturesTimerProvider kGestureTimerProvider = {
    GesturesTimerCreate, GesturesTimerSet, GesturesTimerCancel,
    GesturesTimerFree};

}  // namespace ui
