// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/screen_mus.h"

#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/aura/window.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/display/mojo/display_type_converters.h"
#include "ui/views/mus/screen_mus_delegate.h"
#include "ui/views/mus/window_manager_frame_values.h"

#ifdef NOTIMPLEMENTED
#undef NOTIMPLEMENTED
#define NOTIMPLEMENTED() DVLOG(1) << "notimplemented"
#endif

namespace mojo {

template <>
struct TypeConverter<views::WindowManagerFrameValues,
                     mus::mojom::FrameDecorationValuesPtr> {
  static views::WindowManagerFrameValues Convert(
      const mus::mojom::FrameDecorationValuesPtr& input) {
    views::WindowManagerFrameValues result;
    result.normal_insets = input->normal_client_area_insets;
    result.maximized_insets = input->maximized_client_area_insets;
    result.max_title_bar_button_width = input->max_title_bar_button_width;
    return result;
  }
};

}  // namespace mojo

namespace views {

ScreenMus::ScreenMus(ScreenMusDelegate* delegate)
    : delegate_(delegate),
      display_manager_observer_binding_(this) {
}

ScreenMus::~ScreenMus() {
  DCHECK_EQ(this, display::Screen::GetScreen());
  display::Screen::SetScreenInstance(nullptr);
}

void ScreenMus::Init(shell::Connector* connector) {
  display::Screen::SetScreenInstance(this);

  connector->ConnectToInterface("mojo:mus", &display_manager_);

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
  if (display_list_.displays().empty()) {
    DCHECK(display_manager_.encountered_error() || !success);
    // In this case we install a default display and assume the process is
    // going to exit shortly so that the real value doesn't matter.
    display_list_.AddDisplay(
        display::Display(0xFFFFFFFF, gfx::Rect(0, 0, 801, 802)),
        DisplayList::Type::PRIMARY);
  }
}

void ScreenMus::ProcessDisplayChanged(const display::Display& changed_display,
                                      bool is_primary) {
  if (display_list_.FindDisplayById(changed_display.id()) ==
      display_list_.displays().end()) {
    display_list_.AddDisplay(changed_display,
                             is_primary ? DisplayList::Type::PRIMARY
                                        : DisplayList::Type::NOT_PRIMARY);
    return;
  }
  display_list_.UpdateDisplay(
      changed_display,
      is_primary ? DisplayList::Type::PRIMARY : DisplayList::Type::NOT_PRIMARY);
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
  if (!window)
    return false;

  return window->IsVisible() &&
      window->GetBoundsInScreen().Contains(GetCursorScreenPoint());
}

gfx::NativeWindow ScreenMus::GetWindowAtScreenPoint(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return nullptr;
}

display::Display ScreenMus::GetPrimaryDisplay() const {
  return *display_list_.GetPrimaryDisplayIterator();
}

display::Display ScreenMus::GetDisplayNearestWindow(
    gfx::NativeView view) const {
  NOTIMPLEMENTED();
  return *display_list_.GetPrimaryDisplayIterator();
}

display::Display ScreenMus::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return *display::FindDisplayNearestPoint(display_list_.displays(), point);
}

int ScreenMus::GetNumDisplays() const {
  return static_cast<int>(display_list_.displays().size());
}

std::vector<display::Display> ScreenMus::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display ScreenMus::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* match = display::FindDisplayWithBiggestIntersection(
      display_list_.displays(), match_rect);
  return match ? *match : GetPrimaryDisplay();
}

void ScreenMus::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void ScreenMus::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

void ScreenMus::OnDisplays(
    mojo::Array<mus::mojom::DisplayPtr> transport_displays) {
  // This should only be called once from Init() before any observers have been
  // added.
  DCHECK(display_list_.displays().empty());
  std::vector<display::Display> displays =
      transport_displays.To<std::vector<display::Display>>();
  for (size_t i = 0; i < displays.size(); ++i) {
    const bool is_primary = transport_displays[i]->is_primary;
    display_list_.AddDisplay(displays[i], is_primary
                                              ? DisplayList::Type::PRIMARY
                                              : DisplayList::Type::NOT_PRIMARY);
    if (is_primary) {
      // TODO(sky): Make WindowManagerFrameValues per display.
      WindowManagerFrameValues frame_values =
          transport_displays[i]
              ->frame_decoration_values.To<WindowManagerFrameValues>();
      WindowManagerFrameValues::SetInstance(frame_values);
    }
  }
  DCHECK(!display_list_.displays().empty());
}

void ScreenMus::OnDisplaysChanged(
    mojo::Array<mus::mojom::DisplayPtr> transport_displays) {
  for (size_t i = 0; i < transport_displays.size(); ++i) {
    const bool is_primary = transport_displays[i]->is_primary;
    ProcessDisplayChanged(transport_displays[i].To<display::Display>(),
                          is_primary);
    if (is_primary) {
      WindowManagerFrameValues frame_values =
          transport_displays[i]
              ->frame_decoration_values.To<WindowManagerFrameValues>();
      WindowManagerFrameValues::SetInstance(frame_values);
      if (delegate_)
        delegate_->OnWindowManagerFrameValuesChanged();
    }
  }
}

void ScreenMus::OnDisplayRemoved(int64_t id) {
  display_list_.RemoveDisplay(id);
}

}  // namespace views
