// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/gesture_text_selector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/events/test/mock_motion_event.h"
#include "ui/gfx/geometry/rect_f.h"

using ui::GestureEventData;
using ui::GestureEventDetails;
using ui::MotionEvent;
using ui::test::MockMotionEvent;

namespace content {

class GestureTextSelectorTest : public testing::Test,
                                public GestureTextSelectorClient {
 public:
  GestureTextSelectorTest() {}
  virtual ~GestureTextSelectorTest() {}

  // Test implementation.
  virtual void SetUp() OVERRIDE {
    selector_.reset(new GestureTextSelector(this));
    event_log_.clear();
  }

  virtual void TearDown() OVERRIDE {
    selector_.reset();
    event_log_.clear();
  }

  // GestureTextSelectorClient implementation.
  virtual void ShowSelectionHandlesAutomatically() OVERRIDE {
    event_log_.push_back("Show");
  }

  virtual void SelectRange(float x1, float y1, float x2, float y2) OVERRIDE {
    event_log_.push_back("SelectRange");
  }

  virtual void Unselect() OVERRIDE {
    event_log_.push_back("Unselect");
  }

  virtual void LongPress(base::TimeTicks time, float x, float y) OVERRIDE {
    event_log_.push_back("LongPress");
  }

 protected:
  scoped_ptr<GestureTextSelector> selector_;
  std::vector<std::string> event_log_;
};

TEST_F(GestureTextSelectorTest, ShouldStartTextSelection) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  {  // Touched with a finger.
    MockMotionEvent e(MotionEvent::ACTION_DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::TOOL_TYPE_FINGER);
    e.SetButtonState(0);
    EXPECT_FALSE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, but no button pressed.
    MockMotionEvent e(MotionEvent::ACTION_DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
    e.SetButtonState(0);
    EXPECT_FALSE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, with first button (BUTTON_SECONDARY) pressed.
    MockMotionEvent e(MotionEvent::ACTION_DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
    e.SetButtonState(MotionEvent::BUTTON_SECONDARY);
    EXPECT_TRUE(selector_->ShouldStartTextSelection(e));
  }

  {  // Touched with a stylus, with two buttons pressed.
    MockMotionEvent e(MotionEvent::ACTION_DOWN, event_time, 50.0f, 50.0f);
    e.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
    e.SetButtonState(
        MotionEvent::BUTTON_SECONDARY | MotionEvent::BUTTON_TERTIARY);
    EXPECT_FALSE(selector_->ShouldStartTextSelection(e));
  }
}

TEST_F(GestureTextSelectorTest, FingerTouch) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x = 50.0f;
  const float y = 30.0f;
  // 1. Touched with a finger: ignored
  MockMotionEvent finger(MotionEvent::ACTION_DOWN, event_time, x, y);
  finger.SetToolType(0, MotionEvent::TOOL_TYPE_FINGER);
  EXPECT_FALSE(selector_->OnTouchEvent(finger));
  // We do not consume finger events.
  EXPECT_TRUE(event_log_.empty());
}

TEST_F(GestureTextSelectorTest, PenDragging) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x1 = 50.0f;
  const float y1 = 30.0f;
  const float x2 = 100.0f;
  const float y2 = 90.0f;
  // 1. ACTION_DOWN with stylus + button
  event_time += base::TimeDelta::FromMilliseconds(10);
  MockMotionEvent action_down(MotionEvent::ACTION_DOWN, event_time, x1, y1);
  action_down.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_down.SetButtonState(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_down));
  EXPECT_TRUE(event_log_.empty());

  // 2. ACTION_MOVE
  event_time += base::TimeDelta::FromMilliseconds(10);
  MockMotionEvent action_move(MotionEvent::ACTION_MOVE, event_time, x2, y2);
  action_move.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_move.SetButtonState(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_TRUE(event_log_.empty());

  // 3. DOUBLE TAP
  // Suppress most gesture events when in text selection mode.
  event_time += base::TimeDelta::FromMilliseconds(10);
  const GestureEventData double_tap(
      GestureEventDetails(ui::ET_GESTURE_DOUBLE_TAP, 0, 0), 0, event_time,
      x2, y2, x2, y2, 1, gfx::RectF(0, 0, 0, 0));
  EXPECT_TRUE(selector_->OnGestureEvent(double_tap));
  EXPECT_TRUE(event_log_.empty());

  // 4. ET_GESTURE_SCROLL_BEGIN
  event_time += base::TimeDelta::FromMilliseconds(10);
  const GestureEventData scroll_begin(
      GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 0), 0, event_time,
      x1, y1, x1, y1, 1, gfx::RectF(0, 0, 0, 0));
  EXPECT_TRUE(selector_->OnGestureEvent(scroll_begin));
  EXPECT_EQ(1u, event_log_.size());  // Unselect

  // 5. ET_GESTURE_SCROLL_UPDATE
  event_time += base::TimeDelta::FromMilliseconds(10);
  const GestureEventData scroll_update(
      GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, 0), 0, event_time,
      x2, y2, x2, y2, 1, gfx::RectF(0, 0, 0, 0));
  EXPECT_TRUE(selector_->OnGestureEvent(scroll_update));
  EXPECT_EQ(3u, event_log_.size());  // Unselect, Show, SelectRange
  EXPECT_STREQ("SelectRange", event_log_.back().c_str());

  // 6. ACTION_UP
  event_time += base::TimeDelta::FromMilliseconds(10);
  MockMotionEvent action_up(MotionEvent::ACTION_UP, event_time, x2, y2);
  action_up.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_up.SetButtonState(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_up));
  EXPECT_EQ(3u, event_log_.size());  // NO CHANGE

  // 7. ET_GESTURE_SCROLL_END
  event_time += base::TimeDelta::FromMilliseconds(10);
  const GestureEventData scroll_end(
      GestureEventDetails(ui::ET_GESTURE_SCROLL_END, 0, 0), 0, event_time,
      x2, y2, x2, y2, 1, gfx::RectF(0, 0, 0, 0));
  EXPECT_TRUE(selector_->OnGestureEvent(scroll_end));
  EXPECT_EQ(3u, event_log_.size());  // NO CHANGE
}

