// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_tree_host_mus.h"

#include "base/memory/ptr_util.h"
#include "services/ui/public/cpp/window.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/platform_window/stub/stub_window.h"
#include "ui/views/mus/input_method_mus.h"
#include "ui/views/mus/native_widget_mus.h"

namespace views {

namespace {
static uint32_t accelerated_widget_count = 1;

bool IsUsingTestContext() {
  return aura::Env::GetInstance()->context_factory()->DoesCreateTestContexts();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(NativeWidgetMus* native_widget,
                                     ui::Window* window)
    : native_widget_(native_widget) {
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

  SetPlatformWindow(base::MakeUnique<ui::StubWindow>(
      this,
      false));  // Do not advertise accelerated widget; already set manually.

  compositor()->SetWindow(window);

  // Initialize the stub platform window bounds to those of the ui::Window.
  platform_window()->SetBounds(window->bounds());

  compositor()->SetHostHasTransparentBackground(true);
}

WindowTreeHostMus::~WindowTreeHostMus() {
  DestroyCompositor();
  DestroyDispatcher();
}

void WindowTreeHostMus::DispatchEvent(ui::Event* event) {
  // Key events are sent to InputMethodMus directly from NativeWidgetMus.
  DCHECK(!event->IsKeyEvent());
  WindowTreeHostPlatform::DispatchEvent(event);
}

void WindowTreeHostMus::OnClosed() {
  if (native_widget_)
    native_widget_->OnPlatformWindowClosed();
}

void WindowTreeHostMus::OnActivationChanged(bool active) {
  if (active)
    GetInputMethod()->OnFocus();
  else
    GetInputMethod()->OnBlur();
  if (native_widget_)
    native_widget_->OnActivationChanged(active);
  WindowTreeHostPlatform::OnActivationChanged(active);
}

void WindowTreeHostMus::OnCloseRequest() {
  OnHostCloseRequested();
}

gfx::ICCProfile WindowTreeHostMus::GetICCProfileForCurrentDisplay() {
  // TODO: This should read the profile from mus. crbug.com/647510
  return gfx::ICCProfile();
}

}  // namespace views
