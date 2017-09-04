// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_host_mus.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/platform_window/stub/stub_window.h"

namespace aura {

namespace {
static uint32_t accelerated_widget_count = 1;

bool IsUsingTestContext() {
  return aura::Env::GetInstance()->context_factory()->DoesCreateTestContexts();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(
    std::unique_ptr<WindowPortMus> window_port,
    WindowTreeHostMusDelegate* delegate,
    int64_t display_id,
    const std::map<std::string, std::vector<uint8_t>>* properties)
    : WindowTreeHostPlatform(std::move(window_port)),
      display_id_(display_id),
      delegate_(delegate) {
  // TODO(sky): find a cleaner way to set this! Better solution is to likely
  // have constructor take aura::Window.
  WindowPortMus::Get(window())->window_ = window();
  if (properties) {
    // Apply the properties before initializing the window, that way the
    // server seems them at the time the window is created.
    WindowMus* window_mus = WindowMus::Get(window());
    for (auto& pair : *properties)
      window_mus->SetPropertyFromServer(pair.first, &pair.second);
  }
  CreateCompositor();
  gfx::AcceleratedWidget accelerated_widget;
  if (IsUsingTestContext()) {
    accelerated_widget = gfx::kNullAcceleratedWidget;
  } else {
// We need accelerated widget numbers to be different for each
// window and fit in the smallest sizeof(AcceleratedWidget) uint32_t
// has this property.
#if defined(OS_WIN) || defined(OS_ANDROID)
    accelerated_widget =
        reinterpret_cast<gfx::AcceleratedWidget>(accelerated_widget_count++);
#else
    accelerated_widget =
        static_cast<gfx::AcceleratedWidget>(accelerated_widget_count++);
#endif
  }
  // TODO(markdittmer): Use correct device-scale-factor from |window|.
  OnAcceleratedWidgetAvailable(accelerated_widget, 1.f);

  delegate_->OnWindowTreeHostCreated(this);

  SetPlatformWindow(base::MakeUnique<ui::StubWindow>(
      this,
      false));  // Do not advertise accelerated widget; already set manually.

  input_method_ = base::MakeUnique<InputMethodMus>(this, window());

  compositor()->SetHostHasTransparentBackground(true);

  // Mus windows are assumed hidden.
  compositor()->SetVisible(false);
}

WindowTreeHostMus::WindowTreeHostMus(
    WindowTreeClient* window_tree_client,
    const std::map<std::string, std::vector<uint8_t>>* properties)
    : WindowTreeHostMus(
          static_cast<WindowTreeHostMusDelegate*>(window_tree_client)
              ->CreateWindowPortForTopLevel(),
          window_tree_client,
          display::Screen::GetScreen()->GetPrimaryDisplay().id(),
          properties) {}

WindowTreeHostMus::~WindowTreeHostMus() {
  DestroyCompositor();
  DestroyDispatcher();
}

void WindowTreeHostMus::SetBoundsFromServer(const gfx::Rect& bounds) {
  base::AutoReset<bool> resetter(&in_set_bounds_from_server_, true);
  SetBounds(bounds);
}

display::Display WindowTreeHostMus::GetDisplay() const {
  for (const display::Display& display :
       display::Screen::GetScreen()->GetAllDisplays()) {
    if (display.id() == display_id_)
      return display;
  }
  return display::Display();
}

void WindowTreeHostMus::ShowImpl() {
  WindowTreeHostPlatform::ShowImpl();
  window()->Show();
}

void WindowTreeHostMus::HideImpl() {
  WindowTreeHostPlatform::HideImpl();
  window()->Hide();
}

void WindowTreeHostMus::SetBounds(const gfx::Rect& bounds) {
  if (!in_set_bounds_from_server_)
    delegate_->OnWindowTreeHostBoundsWillChange(this, bounds);
  WindowTreeHostPlatform::SetBounds(bounds);
}

void WindowTreeHostMus::DispatchEvent(ui::Event* event) {
  DCHECK(!event->IsKeyEvent());
  WindowTreeHostPlatform::DispatchEvent(event);
}

void WindowTreeHostMus::OnClosed() {
}

void WindowTreeHostMus::OnActivationChanged(bool active) {
  if (active)
    GetInputMethod()->OnFocus();
  else
    GetInputMethod()->OnBlur();
  WindowTreeHostPlatform::OnActivationChanged(active);
}

void WindowTreeHostMus::OnCloseRequest() {
  OnHostCloseRequested();
}

gfx::ICCProfile WindowTreeHostMus::GetICCProfileForCurrentDisplay() {
  // TODO: This should read the profile from mus. crbug.com/647510
  return gfx::ICCProfile();
}

}  // namespace aura
