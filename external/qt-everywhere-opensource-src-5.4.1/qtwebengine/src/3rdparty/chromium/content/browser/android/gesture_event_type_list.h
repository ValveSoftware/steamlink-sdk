// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards because this file
// is meant to be included inside a macro to generate enum values.

// This file contains a list of GestureEventType's usable by ContentViewCore,
// providing a direct mapping to and from their corresponding
// blink::WebGestureEvent types.

#ifndef DEFINE_GESTURE_EVENT_TYPE
#error "Please define DEFINE_GESTURE_EVENT_TYPE before including this file."
#endif

DEFINE_GESTURE_EVENT_TYPE(SHOW_PRESS, 0)
DEFINE_GESTURE_EVENT_TYPE(DOUBLE_TAP, 1)
DEFINE_GESTURE_EVENT_TYPE(SINGLE_TAP_UP, 2)
DEFINE_GESTURE_EVENT_TYPE(SINGLE_TAP_CONFIRMED, 3)
DEFINE_GESTURE_EVENT_TYPE(SINGLE_TAP_UNCONFIRMED, 4)
DEFINE_GESTURE_EVENT_TYPE(LONG_PRESS, 5)
DEFINE_GESTURE_EVENT_TYPE(SCROLL_START, 6)
DEFINE_GESTURE_EVENT_TYPE(SCROLL_BY, 7)
DEFINE_GESTURE_EVENT_TYPE(SCROLL_END, 8)
DEFINE_GESTURE_EVENT_TYPE(FLING_START, 9)
DEFINE_GESTURE_EVENT_TYPE(FLING_CANCEL, 10)
DEFINE_GESTURE_EVENT_TYPE(FLING_END, 11)
DEFINE_GESTURE_EVENT_TYPE(PINCH_BEGIN, 12)
DEFINE_GESTURE_EVENT_TYPE(PINCH_BY, 13)
DEFINE_GESTURE_EVENT_TYPE(PINCH_END, 14)
DEFINE_GESTURE_EVENT_TYPE(TAP_CANCEL, 15)
DEFINE_GESTURE_EVENT_TYPE(LONG_TAP, 16)
DEFINE_GESTURE_EVENT_TYPE(TAP_DOWN, 17)
