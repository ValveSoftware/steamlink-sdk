// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/converters/blink/blink_input_events_type_converters.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event.h"

namespace mojo {

TEST(BlinkInputEventsConvertersTest, KeyEvent) {
  struct {
    ui::KeyEvent event;
    blink::WebInputEvent::Type web_type;
    int web_modifiers;
  } tests[] = {
      {ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE),
       blink::WebInputEvent::RawKeyDown, 0x0},
      {ui::KeyEvent(L'B', ui::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
       blink::WebInputEvent::Char,
       blink::WebInputEvent::ShiftKey | blink::WebInputEvent::ControlKey},
      {ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_C, ui::EF_ALT_DOWN),
       blink::WebInputEvent::KeyUp, blink::WebInputEvent::AltKey}};

  for (size_t i = 0; i < arraysize(tests); i++) {
    const std::unique_ptr<blink::WebInputEvent> web_event(
        TypeConverter<std::unique_ptr<blink::WebInputEvent>,
                      ui::Event>::Convert(tests[i].event));
    ASSERT_TRUE(web_event);
    ASSERT_TRUE(blink::WebInputEvent::isKeyboardEventType(web_event->type));
    ASSERT_EQ(tests[i].web_type, web_event->type);
    ASSERT_EQ(tests[i].web_modifiers, web_event->modifiers);

    const blink::WebKeyboardEvent* web_key_event =
        static_cast<const blink::WebKeyboardEvent*>(web_event.get());
    ASSERT_EQ(static_cast<int>(tests[i].event.GetLocatedWindowsKeyboardCode()),
              web_key_event->windowsKeyCode);
  }
}

TEST(BlinkInputEventsConvertersTest, WheelEvent) {
  const int kDeltaX = 14;
  const int kDeltaY = -3;
  ui::MouseWheelEvent ui_event(
      ui::MouseEvent(ui::ET_MOUSEWHEEL, gfx::Point(), gfx::Point(),
                     base::TimeTicks(), 0, 0),
      kDeltaX, kDeltaY);
  const std::unique_ptr<blink::WebInputEvent> web_event(
      TypeConverter<std::unique_ptr<blink::WebInputEvent>, ui::Event>::Convert(
          ui_event));
  ASSERT_TRUE(web_event);
  ASSERT_EQ(blink::WebInputEvent::MouseWheel, web_event->type);
  ASSERT_EQ(0, web_event->modifiers);

  const blink::WebMouseWheelEvent* web_wheel_event =
      static_cast<const blink::WebMouseWheelEvent*>(web_event.get());
  ASSERT_EQ(kDeltaX, web_wheel_event->deltaX);
  ASSERT_EQ(kDeltaY, web_wheel_event->deltaY);
}

TEST(BlinkInputEventsConvertersTest, MousePointerEvent) {
  struct {
    ui::EventType ui_type;
    blink::WebInputEvent::Type web_type;
    int ui_modifiers;
    int web_modifiers;
    gfx::Point location;
    gfx::Point screen_location;
  } tests[] = {
      {ui::ET_MOUSE_PRESSED, blink::WebInputEvent::MouseDown, 0x0, 0x0,
       gfx::Point(3, 5), gfx::Point(113, 125)},
      {ui::ET_MOUSE_RELEASED, blink::WebInputEvent::MouseUp,
       ui::EF_LEFT_MOUSE_BUTTON, blink::WebInputEvent::LeftButtonDown,
       gfx::Point(100, 1), gfx::Point(50, 1)},
      {ui::ET_MOUSE_MOVED, blink::WebInputEvent::MouseMove,
       ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
       blink::WebInputEvent::MiddleButtonDown |
           blink::WebInputEvent::RightButtonDown,
       gfx::Point(13, 3), gfx::Point(53, 3)},
  };

  for (size_t i = 0; i < arraysize(tests); i++) {
    ui::PointerEvent ui_event(ui::MouseEvent(
        tests[i].ui_type, tests[i].location, tests[i].screen_location,
        base::TimeTicks(), tests[i].ui_modifiers, 0));
    const std::unique_ptr<blink::WebInputEvent> web_event(
        TypeConverter<std::unique_ptr<blink::WebInputEvent>,
                      ui::Event>::Convert(ui_event));
    ASSERT_TRUE(web_event);
    ASSERT_TRUE(blink::WebInputEvent::isMouseEventType(web_event->type));
    ASSERT_EQ(tests[i].web_type, web_event->type);
    ASSERT_EQ(tests[i].web_modifiers, web_event->modifiers);

    const blink::WebMouseEvent* web_mouse_event =
        static_cast<const blink::WebMouseEvent*>(web_event.get());
    ASSERT_EQ(tests[i].location.x(), web_mouse_event->x);
    ASSERT_EQ(tests[i].location.y(), web_mouse_event->y);
    ASSERT_EQ(tests[i].screen_location.x(), web_mouse_event->globalX);
    ASSERT_EQ(tests[i].screen_location.y(), web_mouse_event->globalY);
  }
}

TEST(BlinkInputEventsConvertersTest, TouchPointerEvent) {
  struct {
    ui::EventType ui_type;
    blink::WebInputEvent::Type web_type;
    gfx::Point location;
    int touch_id;
    float radius_x;
    float radius_y;
  } tests[] = {
      {ui::ET_TOUCH_PRESSED, blink::WebInputEvent::TouchStart, gfx::Point(3, 5),
       1, 4.5, 5.5},
      {ui::ET_TOUCH_RELEASED, blink::WebInputEvent::TouchEnd,
       gfx::Point(100, 1), 2, 3.0, 3.0},
  };

  for (size_t i = 0; i < arraysize(tests); i++) {
    ui::PointerEvent ui_event(ui::TouchEvent(
        tests[i].ui_type, tests[i].location, 0, tests[i].touch_id,
        base::TimeTicks(), tests[i].radius_x, tests[i].radius_y, 0.0, 0.0));
    const std::unique_ptr<blink::WebInputEvent> web_event(
        TypeConverter<std::unique_ptr<blink::WebInputEvent>,
                      ui::Event>::Convert(ui_event));
    ASSERT_TRUE(web_event);
    ASSERT_TRUE(blink::WebInputEvent::isTouchEventType(web_event->type));
    ASSERT_EQ(tests[i].web_type, web_event->type);
    ASSERT_EQ(0, web_event->modifiers);

    const blink::WebTouchEvent* web_touch_event =
        static_cast<const blink::WebTouchEvent*>(web_event.get());
    const blink::WebTouchPoint* web_touch_point =
        &web_touch_event->touches[tests[i].touch_id];
    ASSERT_EQ(tests[i].radius_x, web_touch_point->radiusX);
    ASSERT_EQ(tests[i].radius_y, web_touch_point->radiusY);
  }
}
}  // namespace mojo
