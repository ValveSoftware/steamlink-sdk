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
#include "ui/events/event_target_iterator.h"

namespace aura {

WindowTargeter::WindowTargeter() {}
WindowTargeter::~WindowTargeter() {}

Window* WindowTargeter::FindTargetForLocatedEvent(Window* window,
                                                  ui::LocatedEvent* event) {
  if (!window->parent()) {
    Window* target = FindTargetInRootWindow(window, *event);
    if (target) {
      window->ConvertEventToTarget(target, event);
      return target;
    }
  }
  return FindTargetForLocatedEventRecursively(window, event);
}

bool WindowTargeter::SubtreeCanAcceptEvent(
    Window* window,
    const ui::LocatedEvent& event) const {
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
    Window* window,
    const ui::LocatedEvent& event) const {
  gfx::Point point = event.location();
  if (window->parent())
    Window::ConvertPointToTarget(window->parent(), window, &point);
  return gfx::Rect(window->bounds().size()).Contains(point);
}

ui::EventTarget* WindowTargeter::FindTargetForEvent(ui::EventTarget* root,
                                                    ui::Event* event) {
  Window* window = static_cast<Window*>(root);
  Window* target = event->IsKeyEvent()
                       ? FindTargetForKeyEvent(window, *event->AsKeyEvent())
                       : FindTargetForNonKeyEvent(window, event);
  if (target && !window->parent() && !window->Contains(target)) {
    // |window| is the root window, but |target| is not a descendent of
    // |window|. So do not allow dispatching from here. Instead, dispatch the
    // event through the WindowEventDispatcher that owns |target|.
    Window* new_root = target->GetRootWindow();
    DCHECK(new_root);
    if (event->IsLocatedEvent()) {
      // The event has been transformed to be in |target|'s coordinate system.
      // But dispatching the event through the EventProcessor requires the event
      // to be in the host's coordinate system. So, convert the event to be in
      // the root's coordinate space, and then to the host's coordinate space by
      // applying the host's transform.
      ui::LocatedEvent* located_event = static_cast<ui::LocatedEvent*>(event);
      located_event->ConvertLocationToTarget(target, new_root);
      located_event->UpdateForRootTransform(
          new_root->GetHost()->GetRootTransform());
    }
    ignore_result(
        new_root->GetHost()->event_processor()->OnEventFromSource(event));

    target = nullptr;
  }
  return target;
}

ui::EventTarget* WindowTargeter::FindNextBestTarget(
    ui::EventTarget* previous_target,
    ui::Event* event) {
  return nullptr;
}

bool WindowTargeter::SubtreeShouldBeExploredForEvent(
    Window* window,
    const ui::LocatedEvent& event) {
  return SubtreeCanAcceptEvent(window, event) &&
         EventLocationInsideBounds(window, event);
}

Window* WindowTargeter::FindTargetForKeyEvent(Window* window,
                                              const ui::KeyEvent& key) {
  Window* root_window = window->GetRootWindow();
  client::FocusClient* focus_client = client::GetFocusClient(root_window);
  Window* focused_window = focus_client->GetFocusedWindow();
  if (!focused_window)
    return window;

  client::EventClient* event_client = client::GetEventClient(root_window);
  if (event_client &&
      !event_client->CanProcessEventsWithinSubtree(focused_window)) {
    focus_client->FocusWindow(nullptr);
    return nullptr;
  }
  return focused_window ? focused_window : window;
}

Window* WindowTargeter::FindTargetForNonKeyEvent(Window* root_window,
                                                 ui::Event* event) {
  if (!event->IsLocatedEvent())
    return root_window;
  return FindTargetForLocatedEvent(root_window,
                                   static_cast<ui::LocatedEvent*>(event));
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
    const ui::TouchEvent& touch = *event.AsTouchEvent();
    ui::GestureConsumer* consumer =
        ui::GestureRecognizer::Get()->GetTouchLockedTarget(touch);
    if (consumer)
      return static_cast<Window*>(consumer);
    consumer = ui::GestureRecognizer::Get()->GetTargetForLocation(
        event.location_f(), touch.source_device_id());
    if (consumer)
      return static_cast<Window*>(consumer);

    // If the initial touch is outside the root window, target the root.
    if (!root_window->bounds().Contains(event.location()))
      return root_window;
  }

  return nullptr;
}

Window* WindowTargeter::FindTargetForLocatedEventRecursively(
    Window* root_window,
    ui::LocatedEvent* event) {
  std::unique_ptr<ui::EventTargetIterator> iter =
      root_window->GetChildIterator();
  if (iter) {
    ui::EventTarget* target = root_window;
    for (ui::EventTarget* child = iter->GetNextTarget(); child;
         child = iter->GetNextTarget()) {
      WindowTargeter* targeter =
          static_cast<WindowTargeter*>(child->GetEventTargeter());
      if (!targeter)
        targeter = this;
      if (!targeter->SubtreeShouldBeExploredForEvent(
              static_cast<Window*>(child), *event)) {
        continue;
      }
      target->ConvertEventToTarget(child, event);
      target = child;
      Window* child_target_window =
          static_cast<Window*>(targeter->FindTargetForEvent(child, event));
      if (child_target_window)
        return child_target_window;
    }
    target->ConvertEventToTarget(root_window, event);
  }
  return root_window->CanAcceptEvent(*event) ? root_window : nullptr;
}

}  // namespace aura
