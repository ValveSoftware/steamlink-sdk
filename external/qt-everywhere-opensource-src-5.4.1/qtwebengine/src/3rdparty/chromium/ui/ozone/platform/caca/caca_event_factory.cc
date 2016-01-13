// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/caca_event_factory.h"

#include <caca.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/ozone/platform/caca/caca_connection.h"

namespace ui {

namespace {

ui::KeyboardCode GetKeyboardCode(const caca_event_t& event) {
  // List of special mappings the Caca provides.
  static const ui::KeyboardCode kCacaKeyMap[] = {
    ui::VKEY_UNKNOWN,
    ui::VKEY_A,
    ui::VKEY_B,
    ui::VKEY_C,
    ui::VKEY_D,
    ui::VKEY_E,
    ui::VKEY_F,
    ui::VKEY_G,
    ui::VKEY_BACK,
    ui::VKEY_TAB,
    ui::VKEY_J,
    ui::VKEY_K,
    ui::VKEY_L,
    ui::VKEY_RETURN,
    ui::VKEY_N,
    ui::VKEY_O,
    ui::VKEY_P,
    ui::VKEY_Q,
    ui::VKEY_R,
    ui::VKEY_PAUSE,
    ui::VKEY_T,
    ui::VKEY_U,
    ui::VKEY_V,
    ui::VKEY_W,
    ui::VKEY_X,
    ui::VKEY_Y,
    ui::VKEY_Z,
    ui::VKEY_ESCAPE,
    ui::VKEY_DELETE,
    ui::VKEY_UP,
    ui::VKEY_DOWN,
    ui::VKEY_LEFT,
    ui::VKEY_RIGHT,
    ui::VKEY_INSERT,
    ui::VKEY_HOME,
    ui::VKEY_END,
    ui::VKEY_PRIOR,
    ui::VKEY_NEXT,
    ui::VKEY_F1,
    ui::VKEY_F2,
    ui::VKEY_F3,
    ui::VKEY_F4,
    ui::VKEY_F5,
    ui::VKEY_F6,
    ui::VKEY_F7,
    ui::VKEY_F8,
    ui::VKEY_F9,
    ui::VKEY_F10,
    ui::VKEY_F11,
    ui::VKEY_F12,
  };

  int key_code = caca_get_event_key_ch(&event);
  if (key_code >= 'a' && key_code <= 'z')
    return static_cast<ui::KeyboardCode>(key_code - ('a' - 'A'));
  if (key_code >= '0' && key_code <= 'Z')
    return static_cast<ui::KeyboardCode>(key_code);
  if (static_cast<unsigned int>(key_code) < arraysize(kCacaKeyMap))
    return kCacaKeyMap[key_code];

  return ui::VKEY_UNKNOWN;
}

int ModifierFromKey(const caca_event_t& event) {
  int key_code = caca_get_event_key_ch(&event);
  if (key_code >= 'A' && key_code <= 'Z')
    return ui::EF_SHIFT_DOWN;
  if ((key_code >= CACA_KEY_CTRL_A && key_code <= CACA_KEY_CTRL_G) ||
      (key_code >= CACA_KEY_CTRL_J && key_code <= CACA_KEY_CTRL_L) ||
      (key_code >= CACA_KEY_CTRL_N && key_code <= CACA_KEY_CTRL_R) ||
      (key_code >= CACA_KEY_CTRL_T && key_code <= CACA_KEY_CTRL_Z))
    return ui::EF_CONTROL_DOWN;

  return ui::EF_NONE;
}

int ModifierFromButton(const caca_event_t& event) {
  switch (caca_get_event_mouse_button(&event)) {
    case 1:
      return ui::EF_LEFT_MOUSE_BUTTON;
    case 2:
      return ui::EF_RIGHT_MOUSE_BUTTON;
    case 3:
      return ui::EF_MIDDLE_MOUSE_BUTTON;
  }
  return 0;
}

// Translate coordinates to bitmap coordinates.
gfx::PointF TranslateLocation(const gfx::PointF& location,
                              CacaConnection* connection) {
  gfx::Size physical_size = connection->physical_size();
  gfx::Size bitmap_size = connection->bitmap_size();
  return gfx::PointF(
      location.x() * bitmap_size.width() / physical_size.width(),
      location.y() * bitmap_size.height() / physical_size.height());
}

ui::EventType GetEventTypeFromNative(const caca_event_t& event) {
  switch (caca_get_event_type(&event)) {
    case CACA_EVENT_KEY_PRESS:
      return ui::ET_KEY_PRESSED;
    case CACA_EVENT_KEY_RELEASE:
      return ui::ET_KEY_RELEASED;
    case CACA_EVENT_MOUSE_PRESS:
      return ui::ET_MOUSE_PRESSED;
    case CACA_EVENT_MOUSE_RELEASE:
      return ui::ET_MOUSE_RELEASED;
    case CACA_EVENT_MOUSE_MOTION:
      return ui::ET_MOUSE_MOVED;
    default:
      return ui::ET_UNKNOWN;
  }
}

}  // namespace

CacaEventFactory::CacaEventFactory(CacaConnection* connection)
  : connection_(connection),
    weak_ptr_factory_(this),
    delay_(base::TimeDelta::FromMilliseconds(10)),
    modifier_flags_(0) {
}

CacaEventFactory::~CacaEventFactory() {
}

void CacaEventFactory::WarpCursorTo(gfx::AcceleratedWidget widget,
                                    const gfx::PointF& location) {
  NOTIMPLEMENTED();
}

void CacaEventFactory::OnDispatcherListChanged() {
  ScheduleEventProcessing();
}

void CacaEventFactory::ScheduleEventProcessing() {
  // Caca uses a poll based event retrieval. Since we don't want to block we'd
  // either need to spin up a new thread or just poll. For simplicity just poll
  // for a message every |delay_| time delta.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CacaEventFactory::TryProcessingEvent,
                 weak_ptr_factory_.GetWeakPtr()),
      delay_);
}

