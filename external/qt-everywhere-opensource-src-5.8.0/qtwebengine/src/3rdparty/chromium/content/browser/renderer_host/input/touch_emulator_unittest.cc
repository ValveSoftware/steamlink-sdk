// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_emulator.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/touch_emulator_client.h"
#include "content/common/input/web_input_event_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

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
        ack_touches_synchronously_(true),
        last_mouse_x_(-1),
        last_mouse_y_(-1) {
    last_event_time_seconds_ =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    event_time_delta_seconds_ = 0.1;
  }

  ~TouchEmulatorTest() override {}

  // testing::Test
  void SetUp() override {
    emulator_.reset(new TouchEmulator(this, 1.0f));
    emulator_->SetDoubleTapSupportForPageEnabled(false);
    emulator_->Enable(ui::GestureProviderConfigType::GENERIC_MOBILE);
  }

  void TearDown() override {
    emulator_->Disable();
    EXPECT_EQ("", ExpectedEvents());
  }

  void ForwardEmulatedGestureEvent(
      const blink::WebGestureEvent& event) override {
    forwarded_events_.push_back(event.type);
  }

  void ForwardEmulatedTouchEvent(const blink::WebTouchEvent& event) override {
    forwarded_events_.push_back(event.type);
    EXPECT_EQ(1U, event.touchesLength);
    EXPECT_EQ(last_mouse_x_, event.touches[0].position.x);
    EXPECT_EQ(last_mouse_y_, event.touches[0].position.y);
    const int all_buttons = WebInputEvent::LeftButtonDown |
        WebInputEvent::MiddleButtonDown | WebInputEvent::RightButtonDown;
    EXPECT_EQ(0, event.modifiers & all_buttons);
    WebInputEvent::DispatchType expected_dispatch_type =
        event.type == WebInputEvent::TouchCancel
            ? WebInputEvent::EventNonBlocking
            : WebInputEvent::Blocking;
    EXPECT_EQ(expected_dispatch_type, event.dispatchType);
    if (ack_touches_synchronously_) {
      emulator()->HandleTouchEventAck(
          event, INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    }
  }

  void SetCursor(const WebCursor& cursor) override {}

  void ShowContextMenuAtPoint(const gfx::Point& point) override {}

 protected:
  TouchEmulator* emulator() const {
    return emulator_.get();
  }

  int modifiers() const {
    return (shift_pressed_ ? WebInputEvent::ShiftKey : 0) |
        (mouse_pressed_ ? WebInputEvent::LeftButtonDown : 0);
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

  bool TouchStart(int x, int  y, bool ack) {
    return SendTouchEvent(
        WebInputEvent::TouchStart, WebTouchPoint::StatePressed, x, y, ack);
  }

  bool TouchMove(int x, int  y, bool ack) {
    return SendTouchEvent(
        WebInputEvent::TouchMove, WebTouchPoint::StateMoved, x, y, ack);
  }

  bool TouchEnd(int x, int  y, bool ack) {
    return SendTouchEvent(
        WebInputEvent::TouchEnd, WebTouchPoint::StateReleased, x, y, ack);
  }

  WebTouchEvent MakeTouchEvent(WebInputEvent::Type type,
      WebTouchPoint::State state, int x, int y) {
    WebTouchEvent event;
    event.type = type;
    event.timeStampSeconds = GetNextEventTimeSeconds();
    event.touchesLength = 1;
    event.touches[0].id = 0;
    event.touches[0].state = state;
    event.touches[0].position.x = x;
    event.touches[0].position.y = y;
    event.touches[0].screenPosition.x = x;
    event.touches[0].screenPosition.y = y;
    return event;
  }

  bool SendTouchEvent(WebInputEvent::Type type, WebTouchPoint::State state,
      int x, int y, bool ack) {
    WebTouchEvent event = MakeTouchEvent(type, state, x, y);
    if (emulator()->HandleTouchEvent(event)) {
      // Touch event is not forwarded.
      return false;
    }

    if (ack) {
      // Can't send ack if there are some pending acks.
      DCHECK(!touch_events_to_ack_.size());

      // Touch event is forwarded, ack should not be handled by emulator.
      EXPECT_FALSE(emulator()->HandleTouchEventAck(
          event, INPUT_EVENT_ACK_STATE_CONSUMED));
    } else {
      touch_events_to_ack_.push_back(event);
    }
    return true;
  }

  void AckOldestTouchEvent() {
    DCHECK(touch_events_to_ack_.size());
    WebTouchEvent event = touch_events_to_ack_[0];
    touch_events_to_ack_.erase(touch_events_to_ack_.begin());
    // Emulator should not handle ack from native stream.
    EXPECT_FALSE(emulator()->HandleTouchEventAck(
                 event, INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS));
  }

  void DisableSynchronousTouchAck() { ack_touches_synchronously_ = false; }

 private:
  std::unique_ptr<TouchEmulator> emulator_;
  std::vector<WebInputEvent::Type> forwarded_events_;
  double last_event_time_seconds_;
  double event_time_delta_seconds_;
  bool shift_pressed_;
  bool mouse_pressed_;
  bool ack_touches_synchronously_;
  int last_mouse_x_;
  int last_mouse_y_;
  std::vector<WebTouchEvent> touch_events_to_ack_;
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

TEST_F(TouchEmulatorTest, DoubleTapSupport) {
  emulator()->SetDoubleTapSupportForPageEnabled(true);
  MouseMove(100, 200);
  EXPECT_EQ("", ExpectedEvents());
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  MouseUp(100, 200);
  EXPECT_EQ("TouchEnd GestureTapUnconfirmed", ExpectedEvents());
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapCancel GestureTapDown", ExpectedEvents());
  MouseUp(100, 200);
  EXPECT_EQ("TouchEnd GestureTapCancel GestureDoubleTap", ExpectedEvents());
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

TEST_F(TouchEmulatorTest, CancelWithDelayedAck) {
  DisableSynchronousTouchAck();

  // Simulate a sequence that is interrupted by |CancelTouch()|.
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart", ExpectedEvents());
  MouseDrag(200, 200);
  EXPECT_EQ("TouchMove", ExpectedEvents());
  emulator()->CancelTouch();
  EXPECT_EQ("TouchCancel", ExpectedEvents());
  // The mouse up should have no effect as the sequence was already cancelled.
  MouseUp(400, 200);
  EXPECT_EQ("", ExpectedEvents());

  // Simulate a sequence that fully completes before |CancelTouch()|.
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart", ExpectedEvents());
  MouseUp(100, 200);
  EXPECT_EQ("TouchEnd", ExpectedEvents());
  // |CancelTouch| should have no effect as the sequence was already terminated.
  emulator()->CancelTouch();
  EXPECT_EQ("", ExpectedEvents());
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

  emulator()->Enable(ui::GestureProviderConfigType::GENERIC_MOBILE);
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

TEST_F(TouchEmulatorTest, DisableAndReenableDifferentConfig) {
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

  emulator()->Enable(ui::GestureProviderConfigType::GENERIC_DESKTOP);
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
  emulator()->Enable(ui::GestureProviderConfigType::GENERIC_MOBILE);
  EXPECT_TRUE(SendMouseWheelEvent());
}

TEST_F(TouchEmulatorTest, MultipleTouchStreams) {
  // Native stream should be blocked while emulated is active.
  MouseMove(100, 200);
  EXPECT_EQ("", ExpectedEvents());
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  EXPECT_FALSE(TouchStart(10, 10, true));
  EXPECT_FALSE(TouchMove(20, 20, true));
  MouseUp(200, 200);
  EXPECT_EQ(
      "TouchMove GestureTapCancel GestureScrollBegin GestureScrollUpdate"
      " TouchEnd GestureScrollEnd",
      ExpectedEvents());
  EXPECT_FALSE(TouchEnd(20, 20, true));

  // Emulated stream should be blocked while native is active.
  EXPECT_TRUE(TouchStart(10, 10, true));
  EXPECT_TRUE(TouchMove(20, 20, true));
  MouseDown(300, 200);
  EXPECT_EQ("", ExpectedEvents());
  // Re-enabling in the middle of a touch sequence should not affect this.
  emulator()->Disable();
  emulator()->Enable(ui::GestureProviderConfigType::GENERIC_MOBILE);
  MouseDrag(300, 300);
  EXPECT_EQ("", ExpectedEvents());
  MouseUp(300, 300);
  EXPECT_EQ("", ExpectedEvents());
  EXPECT_TRUE(TouchEnd(20, 20, true));
  EXPECT_EQ("", ExpectedEvents());

  // Late ack for TouchEnd should not mess things up.
  EXPECT_TRUE(TouchStart(10, 10, false));
  EXPECT_TRUE(TouchMove(20, 20, false));
  emulator()->Disable();
  EXPECT_TRUE(TouchEnd(20, 20, false));
  EXPECT_TRUE(TouchStart(30, 30, false));
  AckOldestTouchEvent(); // TouchStart.
  emulator()->Enable(ui::GestureProviderConfigType::GENERIC_MOBILE);
  AckOldestTouchEvent(); // TouchMove.
  AckOldestTouchEvent(); // TouchEnd.
  MouseDown(300, 200);
  EXPECT_EQ("", ExpectedEvents());
  MouseDrag(300, 300);
  EXPECT_EQ("", ExpectedEvents());
  MouseUp(300, 300);
  EXPECT_EQ("", ExpectedEvents());
  AckOldestTouchEvent(); // TouchStart.
  MouseDown(300, 200);
  EXPECT_EQ("", ExpectedEvents());
  EXPECT_TRUE(TouchMove(30, 40, true));
  EXPECT_TRUE(TouchEnd(30, 40, true));
  MouseUp(300, 200);
  EXPECT_EQ("", ExpectedEvents());

  // Emulation should be back to normal.
  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  MouseUp(200, 200);
  EXPECT_EQ(
      "TouchMove GestureTapCancel GestureScrollBegin GestureScrollUpdate"
      " TouchEnd GestureScrollEnd",
      ExpectedEvents());
}

TEST_F(TouchEmulatorTest, MultipleTouchStreamsLateEnable) {
  // Enabling in the middle of native touch sequence should be handled.
  // Send artificial late TouchEnd ack, like it is the first thing emulator
  // does see.
  WebTouchEvent event = MakeTouchEvent(
      WebInputEvent::TouchEnd, WebTouchPoint::StateReleased, 10, 10);
  EXPECT_FALSE(emulator()->HandleTouchEventAck(
      event, INPUT_EVENT_ACK_STATE_CONSUMED));

  MouseDown(100, 200);
  EXPECT_EQ("TouchStart GestureTapDown", ExpectedEvents());
  MouseUp(200, 200);
  EXPECT_EQ(
      "TouchMove GestureTapCancel GestureScrollBegin GestureScrollUpdate"
      " TouchEnd GestureScrollEnd",
      ExpectedEvents());
}

TEST_F(TouchEmulatorTest, CancelAfterDisableDoesNotCrash) {
  DisableSynchronousTouchAck();
  MouseDown(100, 200);
  emulator()->Disable();
  EXPECT_EQ("TouchStart TouchCancel", ExpectedEvents());
  emulator()->CancelTouch();
}

TEST_F(TouchEmulatorTest, ConstructorWithHighDeviceScaleDoesNotCrash) {
  TouchEmulator(this, 4.0f);
}

}  // namespace content
