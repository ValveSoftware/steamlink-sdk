// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/compound_event_filter.h"

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/drag_drop_client.h"

#if defined(OS_CHROMEOS) && defined(USE_X11)
#include "ui/events/devices/x11/touch_factory_x11.h"  // nogncheck
#endif

namespace wm {

namespace {

// Returns true if the cursor should be hidden on touch events.
// TODO(tdanderson|rsadam): Move this function into CursorClient.
bool ShouldHideCursorOnTouch(const ui::TouchEvent& event) {
#if defined(OS_WIN)
  return true;
#elif defined(OS_CHROMEOS)
#if defined(USE_X11)
  int device_id = event.source_device_id();
  if (device_id >= 0 &&
      !ui::TouchFactory::GetInstance()->IsMultiTouchDevice(device_id)) {
    // If the touch event is coming from a mouse-device (i.e. not a real
    // touch-device), then do not hide the cursor.
    return false;
  }
#endif  // defined(USE_X11)
  return true;
#else
  // Linux Aura does not hide the cursor on touch by default.
  // TODO(tdanderson): Change this if having consistency across
  // all platforms which use Aura is desired.
  return false;
#endif
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, public:

CompoundEventFilter::CompoundEventFilter() {
}

CompoundEventFilter::~CompoundEventFilter() {
  // Additional filters are not owned by CompoundEventFilter and they
  // should all be removed when running here. |handlers_| has
  // check_empty == true and will DCHECK failure if it is not empty.
}

// static
gfx::NativeCursor CompoundEventFilter::CursorForWindowComponent(
    int window_component) {
  switch (window_component) {
    case HTBOTTOM:
      return ui::kCursorSouthResize;
    case HTBOTTOMLEFT:
      return ui::kCursorSouthWestResize;
    case HTBOTTOMRIGHT:
      return ui::kCursorSouthEastResize;
    case HTLEFT:
      return ui::kCursorWestResize;
    case HTRIGHT:
      return ui::kCursorEastResize;
    case HTTOP:
      return ui::kCursorNorthResize;
    case HTTOPLEFT:
      return ui::kCursorNorthWestResize;
    case HTTOPRIGHT:
      return ui::kCursorNorthEastResize;
    default:
      return ui::kCursorNull;
  }
}

void CompoundEventFilter::AddHandler(ui::EventHandler* handler) {
  handlers_.AddObserver(handler);
}

void CompoundEventFilter::RemoveHandler(ui::EventHandler* handler) {
  handlers_.RemoveObserver(handler);
}

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, private:

void CompoundEventFilter::UpdateCursor(aura::Window* target,
                                       ui::MouseEvent* event) {
  // If drag and drop is in progress, let the drag drop client set the cursor
  // instead of setting the cursor here.
  aura::Window* root_window = target->GetRootWindow();
  aura::client::DragDropClient* drag_drop_client =
      aura::client::GetDragDropClient(root_window);
  if (drag_drop_client && drag_drop_client->IsDragDropInProgress())
    return;

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client) {
    gfx::NativeCursor cursor = target->GetCursor(event->location());
    if ((event->flags() & ui::EF_IS_NON_CLIENT)) {
      if (target->delegate()) {
        int window_component =
            target->delegate()->GetNonClientComponent(event->location());
        cursor = CursorForWindowComponent(window_component);
      } else {
        // Allow the OS to handle non client cursors if we don't have a
        // a delegate to handle the non client hittest.
        return;
      }
    }
    cursor_client->SetCursor(cursor);
  }
}

void CompoundEventFilter::FilterKeyEvent(ui::KeyEvent* event) {
  if (handlers_.might_have_observers()) {
    base::ObserverListBase<ui::EventHandler>::Iterator it(&handlers_);
    ui::EventHandler* handler;
    while (!event->stopped_propagation() && (handler = it.GetNext()) != NULL)
      handler->OnKeyEvent(event);
  }
}

void CompoundEventFilter::FilterMouseEvent(ui::MouseEvent* event) {
  if (handlers_.might_have_observers()) {
    base::ObserverListBase<ui::EventHandler>::Iterator it(&handlers_);
    ui::EventHandler* handler;
    while (!event->stopped_propagation() && (handler = it.GetNext()) != NULL)
      handler->OnMouseEvent(event);
  }
}

void CompoundEventFilter::FilterTouchEvent(ui::TouchEvent* event) {
  if (handlers_.might_have_observers()) {
    base::ObserverListBase<ui::EventHandler>::Iterator it(&handlers_);
    ui::EventHandler* handler;
    while (!event->stopped_propagation() && (handler = it.GetNext()) != NULL)
      handler->OnTouchEvent(event);
  }
}

void CompoundEventFilter::SetCursorVisibilityOnEvent(aura::Window* target,
                                                     ui::Event* event,
                                                     bool show) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;

