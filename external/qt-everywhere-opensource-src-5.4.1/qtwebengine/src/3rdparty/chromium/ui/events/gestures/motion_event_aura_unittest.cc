// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/gestures/motion_event_aura.h"

namespace {

ui::TouchEvent TouchWithType(ui::EventType type, int id) {
  return ui::TouchEvent(
      type, gfx::PointF(0, 0), id, base::TimeDelta::FromMilliseconds(0));
}

ui::TouchEvent TouchWithPosition(ui::EventType type,
                                 int id,
                                 float x,
                                 float y,
                                 float raw_x,
                                 float raw_y,
                                 float radius,
                                 float pressure) {
  ui::TouchEvent event(type,
                       gfx::PointF(x, y),
                       0,
                       id,
                       base::TimeDelta::FromMilliseconds(0),
                       radius,
                       radius,
                       0,
                       pressure);
  event.set_root_location(gfx::PointF(raw_x, raw_y));
  return event;
}

ui::TouchEvent TouchWithTime(ui::EventType type, int id, int ms) {
  return ui::TouchEvent(
      type, gfx::PointF(0, 0), id, base::TimeDelta::FromMilliseconds(ms));
}

base::TimeTicks MsToTicks(int ms) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(ms);
}

}  // namespace

namespace ui {

TEST(MotionEventAuraTest, PointerCountAndIds) {
  // Test that |PointerCount()| returns the correct number of pointers, and ids
  // are assigned correctly.
  int ids[] = {4, 6, 1};

  MotionEventAura event;
  EXPECT_EQ(0U, event.GetPointerCount());

  TouchEvent press0 = TouchWithType(ET_TOUCH_PRESSED, ids[0]);
  event.OnTouch(press0);
  EXPECT_EQ(1U, event.GetPointerCount());

  EXPECT_EQ(ids[0], event.GetPointerId(0));

  TouchEvent press1 = TouchWithType(ET_TOUCH_PRESSED, ids[1]);
  event.OnTouch(press1);
  EXPECT_EQ(2U, event.GetPointerCount());

  EXPECT_EQ(ids[0], event.GetPointerId(0));
  EXPECT_EQ(ids[1], event.GetPointerId(1));

  TouchEvent press2 = TouchWithType(ET_TOUCH_PRESSED, ids[2]);
  event.OnTouch(press2);
  EXPECT_EQ(3U, event.GetPointerCount());

  EXPECT_EQ(ids[0], event.GetPointerId(0));
  EXPECT_EQ(ids[1], event.GetPointerId(1));
  EXPECT_EQ(ids[2], event.GetPointerId(2));

  TouchEvent release1 = TouchWithType(ET_TOUCH_RELEASED, ids[1]);
  event.OnTouch(release1);
  event.CleanupRemovedTouchPoints(release1);
  EXPECT_EQ(2U, event.GetPointerCount());

  EXPECT_EQ(ids[0], event.GetPointerId(0));
  EXPECT_EQ(ids[2], event.GetPointerId(1));

  // Test cloning of pointer count and id information.
  scoped_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(2U, clone->GetPointerCount());
  EXPECT_EQ(ids[0], clone->GetPointerId(0));
  EXPECT_EQ(ids[2], clone->GetPointerId(1));

  TouchEvent release0 = TouchWithType(ET_TOUCH_RELEASED, ids[0]);
  event.OnTouch(release0);
  event.CleanupRemovedTouchPoints(release0);
  EXPECT_EQ(1U, event.GetPointerCount());

  EXPECT_EQ(ids[2], event.GetPointerId(0));

  TouchEvent release2 = TouchWithType(ET_TOUCH_RELEASED, ids[2]);
  event.OnTouch(release2);
  event.CleanupRemovedTouchPoints(release2);
  EXPECT_EQ(0U, event.GetPointerCount());
}

TEST(MotionEventAuraTest, GetActionIndexAfterRemoval) {
  // Test that |GetActionIndex()| returns the correct index when points have
  // been removed.
  int ids[] = {4, 6, 9};

  MotionEventAura event;
  EXPECT_EQ(0U, event.GetPointerCount());

  TouchEvent press0 = TouchWithType(ET_TOUCH_PRESSED, ids[0]);
  event.OnTouch(press0);
  TouchEvent press1 = TouchWithType(ET_TOUCH_PRESSED, ids[1]);
  event.OnTouch(press1);
  TouchEvent press2 = TouchWithType(ET_TOUCH_PRESSED, ids[2]);
  event.OnTouch(press2);
  EXPECT_EQ(3U, event.GetPointerCount());

  TouchEvent release1 = TouchWithType(ET_TOUCH_RELEASED, ids[1]);
  event.OnTouch(release1);
  event.CleanupRemovedTouchPoints(release1);
  EXPECT_EQ(1, event.GetActionIndex());
  EXPECT_EQ(2U, event.GetPointerCount());

  TouchEvent release2 = TouchWithType(ET_TOUCH_RELEASED, ids[0]);
  event.OnTouch(release2);
  event.CleanupRemovedTouchPoints(release2);
  EXPECT_EQ(0, event.GetActionIndex());
  EXPECT_EQ(1U, event.GetPointerCount());

  TouchEvent release0 = TouchWithType(ET_TOUCH_RELEASED, ids[2]);
  event.OnTouch(release0);
  event.CleanupRemovedTouchPoints(release0);
  EXPECT_EQ(0U, event.GetPointerCount());
}

TEST(MotionEventAuraTest, PointerLocations) {
  // Test that location information is stored correctly.
  MotionEventAura event;

  const float kRawOffsetX = 11.1f;
  const float kRawOffsetY = 13.3f;

  int ids[] = {15, 13};
  float x;
  float y;
  float raw_x;
  float raw_y;
  float r;
  float p;

  x = 14.4f;
  y = 17.3f;
  raw_x = x + kRawOffsetX;
  raw_y = y + kRawOffsetY;
  r = 25.7f;
  p = 48.2f;
  TouchEvent press0 =
      TouchWithPosition(ET_TOUCH_PRESSED, ids[0], x, y, raw_x, raw_y, r, p);
  event.OnTouch(press0);

  EXPECT_EQ(1U, event.GetPointerCount());
  EXPECT_FLOAT_EQ(x, event.GetX(0));
  EXPECT_FLOAT_EQ(y, event.GetY(0));
  EXPECT_FLOAT_EQ(raw_x, event.GetRawX(0));
  EXPECT_FLOAT_EQ(raw_y, event.GetRawY(0));
  EXPECT_FLOAT_EQ(r, event.GetTouchMajor(0) / 2);
  EXPECT_FLOAT_EQ(p, event.GetPressure(0));

  x = 17.8f;
  y = 12.1f;
  raw_x = x + kRawOffsetX;
  raw_y = y + kRawOffsetY;
  r = 21.2f;
  p = 18.4f;
  TouchEvent press1 =
      TouchWithPosition(ET_TOUCH_PRESSED, ids[1], x, y, raw_x, raw_y, r, p);
  event.OnTouch(press1);

  EXPECT_EQ(2U, event.GetPointerCount());
  EXPECT_FLOAT_EQ(x, event.GetX(1));
  EXPECT_FLOAT_EQ(y, event.GetY(1));
  EXPECT_FLOAT_EQ(raw_x, event.GetRawX(1));
  EXPECT_FLOAT_EQ(raw_y, event.GetRawY(1));
  EXPECT_FLOAT_EQ(r, event.GetTouchMajor(1) / 2);
  EXPECT_FLOAT_EQ(p, event.GetPressure(1));

  // Test cloning of pointer location information.
  scoped_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(2U, clone->GetPointerCount());
  EXPECT_FLOAT_EQ(x, clone->GetX(1));
  EXPECT_FLOAT_EQ(y, clone->GetY(1));
  EXPECT_FLOAT_EQ(raw_x, event.GetRawX(1));
  EXPECT_FLOAT_EQ(raw_y, event.GetRawY(1));
  EXPECT_FLOAT_EQ(r, clone->GetTouchMajor(1) / 2);
  EXPECT_FLOAT_EQ(p, clone->GetPressure(1));

  x = 27.9f;
  y = 22.3f;
  raw_x = x + kRawOffsetX;
  raw_y = y + kRawOffsetY;
  r = 7.6f;
  p = 82.1f;
  TouchEvent move1 =
      TouchWithPosition(ET_TOUCH_MOVED, ids[1], x, y, raw_x, raw_y, r, p);
  event.OnTouch(move1);

  EXPECT_FLOAT_EQ(x, event.GetX(1));
  EXPECT_FLOAT_EQ(y, event.GetY(1));
  EXPECT_FLOAT_EQ(raw_x, event.GetRawX(1));
  EXPECT_FLOAT_EQ(raw_y, event.GetRawY(1));
  EXPECT_FLOAT_EQ(r, event.GetTouchMajor(1) / 2);
  EXPECT_FLOAT_EQ(p, event.GetPressure(1));

  x = 34.6f;
  y = 23.8f;
  raw_x = x + kRawOffsetX;
  raw_y = y + kRawOffsetY;
  r = 12.9f;
  p = 14.2f;
  TouchEvent move0 =
      TouchWithPosition(ET_TOUCH_MOVED, ids[0], x, y, raw_x, raw_y, r, p);
  event.OnTouch(move0);

  EXPECT_FLOAT_EQ(x, event.GetX(0));
  EXPECT_FLOAT_EQ(y, event.GetY(0));
  EXPECT_FLOAT_EQ(raw_x, event.GetRawX(0));
  EXPECT_FLOAT_EQ(raw_y, event.GetRawY(0));
  EXPECT_FLOAT_EQ(r, event.GetTouchMajor(0) / 2);
  EXPECT_FLOAT_EQ(p, event.GetPressure(0));
}

TEST(MotionEventAuraTest, Timestamps) {
  // Test that timestamp information is stored and converted correctly.
  MotionEventAura event;
  int ids[] = {7, 13};
  int times_in_ms[] = {59436, 60263, 82175};

  TouchEvent press0 = TouchWithTime(
      ui::ET_TOUCH_PRESSED, ids[0], times_in_ms[0]);
  event.OnTouch(press0);
  EXPECT_EQ(MsToTicks(times_in_ms[0]), event.GetEventTime());

  TouchEvent press1 = TouchWithTime(
      ui::ET_TOUCH_PRESSED, ids[1], times_in_ms[1]);
  event.OnTouch(press1);
  EXPECT_EQ(MsToTicks(times_in_ms[1]), event.GetEventTime());

  TouchEvent move0 = TouchWithTime(
      ui::ET_TOUCH_MOVED, ids[0], times_in_ms[2]);
  event.OnTouch(move0);
  EXPECT_EQ(MsToTicks(times_in_ms[2]), event.GetEventTime());

  // Test cloning of timestamp information.
  scoped_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(MsToTicks(times_in_ms[2]), clone->GetEventTime());
}

TEST(MotionEventAuraTest, CachedAction) {
  // Test that the cached action and cached action index are correct.
  int ids[] = {4, 6};
  MotionEventAura event;

  TouchEvent press0 = TouchWithType(ET_TOUCH_PRESSED, ids[0]);
  event.OnTouch(press0);
  EXPECT_EQ(MotionEvent::ACTION_DOWN, event.GetAction());
  EXPECT_EQ(1U, event.GetPointerCount());

  TouchEvent press1 = TouchWithType(ET_TOUCH_PRESSED, ids[1]);
  event.OnTouch(press1);
  EXPECT_EQ(MotionEvent::ACTION_POINTER_DOWN, event.GetAction());
  EXPECT_EQ(1, event.GetActionIndex());
  EXPECT_EQ(2U, event.GetPointerCount());

  // Test cloning of CachedAction information.
  scoped_ptr<MotionEvent> clone = event.Clone();
  EXPECT_EQ(MotionEvent::ACTION_POINTER_DOWN, clone->GetAction());
  EXPECT_EQ(1, clone->GetActionIndex());

  TouchEvent move0 = TouchWithType(ET_TOUCH_MOVED, ids[0]);
  event.OnTouch(move0);
  EXPECT_EQ(MotionEvent::ACTION_MOVE, event.GetAction());
  EXPECT_EQ(2U, event.GetPointerCount());

  TouchEvent release0 = TouchWithType(ET_TOUCH_RELEASED, ids[0]);
  event.OnTouch(release0);
  EXPECT_EQ(MotionEvent::ACTION_POINTER_UP, event.GetAction());
  EXPECT_EQ(2U, event.GetPointerCount());
  event.CleanupRemovedTouchPoints(release0);
  EXPECT_EQ(1U, event.GetPointerCount());

  TouchEvent release1 = TouchWithType(ET_TOUCH_RELEASED, ids[1]);
  event.OnTouch(release1);
  EXPECT_EQ(MotionEvent::ACTION_UP, event.GetAction());
  EXPECT_EQ(1U, event.GetPointerCount());
  event.CleanupRemovedTouchPoints(release1);
  EXPECT_EQ(0U, event.GetPointerCount());
}

TEST(MotionEventAuraTest, Cancel) {
  int ids[] = {4, 6};
  MotionEventAura event;

  TouchEvent press0 = TouchWithType(ET_TOUCH_PRESSED, ids[0]);
  event.OnTouch(press0);
  EXPECT_EQ(MotionEvent::ACTION_DOWN, event.GetAction());
  EXPECT_EQ(1U, event.GetPointerCount());

  TouchEvent press1 = TouchWithType(ET_TOUCH_PRESSED, ids[1]);
  event.OnTouch(press1);
  EXPECT_EQ(MotionEvent::ACTION_POINTER_DOWN, event.GetAction());
  EXPECT_EQ(1, event.GetActionIndex());
  EXPECT_EQ(2U, event.GetPointerCount());

  scoped_ptr<MotionEvent> cancel = event.Cancel();
  EXPECT_EQ(MotionEvent::ACTION_CANCEL, cancel->GetAction());
  EXPECT_EQ(2U, static_cast<MotionEventAura*>(cancel.get())->GetPointerCount());
}

}  // namespace ui
