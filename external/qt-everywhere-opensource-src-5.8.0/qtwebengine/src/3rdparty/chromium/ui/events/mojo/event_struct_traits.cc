// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/mojo/event_struct_traits.h"

#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/mojo/event_constants.mojom.h"

namespace mojo {
namespace {

ui::mojom::EventType UIEventTypeToMojo(ui::EventType type) {
  switch (type) {
    case ui::ET_POINTER_DOWN:
      return ui::mojom::EventType::POINTER_DOWN;

    case ui::ET_POINTER_MOVED:
      return ui::mojom::EventType::POINTER_MOVE;

    case ui::ET_POINTER_EXITED:
      return ui::mojom::EventType::MOUSE_EXIT;

    case ui::ET_MOUSEWHEEL:
      return ui::mojom::EventType::WHEEL;

    case ui::ET_POINTER_UP:
      return ui::mojom::EventType::POINTER_UP;

    case ui::ET_POINTER_CANCELLED:
      return ui::mojom::EventType::POINTER_CANCEL;

    case ui::ET_KEY_PRESSED:
      return ui::mojom::EventType::KEY_PRESSED;

    case ui::ET_KEY_RELEASED:
      return ui::mojom::EventType::KEY_RELEASED;

    default:
      break;
  }
  return ui::mojom::EventType::UNKNOWN;
}

ui::EventType MojoPointerEventTypeToUIEvent(ui::mojom::EventType action) {
  switch (action) {
    case ui::mojom::EventType::POINTER_DOWN:
      return ui::ET_POINTER_DOWN;

    case ui::mojom::EventType::POINTER_UP:
      return ui::ET_POINTER_UP;

    case ui::mojom::EventType::POINTER_MOVE:
      return ui::ET_POINTER_MOVED;

    case ui::mojom::EventType::POINTER_CANCEL:
      return ui::ET_POINTER_CANCELLED;

    case ui::mojom::EventType::MOUSE_EXIT:
      return ui::ET_POINTER_EXITED;

    default:
      NOTREACHED();
  }

  return ui::ET_UNKNOWN;
}

}  // namespace

static_assert(ui::mojom::kEventFlagNone == static_cast<int32_t>(ui::EF_NONE),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagIsSynthesized ==
                  static_cast<int32_t>(ui::EF_IS_SYNTHESIZED),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagShiftDown ==
                  static_cast<int32_t>(ui::EF_SHIFT_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagControlDown ==
                  static_cast<int32_t>(ui::EF_CONTROL_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagAltDown ==
                  static_cast<int32_t>(ui::EF_ALT_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagCommandDown ==
                  static_cast<int32_t>(ui::EF_COMMAND_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagAltgrDown ==
                  static_cast<int32_t>(ui::EF_ALTGR_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagMod3Down ==
                  static_cast<int32_t>(ui::EF_MOD3_DOWN),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagNumLockOn ==
                  static_cast<int32_t>(ui::EF_NUM_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagCapsLockOn ==
                  static_cast<int32_t>(ui::EF_CAPS_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagScrollLockOn ==
                  static_cast<int32_t>(ui::EF_SCROLL_LOCK_ON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagLeftMouseButton ==
                  static_cast<int32_t>(ui::EF_LEFT_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagMiddleMouseButton ==
                  static_cast<int32_t>(ui::EF_MIDDLE_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagRightMouseButton ==
                  static_cast<int32_t>(ui::EF_RIGHT_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagBackMouseButton ==
                  static_cast<int32_t>(ui::EF_BACK_MOUSE_BUTTON),
              "EVENT_FLAGS must match");
static_assert(ui::mojom::kEventFlagForwardMouseButton ==
                  static_cast<int32_t>(ui::EF_FORWARD_MOUSE_BUTTON),
              "EVENT_FLAGS must match");

ui::mojom::EventType StructTraits<ui::mojom::Event, EventUniquePtr>::action(
    const EventUniquePtr& event) {
  return UIEventTypeToMojo(event->type());
}

int32_t StructTraits<ui::mojom::Event, EventUniquePtr>::flags(
    const EventUniquePtr& event) {
  return event->flags();
}

int64_t StructTraits<ui::mojom::Event, EventUniquePtr>::time_stamp(
    const EventUniquePtr& event) {
  return event->time_stamp().ToInternalValue();
}

ui::mojom::KeyDataPtr StructTraits<ui::mojom::Event, EventUniquePtr>::key_data(
    const EventUniquePtr& event) {
  if (!event->IsKeyEvent())
    return nullptr;

  const ui::KeyEvent* key_event = event->AsKeyEvent();
  ui::mojom::KeyDataPtr key_data(ui::mojom::KeyData::New());
  key_data->key_code = key_event->GetConflatedWindowsKeyCode();
  key_data->native_key_code =
      ui::KeycodeConverter::DomCodeToNativeKeycode(key_event->code());
  key_data->is_char = key_event->is_char();
  key_data->character = key_event->GetCharacter();
  key_data->windows_key_code = static_cast<ui::mojom::KeyboardCode>(
      key_event->GetLocatedWindowsKeyboardCode());
  key_data->text = key_event->GetText();
  key_data->unmodified_text = key_event->GetUnmodifiedText();

  return key_data;
}

ui::mojom::PointerDataPtr
StructTraits<ui::mojom::Event, EventUniquePtr>::pointer_data(
    const EventUniquePtr& event) {
  if (!event->IsPointerEvent() && !event->IsMouseWheelEvent())
    return nullptr;

  ui::mojom::PointerDataPtr pointer_data(ui::mojom::PointerData::New());

  const ui::PointerDetails* pointer_details = nullptr;
  if (event->IsPointerEvent()) {
    const ui::PointerEvent* pointer_event = event->AsPointerEvent();
    pointer_data->pointer_id = pointer_event->pointer_id();
    pointer_details = &pointer_event->pointer_details();
  } else {
    const ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();
    pointer_data->pointer_id = ui::PointerEvent::kMousePointerId;
    pointer_details = &wheel_event->pointer_details();
  }

  switch (pointer_details->pointer_type) {
    case ui::EventPointerType::POINTER_TYPE_MOUSE:
      pointer_data->kind = ui::mojom::PointerKind::MOUSE;
      break;
    case ui::EventPointerType::POINTER_TYPE_TOUCH:
      pointer_data->kind = ui::mojom::PointerKind::TOUCH;
      break;
    default:
      NOTREACHED();
  }

  ui::mojom::BrushDataPtr brush_data(ui::mojom::BrushData::New());
  // TODO(rjk): this is in the wrong coordinate system
  brush_data->width = pointer_details->radius_x;
  brush_data->height = pointer_details->radius_y;
  // TODO(rjk): update for touch_event->rotation_angle();
  brush_data->pressure = pointer_details->force;
  brush_data->tilt_x = pointer_details->tilt_x;
  brush_data->tilt_y = pointer_details->tilt_y;
  pointer_data->brush_data = std::move(brush_data);

  // TODO(rjkroege): Plumb raw pointer events on windows.
  // TODO(rjkroege): Handle force-touch on MacOS
  // TODO(rjkroege): Adjust brush data appropriately for Android.

  ui::mojom::LocationDataPtr location_data(ui::mojom::LocationData::New());
  const ui::LocatedEvent* located_event = event->AsLocatedEvent();
  location_data->x = located_event->location_f().x();
  location_data->y = located_event->location_f().y();
  location_data->screen_x = located_event->root_location_f().x();
  location_data->screen_y = located_event->root_location_f().y();
  pointer_data->location = std::move(location_data);

  if (event->IsMouseWheelEvent()) {
    const ui::MouseWheelEvent* wheel_event = event->AsMouseWheelEvent();

    ui::mojom::WheelDataPtr wheel_data(ui::mojom::WheelData::New());

    // TODO(rjkroege): Support page scrolling on windows by directly
    // cracking into a mojo event when the native event is available.
    wheel_data->mode = ui::mojom::WheelMode::LINE;
    // TODO(rjkroege): Support precise scrolling deltas.

    if ((event->flags() & ui::EF_SHIFT_DOWN) != 0 &&
        wheel_event->x_offset() == 0) {
      wheel_data->delta_x = wheel_event->y_offset();
      wheel_data->delta_y = 0;
      wheel_data->delta_z = 0;
    } else {
      // TODO(rjkroege): support z in ui::Events.
      wheel_data->delta_x = wheel_event->x_offset();
      wheel_data->delta_y = wheel_event->y_offset();
      wheel_data->delta_z = 0;
    }
    pointer_data->wheel_data = std::move(wheel_data);
  }

  return pointer_data;
}

bool StructTraits<ui::mojom::Event, EventUniquePtr>::Read(
    ui::mojom::EventDataView event,
    EventUniquePtr* out) {
  switch (event.action()) {
    case ui::mojom::EventType::KEY_PRESSED:
    case ui::mojom::EventType::KEY_RELEASED: {
      ui::mojom::KeyDataPtr key_data;
      if (!event.ReadKeyData<ui::mojom::KeyDataPtr>(&key_data))
        return false;

      if (key_data->is_char) {
        out->reset(new ui::KeyEvent(
            static_cast<base::char16>(key_data->character),
            static_cast<ui::KeyboardCode>(key_data->key_code), event.flags()));
        return true;
      }
      out->reset(new ui::KeyEvent(
          event.action() == ui::mojom::EventType::KEY_PRESSED
              ? ui::ET_KEY_PRESSED
              : ui::ET_KEY_RELEASED,

          static_cast<ui::KeyboardCode>(key_data->key_code), event.flags()));
      return true;
    }
    case ui::mojom::EventType::POINTER_DOWN:
    case ui::mojom::EventType::POINTER_UP:
    case ui::mojom::EventType::POINTER_MOVE:
    case ui::mojom::EventType::POINTER_CANCEL:
    case ui::mojom::EventType::MOUSE_EXIT:
    case ui::mojom::EventType::WHEEL: {
      ui::mojom::PointerDataPtr pointer_data;
      if (!event.ReadPointerData<ui::mojom::PointerDataPtr>(&pointer_data))
        return false;

      const gfx::Point location(pointer_data->location->x,
                                pointer_data->location->y);
      const gfx::Point screen_location(pointer_data->location->screen_x,
                                       pointer_data->location->screen_y);

      switch (pointer_data->kind) {
        case ui::mojom::PointerKind::MOUSE: {
          if (event.action() == ui::mojom::EventType::WHEEL) {
            out->reset(new ui::MouseWheelEvent(
                gfx::Vector2d(
                    static_cast<int>(pointer_data->wheel_data->delta_x),
                    static_cast<int>(pointer_data->wheel_data->delta_y)),
                location, screen_location, ui::EventTimeForNow(),
                ui::EventFlags(event.flags()), ui::EventFlags(event.flags())));
            return true;
          }
          out->reset(new ui::PointerEvent(
              MojoPointerEventTypeToUIEvent(event.action()), location,
              screen_location, event.flags(), ui::PointerEvent::kMousePointerId,
              ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE),
              ui::EventTimeForNow()));
          return true;
        }
        case ui::mojom::PointerKind::TOUCH: {
          out->reset(new ui::PointerEvent(
              MojoPointerEventTypeToUIEvent(event.action()), location,
              screen_location, event.flags(), pointer_data->pointer_id,
              ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                                 pointer_data->brush_data->width,
                                 pointer_data->brush_data->height,
                                 pointer_data->brush_data->pressure,
                                 pointer_data->brush_data->tilt_x,
                                 pointer_data->brush_data->tilt_y),
              ui::EventTimeForNow()));
          return true;
        }
        case ui::mojom::PointerKind::PEN:
          NOTIMPLEMENTED();
          return false;
      }
    }
    case ui::mojom::EventType::UNKNOWN:
      return false;
  }

  return false;
}

}  // namespace mojo
