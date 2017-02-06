// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_platform.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"

#if defined(OS_ANDROID)
#include "ui/platform_window/android/platform_window_android.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_WIN)
#include "base/message_loop/message_loop.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/platform_window/win/win_window.h"
#endif

namespace aura {

#if defined(OS_WIN) || defined(OS_ANDROID) || defined(USE_OZONE)
// static
WindowTreeHost* WindowTreeHost::Create(const gfx::Rect& bounds) {
  return new WindowTreeHostPlatform(bounds);
}
#endif

WindowTreeHostPlatform::WindowTreeHostPlatform(const gfx::Rect& bounds)
    : WindowTreeHostPlatform() {
#if defined(USE_OZONE)
  window_ =
      ui::OzonePlatform::GetInstance()->CreatePlatformWindow(this, bounds);
#elif defined(OS_WIN)
  window_.reset(new ui::WinWindow(this, bounds));
#elif defined(OS_ANDROID)
  window_.reset(new ui::PlatformWindowAndroid(this));
#else
  NOTIMPLEMENTED();
#endif
}

WindowTreeHostPlatform::WindowTreeHostPlatform()
    : widget_(gfx::kNullAcceleratedWidget),
      current_cursor_(ui::kCursorNull) {
  CreateCompositor();
}

void WindowTreeHostPlatform::SetPlatformWindow(
    std::unique_ptr<ui::PlatformWindow> window) {
  window_ = std::move(window);
}

WindowTreeHostPlatform::~WindowTreeHostPlatform() {
  DestroyCompositor();
  DestroyDispatcher();
}

ui::EventSource* WindowTreeHostPlatform::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget WindowTreeHostPlatform::GetAcceleratedWidget() {
  return widget_;
}

void WindowTreeHostPlatform::ShowImpl() {
  window_->Show();
}

void WindowTreeHostPlatform::HideImpl() {
  window_->Hide();
}

gfx::Rect WindowTreeHostPlatform::GetBounds() const {
  return window_ ? window_->GetBounds() : gfx::Rect();
}

void WindowTreeHostPlatform::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
}

gfx::Point WindowTreeHostPlatform::GetLocationOnNativeScreen() const {
  return window_->GetBounds().origin();
}

void WindowTreeHostPlatform::SetCapture() {
  window_->SetCapture();
}

void WindowTreeHostPlatform::ReleaseCapture() {
  window_->ReleaseCapture();
}

void WindowTreeHostPlatform::SetCursorNative(gfx::NativeCursor cursor) {
  if (cursor == current_cursor_)
    return;
  current_cursor_ = cursor;

#if defined(OS_WIN)
  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor);
#endif

  window_->SetCursor(cursor.platform());
}

void WindowTreeHostPlatform::MoveCursorToNative(const gfx::Point& location) {
  window_->MoveCursorTo(location);
}

void WindowTreeHostPlatform::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

void WindowTreeHostPlatform::OnBoundsChanged(const gfx::Rect& new_bounds) {
  float current_scale = compositor()->device_scale_factor();
  float new_scale = display::Screen::GetScreen()
                        ->GetDisplayNearestWindow(window())
                        .device_scale_factor();
  gfx::Rect old_bounds = bounds_;
  bounds_ = new_bounds;
  if (bounds_.origin() != old_bounds.origin()) {
    OnHostMoved(bounds_.origin());
  }
  if (bounds_.size() != old_bounds.size() || current_scale != new_scale) {
    OnHostResized(bounds_.size());
  }
}

void WindowTreeHostPlatform::OnDamageRect(const gfx::Rect& damage_rect) {
  compositor()->ScheduleRedrawRect(damage_rect);
}

void WindowTreeHostPlatform::DispatchEvent(ui::Event* event) {
  TRACE_EVENT0("input", "WindowTreeHostPlatform::DispatchEvent");
  ui::EventDispatchDetails details = SendEventToProcessor(event);
  if (details.dispatcher_destroyed)
    event->SetHandled();
}

void WindowTreeHostPlatform::OnCloseRequest() {
#if defined(OS_WIN)
  // TODO: this obviously shouldn't be here.
  base::MessageLoopForUI::current()->QuitWhenIdle();
#else
  OnHostCloseRequested();
#endif
}

void WindowTreeHostPlatform::OnClosed() {}

void WindowTreeHostPlatform::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {}

void WindowTreeHostPlatform::OnLostCapture() {
  OnHostLostWindowCapture();
}

void WindowTreeHostPlatform::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_pixel_ratio) {
  widget_ = widget;
  WindowTreeHost::OnAcceleratedWidgetAvailable();
}

void WindowTreeHostPlatform::OnAcceleratedWidgetDestroyed() {
  gfx::AcceleratedWidget widget = compositor()->ReleaseAcceleratedWidget();
  DCHECK_EQ(widget, widget_);
  widget_ = gfx::kNullAcceleratedWidget;
}

void WindowTreeHostPlatform::OnActivationChanged(bool active) {
  if (active)
    OnHostActivated();
}

}  // namespace aura
