// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/tap_suppression_controller.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/browser/renderer_host/input/tap_suppression_controller_client.h"

namespace content {

TapSuppressionController::Config::Config()
    : enabled(false),
      max_cancel_to_down_time(base::TimeDelta::FromMilliseconds(180)),
      max_tap_gap_time(base::TimeDelta::FromMilliseconds(500)) {
}

TapSuppressionController::TapSuppressionController(
    TapSuppressionControllerClient* client,
    const Config& config)
    : client_(client),
      state_(config.enabled ? NOTHING : DISABLED),
      max_cancel_to_down_time_(config.max_cancel_to_down_time),
      max_tap_gap_time_(config.max_tap_gap_time) {
}

TapSuppressionController::~TapSuppressionController() {}

void TapSuppressionController::GestureFlingCancel() {
  switch (state_) {
    case DISABLED:
      break;
    case NOTHING:
    case GFC_IN_PROGRESS:
    case LAST_CANCEL_STOPPED_FLING:
      state_ = GFC_IN_PROGRESS;
      break;
    case TAP_DOWN_STASHED:
      break;
  }
}

void TapSuppressionController::GestureFlingCancelAck(bool processed) {
  base::TimeTicks event_time = Now();
  switch (state_) {
    case DISABLED:
    case NOTHING:
      break;
    case GFC_IN_PROGRESS:
      if (processed)
        fling_cancel_time_ = event_time;
      state_ = LAST_CANCEL_STOPPED_FLING;
      break;
    case TAP_DOWN_STASHED:
      if (!processed) {
        TRACE_EVENT0("browser",
                     "TapSuppressionController::GestureFlingCancelAck");
        StopTapDownTimer();
        client_->ForwardStashedTapDown();
        state_ = NOTHING;
      }  // Else waiting for the timer to release the stashed tap down.
      break;
    case LAST_CANCEL_STOPPED_FLING:
      break;
  }
}

bool TapSuppressionController::ShouldDeferTapDown() {
  base::TimeTicks event_time = Now();
  switch (state_) {
    case DISABLED:
    case NOTHING:
      return false;
    case GFC_IN_PROGRESS:
      state_ = TAP_DOWN_STASHED;
      StartTapDownTimer(max_tap_gap_time_);
      return true;
    case TAP_DOWN_STASHED:
      NOTREACHED() << "TapDown on TAP_DOWN_STASHED state";
      state_ = NOTHING;
      return false;
    case LAST_CANCEL_STOPPED_FLING:
      if ((event_time - fling_cancel_time_) < max_cancel_to_down_time_) {
        state_ = TAP_DOWN_STASHED;
        StartTapDownTimer(max_tap_gap_time_);
        return true;
      } else {
        state_ = NOTHING;
        return false;
      }
  }
  NOTREACHED() << "Invalid state";
  return false;
}

bool TapSuppressionController::ShouldSuppressTapEnd() {
  switch (state_) {
    case DISABLED:
    case NOTHING:
    case GFC_IN_PROGRESS:
      return false;
    case TAP_DOWN_STASHED:
      state_ = NOTHING;
      StopTapDownTimer();
      client_->DropStashedTapDown();
      return true;
    case LAST_CANCEL_STOPPED_FLING:
      NOTREACHED() << "Invalid tap end on LAST_CANCEL_STOPPED_FLING state";
  }
  return false;
}

base::TimeTicks TapSuppressionController::Now() {
  return base::TimeTicks::Now();
}

void TapSuppressionController::StartTapDownTimer(const base::TimeDelta& delay) {
  tap_down_timer_.Start(FROM_HERE, delay, this,
                        &TapSuppressionController::TapDownTimerExpired);
}

void TapSuppressionController::StopTapDownTimer() {
  tap_down_timer_.Stop();
}

void TapSuppressionController::TapDownTimerExpired() {
  switch (state_) {
    case DISABLED:
    case NOTHING:
      NOTREACHED() << "Timer fired on invalid state.";
      break;
    case GFC_IN_PROGRESS:
    case LAST_CANCEL_STOPPED_FLING:
      NOTREACHED() << "Timer fired on invalid state.";
      state_ = NOTHING;
      break;
    case TAP_DOWN_STASHED:
      TRACE_EVENT0("browser",
                   "TapSuppressionController::TapDownTimerExpired");
      client_->ForwardStashedTapDown();
      state_ = NOTHING;
      break;
  }
}

}  // namespace content
