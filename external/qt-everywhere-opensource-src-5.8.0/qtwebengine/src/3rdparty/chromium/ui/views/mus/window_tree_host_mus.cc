// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_tree_host_mus.h"

#include "base/memory/ptr_util.h"
#include "components/mus/public/cpp/window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/platform_window/stub/stub_window.h"
#include "ui/views/mus/input_method_mus.h"
#include "ui/views/mus/native_widget_mus.h"

namespace views {

namespace {
static uint32_t accelerated_widget_count = 1;
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(NativeWidgetMus* native_widget,
                                     mus::Window* window)
    : native_widget_(native_widget) {
// We need accelerated widget numbers to be different for each
// window and fit in the smallest sizeof(AcceleratedWidget) uint32_t
// has this property.
#if defined(OS_WIN) || defined(OS_ANDROID)
  gfx::AcceleratedWidget accelerated_widget =
      reinterpret_cast<gfx::AcceleratedWidget>(accelerated_widget_count++);
#else
  gfx::AcceleratedWidget accelerated_widget =
      static_cast<gfx::AcceleratedWidget>(accelerated_widget_count++);
#endif
  // TODO(markdittmer): Use correct device-scale-factor from |window|.
  OnAcceleratedWidgetAvailable(accelerated_widget, 1.f);

  SetPlatformWindow(base::WrapUnique(new ui::StubWindow(
      this,
      false)));  // Do not advertise accelerated widget; already set manually.

  // Initialize the stub platform window bounds to those of the mus::Window.
  platform_window()->SetBounds(window->bounds());

  // The location of events is already transformed, and there is no way to
  // correctly determine the reverse transform. So, don't attempt to transform
  // event locations, else the root location is wrong.
  // TODO(sky): we need to transform for device scale though.
  dispatcher()->set_transform_events(false);
  compositor()->SetHostHasTransparentBackground(true);

  input_method_.reset(new InputMethodMUS(this, window));
  SetSharedInputMethod(input_method_.get());
}

WindowTreeHostMus::~WindowTreeHostMus() {
  DestroyCompositor();
  DestroyDispatcher();
}

void WindowTreeHostMus::DispatchEvent(ui::Event* event) {
  if (event->IsKeyEvent() && GetInputMethod()) {
    GetInputMethod()->DispatchKeyEvent(event->AsKeyEvent());
    event->StopPropagation();
    return;
  }
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

}  // namespace views
