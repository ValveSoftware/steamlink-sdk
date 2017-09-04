// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_mac.h"

namespace aura {

WindowTreeHostMac::WindowTreeHostMac(const gfx::Rect& bounds) {
  window_.reset(
      [[NSWindow alloc]
          initWithContentRect:NSRectFromCGRect(bounds.ToCGRect())
                    styleMask:NSBorderlessWindowMask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
  CreateCompositor();
  OnAcceleratedWidgetAvailable();
}

WindowTreeHostMac::~WindowTreeHostMac() {
  DestroyDispatcher();
}

EventSource* WindowTreeHostMac::GetEventSource() {
  NOTIMPLEMENTED();
  return nil;
}

gfx::AcceleratedWidget WindowTreeHostMac::GetAcceleratedWidget() {
  return [window_ contentView];
}
void WindowTreeHostMac::ShowImpl() {
  [window_ makeKeyAndOrderFront:nil];
}

void WindowTreeHostMac::HideImpl() {
  [window_ orderOut:nil];
}

void WindowTreeHostMac::ToggleFullScreen() {
}

gfx::Rect WindowTreeHostMac::GetBounds() const {
  return gfx::Rect(NSRectToCGRect([window_ frame]));
}

void WindowTreeHostMac::SetBounds(const gfx::Rect& bounds) {
  [window_ setFrame:NSRectFromCGRect(bounds.ToCGRect()) display:YES animate:NO];
}

gfx::Insets WindowTreeHostMac::GetInsets() const {
  NOTIMPLEMENTED();
  return gfx::Insets();
}

void WindowTreeHostMac::SetInsets(const gfx::Insets& insets) {
  NOTIMPLEMENTED();
}

gfx::Point WindowTreeHostMac::GetLocationOnNativeScreen() const {
  NOTIMPLEMENTED();
  return gfx::Point(0, 0);
}

void WindowTreeHostMac::SetCapture() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMac::ReleaseCapture() {
  NOTIMPLEMENTED();
}

bool WindowTreeHostMac::ConfineCursorToRootWindow() {
  return false;
}

void WindowTreeHostMac::UnConfineCursor() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMac::SetCursorNative(gfx::NativeCursor cursor_type) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMac::MoveCursorToNative(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMac::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMac::OnDeviceScaleFactorChanged(float device_scale_factor) {
  NOTIMPLEMENTED();
}

// static
WindowTreeHost* WindowTreeHost::Create(const gfx::Rect& bounds) {
  return new WindowTreeHostMac(bounds);
}

}  // namespace aura
