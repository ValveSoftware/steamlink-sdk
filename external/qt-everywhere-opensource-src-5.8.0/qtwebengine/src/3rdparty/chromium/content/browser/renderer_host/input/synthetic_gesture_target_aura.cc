// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_aura.h"

#include <stddef.h>

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

using blink::WebTouchEvent;
using blink::WebMouseWheelEvent;

namespace content {

SyntheticGestureTargetAura::SyntheticGestureTargetAura(
    RenderWidgetHostImpl* host)
    : SyntheticGestureTargetBase(host) {
  blink::WebScreenInfo screenInfo;
  host->GetWebScreenInfo(&screenInfo);
  device_scale_factor_ = screenInfo.deviceScaleFactor;
}

void SyntheticGestureTargetAura::DispatchWebTouchEventToPlatform(
    const WebTouchEvent& web_touch,
    const ui::LatencyInfo& latency_info) {
  TouchEventWithLatencyInfo touch_with_latency(web_touch, latency_info);
  for (size_t i = 0; i < touch_with_latency.event.touchesLength; i++) {
    touch_with_latency.event.touches[i].position.x *= device_scale_factor_;
    touch_with_latency.event.touches[i].position.y *= device_scale_factor_;
    touch_with_latency.event.touches[i].radiusX *= device_scale_factor_;
    touch_with_latency.event.touches[i].radiusY *= device_scale_factor_;
  }
  ScopedVector<ui::TouchEvent> events;
  bool conversion_success = MakeUITouchEventsFromWebTouchEvents(
      touch_with_latency, &events, LOCAL_COORDINATES);
  DCHECK(conversion_success);

  aura::Window* window = GetWindow();
  aura::WindowTreeHost* host = window->GetHost();
  for (ScopedVector<ui::TouchEvent>::iterator iter = events.begin(),
      end = events.end(); iter != end; ++iter) {
    (*iter)->ConvertLocationToTarget(window, host->window());
    ui::EventDispatchDetails details =
        host->event_processor()->OnEventFromSource(*iter);
    if (details.dispatcher_destroyed)
      break;
  }
}

void SyntheticGestureTargetAura::DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo&) {
  ui::MouseEvent mouse_event(ui::ET_MOUSEWHEEL, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  gfx::PointF location(web_wheel.x * device_scale_factor_,
                       web_wheel.y * device_scale_factor_);
  mouse_event.set_location_f(location);
  mouse_event.set_root_location_f(location);
  ui::MouseWheelEvent wheel_event(
      mouse_event, web_wheel.deltaX, web_wheel.deltaY);

  aura::Window* window = GetWindow();
  wheel_event.ConvertLocationToTarget(window, window->GetRootWindow());
  ui::EventDispatchDetails details =
      window->GetHost()->event_processor()->OnEventFromSource(&wheel_event);
  if (details.dispatcher_destroyed)
    return;
}

namespace {

ui::EventType
WebMouseEventTypeToEventType(blink::WebInputEvent::Type web_type) {
  switch (web_type) {
    case blink::WebInputEvent::MouseDown:
      return ui::ET_MOUSE_PRESSED;

    case blink::WebInputEvent::MouseUp:
      return ui::ET_MOUSE_RELEASED;

    case blink::WebInputEvent::MouseMove:
      return ui::ET_MOUSE_MOVED;

    case blink::WebInputEvent::MouseEnter:
      return ui::ET_MOUSE_ENTERED;

    case blink::WebInputEvent::MouseLeave:
      return ui::ET_MOUSE_EXITED;

    case blink::WebInputEvent::ContextMenu:
      NOTREACHED() << "WebInputEvent::ContextMenu not supported by"
          "SyntheticGestureTargetAura";

    default:
      NOTREACHED();
  }

  return ui::ET_UNKNOWN;
}

int WebMouseEventButtonToFlags(blink::WebMouseEvent::Button button) {
  switch (button) {
    case blink::WebMouseEvent::ButtonLeft:
      return ui::EF_LEFT_MOUSE_BUTTON;

    case blink::WebMouseEvent::ButtonMiddle:
      return ui::EF_MIDDLE_MOUSE_BUTTON;

    case blink::WebMouseEvent::ButtonRight:
      return ui::EF_RIGHT_MOUSE_BUTTON;

    default:
      NOTREACHED();
  }

  return 0;
}

}  // namespace

void SyntheticGestureTargetAura::DispatchWebMouseEventToPlatform(
      const blink::WebMouseEvent& web_mouse,
      const ui::LatencyInfo& latency_info) {
  ui::EventType event_type = WebMouseEventTypeToEventType(web_mouse.type);
  int flags = WebMouseEventButtonToFlags(web_mouse.button);
  ui::MouseEvent mouse_event(event_type, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), flags, flags);
  gfx::PointF location(web_mouse.x * device_scale_factor_,
                       web_mouse.y * device_scale_factor_);
  mouse_event.set_location_f(location);
  mouse_event.set_root_location_f(location);

  aura::Window* window = GetWindow();
  mouse_event.ConvertLocationToTarget(window, window->GetRootWindow());
  ui::EventDispatchDetails details =
      window->GetHost()->event_processor()->OnEventFromSource(&mouse_event);
  if (details.dispatcher_destroyed)
    return;
}

SyntheticGestureParams::GestureSourceType
SyntheticGestureTargetAura::GetDefaultSyntheticGestureSourceType() const {
  return SyntheticGestureParams::TOUCH_INPUT;
}

float SyntheticGestureTargetAura::GetTouchSlopInDips() const {
  // - 1 because Aura considers a pointer to be moving if it has moved at least
  // 'max_touch_move_in_pixels_for_click' pixels.
  return ui::GestureConfiguration::GetInstance()
             ->max_touch_move_in_pixels_for_click() -
         1;
}

float SyntheticGestureTargetAura::GetMinScalingSpanInDips() const {
  return ui::GestureConfiguration::GetInstance()
      ->min_distance_for_pinch_scroll_in_pixels();
}

aura::Window* SyntheticGestureTargetAura::GetWindow() const {
  aura::Window* window = render_widget_host()->GetView()->GetNativeView();
  DCHECK(window);
  return window;
}

}  // namespace content