TEST_F(GestureTextSelectorTest, TapToSelectWord) {
  base::TimeTicks event_time = base::TimeTicks::Now();
  const float x1 = 50.0f;
  const float y1 = 30.0f;
  const float x2 = 51.0f;
  const float y2 = 31.0f;
  // 1. ACTION_DOWN with stylus + button
  event_time += base::TimeDelta::FromMilliseconds(10);
  MockMotionEvent action_down(MotionEvent::ACTION_DOWN, event_time, x1, y1);
  action_down.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_down.SetButtonState(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_down));
  EXPECT_TRUE(event_log_.empty());

  // 5. TAP_DOWN
  event_time += base::TimeDelta::FromMilliseconds(10);
  const GestureEventData tap_down(
      GestureEventDetails(ui::ET_GESTURE_TAP_DOWN, 0, 0), 0, event_time,
      x2, y2, x2, y2, 1, gfx::RectF(0, 0, 0, 0));
  EXPECT_TRUE(selector_->OnGestureEvent(tap_down));
  EXPECT_TRUE(event_log_.empty());

  // 2. ACTION_MOVE
  event_time += base::TimeDelta::FromMilliseconds(10);
  MockMotionEvent action_move(MotionEvent::ACTION_MOVE, event_time, x2, y2);
  action_move.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_move.SetButtonState(MotionEvent::BUTTON_SECONDARY);
  EXPECT_TRUE(selector_->OnTouchEvent(action_move));
  EXPECT_TRUE(event_log_.empty());

  // 3. ACTION_UP
  event_time += base::TimeDelta::FromMilliseconds(10);
  MockMotionEvent action_up(MotionEvent::ACTION_UP, event_time, x2, y2);
  action_up.SetToolType(0, MotionEvent::TOOL_TYPE_STYLUS);
  action_up.SetButtonState(0);
  EXPECT_TRUE(selector_->OnTouchEvent(action_up));
  EXPECT_TRUE(event_log_.empty());

  // 4. TAP
  event_time += base::TimeDelta::FromMilliseconds(10);
  const GestureEventData tap(
      GestureEventDetails(ui::ET_GESTURE_TAP, 0, 0), 0, event_time,
      x1, y1, x1, y1, 1, gfx::RectF(0, 0, 0, 0));
  EXPECT_TRUE(selector_->OnGestureEvent(tap));
  EXPECT_EQ(1u, event_log_.size());  // LongPress
  EXPECT_STREQ("LongPress", event_log_.back().c_str());
}

}  // namespace content
