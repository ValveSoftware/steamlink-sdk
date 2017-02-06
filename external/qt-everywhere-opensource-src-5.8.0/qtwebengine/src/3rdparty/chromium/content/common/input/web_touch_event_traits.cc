// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/web_touch_event_traits.h"

#include <stddef.h>

#include "base/logging.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

bool WebTouchEventTraits::AllTouchPointsHaveState(
    const WebTouchEvent& event,
    blink::WebTouchPoint::State state) {
  if (!event.touchesLength)
    return false;
  for (size_t i = 0; i < event.touchesLength; ++i) {
    if (event.touches[i].state != state)
      return false;
  }
  return true;
}

bool WebTouchEventTraits::IsTouchSequenceStart(const WebTouchEvent& event) {
  DCHECK(event.touchesLength ||
         event.type == WebInputEvent::TouchScrollStarted);
  if (event.type != WebInputEvent::TouchStart)
    return false;
  return AllTouchPointsHaveState(event, blink::WebTouchPoint::StatePressed);
}

bool WebTouchEventTraits::IsTouchSequenceEnd(const WebTouchEvent& event) {
  if (event.type != WebInputEvent::TouchEnd &&
      event.type != WebInputEvent::TouchCancel)
    return false;
  if (!event.touchesLength)
    return true;
  for (size_t i = 0; i < event.touchesLength; ++i) {
    if (event.touches[i].state != blink::WebTouchPoint::StateReleased &&
        event.touches[i].state != blink::WebTouchPoint::StateCancelled)
      return false;
  }
  return true;
}

void WebTouchEventTraits::ResetType(WebInputEvent::Type type,
                                    double timestamp_sec,
                                    WebTouchEvent* event) {
  DCHECK(WebInputEvent::isTouchEventType(type));
  DCHECK(type != WebInputEvent::TouchScrollStarted);

  event->type = type;
  event->dispatchType = type == WebInputEvent::TouchCancel
                            ? WebInputEvent::EventNonBlocking
                            : WebInputEvent::Blocking;
  event->timeStampSeconds = timestamp_sec;
}

void WebTouchEventTraits::ResetTypeAndTouchStates(WebInputEvent::Type type,
                                                  double timestamp_sec,
                                                  WebTouchEvent* event) {
  ResetType(type, timestamp_sec, event);

  WebTouchPoint::State newState = WebTouchPoint::StateUndefined;
  switch (event->type) {
    case WebInputEvent::TouchStart:
      newState = WebTouchPoint::StatePressed;
      break;
    case WebInputEvent::TouchMove:
      newState = WebTouchPoint::StateMoved;
      break;
    case WebInputEvent::TouchEnd:
      newState = WebTouchPoint::StateReleased;
      break;
    case WebInputEvent::TouchCancel:
      newState = WebTouchPoint::StateCancelled;
      break;
    default:
      NOTREACHED();
      break;
  }
  for (size_t i = 0; i < event->touchesLength; ++i)
    event->touches[i].state = newState;
}

}  // namespace content
