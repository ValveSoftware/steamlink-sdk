// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/input/touch_emulator_client.h"
#include "content/common/input/web_input_event_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/gesture_detection/gesture_config_helper.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/aura/test/test_screen.h"
#endif

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

class TouchEmulatorTest : public testing::Test,
                          public TouchEmulatorClient {
 public:
  TouchEmulatorTest()
      : shift_pressed_(false),
        mouse_pressed_(false),
        last_mouse_x_(-1),
        last_mouse_y_(-1) {
    last_event_time_seconds_ =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    event_time_delta_seconds_ = 0.1;
  }

  virtual ~TouchEmulatorTest() {}

  // testing::Test
  virtual void SetUp() OVERRIDE {
#if defined(USE_AURA)
    aura::Env::CreateInstance(true);
    screen_.reset(aura::TestScreen::Create(gfx::Size()));
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
#endif

    emulator_.reset(new TouchEmulator(this));
    emulator_->Enable(true /* allow_pinch */);
  }

  virtual void TearDown() OVERRIDE {
    emulator_->Disable();
    EXPECT_EQ("", ExpectedEvents());

#if defined(USE_AURA)
    aura::Env::DeleteInstance();
    screen_.reset();
#endif
  }

  virtual void ForwardGestureEvent(
      const blink::WebGestureEvent& event) OVERRIDE {
    forwarded_events_.push_back(event.type);
  }

  virtual void ForwardTouchEvent(
      const blink::WebTouchEvent& event) OVERRIDE {
    forwarded_events_.push_back(event.type);
    EXPECT_EQ(1U, event.touchesLength);
    EXPECT_EQ(last_mouse_x_, event.touches[0].position.x);
    EXPECT_EQ(last_mouse_y_, event.touches[0].position.y);
    int expectedCancelable = event.type != WebInputEvent::TouchCancel;
    EXPECT_EQ(expectedCancelable, event.cancelable);
    emulator()->HandleTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  }

  virtual void SetCursor(const WebCursor& cursor) OVERRIDE {}

  virtual void ShowContextMenuAtPoint(const gfx::Point& point) OVERRIDE {}

 protected:
  TouchEmulator* emulator() const {
    return emulator_.get();
  }

  int modifiers() const {
    return shift_pressed_ ? WebInputEvent::ShiftKey : 0;
  }

  std::string ExpectedEvents() {
    std::string result;
    for (size_t i = 0; i < forwarded_events_.size(); ++i) {
      if (i != 0)
        result += " ";
      result += WebInputEventTraits::GetName(forwarded_events_[i]);
    }
    forwarded_events_.clear();
    return result;
  }

  double GetNextEventTimeSeconds() {
    last_event_time_seconds_ += event_time_delta_seconds_;
    return last_event_time_seconds_;
  }

  void set_event_time_delta_seconds_(double delta) {
    event_time_delta_seconds_ = delta;
  }

  void SendKeyboardEvent(WebInputEvent::Type type) {
    WebKeyboardEvent event;
    event.timeStampSeconds = GetNextEventTimeSeconds();
    event.type = type;
    event.modifiers = modifiers();
    emulator()->HandleKeyboardEvent(event);
  }

  void PressShift() {
    DCHECK(!shift_pressed_);
    shift_pressed_ = true;
    SendKeyboardEvent(WebInputEvent::KeyDown);
  }

  void ReleaseShift() {
    DCHECK(shift_pressed_);
    shift_pressed_ = false;
    SendKeyboardEvent(WebInputEvent::KeyUp);
  }

  void SendMouseEvent(WebInputEvent::Type type, int  x, int y) {
    WebMouseEvent event;
    event.timeStampSeconds = GetNextEventTimeSeconds();
    event.type = type;
    event.button = mouse_pressed_ ? WebMouseEvent::ButtonLeft :
        WebMouseEvent::ButtonNone;
    event.modifiers = modifiers();
    last_mouse_x_ = x;
    last_mouse_y_ = y;
    event.x = event.windowX = event.globalX = x;
    event.y = event.windowY = event.globalY = y;
    emulator()->HandleMouseEvent(event);
  }

  bool SendMouseWheelEvent() {
    WebMouseWheelEvent event;
    event.type = WebInputEvent::MouseWheel;
    event.timeStampSeconds = GetNextEventTimeSeconds();
    // Return whether mouse wheel is forwarded.
    return !emulator()->HandleMouseWheelEvent(event);
  }

  void MouseDown(int x, int y) {
    DCHECK(!mouse_pressed_);
    if (x != last_mouse_x_ || y != last_mouse_y_)
      SendMouseEvent(WebInputEvent::MouseMove, x, y);
    mouse_pressed_ = true;
    SendMouseEvent(WebInputEvent::MouseDown, x, y);
  }

  void MouseDrag(int x, int y) {
    DCHECK(mouse_pressed_);
    SendMouseEvent(WebInputEvent::MouseMove, x, y);
  }

  void MouseMove(int x, int y) {
    DCHECK(!mouse_pressed_);
    SendMouseEvent(WebInputEvent::MouseMove, x, y);
  }

  void MouseUp(int x, int y) {
    DCHECK(mouse_pressed_);
    if (x != last_mouse_x_ || y != last_mouse_y_)
      SendMouseEvent(WebInputEvent::MouseMove, x, y);
    SendMouseEvent(WebInputEvent::MouseUp, x, y);
    mouse_pressed_ = false;
  }

 private:
  scoped_ptr<TouchEmulator> emulator_;
  std::vector<WebInputEvent::Type> forwarded_events_;
#if defined(USE_AURA)
  scoped_ptr<gfx::Screen> screen_;
#endif
  double last_event_time_seconds_;
  double event_time_delta_seconds_;
  bool shift_pressed_;
  bool mouse_pressed_;
  int last_mouse_x_;
  int last_mouse_y_;
  base::MessageLoopForUI message_loop_;
};


