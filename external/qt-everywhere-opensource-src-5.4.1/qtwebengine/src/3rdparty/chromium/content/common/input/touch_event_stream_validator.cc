// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/touch_event_stream_validator.h"

#include "base/logging.h"
#include "content/common/input/web_touch_event_traits.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {
namespace {

const WebTouchPoint* FindTouchPoint(const WebTouchEvent& event, int id) {
  for (unsigned i = 0; i < event.touchesLength; ++i) {
    if (event.touches[i].id == id)
      return &event.touches[i];
  }
  return NULL;
}

}  // namespace

TouchEventStreamValidator::TouchEventStreamValidator() {
}

TouchEventStreamValidator::~TouchEventStreamValidator() {
}

bool TouchEventStreamValidator::Validate(const WebTouchEvent& event,
                                         std::string* error_msg) {
  DCHECK(error_msg);
  error_msg->clear();

  WebTouchEvent previous_event = previous_event_;
  previous_event_ = event;

  // Allow "hard" restarting of touch stream validation. This is necessary
  // in cases where touch event forwarding ceases in response to the event ack
  // or removal of touch handlers.
  if (WebTouchEventTraits::IsTouchSequenceStart(event))
    previous_event = WebTouchEvent();

  // Unreleased points from the previous event should exist in the latest event.
  for (unsigned i = 0; i < previous_event.touchesLength; ++i) {
    const WebTouchPoint& previous_point = previous_event.touches[i];
    if (previous_point.state == WebTouchPoint::StateCancelled ||
        previous_point.state == WebTouchPoint::StateReleased)
      continue;

    const WebTouchPoint* point = FindTouchPoint(event, previous_point.id);
    if (!point)
      error_msg->append("Previously active touch point not in new event.\n");
  }

  for (unsigned i = 0; i < event.touchesLength; ++i) {
    const WebTouchPoint& point = event.touches[i];
    const WebTouchPoint* previous_point =
        FindTouchPoint(previous_event, point.id);

    // The point should exist in the previous event if it is not a new point.
    if (!previous_point) {
      if (point.state != WebTouchPoint::StatePressed)
        error_msg->append("Active touch point not found in previous event.\n");
    } else {
      if (point.state == WebTouchPoint::StatePressed &&
          previous_point->state != WebTouchPoint::StateCancelled &&
          previous_point->state != WebTouchPoint::StateReleased) {
        error_msg->append("Pressed touch point id already exists.\n");
      }
    }

    switch (point.state) {
      case WebTouchPoint::StateUndefined:
        error_msg->append("Undefined WebTouchPoint state.\n");
        break;

      case WebTouchPoint::StateReleased:
        if (event.type != WebInputEvent::TouchEnd)
          error_msg->append("Released touch point outside touchend.\n");
        break;

      case WebTouchPoint::StatePressed:
        if (event.type != WebInputEvent::TouchStart)
          error_msg->append("Pressed touch point outside touchstart.\n");
        break;

      case WebTouchPoint::StateMoved:
        if (event.type != WebInputEvent::TouchMove)
          error_msg->append("Moved touch point outside touchmove.\n");
        break;

      case WebTouchPoint::StateStationary:
        break;

      case WebTouchPoint::StateCancelled:
        if (event.type != WebInputEvent::TouchCancel)
          error_msg->append("Cancelled touch point outside touchcancel.\n");
        break;
    }
  }
  return error_msg->empty();
}

}  // namespace content
