// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_USER_ACTIVITY_DETECTOR_H_
#define UI_WM_CORE_USER_ACTIVITY_DETECTOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/events/event_handler.h"
#include "ui/wm/wm_export.h"

namespace wm {

class UserActivityObserver;

// Watches for input events and notifies observers that the user is active.
class WM_EXPORT UserActivityDetector : public ui::EventHandler {
 public:
  // Minimum amount of time between notifications to observers.
  static const int kNotifyIntervalMs;

  // Amount of time that mouse events should be ignored after notification
  // is received that displays' power states are being changed.
  static const int kDisplayPowerChangeIgnoreMouseMs;

  UserActivityDetector();
  virtual ~UserActivityDetector();

  base::TimeTicks last_activity_time() const { return last_activity_time_; }

  void set_now_for_test(base::TimeTicks now) { now_for_test_ = now; }

  bool HasObserver(UserActivityObserver* observer) const;
  void AddObserver(UserActivityObserver* observer);
  void RemoveObserver(UserActivityObserver* observer);

  // Called when displays are about to be turned on or off.
  void OnDisplayPowerChanging();

  // ui::EventHandler implementation.
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  // Returns |now_for_test_| if set or base::TimeTicks::Now() otherwise.
  base::TimeTicks GetCurrentTime() const;

  // Updates |last_activity_time_|.  Additionally notifies observers and
  // updates |last_observer_notification_time_| if enough time has passed
  // since the last notification.
  void HandleActivity(const ui::Event* event);

  ObserverList<UserActivityObserver> observers_;

  // Last time at which user activity was observed.
  base::TimeTicks last_activity_time_;

  // Last time at which we notified observers that the user was active.
  base::TimeTicks last_observer_notification_time_;

  // If set, used when the current time is needed.  This can be set by tests to
  // simulate the passage of time.
  base::TimeTicks now_for_test_;

  // If set, mouse events will be ignored until this time is reached. This
  // is to avoid reporting mouse events that occur when displays are turned
  // on or off as user activity.
  base::TimeTicks honor_mouse_events_time_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityDetector);
};

}  // namespace wm

#endif  // UI_WM_CORE_USER_ACTIVITY_DETECTOR_H_
