// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/display.h"

#include <set>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "mojo/common/common_type_converters.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/ui/common/types.h"
#include "services/ui/public/interfaces/cursor.mojom.h"
#include "services/ui/ws/display_binding.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/focus_controller.h"
#include "services/ui/ws/platform_display.h"
#include "services/ui/ws/platform_display_init_params.h"
#include "services/ui/ws/user_activity_monitor.h"
#include "services/ui/ws/window_manager_display_root.h"
#include "services/ui/ws/window_manager_state.h"
#include "services/ui/ws/window_manager_window_tree_factory.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_server_delegate.h"
#include "services/ui/ws/window_tree.h"
#include "services/ui/ws/window_tree_binding.h"
#include "ui/base/cursor/cursor.h"

namespace ui {
namespace ws {

Display::Display(WindowServer* window_server)
    : window_server_(window_server), last_cursor_(mojom::Cursor::CURSOR_NULL) {
  window_server_->window_manager_window_tree_factory_set()->AddObserver(this);
  window_server_->user_id_tracker()->AddObserver(this);
}

Display::~Display() {
  window_server_->user_id_tracker()->RemoveObserver(this);

  window_server_->window_manager_window_tree_factory_set()->RemoveObserver(
      this);

  if (!focus_controller_) {
    focus_controller_->RemoveObserver(this);
    focus_controller_.reset();
  }

  if (!binding_) {
    for (auto& pair : window_manager_display_root_map_)
      pair.second->window_manager_state()->OnDisplayDestroying(this);
  } else if (!window_manager_display_root_map_.empty()) {
    // If there is a |binding_| then the tree was created specifically for this
    // display (which corresponds to a WindowTreeHost).
    window_server_->DestroyTree(window_manager_display_root_map_.begin()
                                    ->second->window_manager_state()
                                    ->window_tree());
  }
}

void Display::Init(const PlatformDisplayInitParams& init_params,
                   std::unique_ptr<DisplayBinding> binding) {
  binding_ = std::move(binding);
  display_manager()->AddDisplay(this);

  CreateRootWindow(init_params.metrics.pixel_size);
  PlatformDisplayInitParams params_copy = init_params;
  params_copy.root_window = root_.get();

  platform_display_ = PlatformDisplay::Create(params_copy);
  platform_display_->Init(this);
}

int64_t Display::GetId() const {
  return platform_display_->GetId();
}

DisplayManager* Display::display_manager() {
  return window_server_->display_manager();
}

const DisplayManager* Display::display_manager() const {
  return window_server_->display_manager();
}

display::Display Display::ToDisplay() const {
  display::Display display(GetId());

  const display::ViewportMetrics& metrics =
      platform_display_->GetViewportMetrics();

  display.set_bounds(metrics.bounds);
  display.set_work_area(metrics.work_area);
  display.set_device_scale_factor(metrics.device_scale_factor);
  display.set_rotation(metrics.rotation);
  display.set_touch_support(
      display::Display::TouchSupport::TOUCH_SUPPORT_UNKNOWN);

  return display;
}

gfx::Size Display::GetSize() const {
  DCHECK(root_);
  return root_->bounds().size();
}

ServerWindow* Display::GetRootWithId(const WindowId& id) {
  if (id == root_->id())
    return root_.get();
  for (auto& pair : window_manager_display_root_map_) {
    if (pair.second->root()->id() == id)
      return pair.second->root();
  }
  return nullptr;
}

WindowManagerDisplayRoot* Display::GetWindowManagerDisplayRootWithRoot(
    const ServerWindow* window) {
  for (auto& pair : window_manager_display_root_map_) {
    if (pair.second->root() == window)
      return pair.second;
  }
  return nullptr;
}

const WindowManagerDisplayRoot* Display::GetWindowManagerDisplayRootForUser(
    const UserId& user_id) const {
  auto iter = window_manager_display_root_map_.find(user_id);
  return iter == window_manager_display_root_map_.end() ? nullptr
                                                        : iter->second;
}

const WindowManagerDisplayRoot* Display::GetActiveWindowManagerDisplayRoot()
    const {
  return GetWindowManagerDisplayRootForUser(
      window_server_->user_id_tracker()->active_id());
}

bool Display::SetFocusedWindow(ServerWindow* new_focused_window) {
  ServerWindow* old_focused_window = focus_controller_->GetFocusedWindow();
  if (old_focused_window == new_focused_window)
    return true;
  DCHECK(!new_focused_window || root_window()->Contains(new_focused_window));
  return focus_controller_->SetFocusedWindow(new_focused_window);
}

ServerWindow* Display::GetFocusedWindow() {
  return focus_controller_->GetFocusedWindow();
}

void Display::ActivateNextWindow() {
  // TODO(sky): this is wrong, needs to figure out the next window to activate
  // and then route setting through WindowServer.
  focus_controller_->ActivateNextWindow();
}

void Display::AddActivationParent(ServerWindow* window) {
  activation_parents_.Add(window);
}

void Display::RemoveActivationParent(ServerWindow* window) {
  activation_parents_.Remove(window);
}

void Display::UpdateTextInputState(ServerWindow* window,
                                   const ui::TextInputState& state) {
  // Do not need to update text input for unfocused windows.
  if (!platform_display_ || focus_controller_->GetFocusedWindow() != window)
    return;
  platform_display_->UpdateTextInputState(state);
}

void Display::SetImeVisibility(ServerWindow* window, bool visible) {
  // Do not need to show or hide IME for unfocused window.
  if (focus_controller_->GetFocusedWindow() != window)
    return;
  platform_display_->SetImeVisibility(visible);
}

void Display::OnWillDestroyTree(WindowTree* tree) {
  for (auto it = window_manager_display_root_map_.begin();
       it != window_manager_display_root_map_.end(); ++it) {
    if (it->second->window_manager_state()->window_tree() == tree) {
      window_manager_display_root_map_.erase(it);
      break;
    }
  }
}

void Display::UpdateNativeCursor(mojom::Cursor cursor_id) {
  if (cursor_id != last_cursor_) {
    platform_display_->SetCursorById(cursor_id);
    last_cursor_ = cursor_id;
  }
}

void Display::SetSize(const gfx::Size& size) {
  platform_display_->SetViewportSize(size);
}

void Display::SetTitle(const mojo::String& title) {
  platform_display_->SetTitle(title.To<base::string16>());
}

void Display::InitWindowManagerDisplayRoots() {
  if (binding_) {
    std::unique_ptr<WindowManagerDisplayRoot> display_root_ptr(
        new WindowManagerDisplayRoot(this));
    WindowManagerDisplayRoot* display_root = display_root_ptr.get();
    // For this case we never create additional displays roots, so any
    // id works.
    window_manager_display_root_map_[service_manager::mojom::kRootUserID] =
        display_root_ptr.get();
    WindowTree* window_tree = binding_->CreateWindowTree(display_root->root());
    display_root->window_manager_state_ = window_tree->window_manager_state();
    window_tree->window_manager_state()->AddWindowManagerDisplayRoot(
        std::move(display_root_ptr));
  } else {
    CreateWindowManagerDisplayRootsFromFactories();
  }
  display_manager()->OnDisplayUpdate(this);
}

void Display::CreateWindowManagerDisplayRootsFromFactories() {
  std::vector<WindowManagerWindowTreeFactory*> factories =
      window_server_->window_manager_window_tree_factory_set()->GetFactories();
  for (WindowManagerWindowTreeFactory* factory : factories) {
    if (factory->window_tree())
      CreateWindowManagerDisplayRootFromFactory(factory);
  }
}

void Display::CreateWindowManagerDisplayRootFromFactory(
    WindowManagerWindowTreeFactory* factory) {
  std::unique_ptr<WindowManagerDisplayRoot> display_root_ptr(
      new WindowManagerDisplayRoot(this));
  WindowManagerDisplayRoot* display_root = display_root_ptr.get();
  window_manager_display_root_map_[factory->user_id()] = display_root_ptr.get();
  WindowManagerState* window_manager_state =
      factory->window_tree()->window_manager_state();
  display_root->window_manager_state_ = window_manager_state;
  const bool is_active =
      factory->user_id() == window_server_->user_id_tracker()->active_id();
  display_root->root()->SetVisible(is_active);
  window_manager_state->window_tree()->AddRootForWindowManager(
      display_root->root());
  window_manager_state->AddWindowManagerDisplayRoot(
      std::move(display_root_ptr));
}

void Display::CreateRootWindow(const gfx::Size& size) {
  DCHECK(!root_);

  root_.reset(window_server_->CreateServerWindow(
      display_manager()->GetAndAdvanceNextRootId(),
      ServerWindow::Properties()));
  root_->SetBounds(gfx::Rect(size));
  root_->SetVisible(true);
  focus_controller_ = base::MakeUnique<FocusController>(this, root_.get());
  focus_controller_->AddObserver(this);
}

ServerWindow* Display::GetRootWindow() {
  return root_.get();
}

void Display::OnAcceleratedWidgetAvailable() {
  display_manager()->OnDisplayAcceleratedWidgetAvailable(this);
  InitWindowManagerDisplayRoots();
}

bool Display::IsInHighContrastMode() {
  return window_server_->IsActiveUserInHighContrastMode();
}

void Display::OnEvent(const ui::Event& event) {
  WindowManagerDisplayRoot* display_root = GetActiveWindowManagerDisplayRoot();
  if (display_root)
    display_root->window_manager_state()->ProcessEvent(event);
  window_server_
      ->GetUserActivityMonitorForUser(
          window_server_->user_id_tracker()->active_id())
      ->OnUserActivity();
}

void Display::OnNativeCaptureLost() {
  WindowManagerDisplayRoot* display_root = GetActiveWindowManagerDisplayRoot();
  if (display_root)
    display_root->window_manager_state()->SetCapture(nullptr, kInvalidClientId);
}

void Display::OnViewportMetricsChanged(
    const display::ViewportMetrics& metrics) {
  if (root_->bounds().size() == metrics.pixel_size)
    return;

  gfx::Rect new_bounds(metrics.pixel_size);
  root_->SetBounds(new_bounds);
  for (auto& pair : window_manager_display_root_map_)
    pair.second->root()->SetBounds(new_bounds);
}

bool Display::CanHaveActiveChildren(ServerWindow* window) const {
  return window && activation_parents_.Contains(window);
}

void Display::OnActivationChanged(ServerWindow* old_active_window,
                                  ServerWindow* new_active_window) {
  // Don't do anything here. We assume the window manager handles restacking. If
  // we did attempt to restack than we would have to ensure clients see the
  // restack.
}

void Display::OnFocusChanged(FocusControllerChangeSource change_source,
                             ServerWindow* old_focused_window,
                             ServerWindow* new_focused_window) {
  // TODO(sky): focus is global, not per windowtreehost. Move.

  // There are up to four clients that need to be notified:
  // . the client containing |old_focused_window|.
  // . the client with |old_focused_window| as its root.
  // . the client containing |new_focused_window|.
  // . the client with |new_focused_window| as its root.
  // Some of these client may be the same. The following takes care to notify
  // each only once.
  WindowTree* owning_tree_old = nullptr;
  WindowTree* embedded_tree_old = nullptr;

  if (old_focused_window) {
    owning_tree_old =
        window_server_->GetTreeWithId(old_focused_window->id().client_id);
    if (owning_tree_old) {
      owning_tree_old->ProcessFocusChanged(old_focused_window,
                                           new_focused_window);
    }
    embedded_tree_old = window_server_->GetTreeWithRoot(old_focused_window);
    if (embedded_tree_old) {
      DCHECK_NE(owning_tree_old, embedded_tree_old);
      embedded_tree_old->ProcessFocusChanged(old_focused_window,
                                             new_focused_window);
    }
  }
  WindowTree* owning_tree_new = nullptr;
  WindowTree* embedded_tree_new = nullptr;
  if (new_focused_window) {
    owning_tree_new =
        window_server_->GetTreeWithId(new_focused_window->id().client_id);
    if (owning_tree_new && owning_tree_new != owning_tree_old &&
        owning_tree_new != embedded_tree_old) {
      owning_tree_new->ProcessFocusChanged(old_focused_window,
                                           new_focused_window);
    }
    embedded_tree_new = window_server_->GetTreeWithRoot(new_focused_window);
    if (embedded_tree_new && embedded_tree_new != owning_tree_old &&
        embedded_tree_new != embedded_tree_old) {
      DCHECK_NE(owning_tree_new, embedded_tree_new);
      embedded_tree_new->ProcessFocusChanged(old_focused_window,
                                             new_focused_window);
    }
  }

  // WindowManagers are always notified of focus changes.
  WindowManagerDisplayRoot* display_root = GetActiveWindowManagerDisplayRoot();
  if (display_root) {
    WindowTree* wm_tree = display_root->window_manager_state()->window_tree();
    if (wm_tree != owning_tree_old && wm_tree != embedded_tree_old &&
        wm_tree != owning_tree_new && wm_tree != embedded_tree_new) {
      wm_tree->ProcessFocusChanged(old_focused_window, new_focused_window);
    }
  }

  UpdateTextInputState(new_focused_window,
                       new_focused_window->text_input_state());
}

void Display::OnUserIdRemoved(const UserId& id) {
  window_manager_display_root_map_.erase(id);
}

void Display::OnWindowManagerWindowTreeFactoryReady(
    WindowManagerWindowTreeFactory* factory) {
  if (!binding_)
    CreateWindowManagerDisplayRootFromFactory(factory);
}

}  // namespace ws
}  // namespace ui
