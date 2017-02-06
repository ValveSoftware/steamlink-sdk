// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/window_activity_tracker.h"

#include "base/time/time.h"

namespace content {

namespace {

// The time period within which a triggered UI event is considered
// currently active.
const int kTimePeriodUiEventMicros = 100000;  // 100 ms

// Minimum number of user interactions before we consider the user to be in
// interactive mode. The goal is to prevent user interactions to launch
// animated content from causing target playout time flip-flop.
const int kMinUserInteractions = 5;

}  // namespace

WindowActivityTracker::WindowActivityTracker() {
  Reset();
}

WindowActivityTracker::~WindowActivityTracker() {}

bool WindowActivityTracker::IsUiInteractionActive() const {
  return ui_events_count_ > kMinUserInteractions;
}

void WindowActivityTracker::RegisterMouseInteractionObserver(
    const base::Closure& observer) {
  mouse_interaction_observer_ = observer;
}

void WindowActivityTracker::Reset() {
  ui_events_count_ = 0;
  last_time_ui_event_detected_ = base::TimeTicks();
}

void WindowActivityTracker::OnMouseActivity() {
  if (!mouse_interaction_observer_.is_null())
    mouse_interaction_observer_.Run();
  if (base::TimeTicks::Now() - last_time_ui_event_detected_ >
      base::TimeDelta::FromMicroseconds(kTimePeriodUiEventMicros)) {
    ui_events_count_++;
  }
  last_time_ui_event_detected_ = base::TimeTicks::Now();
}

}  // namespace content
