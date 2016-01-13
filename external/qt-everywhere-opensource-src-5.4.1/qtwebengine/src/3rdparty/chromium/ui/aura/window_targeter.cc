// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_targeter.h"

#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_target.h"

namespace aura {

WindowTargeter::WindowTargeter() {}
WindowTargeter::~WindowTargeter() {}

ui::EventTarget* WindowTargeter::FindTargetForEvent(ui::EventTarget* root,
                                                    ui::Event* event) {
  Window* window = static_cast<Window*>(root);
  Window* target = event->IsKeyEvent() ?
      FindTargetForKeyEvent(window, *static_cast<ui::KeyEvent*>(event)) :
      static_cast<Window*>(EventTargeter::FindTargetForEvent(root, event));
  if (target && !window->parent()) {
    // |window| is the root window.
    if (!window->Contains(target)) {
      // |target| is not a descendent of |window|. So do not allow dispatching
      // from here. Instead, dispatch the event through the
      // WindowEventDispatcher that owns |target|.
      ui::EventDispatchDetails details ALLOW_UNUSED =
          target->GetHost()->event_processor()->OnEventFromSource(event);
      target = NULL;
    }
  }
  return target;
}

bool WindowTargeter::SubtreeCanAcceptEvent(
    ui::EventTarget* target,
    const ui::LocatedEvent& event) const {
  aura::Window* window = static_cast<aura::Window*>(target);
  if (!window->IsVisible())
    return false;
  if (window->ignore_events())
    return false;
  client::EventClient* client = client::GetEventClient(window->GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(window))
    return false;

  Window* parent = window->parent();
  if (parent && parent->delegate_ && !parent->delegate_->
      ShouldDescendIntoChildForEventHandling(window, event.location())) {
    return false;
  }
  return true;
}

bool WindowTargeter::EventLocationInsideBounds(
    ui::EventTarget* target,
    const ui::LocatedEvent& event) const {
  aura::Window* window = static_cast<aura::Window*>(target);
  gfx::Point point = event.location();
  if (window->parent())
    aura::Window::ConvertPointToTarget(window->parent(), window, &point);
  return gfx::Rect(window->bounds().size()).Contains(point);
}

ui::EventTarget* WindowTargeter::FindTargetForLocatedEvent(
    ui::EventTarget* root,
    ui::LocatedEvent* event) {
  Window* window = static_cast<Window*>(root);
  if (!window->parent()) {
    Window* target = FindTargetInRootWindow(window, *event);
    if (target) {
      window->ConvertEventToTarget(target, event);
      return target;
    }
  }
  return EventTargeter::FindTargetForLocatedEvent(root, event);
}

Window* WindowTargeter::FindTargetForKeyEvent(Window* window,
                                              const ui::KeyEvent& key) {
  Window* root_window = window->GetRootWindow();
  if (key.key_code() == ui::VKEY_UNKNOWN &&
      (key.flags() & ui::EF_IME_FABRICATED_KEY) == 0 &&
      key.GetCharacter() == 0)
    return NULL;
  client::FocusClient* focus_client = client::GetFocusClient(root_window);
  Window* focused_window = focus_client->GetFocusedWindow();
  if (!focused_window)
    return window;

  client::EventClient* event_client = client::GetEventClient(root_window);
  if (event_client &&
      !event_client->CanProcessEventsWithinSubtree(focused_window)) {
    focus_client->FocusWindow(NULL);
    return NULL;
  }
  return focused_window ? focused_window : window;
}

Window* WindowTargeter::FindTargetInRootWindow(Window* root_window,
                                               const ui::LocatedEvent& event) {
  DCHECK_EQ(root_window, root_window->GetRootWindow());

  // Mouse events should be dispatched to the window that processed the
  // mouse-press events (if any).
  if (event.IsScrollEvent() || event.IsMouseEvent()) {
    WindowEventDispatcher* dispatcher = root_window->GetHost()->dispatcher();
    if (dispatcher->mouse_pressed_handler())
      return dispatcher->mouse_pressed_handler();
  }

  // All events should be directed towards the capture window (if any).
  Window* capture_window = client::GetCaptureWindow(root_window);
  if (capture_window)
    return capture_window;

  if (event.IsTouchEvent()) {
    // Query the gesture-recognizer to find targets for touch events.
    const ui::TouchEvent& touch = static_cast<const ui::TouchEvent&>(event);
    ui::GestureConsumer* consumer =
        ui::GestureRecognizer::Get()->GetTouchLockedTarget(touch);
    if (consumer)
      return static_cast<Window*>(consumer);
    consumer =
        ui::GestureRecognizer::Get()->GetTargetForLocation(
            event.location(), touch.source_device_id());
    if (consumer)
      return static_cast<Window*>(consumer);

    // If the initial touch is outside the root window, target the root.
    if (!root_window->bounds().Contains(event.location()))
      return root_window;
  }

  return NULL;
}

}  // namespace aura
