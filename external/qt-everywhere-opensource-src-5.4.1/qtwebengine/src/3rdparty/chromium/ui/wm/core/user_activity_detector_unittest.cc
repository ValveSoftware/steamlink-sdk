// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/user_activity_detector.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"
#include "ui/wm/core/user_activity_observer.h"

namespace wm {

// Implementation that just counts the number of times we've been told that the
// user is active.
class TestUserActivityObserver : public UserActivityObserver {
 public:
  TestUserActivityObserver() : num_invocations_(0) {}

  int num_invocations() const { return num_invocations_; }
  void reset_stats() { num_invocations_ = 0; }

  // UserActivityObserver implementation.
  virtual void OnUserActivity(const ui::Event* event) OVERRIDE {
    num_invocations_++;
  }

 private:
  // Number of times that OnUserActivity() has been called.
  int num_invocations_;

  DISALLOW_COPY_AND_ASSIGN(TestUserActivityObserver);
};

class UserActivityDetectorTest : public aura::test::AuraTestBase {
 public:
  UserActivityDetectorTest() {}
  virtual ~UserActivityDetectorTest() {}

  virtual void SetUp() OVERRIDE {
    AuraTestBase::SetUp();
    observer_.reset(new TestUserActivityObserver);
    detector_.reset(new UserActivityDetector);
    detector_->AddObserver(observer_.get());

    now_ = base::TimeTicks::Now();
    detector_->set_now_for_test(now_);
  }

  virtual void TearDown() OVERRIDE {
    detector_->RemoveObserver(observer_.get());
    AuraTestBase::TearDown();
  }

 protected:
  // Move |detector_|'s idea of the current time forward by |delta|.
  void AdvanceTime(base::TimeDelta delta) {
    now_ += delta;
    detector_->set_now_for_test(now_);
  }

  scoped_ptr<UserActivityDetector> detector_;
  scoped_ptr<TestUserActivityObserver> observer_;

  base::TimeTicks now_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserActivityDetectorTest);
};

// Checks that the observer is notified in response to different types of input
// events.
TEST_F(UserActivityDetectorTest, Basic) {
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE, false);
  detector_->OnKeyEvent(&key_event);
  EXPECT_FALSE(key_event.handled());
  EXPECT_EQ(now_.ToInternalValue(),
            detector_->last_activity_time().ToInternalValue());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  base::TimeDelta advance_delta = base::TimeDelta::FromMilliseconds(
      UserActivityDetector::kNotifyIntervalMs);
  AdvanceTime(advance_delta);
  ui::MouseEvent mouse_event(
      ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(), ui::EF_NONE, ui::EF_NONE);
  detector_->OnMouseEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_EQ(now_.ToInternalValue(),
            detector_->last_activity_time().ToInternalValue());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  base::TimeTicks time_before_ignore = now_;

  // Temporarily ignore mouse events when displays are turned on or off.
  detector_->OnDisplayPowerChanging();
  detector_->OnMouseEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_EQ(time_before_ignore.ToInternalValue(),
            detector_->last_activity_time().ToInternalValue());
  EXPECT_EQ(0, observer_->num_invocations());
  observer_->reset_stats();

  const base::TimeDelta kIgnoreMouseTime =
      base::TimeDelta::FromMilliseconds(
          UserActivityDetector::kDisplayPowerChangeIgnoreMouseMs);
  AdvanceTime(kIgnoreMouseTime / 2);
  detector_->OnMouseEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_EQ(time_before_ignore.ToInternalValue(),
            detector_->last_activity_time().ToInternalValue());
  EXPECT_EQ(0, observer_->num_invocations());
  observer_->reset_stats();

  // After enough time has passed, mouse events should be reported again.
  AdvanceTime(std::max(kIgnoreMouseTime, advance_delta));
  detector_->OnMouseEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_EQ(now_.ToInternalValue(),
            detector_->last_activity_time().ToInternalValue());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  AdvanceTime(advance_delta);
  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, gfx::Point(), 0, base::TimeDelta());
  detector_->OnTouchEvent(&touch_event);
  EXPECT_FALSE(touch_event.handled());
  EXPECT_EQ(now_.ToInternalValue(),
            detector_->last_activity_time().ToInternalValue());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  AdvanceTime(advance_delta);
  ui::GestureEvent gesture_event(
      ui::ET_GESTURE_TAP, 0, 0, ui::EF_NONE,
      base::TimeDelta::FromMilliseconds(base::Time::Now().ToDoubleT() * 1000),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP, 0, 0), 0U);
  detector_->OnGestureEvent(&gesture_event);
  EXPECT_FALSE(gesture_event.handled());
  EXPECT_EQ(now_.ToInternalValue(),
            detector_->last_activity_time().ToInternalValue());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();
}

// Checks that observers aren't notified too frequently.
TEST_F(UserActivityDetectorTest, RateLimitNotifications) {
  // The observer should be notified about a key event.
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE, false);
  detector_->OnKeyEvent(&event);
  EXPECT_FALSE(event.handled());
  EXPECT_EQ(1, observer_->num_invocations());
  observer_->reset_stats();

  // It shouldn't be notified if a second event occurs in the same instant in
  // time.
  detector_->OnKeyEvent(&event);
  EXPECT_FALSE(event.handled());
  EXPECT_EQ(0, observer_->num_invocations());
  observer_->reset_stats();

  // Advance the time, but not quite enough for another notification to be sent.
  AdvanceTime(
      base::TimeDelta::FromMilliseconds(
          UserActivityDetector::kNotifyIntervalMs - 100));
  detector_->OnKeyEvent(&event);
  EXPECT_FALSE(event.handled());
  EXPECT_EQ(0, observer_->num_invocations());
  observer_->reset_stats();

  // Advance time by the notification interval, definitely moving out of the
  // rate limit. This should let us trigger another notification.
  AdvanceTime(base::TimeDelta::FromMilliseconds(
      UserActivityDetector::kNotifyIntervalMs));

  detector_->OnKeyEvent(&event);
  EXPECT_FALSE(event.handled());
  EXPECT_EQ(1, observer_->num_invocations());
}

// Checks that the detector ignores synthetic mouse events.
TEST_F(UserActivityDetectorTest, IgnoreSyntheticMouseEvents) {
  ui::MouseEvent mouse_event(
      ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(), ui::EF_IS_SYNTHESIZED,
      ui::EF_NONE);
  detector_->OnMouseEvent(&mouse_event);
  EXPECT_FALSE(mouse_event.handled());
  EXPECT_EQ(base::TimeTicks().ToInternalValue(),
            detector_->last_activity_time().ToInternalValue());
  EXPECT_EQ(0, observer_->num_invocations());
}

}  // namespace wm
