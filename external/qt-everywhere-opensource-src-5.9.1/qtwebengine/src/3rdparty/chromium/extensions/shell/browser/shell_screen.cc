// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_screen.h"

#include <stdint.h>
#include <vector>

#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace extensions {
namespace {

const int64_t kDisplayId = 0;

}  // namespace

ShellScreen::ShellScreen(const gfx::Size& size) : host_(nullptr) {
  DCHECK(!size.IsEmpty());
  // Screen is positioned at (0,0).
  display::Display display(kDisplayId);
  gfx::Rect bounds(size);
  display.SetScaleAndBounds(1.0f, bounds);
  ProcessDisplayChanged(display, true /* is_primary */);
}

ShellScreen::~ShellScreen() {
  DCHECK(!host_) << "Window not closed before destroying ShellScreen";
}

aura::WindowTreeHost* ShellScreen::CreateHostForPrimaryDisplay() {
  DCHECK(!host_);
  host_ = aura::WindowTreeHost::Create(
      gfx::Rect(GetPrimaryDisplay().GetSizeInPixel()));
  host_->window()->AddObserver(this);
  host_->InitHost();
  return host_;
}

// aura::WindowObserver overrides:

void ShellScreen::OnWindowBoundsChanged(aura::Window* window,
                                        const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds) {
  DCHECK_EQ(host_->window(), window);
  display::Display display(GetPrimaryDisplay());
  display.SetSize(new_bounds.size());
  display_list().UpdateDisplay(display);
}

void ShellScreen::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(host_->window(), window);
  host_->window()->RemoveObserver(this);
  host_ = nullptr;
}

// display::Screen overrides:

gfx::Point ShellScreen::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool ShellScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow ShellScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return host_->window()->GetTopWindowContainingPoint(point);
}

display::Display ShellScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return GetPrimaryDisplay();
}

}  // namespace extensions
