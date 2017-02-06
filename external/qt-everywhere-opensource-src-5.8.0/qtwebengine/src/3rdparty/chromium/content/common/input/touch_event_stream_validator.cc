// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/touch_event_stream_validator.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/common/input/web_touch_event_traits.h"

using base::StringPrintf;
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

std::string TouchPointIdsToString(const WebTouchEvent& event) {
  std::stringstream ss;
  for (unsigned i = 0; i < event.touchesLength; ++i) {
    if (i)
      ss << ",";
    ss << event.touches[i].id;
  }
  return ss.str();
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

  // TouchScrollStarted is not part of a regular touch event stream.
  if (event.type == WebInputEvent::TouchScrollStarted)
    return true;

  WebTouchEvent previous_event = previous_event_;
  previous_event_ = event;

  if (!event.touchesLength) {
    error_msg->append("Touch event is empty.\n");
    return false;
  }

  if (!WebInputEvent::isTouchEventType(event.type)) {
    error_msg->append(StringPrintf("Touch event has invalid type: %s\n",
                                   WebInputEventTraits::GetName(event.type)));
  }

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
    if (!point) {
      error_msg->append(StringPrintf(
          "Previously active touch point (id=%d) not in new event (ids=%s).\n",
          previous_point.id, TouchPointIdsToString(event).c_str()));
    }
  }

  bool found_valid_state_for_type = false;
  for (unsigned i = 0; i < event.touchesLength; ++i) {
    const WebTouchPoint& point = event.touches[i];
    const WebTouchPoint* previous_point =
        FindTouchPoint(previous_event, point.id);

    // The point should exist in the previous event if it is not a new point.
    if (!previous_point) {
      if (point.state != WebTouchPoint::StatePressed)
        error_msg->append(StringPrintf(
            "Active touch point (id=%d) not in previous event (ids=%s).\n",
            point.id, TouchPointIdsToString(previous_event).c_str()));
    } else {
      if (point.state == WebTouchPoint::StatePressed &&
          previous_point->state != WebTouchPoint::StateCancelled &&
          previous_point->state != WebTouchPoint::StateReleased) {
        error_msg->append(StringPrintf(
            "Pressed touch point (id=%d) already exists in previous event "
            "(ids=%s).\n",
            point.id, TouchPointIdsToString(previous_event).c_str()));
      }
    }

    switch (point.state) {
      case WebTouchPoint::StateUndefined:
        error_msg->append(
            StringPrintf("Undefined touch point state (id=%d).\n", point.id));
        break;

      case WebTouchPoint::StateReleased:
        if (event.type != WebInputEvent::TouchEnd) {
          error_msg->append(StringPrintf(
              "Released touch point (id=%d) outside touchend.\n", point.id));
        } else {
          found_valid_state_for_type = true;
        }
        break;

      case WebTouchPoint::StatePressed:
        if (event.type != WebInputEvent::TouchStart) {
          error_msg->append(StringPrintf(
              "Pressed touch point (id=%d) outside touchstart.\n", point.id));
        } else {
          found_valid_state_for_type = true;
        }
        break;

      case WebTouchPoint::StateMoved:
        if (event.type != WebInputEvent::TouchMove) {
          error_msg->append(StringPrintf(
              "Moved touch point (id=%d) outside touchmove.\n", point.id));
        } else {
          found_valid_state_for_type = true;
        }
        break;

      case WebTouchPoint::StateStationary:
        break;

      case WebTouchPoint::StateCancelled:
        if (event.type != WebInputEvent::TouchCancel) {
          error_msg->append(StringPrintf(
              "Cancelled touch point (id=%d) outside touchcancel.\n",
              point.id));
        } else {
          found_valid_state_for_type = true;
        }
        break;
    }
  }

  if (!found_valid_state_for_type) {
    error_msg->append(
        StringPrintf("No valid touch point corresponding to event type: %s\n",
                     WebInputEventTraits::GetName(event.type)));
  }

  return error_msg->empty();
}

}  // namespace content
