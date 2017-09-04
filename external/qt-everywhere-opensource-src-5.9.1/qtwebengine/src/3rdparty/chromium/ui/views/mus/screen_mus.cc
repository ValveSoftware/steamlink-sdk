// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This has to be before any other includes, else default is picked up.
// See base/logging for details on this.
#define NOTIMPLEMENTED_POLICY 5

#include "ui/views/mus/screen_mus.h"

#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/aura/window.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/screen_mus_delegate.h"
#include "ui/views/mus/window_manager_frame_values.h"

namespace mojo {

template <>
struct TypeConverter<views::WindowManagerFrameValues,
                     ui::mojom::FrameDecorationValuesPtr> {
  static views::WindowManagerFrameValues Convert(
      const ui::mojom::FrameDecorationValuesPtr& input) {
    views::WindowManagerFrameValues result;
    result.normal_insets = input->normal_client_area_insets;
    result.maximized_insets = input->maximized_client_area_insets;
    result.max_title_bar_button_width = input->max_title_bar_button_width;
    return result;
  }
};

}  // namespace mojo

namespace views {

using Type = display::DisplayList::Type;

ScreenMus::ScreenMus(ScreenMusDelegate* delegate)
    : delegate_(delegate), display_manager_observer_binding_(this) {
  display::Screen::SetScreenInstance(this);
}

ScreenMus::~ScreenMus() {
  DCHECK_EQ(this, display::Screen::GetScreen());
  display::Screen::SetScreenInstance(nullptr);
}

void ScreenMus::Init(service_manager::Connector* connector) {
  connector->ConnectToInterface(ui::mojom::kServiceName, &display_manager_);

  display_manager_->AddObserver(
      display_manager_observer_binding_.CreateInterfacePtrAndBind());

  // We need the set of displays before we can continue. Wait for it.
  //
  // TODO(rockot): Do something better here. This should not have to block tasks
  // from running on the calling thread. http://crbug.com/594852.
  bool success = display_manager_observer_binding_.WaitForIncomingMethodCall();

  // The WaitForIncomingMethodCall() should have supplied the set of Displays,
  // unless mus is going down, in which case encountered_error() is true, or the
  // call to WaitForIncomingMethodCall() failed.
  if (display_list().displays().empty()) {
    DCHECK(display_manager_.encountered_error() || !success);
    // In this case we install a default display and assume the process is
    // going to exit shortly so that the real value doesn't matter.
    display_list().AddDisplay(
        display::Display(0xFFFFFFFF, gfx::Rect(0, 0, 801, 802)), Type::PRIMARY);
  }
}

gfx::Point ScreenMus::GetCursorScreenPoint() {
  if (!delegate_) {
    // TODO(erg): If we need the cursor point in the window manager, we'll need
    // to make |delegate_| required. It only recently changed to be optional.
    NOTIMPLEMENTED();
    return gfx::Point();
  }

  return delegate_->GetCursorScreenPoint();
}

bool ScreenMus::IsWindowUnderCursor(gfx::NativeWindow window) {
  return window && window->IsVisible() &&
         window->GetBoundsInScreen().Contains(GetCursorScreenPoint());
}

aura::Window* ScreenMus::GetWindowAtScreenPoint(const gfx::Point& point) {
  return delegate_->GetWindowAtScreenPoint(point);
}

void ScreenMus::OnDisplays(std::vector<ui::mojom::WsDisplayPtr> ws_displays,
                           int64_t primary_display_id,
                           int64_t internal_display_id) {
  // This should only be called once when ScreenMus is added as an observer.
  DCHECK(display_list().displays().empty());

  for (size_t i = 0; i < ws_displays.size(); ++i) {
    const display::Display& display = ws_displays[i]->display;
    const bool is_primary = display.id() == primary_display_id;
    display_list().AddDisplay(display,
                              is_primary ? Type::PRIMARY : Type::NOT_PRIMARY);
    if (is_primary) {
      // TODO(sky): Make WindowManagerFrameValues per display.
      WindowManagerFrameValues frame_values =
          ws_displays[i]
              ->frame_decoration_values.To<WindowManagerFrameValues>();
      WindowManagerFrameValues::SetInstance(frame_values);
    }
  }

  DCHECK(display_list().GetPrimaryDisplayIterator() !=
         display_list().displays().end());

  if (internal_display_id != display::Display::kInvalidDisplayID)
    display::Display::SetInternalDisplayId(internal_display_id);

  DCHECK(!display_list().displays().empty());
}

void ScreenMus::OnDisplaysChanged(
    std::vector<ui::mojom::WsDisplayPtr> ws_displays) {
  for (size_t i = 0; i < ws_displays.size(); ++i) {
    const display::Display& display = ws_displays[i]->display;
    const bool is_primary =
        display.id() == display_list().GetPrimaryDisplayIterator()->id();
    ProcessDisplayChanged(display, is_primary);
    if (is_primary) {
      WindowManagerFrameValues frame_values =
          ws_displays[i]
              ->frame_decoration_values.To<WindowManagerFrameValues>();
      WindowManagerFrameValues::SetInstance(frame_values);
      if (delegate_)
        delegate_->OnWindowManagerFrameValuesChanged();
    }
  }
}

void ScreenMus::OnDisplayRemoved(int64_t display_id) {
  display_list().RemoveDisplay(display_id);
}

void ScreenMus::OnPrimaryDisplayChanged(int64_t primary_display_id) {
  // TODO(kylechar): DisplayList would need to change to handle having no
  // primary display.
  if (primary_display_id == display::Display::kInvalidDisplayID)
    return;

  ProcessDisplayChanged(*display_list().FindDisplayById(primary_display_id),
                        true);
}

}  // namespace views