void CacaEventFactory::TryProcessingEvent() {
  caca_event_t event;
  int event_mask = CACA_EVENT_KEY_PRESS | CACA_EVENT_KEY_RELEASE |
                   CACA_EVENT_MOUSE_PRESS | CACA_EVENT_MOUSE_RELEASE |
                   CACA_EVENT_MOUSE_MOTION;
  if (connection_->display() &&
      caca_get_event(connection_->display(), event_mask, &event, 0)) {

    ui::EventType type = GetEventTypeFromNative(event);
    bool pressed = type == ui::ET_KEY_PRESSED || type == ui::ET_MOUSE_PRESSED;

    switch (type) {
      case ui::ET_KEY_PRESSED:
      case ui::ET_KEY_RELEASED: {
        if (pressed)
          modifier_flags_ |= ModifierFromKey(event);
        else
          modifier_flags_ &= ~ModifierFromKey(event);

        ui::KeyEvent key_event(
            type, GetKeyboardCode(event), modifier_flags_, true);
        DispatchEvent(&key_event);
        break;
      }
      case ui::ET_MOUSE_MOVED:
        last_cursor_location_.SetPoint(caca_get_event_mouse_x(&event),
                                       caca_get_event_mouse_y(&event));
        // Update cursor location.
        caca_gotoxy(caca_get_canvas(connection_->display()),
                    last_cursor_location_.x(),
                    last_cursor_location_.y());
      // fallthrough
      case ui::ET_MOUSE_PRESSED:
      case ui::ET_MOUSE_RELEASED: {
        int flags = 0;
        int changed_flags = 0;
        if (type != ui::ET_MOUSE_MOVED) {
          if (pressed) {
            changed_flags = ModifierFromButton(event);
            modifier_flags_ |= changed_flags;
          } else {
            modifier_flags_ &= ~changed_flags;
          }
          // On release the button pressed is removed from |modifier_flags_|,
          // but sending the event needs it set.
          flags = modifier_flags_ | changed_flags;
        }
        gfx::PointF location = TranslateLocation(last_cursor_location_,
                                                 connection_);
        ui::MouseEvent mouse_event(
            type, location, location, flags, changed_flags);
        DispatchEvent(&mouse_event);
        break;
      }
      default:
        NOTIMPLEMENTED();
        break;
    }
  }

  ScheduleEventProcessing();
}

}  // namespace ui