  aura::client::CursorClient* client =
      aura::client::GetCursorClient(target->GetRootWindow());
  if (!client)
    return;

  if (show)
    client->ShowCursor();
  else
    client->HideCursor();
}

void CompoundEventFilter::SetMouseEventsEnableStateOnEvent(aura::Window* target,
                                                           ui::Event* event,
                                                           bool enable) {
  if (event->flags() & ui::EF_IS_SYNTHESIZED)
    return;
  aura::client::CursorClient* client =
      aura::client::GetCursorClient(target->GetRootWindow());
  if (!client)
    return;

  if (enable)
    client->EnableMouseEvents();
  else
    client->DisableMouseEvents();
}

////////////////////////////////////////////////////////////////////////////////
// CompoundEventFilter, ui::EventHandler implementation:

void CompoundEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::client::CursorClient* client =
      aura::client::GetCursorClient(target->GetRootWindow());
  if (client && client->ShouldHideCursorOnKeyEvent(*event))
    SetCursorVisibilityOnEvent(target, event, false);

  FilterKeyEvent(event);
}

void CompoundEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* window = static_cast<aura::Window*>(event->target());

  // We must always update the cursor, otherwise the cursor can get stuck if an
  // event filter registered with us consumes the event.
  // It should also update the cursor for clicking and wheels for ChromeOS boot.
  // When ChromeOS is booted, it hides the mouse cursor but immediate mouse
  // operation will show the cursor.
  // We also update the cursor for mouse enter in case a mouse cursor is sent to
  // outside of the root window and moved back for some reasons (e.g. running on
  // on Desktop for testing, or a bug in pointer barrier).
  if (!(event->flags() & ui::EF_FROM_TOUCH) &&
       (event->type() == ui::ET_MOUSE_ENTERED ||
        event->type() == ui::ET_MOUSE_MOVED ||
        event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_MOUSEWHEEL)) {
    SetMouseEventsEnableStateOnEvent(window, event, true);
    SetCursorVisibilityOnEvent(window, event, true);
    UpdateCursor(window, event);
  }

  FilterMouseEvent(event);
}

void CompoundEventFilter::OnScrollEvent(ui::ScrollEvent* event) {
}

void CompoundEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  FilterTouchEvent(event);
  if (!event->handled() && event->type() == ui::ET_TOUCH_PRESSED &&
      ShouldHideCursorOnTouch(*event) &&
      !aura::Env::GetInstance()->IsMouseButtonDown()) {
    SetMouseEventsEnableStateOnEvent(
        static_cast<aura::Window*>(event->target()), event, false);
  }
}

void CompoundEventFilter::OnGestureEvent(ui::GestureEvent* event) {
  if (handlers_.might_have_observers()) {
    base::ObserverListBase<ui::EventHandler>::Iterator it(&handlers_);
    ui::EventHandler* handler;
    while (!event->stopped_propagation() && (handler = it.GetNext()) != NULL)
      handler->OnGestureEvent(event);
  }

#if defined(OS_WIN)
  // A Win8 edge swipe event is a special event that does not have location
  // information associated with it, and is not preceeded by an ET_TOUCH_PRESSED
  // event.  So we treat it specially here.
  if (!event->handled() && event->type() == ui::ET_GESTURE_WIN8_EDGE_SWIPE &&
      !aura::Env::GetInstance()->IsMouseButtonDown()) {
    SetMouseEventsEnableStateOnEvent(
        static_cast<aura::Window*>(event->target()), event, false);
  }
#endif
}

}  // namespace wm
