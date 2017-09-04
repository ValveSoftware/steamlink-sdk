// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_WEB_INPUT_EVENT_H_
#define UI_EVENTS_BLINK_WEB_INPUT_EVENT_H_

#include "base/callback.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace gfx {
class Point;
}

namespace ui {
class GestureEvent;
class KeyEvent;
class LocatedEvent;
class MouseEvent;
class MouseWheelEvent;
class ScrollEvent;

// Several methods take a |screen_location_callback| which should translate the
// provided coordinates relative to the hosting window, rather than the top
// level platform window.
//
// If a valid event cannot be created, then the returned events will have the
// type UNKNOWN.
//
// TODO(jonross): Ideally this callback would not be needed. The callback should
// be removed once ui::Event::root_location has been deprecated and replaced
// with ui::Event::screen_location (crbug.com/608547)
blink::WebMouseEvent MakeWebMouseEvent(
    const MouseEvent& event,
    const base::Callback<gfx::Point(const ui::LocatedEvent& event)>&
        screen_location_callback);
blink::WebMouseWheelEvent MakeWebMouseWheelEvent(
    const MouseWheelEvent& event,
    const base::Callback<gfx::Point(const ui::LocatedEvent& event)>&
        screen_location_callback);
blink::WebMouseWheelEvent MakeWebMouseWheelEvent(
    const ScrollEvent& event,
    const base::Callback<gfx::Point(const ui::LocatedEvent& event)>&
        screen_location_callback);
blink::WebKeyboardEvent MakeWebKeyboardEvent(const KeyEvent& event);
blink::WebGestureEvent MakeWebGestureEvent(
    const GestureEvent& event,
    const base::Callback<gfx::Point(const ui::LocatedEvent& event)>&
        screen_location_callback);
blink::WebGestureEvent MakeWebGestureEvent(
    const ScrollEvent& event,
    const base::Callback<gfx::Point(const ui::LocatedEvent& event)>&
        screen_location_callback);
blink::WebGestureEvent MakeWebGestureEventFlingCancel();

}  // namespace ui

#endif  // UI_EVENTS_BLINK_WEB_INPUT_EVENT_H_
