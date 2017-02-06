// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_display.h"

#include <xdg-shell-unstable-v5-client-protocol.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

static_assert(XDG_SHELL_VERSION_CURRENT == 5, "Unsupported xdg-shell version");

namespace ui {
namespace {
const uint32_t kMaxCompositorVersion = 4;
const uint32_t kMaxSeatVersion = 4;
const uint32_t kMaxShmVersion = 1;
const uint32_t kMaxXdgShellVersion = 1;
}  // namespace

WaylandDisplay::WaylandDisplay() {}

WaylandDisplay::~WaylandDisplay() {}

bool WaylandDisplay::Initialize() {
  static const wl_registry_listener registry_listener = {
      &WaylandDisplay::Global, &WaylandDisplay::GlobalRemove,
  };

  display_.reset(wl_display_connect(nullptr));
  if (!display_) {
    LOG(ERROR) << "Failed to connect to Wayland display";
    return false;
  }

  registry_.reset(wl_display_get_registry(display_.get()));
  if (!registry_) {
    LOG(ERROR) << "Failed to get Wayland registry";
    return false;
  }

  wl_registry_add_listener(registry_.get(), &registry_listener, this);
  wl_display_roundtrip(display_.get());

  if (!compositor_) {
    LOG(ERROR) << "No wl_compositor object";
    return false;
  }
  if (!shm_) {
    LOG(ERROR) << "No wl_shm object";
    return false;
  }
  if (!seat_) {
    LOG(ERROR) << "No wl_seat object";
    return false;
  }
  if (!shell_) {
    LOG(ERROR) << "No xdg_shell object";
    return false;
  }

  return true;
}

bool WaylandDisplay::StartProcessingEvents() {
  if (watching_)
    return true;

  DCHECK(display_);
  wl_display_flush(display_.get());

  DCHECK(base::MessageLoopForUI::IsCurrent());
  if (!base::MessageLoopForUI::current()->WatchFileDescriptor(
          wl_display_get_fd(display_.get()), true,
          base::MessagePumpLibevent::WATCH_READ, &controller_, this))
    return false;

  watching_ = true;
  return true;
}

void WaylandDisplay::ScheduleFlush() {
  if (scheduled_flush_ || !watching_)
    return;
  base::MessageLoopForUI::current()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&WaylandDisplay::Flush, base::Unretained(this)));
  scheduled_flush_ = true;
}

WaylandWindow* WaylandDisplay::GetWindow(gfx::AcceleratedWidget widget) {
  auto it = window_map_.find(widget);
  return it == window_map_.end() ? nullptr : it->second;
}

void WaylandDisplay::AddWindow(gfx::AcceleratedWidget widget,
                               WaylandWindow* window) {
  window_map_[widget] = window;
}

void WaylandDisplay::RemoveWindow(gfx::AcceleratedWidget widget) {
  window_map_.erase(widget);
}

void WaylandDisplay::OnDispatcherListChanged() {
  StartProcessingEvents();
}

void WaylandDisplay::Flush() {
  wl_display_flush(display_.get());
  scheduled_flush_ = false;
}

void WaylandDisplay::DispatchUiEvent(Event* event) {
  PlatformEventSource::DispatchEvent(event);
}

void WaylandDisplay::OnFileCanReadWithoutBlocking(int fd) {
  wl_display_dispatch(display_.get());
  for (const auto& window : window_map_)
    window.second->ApplyPendingBounds();
}

void WaylandDisplay::OnFileCanWriteWithoutBlocking(int fd) {}

// static
void WaylandDisplay::Global(void* data,
                            wl_registry* registry,
                            uint32_t name,
                            const char* interface,
                            uint32_t version) {
  static const wl_seat_listener seat_listener = {
      &WaylandDisplay::Capabilities, &WaylandDisplay::Name,
  };
  static const xdg_shell_listener shell_listener = {
      &WaylandDisplay::Ping,
  };

  WaylandDisplay* display = static_cast<WaylandDisplay*>(data);
  if (!display->compositor_ && strcmp(interface, "wl_compositor") == 0) {
    display->compositor_ = wl::Bind<wl_compositor>(
        registry, name, std::min(version, kMaxCompositorVersion));
    if (!display->compositor_)
      LOG(ERROR) << "Failed to bind to wl_compositor global";
  } else if (!display->shm_ && strcmp(interface, "wl_shm") == 0) {
    display->shm_ =
        wl::Bind<wl_shm>(registry, name, std::min(version, kMaxShmVersion));
    if (!display->shm_)
      LOG(ERROR) << "Failed to bind to wl_shm global";
  } else if (!display->seat_ && strcmp(interface, "wl_seat") == 0) {
    display->seat_ =
        wl::Bind<wl_seat>(registry, name, std::min(version, kMaxSeatVersion));
    if (!display->seat_) {
      LOG(ERROR) << "Failed to bind to wl_seat global";
      return;
    }
    wl_seat_add_listener(display->seat_.get(), &seat_listener, display);
  } else if (!display->shell_ && strcmp(interface, "xdg_shell") == 0) {
    display->shell_ = wl::Bind<xdg_shell>(
        registry, name, std::min(version, kMaxXdgShellVersion));
    if (!display->shell_) {
      LOG(ERROR) << "Failed to  bind to xdg_shell global";
      return;
    }
    xdg_shell_add_listener(display->shell_.get(), &shell_listener, display);
    xdg_shell_use_unstable_version(display->shell_.get(),
                                   XDG_SHELL_VERSION_CURRENT);
  }

  display->ScheduleFlush();
}

// static
void WaylandDisplay::GlobalRemove(void* data,
                                  wl_registry* registry,
                                  uint32_t name) {
  NOTIMPLEMENTED();
}

// static
void WaylandDisplay::Capabilities(void* data,
                                  wl_seat* seat,
                                  uint32_t capabilities) {
  WaylandDisplay* display = static_cast<WaylandDisplay*>(data);
  if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
    if (!display->pointer_) {
      wl_pointer* pointer = wl_seat_get_pointer(display->seat_.get());
      if (!pointer) {
        LOG(ERROR) << "Failed to get wl_pointer from seat";
        return;
      }
      display->pointer_ = base::WrapUnique(new WaylandPointer(
          pointer, base::Bind(&WaylandDisplay::DispatchUiEvent,
                              base::Unretained(display))));
    }
  } else if (display->pointer_) {
    display->pointer_.reset();
  }
  display->ScheduleFlush();
}

// static
void WaylandDisplay::Name(void* data, wl_seat* seat, const char* name) {}

// static
void WaylandDisplay::Ping(void* data, xdg_shell* shell, uint32_t serial) {
  WaylandDisplay* display = static_cast<WaylandDisplay*>(data);
  xdg_shell_pong(shell, serial);
  display->ScheduleFlush();
}

}  // namespace ui
