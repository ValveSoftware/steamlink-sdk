// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/lap_timer.h"

#include "base/logging.h"

namespace cc {

namespace {

base::TimeTicks Now() {
  return base::TimeTicks::IsThreadNowSupported()
             ? base::TimeTicks::ThreadNow()
             : base::TimeTicks::HighResNow();
}

}  // namespace

LapTimer::LapTimer(int warmup_laps,
                   base::TimeDelta time_limit,
                   int check_interval)
    : warmup_laps_(warmup_laps),
      remaining_warmups_(0),
      remaining_no_check_laps_(0),
      time_limit_(time_limit),
      check_interval_(check_interval) {
  DCHECK_GT(check_interval, 0);
  Reset();
}

void LapTimer::Reset() {
  accumulator_ = base::TimeDelta();
  num_laps_ = 0;
  remaining_warmups_ = warmup_laps_;
  remaining_no_check_laps_ = check_interval_;
  Start();
}

void LapTimer::Start() {
  start_time_ = Now();
}

bool LapTimer::IsWarmedUp() { return remaining_warmups_ <= 0; }

void LapTimer::NextLap() {
  if (!IsWarmedUp()) {
    --remaining_warmups_;
    if (IsWarmedUp()) {
      Start();
    }
    return;
  }
  ++num_laps_;
  --remaining_no_check_laps_;
  if (!remaining_no_check_laps_) {
    base::TimeTicks now = Now();
    accumulator_ += now - start_time_;
    start_time_ = now;
    remaining_no_check_laps_ = check_interval_;
  }
}

bool LapTimer::HasTimeLimitExpired() { return accumulator_ >= time_limit_; }

bool LapTimer::HasTimedAllLaps() { return !(num_laps_ % check_interval_); }

float LapTimer::MsPerLap() {
  DCHECK(HasTimedAllLaps());
  return accumulator_.InMillisecondsF() / num_laps_;
}

float LapTimer::LapsPerSecond() {
  DCHECK(HasTimedAllLaps());
  return num_laps_ / accumulator_.InSecondsF();
}

int LapTimer::NumLaps() { return num_laps_; }

}  // namespace cc