TEST_F(TouchEmulatorTest, NoTouches) {
  MouseMove(100, 200);
  MouseMove(300, 300);
  EXPECT_EQ("", ExpectedEvents());
}

TEST_F(TouchEmulatorTest, Touch) {
  MouseMove(100, 200);
  EXPECT_EQ("", ExpectedEvents());
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  MouseUp(200, 200);
  EXPECT_EQ(
      "TouchMove GestureTapCancel GestureScrollBegin GestureScrollUpdate"
      " TouchEnd GestureScrollEnd",
      ExpectedEvents());
}

TEST_F(TouchEmulatorTest, MultipleTouches) {
  MouseMove(100, 200);
  EXPECT_EQ("", ExpectedEvents());
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  MouseUp(200, 200);
  EXPECT_EQ(
      "TouchMove GestureTapCancel GestureScrollBegin GestureScrollUpdate"
      " TouchEnd GestureScrollEnd",
      ExpectedEvents());
  MouseMove(300, 200);
  MouseMove(200, 200);
  EXPECT_EQ("", ExpectedEvents());
  MouseDown(300, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  MouseDrag(300, 300);
  EXPECT_EQ(
      "TouchMove GestureTapCancel GestureScrollBegin GestureScrollUpdate",
      ExpectedEvents());
  MouseDrag(300, 400);
  EXPECT_EQ("TouchMove GestureScrollUpdate", ExpectedEvents());
  MouseUp(300, 500);
  EXPECT_EQ(
      "TouchMove GestureScrollUpdate TouchEnd GestureScrollEnd",
      ExpectedEvents());
}

TEST_F(TouchEmulatorTest, Pinch) {
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  MouseDrag(200, 200);
  EXPECT_EQ(
      "TouchMove GestureTapCancel GestureScrollBegin GestureScrollUpdate",
      ExpectedEvents());
  PressShift();
  EXPECT_EQ("", ExpectedEvents());
  MouseDrag(300, 200);
  EXPECT_EQ("TouchMove GesturePinchBegin", ExpectedEvents());
  ReleaseShift();
  EXPECT_EQ("", ExpectedEvents());
  MouseDrag(400, 200);
  EXPECT_EQ(
      "TouchMove GesturePinchEnd GestureScrollUpdate",
      ExpectedEvents());
  MouseUp(400, 200);
  EXPECT_EQ("TouchEnd GestureScrollEnd", ExpectedEvents());
}

TEST_F(TouchEmulatorTest, DisableAndReenable) {
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  MouseDrag(200, 200);
  EXPECT_EQ(
      "TouchMove GestureTapCancel GestureScrollBegin GestureScrollUpdate",
      ExpectedEvents());
  PressShift();
  MouseDrag(300, 200);
  EXPECT_EQ("TouchMove GesturePinchBegin", ExpectedEvents());

  // Disable while pinch is in progress.
  emulator()->Disable();
  EXPECT_EQ("TouchCancel GesturePinchEnd GestureScrollEnd", ExpectedEvents());
  MouseUp(300, 200);
  ReleaseShift();
  MouseMove(300, 300);
  EXPECT_EQ("", ExpectedEvents());

  emulator()->Enable(true /* allow_pinch */);
  MouseDown(300, 300);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  MouseDrag(300, 400);
  EXPECT_EQ(
      "TouchMove GestureTapCancel GestureScrollBegin GestureScrollUpdate",
      ExpectedEvents());

  // Disable while scroll is in progress.
  emulator()->Disable();
  EXPECT_EQ("TouchCancel GestureScrollEnd", ExpectedEvents());
}

TEST_F(TouchEmulatorTest, MouseMovesDropped) {
  MouseMove(100, 200);
  EXPECT_EQ("", ExpectedEvents());
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());

  // Mouse move after mouse down is never dropped.
  set_event_time_delta_seconds_(0.001);
  MouseDrag(200, 200);
  EXPECT_EQ(
      "TouchMove GestureTapCancel GestureScrollBegin GestureScrollUpdate",
      ExpectedEvents());

  // The following mouse moves are dropped.
  MouseDrag(300, 200);
  EXPECT_EQ("", ExpectedEvents());
  MouseDrag(350, 200);
  EXPECT_EQ("", ExpectedEvents());

  // Dispatching again.
  set_event_time_delta_seconds_(0.1);
  MouseDrag(400, 200);
  EXPECT_EQ(
      "TouchMove GestureScrollUpdate",
      ExpectedEvents());
  MouseUp(400, 200);
  EXPECT_EQ(
      "TouchEnd GestureScrollEnd",
      ExpectedEvents());
}

TEST_F(TouchEmulatorTest, MouseWheel) {
  MouseMove(100, 200);
  EXPECT_EQ("", ExpectedEvents());
  EXPECT_TRUE(SendMouseWheelEvent());
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  EXPECT_FALSE(SendMouseWheelEvent());
  MouseUp(100, 200);
  EXPECT_EQ("TouchEnd GestureShowPress GestureTap", ExpectedEvents());
  EXPECT_TRUE(SendMouseWheelEvent());
  MouseDown(300, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  EXPECT_FALSE(SendMouseWheelEvent());
  emulator()->Disable();
  EXPECT_EQ("TouchCancel GestureTapCancel", ExpectedEvents());
  EXPECT_TRUE(SendMouseWheelEvent());
  emulator()->Enable(true /* allow_pinch */);
  EXPECT_TRUE(SendMouseWheelEvent());
}

}  // namespace content
