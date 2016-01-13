// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_event_details.h"

namespace ui {

GestureEventDetails::GestureEventDetails() : type_(ET_UNKNOWN) {}

GestureEventDetails::GestureEventDetails(ui::EventType type,
                                         float delta_x,
                                         float delta_y)
    : type_(type),
      touch_points_(1) {
  DCHECK_GE(type, ET_GESTURE_TYPE_START);
  DCHECK_LE(type, ET_GESTURE_TYPE_END);
  switch (type_) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      data.scroll_begin.x_hint = delta_x;
      data.scroll_begin.y_hint = delta_y;
      break;

    case ui::ET_GESTURE_SCROLL_UPDATE:
      data.scroll_update.x = delta_x;
      data.scroll_update.y = delta_y;
      break;

    case ui::ET_SCROLL_FLING_START:
      data.fling_velocity.x = delta_x;
      data.fling_velocity.y = delta_y;
      break;

    case ui::ET_GESTURE_TWO_FINGER_TAP:
      data.first_finger_enclosing_rectangle.width = delta_x;
      data.first_finger_enclosing_rectangle.height = delta_y;
      break;

    case ui::ET_GESTURE_PINCH_UPDATE:
      data.scale = delta_x;
      CHECK_EQ(0.f, delta_y) << "Unknown data in delta_y for pinch";
      break;

    case ui::ET_GESTURE_SWIPE:
      data.swipe.left = delta_x < 0;
      data.swipe.right = delta_x > 0;
      data.swipe.up = delta_y < 0;
      data.swipe.down = delta_y > 0;
      break;

    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_DOUBLE_TAP:
    case ui::ET_GESTURE_TAP_UNCONFIRMED:
      data.tap_count = static_cast<int>(delta_x);
      CHECK_EQ(0.f, delta_y) << "Unknown data in delta_y for tap.";
      break;

    default:
      if (delta_x != 0.f || delta_y != 0.f) {
        DLOG(WARNING) << "A gesture event (" << type << ") had unknown data: ("
                      << delta_x << "," << delta_y;
      }
      break;
  }
}

GestureEventDetails::Details::Details() {
  memset(this, 0, sizeof(Details));
}

}  // namespace ui
