// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_pointer.h"

#include <linux/input.h>
#include <wayland-client.h>

#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

// TODO(forney): Handle version 5 of wl_pointer.

namespace ui {

WaylandPointer::WaylandPointer(wl_pointer* pointer,
                               const EventDispatchCallback& callback)
    : obj_(pointer), callback_(callback) {
  static const wl_pointer_listener listener = {
      &WaylandPointer::Enter,  &WaylandPointer::Leave, &WaylandPointer::Motion,
      &WaylandPointer::Button, &WaylandPointer::Axis,
  };

  wl_pointer_add_listener(obj_.get(), &listener, this);
}

WaylandPointer::~WaylandPointer() {}

// static
void WaylandPointer::Enter(void* data,
                           wl_pointer* obj,
                           uint32_t serial,
                           wl_surface* surface,
                           wl_fixed_t surface_x,
                           wl_fixed_t surface_y) {
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  pointer->location_.SetPoint(wl_fixed_to_double(surface_x),
                              wl_fixed_to_double(surface_y));
  if (surface)
    WaylandWindow::FromSurface(surface)->set_pointer_focus(true);
}

// static
void WaylandPointer::Leave(void* data,
                           wl_pointer* obj,
                           uint32_t serial,
                           wl_surface* surface) {
  if (surface)
    WaylandWindow::FromSurface(surface)->set_pointer_focus(false);
}

// static
void WaylandPointer::Motion(void* data,
                            wl_pointer* obj,
                            uint32_t time,
                            wl_fixed_t surface_x,
                            wl_fixed_t surface_y) {
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  pointer->location_.SetPoint(wl_fixed_to_double(surface_x),
                              wl_fixed_to_double(surface_y));
  MouseEvent event(ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                   base::TimeTicks() + base::TimeDelta::FromMilliseconds(time),
                   pointer->flags_, 0);
  event.set_location_f(pointer->location_);
  event.set_root_location_f(pointer->location_);
  pointer->callback_.Run(&event);
}

// static
void WaylandPointer::Button(void* data,
                            wl_pointer* obj,
                            uint32_t serial,
                            uint32_t time,
                            uint32_t button,
                            uint32_t state) {
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  int flag;
  switch (button) {
    case BTN_LEFT:
      flag = EF_LEFT_MOUSE_BUTTON;
      break;
    case BTN_MIDDLE:
      flag = EF_MIDDLE_MOUSE_BUTTON;
      break;
    case BTN_RIGHT:
      flag = EF_RIGHT_MOUSE_BUTTON;
      break;
    case BTN_BACK:
      flag = EF_BACK_MOUSE_BUTTON;
      break;
    case BTN_FORWARD:
      flag = EF_FORWARD_MOUSE_BUTTON;
      break;
    default:
      return;
  }
  int flags = pointer->flags_ | flag;
  EventType type;
  if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
    type = ET_MOUSE_PRESSED;
    pointer->flags_ |= flag;
  } else {
    type = ET_MOUSE_RELEASED;
    pointer->flags_ &= ~flag;
  }
  MouseEvent event(type, gfx::Point(), gfx::Point(),
                   base::TimeTicks() + base::TimeDelta::FromMilliseconds(time),
                   flags, flag);
  event.set_location_f(pointer->location_);
  event.set_root_location_f(pointer->location_);
  pointer->callback_.Run(&event);
}

// static
void WaylandPointer::Axis(void* data,
                          wl_pointer* obj,
                          uint32_t time,
                          uint32_t axis,
                          wl_fixed_t value) {
  static const double kAxisValueScale = 10.0;
  WaylandPointer* pointer = static_cast<WaylandPointer*>(data);
  gfx::Vector2d offset;
  // Wayland compositors send axis events with values in the surface coordinate
  // space. They send a value of 10 per mouse wheel click by convention, so
  // clients (e.g. GTK+) typically scale down by this amount to convert to
  // discrete step coordinates. wl_pointer version 5 improves the situation by
  // adding axis sources and discrete axis events.
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
    offset.set_y(-wl_fixed_to_double(value) / kAxisValueScale *
                 MouseWheelEvent::kWheelDelta);
  else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
    offset.set_x(wl_fixed_to_double(value) / kAxisValueScale *
                 MouseWheelEvent::kWheelDelta);
  else
    return;
  MouseWheelEvent event(
      offset, gfx::Point(), gfx::Point(),
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(time),
      pointer->flags_, 0);
  event.set_location_f(pointer->location_);
  event.set_root_location_f(pointer->location_);
  pointer->callback_.Run(&event);
}

}  // namespace ui
