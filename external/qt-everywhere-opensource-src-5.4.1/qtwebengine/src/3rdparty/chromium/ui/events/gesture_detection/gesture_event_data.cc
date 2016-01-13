// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_event_data.h"

#include "base/logging.h"

namespace ui {

GestureEventData::GestureEventData(const GestureEventDetails& details,
                                   int motion_event_id,
                                   base::TimeTicks time,
                                   float x,
                                   float y,
                                   float raw_x,
                                   float raw_y,
                                   size_t touch_point_count,
                                   const gfx::RectF& bounding_box)
    : details(details),
      motion_event_id(motion_event_id),
      time(time),
      x(x),
      y(y),
      raw_x(raw_x),
      raw_y(raw_y) {
  DCHECK_GE(motion_event_id, 0);
  DCHECK_NE(0U, touch_point_count);
  this->details.set_touch_points(static_cast<int>(touch_point_count));
  this->details.set_bounding_box(bounding_box);
}

GestureEventData::GestureEventData(EventType type,
                                   const GestureEventData& other)
    : details(type, 0, 0),
      motion_event_id(other.motion_event_id),
      time(other.time),
      x(other.x),
      y(other.y),
      raw_x(other.raw_x),
      raw_y(other.raw_y) {
  details.set_touch_points(other.details.touch_points());
  details.set_bounding_box(other.details.bounding_box_f());
}

GestureEventData::GestureEventData()
    : motion_event_id(0), x(0), y(0), raw_x(0), raw_y(0) {
}

}  //  namespace ui
