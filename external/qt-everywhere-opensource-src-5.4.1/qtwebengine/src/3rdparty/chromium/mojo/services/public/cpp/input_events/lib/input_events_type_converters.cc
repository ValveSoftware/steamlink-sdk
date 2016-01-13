// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/input_events/input_events_type_converters.h"

#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace mojo {

// static
EventPtr TypeConverter<EventPtr, ui::Event>::ConvertFrom(
    const ui::Event& input) {
  EventPtr event(Event::New());
  event->action = input.type();
  event->flags = input.flags();
  event->time_stamp = input.time_stamp().ToInternalValue();

  if (input.IsMouseEvent() || input.IsTouchEvent()) {
    const ui::LocatedEvent* located_event =
        static_cast<const ui::LocatedEvent*>(&input);
    event->location =
        TypeConverter<PointPtr, gfx::Point>::ConvertFrom(
            located_event->location());
  }

  if (input.IsTouchEvent()) {
    const ui::TouchEvent* touch_event =
        static_cast<const ui::TouchEvent*>(&input);
    TouchDataPtr touch_data(TouchData::New());
    touch_data->pointer_id = touch_event->touch_id();
    event->touch_data = touch_data.Pass();
  } else if (input.IsKeyEvent()) {
    const ui::KeyEvent* key_event = static_cast<const ui::KeyEvent*>(&input);
    KeyDataPtr key_data(KeyData::New());
    key_data->key_code = key_event->key_code();
    key_data->is_char = key_event->is_char();
    event->key_data = key_data.Pass();
  }
  return event.Pass();
}

// static
scoped_ptr<ui::Event>
TypeConverter<EventPtr, scoped_ptr<ui::Event> >::ConvertTo(
    const EventPtr& input) {
  scoped_ptr<ui::Event> ui_event;
  switch (input->action) {
    case ui::ET_KEY_PRESSED:
    case ui::ET_KEY_RELEASED:
      ui_event.reset(new ui::KeyEvent(
                         static_cast<ui::EventType>(input->action),
                         static_cast<ui::KeyboardCode>(
                             input->key_data->key_code),
                         input->flags,
                         input->key_data->is_char));
      break;
    default:
      // TODO: support other types.
      // NOTIMPLEMENTED();
      ;
  }
  // TODO: need to support time_stamp.
  return ui_event.Pass();
}

}  // namespace mojo
