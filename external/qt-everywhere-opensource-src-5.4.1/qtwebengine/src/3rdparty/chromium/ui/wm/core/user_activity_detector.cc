// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/user_activity_detector.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "ui/events/event.h"
#include "ui/wm/core/user_activity_observer.h"

namespace wm {

namespace {

// Returns a string describing |event|.
std::string GetEventDebugString(const ui::Event* event) {
  std::string details = base::StringPrintf(
      "type=%d name=%s flags=%d time=%" PRId64,
      event->type(), event->name().c_str(), event->flags(),
      event->time_stamp().InMilliseconds());

  if (event->IsKeyEvent()) {
    details += base::StringPrintf(" key_code=%d",
        static_cast<const ui::KeyEvent*>(event)->key_code());
  } else if (event->IsMouseEvent() || event->IsTouchEvent() ||
             event->IsGestureEvent()) {
    details += base::StringPrintf(" location=%s",
        static_cast<const ui::LocatedEvent*>(
            event)->location().ToString().c_str());
  }

  return details;
}

}  // namespace

const int UserActivityDetector::kNotifyIntervalMs = 200;

// Too low and mouse events generated at the tail end of reconfiguration
// will be reported as user activity and turn the screen back on; too high
// and we'll ignore legitimate activity.
const int UserActivityDetector::kDisplayPowerChangeIgnoreMouseMs = 1000;

UserActivityDetector::UserActivityDetector() {
}

UserActivityDetector::~UserActivityDetector() {
}

bool UserActivityDetector::HasObserver(UserActivityObserver* observer) const {
  return observers_.HasObserver(observer);
}

void UserActivityDetector::AddObserver(UserActivityObserver* observer) {
  observers_.AddObserver(observer);
}

void UserActivityDetector::RemoveObserver(UserActivityObserver* observer) {
  observers_.RemoveObserver(observer);
}

void UserActivityDetector::OnDisplayPowerChanging() {
  honor_mouse_events_time_ = GetCurrentTime() +
      base::TimeDelta::FromMilliseconds(kDisplayPowerChangeIgnoreMouseMs);
}

void UserActivityDetector::OnKeyEvent(ui::KeyEvent* event) {
  HandleActivity(event);
}

void UserActivityDetector::OnMouseEvent(ui::MouseEvent* event) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;
  if (!honor_mouse_events_time_.is_null() &&
      GetCurrentTime() < honor_mouse_events_time_)
    return;

  HandleActivity(event);
}

void UserActivityDetector::OnScrollEvent(ui::ScrollEvent* event) {
  HandleActivity(event);
}

void UserActivityDetector::OnTouchEvent(ui::TouchEvent* event) {
  HandleActivity(event);
}

void UserActivityDetector::OnGestureEvent(ui::GestureEvent* event) {
  HandleActivity(event);
}

base::TimeTicks UserActivityDetector::GetCurrentTime() const {
  return !now_for_test_.is_null() ? now_for_test_ : base::TimeTicks::Now();
}

void UserActivityDetector::HandleActivity(const ui::Event* event) {
  base::TimeTicks now = GetCurrentTime();
  last_activity_time_ = now;
  if (last_observer_notification_time_.is_null() ||
      (now - last_observer_notification_time_).InMillisecondsF() >=
      kNotifyIntervalMs) {
    if (VLOG_IS_ON(1))
      VLOG(1) << "Reporting user activity: " << GetEventDebugString(event);
    FOR_EACH_OBSERVER(UserActivityObserver, observers_, OnUserActivity(event));
    last_observer_notification_time_ = now;
  }
}

}  // namespace wm
