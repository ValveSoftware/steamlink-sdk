// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_OBJECT_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_OBJECT_H_

#include <wayland-client-core.h>

#include <memory>

struct wl_buffer;
struct wl_compositor;
struct wl_pointer;
struct wl_registry;
struct wl_seat;
struct wl_shm;
struct wl_shm_pool;
struct wl_surface;
struct xdg_shell;
struct xdg_surface;

namespace wl {

template <typename T>
struct ObjectTraits;

template <>
struct ObjectTraits<wl_buffer> {
  static const wl_interface* interface;
  static void (*deleter)(wl_buffer*);
};

template <>
struct ObjectTraits<wl_compositor> {
  static const wl_interface* interface;
  static void (*deleter)(wl_compositor*);
};

template <>
struct ObjectTraits<wl_display> {
  static const wl_interface* interface;
  static void (*deleter)(wl_display*);
};

template <>
struct ObjectTraits<wl_pointer> {
  static const wl_interface* interface;
  static void (*deleter)(wl_pointer*);
};

template <>
struct ObjectTraits<wl_registry> {
  static const wl_interface* interface;
  static void (*deleter)(wl_registry*);
};

template <>
struct ObjectTraits<wl_seat> {
  static const wl_interface* interface;
  static void (*deleter)(wl_seat*);
};

template <>
struct ObjectTraits<wl_shm> {
  static const wl_interface* interface;
  static void (*deleter)(wl_shm*);
};

template <>
struct ObjectTraits<wl_shm_pool> {
  static const wl_interface* interface;
  static void (*deleter)(wl_shm_pool*);
};

template <>
struct ObjectTraits<wl_surface> {
  static const wl_interface* interface;
  static void (*deleter)(wl_surface*);
};

template <>
struct ObjectTraits<xdg_shell> {
  static const wl_interface* interface;
  static void (*deleter)(xdg_shell*);
};

template <>
struct ObjectTraits<xdg_surface> {
  static const wl_interface* interface;
  static void (*deleter)(xdg_surface*);
};

struct Deleter {
  template <typename T>
  void operator()(T* obj) {
    ObjectTraits<T>::deleter(obj);
  }
};

template <typename T>
class Object : public std::unique_ptr<T, Deleter> {
 public:
  Object() {}
  explicit Object(T* obj) : std::unique_ptr<T, Deleter>(obj) {}

  uint32_t id() {
    return wl_proxy_get_id(
        reinterpret_cast<wl_proxy*>(std::unique_ptr<T, Deleter>::get()));
  }
};

template <typename T>
wl::Object<T> Bind(wl_registry* registry, uint32_t name, uint32_t version) {
  return wl::Object<T>(static_cast<T*>(
      wl_registry_bind(registry, name, ObjectTraits<T>::interface, version)));
}

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_OBJECT_H_
