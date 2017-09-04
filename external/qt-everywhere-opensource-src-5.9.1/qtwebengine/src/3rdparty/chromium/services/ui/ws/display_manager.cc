// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/display_manager.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "services/ui/display/platform_screen.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_binding.h"
#include "services/ui/ws/event_dispatcher.h"
#include "services/ui/ws/platform_display_init_params.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/user_display_manager.h"
#include "services/ui/ws/user_display_manager_delegate.h"
#include "services/ui/ws/user_id_tracker.h"
#include "services/ui/ws/window_manager_state.h"
#include "services/ui/ws/window_manager_window_tree_factory.h"
#include "services/ui/ws/window_server_delegate.h"
#include "services/ui/ws/window_tree.h"

namespace ui {
namespace ws {

DisplayManager::DisplayManager(WindowServer* window_server,
                               UserIdTracker* user_id_tracker)
    // |next_root_id_| is used as the lower bits, so that starting at 0 is
    // fine. |next_display_id_| is used by itself, so we start at 1 to reserve
    // 0 as invalid.
    : window_server_(window_server),
      user_id_tracker_(user_id_tracker),
      next_root_id_(0) {
  user_id_tracker_->AddObserver(this);
}

DisplayManager::~DisplayManager() {
  user_id_tracker_->RemoveObserver(this);
  DestroyAllDisplays();
}

UserDisplayManager* DisplayManager::GetUserDisplayManager(
    const UserId& user_id) {
  if (!user_display_managers_.count(user_id)) {
    user_display_managers_[user_id] =
        base::MakeUnique<UserDisplayManager>(this, window_server_, user_id);
  }
  return user_display_managers_[user_id].get();
}

void DisplayManager::AddDisplay(Display* display) {
  DCHECK_EQ(0u, pending_displays_.count(display));
  pending_displays_.insert(display);
}

void DisplayManager::DestroyDisplay(Display* display) {
  if (pending_displays_.count(display)) {
    pending_displays_.erase(display);
  } else {
    for (const auto& pair : user_display_managers_)
      pair.second->OnWillDestroyDisplay(display);

    DCHECK(displays_.count(display));
    displays_.erase(display);
  }
  delete display;

  // If we have no more roots left, let the app know so it can terminate.
  // TODO(sky): move to delegate/observer.
  if (displays_.empty() && pending_displays_.empty())
    window_server_->OnNoMoreDisplays();
}

void DisplayManager::DestroyAllDisplays() {
  while (!pending_displays_.empty())
    DestroyDisplay(*pending_displays_.begin());
  DCHECK(pending_displays_.empty());

  while (!displays_.empty())
    DestroyDisplay(*displays_.begin());
  DCHECK(displays_.empty());
}

std::set<const Display*> DisplayManager::displays() const {
  std::set<const Display*> ret_value(displays_.begin(), displays_.end());
  return ret_value;
}

void DisplayManager::OnDisplayUpdate(Display* display) {
  for (const auto& pair : user_display_managers_)
    pair.second->OnDisplayUpdate(display);
}

Display* DisplayManager::GetDisplayContaining(const ServerWindow* window) {
  return const_cast<Display*>(
      static_cast<const DisplayManager*>(this)->GetDisplayContaining(window));
}

const Display* DisplayManager::GetDisplayContaining(
    const ServerWindow* window) const {
  while (window && window->parent())
    window = window->parent();
  for (Display* display : displays_) {
    if (window == display->root_window())
      return display;
  }
  return nullptr;
}

Display* DisplayManager::GetDisplayById(int64_t display_id) {
  for (Display* display : displays_) {
    if (display->GetId() == display_id)
      return display;
  }
  return nullptr;
}

const WindowManagerDisplayRoot* DisplayManager::GetWindowManagerDisplayRoot(
    const ServerWindow* window) const {
  const ServerWindow* last = window;
  while (window && window->parent()) {
    last = window;
    window = window->parent();
  }
  for (Display* display : displays_) {
    if (window == display->root_window())
      return display->GetWindowManagerDisplayRootWithRoot(last);
  }
  return nullptr;
}

WindowManagerDisplayRoot* DisplayManager::GetWindowManagerDisplayRoot(
    const ServerWindow* window) {
  return const_cast<WindowManagerDisplayRoot*>(
      const_cast<const DisplayManager*>(this)->GetWindowManagerDisplayRoot(
          window));
}

WindowId DisplayManager::GetAndAdvanceNextRootId() {
  // TODO(sky): handle wrapping!
  const uint16_t id = next_root_id_++;
  DCHECK_LT(id, next_root_id_);
  return RootWindowId(id);
}

void DisplayManager::OnDisplayAcceleratedWidgetAvailable(Display* display) {
  DCHECK_NE(0u, pending_displays_.count(display));
  DCHECK_EQ(0u, displays_.count(display));
  const bool is_first_display = displays_.empty();
  displays_.insert(display);
  pending_displays_.erase(display);
  window_server_->OnDisplayReady(display, is_first_display);
}

void DisplayManager::OnActiveUserIdChanged(const UserId& previously_active_id,
                                           const UserId& active_id) {
  WindowManagerState* previous_window_manager_state =
      window_server_->GetWindowManagerStateForUser(previously_active_id);
  gfx::Point mouse_location_on_screen;
  if (previous_window_manager_state) {
    mouse_location_on_screen = previous_window_manager_state->event_dispatcher()
                                   ->mouse_pointer_last_location();
    previous_window_manager_state->Deactivate();
  }

  WindowManagerState* current_window_manager_state =
      window_server_->GetWindowManagerStateForUser(active_id);
  if (current_window_manager_state)
    current_window_manager_state->Activate(mouse_location_on_screen);
}

void DisplayManager::OnDisplayAdded(int64_t id,
                                    const display::ViewportMetrics& metrics) {
  TRACE_EVENT1("mus-ws", "OnDisplayAdded", "id", id);
  PlatformDisplayInitParams params;
  params.display_id = id;
  params.metrics = metrics;

  ws::Display* display = new ws::Display(window_server_);
  display->Init(params, nullptr);

  window_server_->delegate()->UpdateTouchTransforms();
}

void DisplayManager::OnDisplayRemoved(int64_t id) {
  TRACE_EVENT1("mus-ws", "OnDisplayRemoved", "id", id);
  Display* display = GetDisplayById(id);
  if (display)
    DestroyDisplay(display);
}

void DisplayManager::OnDisplayModified(
    int64_t id,
    const display::ViewportMetrics& metrics) {
  TRACE_EVENT1("mus-ws", "OnDisplayModified", "id", id);

  Display* display = GetDisplayById(id);
  DCHECK(display);

  // Update the platform display and check if anything has actually changed.
  if (!display->platform_display()->UpdateViewportMetrics(metrics))
    return;

  // Send IPCs to WM clients first with new display information.
  std::vector<WindowManagerWindowTreeFactory*> factories =
      window_server_->window_manager_window_tree_factory_set()->GetFactories();
  for (WindowManagerWindowTreeFactory* factory : factories) {
    if (factory->window_tree())
      factory->window_tree()->OnWmDisplayModified(display->ToDisplay());
  }

  // Change the root ServerWindow size after sending IPC to WM.
  display->OnViewportMetricsChanged(metrics);
  OnDisplayUpdate(display);
}

void DisplayManager::OnPrimaryDisplayChanged(int64_t primary_display_id) {
  // TODO(kylechar): Send IPCs to WM clients first.

  // Send IPCs to any DisplayManagerObservers.
  for (const auto& pair : user_display_managers_)
    pair.second->OnPrimaryDisplayChanged(primary_display_id);
}

}  // namespace ws
}  // namespace ui
