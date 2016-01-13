// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "content/common/input_messages.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event.h"
#include "ui/events/latency_info.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;

namespace content {
namespace {

// This value was determined experimentally. It was sufficient to not cause a
// fling on Android and Aura.
const int kPointerAssumedStoppedTimeMs = 100;

// SyntheticGestureTargetBase passes input events straight on to the renderer
// without going through a gesture recognition framework. There is thus no touch
// slop.
const float kTouchSlopInDips = 0.0f;

}  // namespace

SyntheticGestureTargetBase::SyntheticGestureTargetBase(
    RenderWidgetHostImpl* host)
    : host_(host) {
  DCHECK(host);
}

SyntheticGestureTargetBase::~SyntheticGestureTargetBase() {
}

void SyntheticGestureTargetBase::DispatchInputEventToPlatform(
    const WebInputEvent& event) {
  TRACE_EVENT1("input",
               "SyntheticGestureTarget::DispatchInputEventToPlatform",
               "type", WebInputEventTraits::GetName(event.type));

  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, 0);

  if (WebInputEvent::isTouchEventType(event.type)) {
    const WebTouchEvent& web_touch =
        static_cast<const WebTouchEvent&>(event);

    // Check that all touch pointers are within the content bounds.
    if (web_touch.type == WebInputEvent::TouchStart) {
      for (unsigned i = 0; i < web_touch.touchesLength; i++)
        CHECK(web_touch.touches[i].state != WebTouchPoint::StatePressed ||
              PointIsWithinContents(web_touch.touches[i].position.x,
                                    web_touch.touches[i].position.y))
            << "Touch coordinates are not within content bounds on TouchStart.";
    }

    DispatchWebTouchEventToPlatform(web_touch, latency_info);
  } else if (event.type == WebInputEvent::MouseWheel) {
    const WebMouseWheelEvent& web_wheel =
        static_cast<const WebMouseWheelEvent&>(event);
    CHECK(PointIsWithinContents(web_wheel.x, web_wheel.y))
        << "Mouse wheel position is not within content bounds.";
    DispatchWebMouseWheelEventToPlatform(web_wheel, latency_info);
  } else if (WebInputEvent::isMouseEventType(event.type)) {
    const WebMouseEvent& web_mouse =
        static_cast<const WebMouseEvent&>(event);
    CHECK(event.type != WebInputEvent::MouseDown ||
          PointIsWithinContents(web_mouse.x, web_mouse.y))
        << "Mouse pointer is not within content bounds on MouseDown.";
    DispatchWebMouseEventToPlatform(web_mouse, latency_info);
  } else {
    NOTREACHED();
  }
}

void SyntheticGestureTargetBase::DispatchWebTouchEventToPlatform(
      const blink::WebTouchEvent& web_touch,
      const ui::LatencyInfo& latency_info) {
  // We assume that platforms supporting touch have their own implementation of
  // SyntheticGestureTarget to route the events through their respective input
  // stack.
  CHECK(false) << "Touch events not supported for this browser.";
}

void SyntheticGestureTargetBase::DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo& latency_info) {
  host_->ForwardWheelEventWithLatencyInfo(web_wheel, latency_info);
}

void SyntheticGestureTargetBase::DispatchWebMouseEventToPlatform(
      const blink::WebMouseEvent& web_mouse,
      const ui::LatencyInfo& latency_info) {
  host_->ForwardMouseEventWithLatencyInfo(web_mouse, latency_info);
}

void SyntheticGestureTargetBase::SetNeedsFlush() {
  host_->SetNeedsFlush();
}

SyntheticGestureParams::GestureSourceType
SyntheticGestureTargetBase::GetDefaultSyntheticGestureSourceType() const {
  return SyntheticGestureParams::MOUSE_INPUT;
}

base::TimeDelta SyntheticGestureTargetBase::PointerAssumedStoppedTime()
    const {
  return base::TimeDelta::FromMilliseconds(kPointerAssumedStoppedTimeMs);
}

float SyntheticGestureTargetBase::GetTouchSlopInDips() const {
  return kTouchSlopInDips;
}

float SyntheticGestureTargetBase::GetMinScalingSpanInDips() const {
  // The minimum scaling distance is only relevant for touch gestures and the
  // base target doesn't support touch.
  NOTREACHED();
  return 0.0f;
}

bool SyntheticGestureTargetBase::PointIsWithinContents(int x, int y) const {
  gfx::Rect bounds = host_->GetView()->GetViewBounds();
  bounds -= bounds.OffsetFromOrigin();  // Translate the bounds to (0,0).
  return bounds.Contains(x, y);
}

}  // namespace content
