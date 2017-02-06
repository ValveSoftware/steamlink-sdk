// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_POINTER_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_POINTER_H_

#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/wayland_object.h"

namespace ui {

class WaylandPointer {
 public:
  WaylandPointer(wl_pointer* pointer, const EventDispatchCallback& callback);
  virtual ~WaylandPointer();

 private:
  // wl_pointer_listener
  static void Enter(void* data,
                    wl_pointer* obj,
                    uint32_t serial,
                    wl_surface* surface,
                    wl_fixed_t surface_x,
                    wl_fixed_t surface_y);
  static void Leave(void* data,
                    wl_pointer* obj,
                    uint32_t serial,
                    wl_surface* surface);
  static void Motion(void* data,
                     wl_pointer* obj,
                     uint32_t time,
                     wl_fixed_t surface_x,
                     wl_fixed_t surface_y);
  static void Button(void* data,
                     wl_pointer* obj,
                     uint32_t serial,
                     uint32_t time,
                     uint32_t button,
                     uint32_t state);
  static void Axis(void* data,
                   wl_pointer* obj,
                   uint32_t time,
                   uint32_t axis,
                   wl_fixed_t value);

  wl::Object<wl_pointer> obj_;
  EventDispatchCallback callback_;
  gfx::PointF location_;
  int flags_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_POINTER_H_
