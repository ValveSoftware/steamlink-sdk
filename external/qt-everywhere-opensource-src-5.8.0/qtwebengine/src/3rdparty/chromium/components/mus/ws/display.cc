// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/display.h"

#include <set>
#include <vector>

#include "base/debug/debugger.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mus/common/types.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/focus_controller.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/platform_display_init_params.h"
#include "components/mus/ws/user_activity_monitor.h"
#include "components/mus/ws/window_manager_display_root.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_manager_window_tree_factory.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_server_delegate.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"
#include "mojo/common/common_type_converters.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "ui/base/cursor/cursor.h"

namespace mus {
namespace ws {

Display::Display(WindowServer* window_server,
                 const PlatformDisplayInitParams& platform_display_init_params)
    : id_(window_server->display_manager()->GetAndAdvanceNextDisplayId()),
      window_server_(window_server),
      platform_display_(PlatformDisplay::Create(platform_display_init_params)),
      last_cursor_(ui::kCursorNone) {
  platform_display_->Init(this);

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

  for (ServerWindow* window : windows_needing_frame_destruction_)
    window->RemoveObserver(this);

  // If there is a |binding_| then the tree was created specifically for this
  // display (which corresponds to a WindowTreeHost).
  if (binding_ && !window_manager_display_root_map_.empty()) {
    window_server_->DestroyTree(window_manager_display_root_map_.begin()
                                    ->second->window_manager_state()
                                    ->window_tree());
  }
}

void Display::Init(std::unique_ptr<DisplayBinding> binding) {
  init_called_ = true;
  binding_ = std::move(binding);
  display_manager()->AddDisplay(this);
  InitWindowManagerDisplayRootsIfNecessary();
}

DisplayManager* Display::display_manager() {
  return window_server_->display_manager();
}

const DisplayManager* Display::display_manager() const {
  return window_server_->display_manager();
}

mojom::DisplayPtr Display::ToMojomDisplay() const {
  mojom::DisplayPtr display_ptr = mojom::Display::New();
  display_ptr = mojom::Display::New();
  display_ptr->id = id_;
  // TODO(sky): Display should know it's origin.
  display_ptr->bounds.SetRect(0, 0, root_->bounds().size().width(),
                              root_->bounds().size().height());
  // TODO(sky): window manager needs an API to set the work area.
  display_ptr->work_area = display_ptr->bounds;
  display_ptr->device_pixel_ratio = platform_display_->GetDeviceScaleFactor();
  display_ptr->rotation = platform_display_->GetRotation();
  // TODO(sky): make this real.
  display_ptr->is_primary = true;
  // TODO(sky): make this real.
  display_ptr->touch_support = mojom::TouchSupport::UNKNOWN;
  display_ptr->frame_decoration_values = mojom::FrameDecorationValues::New();
  return display_ptr;
}

void Display::SchedulePaint(const ServerWindow* window,
                            const gfx::Rect& bounds) {
  DCHECK(root_->Contains(window));
  platform_display_->SchedulePaint(window, bounds);
}

void Display::ScheduleSurfaceDestruction(ServerWindow* window) {
  if (!platform_display_->IsFramePending()) {
    window->DestroySurfacesScheduledForDestruction();
    return;
  }
  if (windows_needing_frame_destruction_.count(window))
    return;
  windows_needing_frame_destruction_.insert(window);
  window->AddObserver(this);
}

mojom::Rotation Display::GetRotation() const {
  return platform_display_->GetRotation();
}

gfx::Size Display::GetSize() const {
  return root_->bounds().size();
}

int64_t Display::GetPlatformDisplayId() const {
  return platform_display_->GetDisplayId();
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
      return pair.second.get();
  }
  return nullptr;
}

const WindowManagerDisplayRoot* Display::GetWindowManagerDisplayRootForUser(
    const UserId& user_id) const {
  auto iter = window_manager_display_root_map_.find(user_id);
  return iter == window_manager_display_root_map_.end() ? nullptr
                                                        : iter->second.get();
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

void Display::UpdateNativeCursor(int32_t cursor_id) {
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

void Display::InitWindowManagerDisplayRootsIfNecessary() {
  if (!init_called_ || !root_)
    return;

  display_manager()->OnDisplayAcceleratedWidgetAvailable(this);
  if (binding_) {
    std::unique_ptr<WindowManagerDisplayRoot> display_root_ptr(
        new WindowManagerDisplayRoot(this));
    WindowManagerDisplayRoot* display_root = display_root_ptr.get();
    // For this case we never create additional displays roots, so any
    // id works.
    window_manager_display_root_map_[shell::mojom::kRootUserID] =
        std::move(display_root_ptr);
    WindowTree* window_tree = binding_->CreateWindowTree(display_root->root());
    display_root->window_manager_state_ = window_tree->window_manager_state();
  } else {
    CreateWindowManagerDisplayRootsFromFactories();
  }
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
  window_manager_display_root_map_[factory->user_id()] =
      std::move(display_root_ptr);
  display_root->window_manager_state_ =
      factory->window_tree()->window_manager_state();
  const bool is_active =
      factory->user_id() == window_server_->user_id_tracker()->active_id();
  display_root->root()->SetVisible(is_active);
  display_root->window_manager_state()->window_tree()->AddRootForWindowManager(
      display_root->root());
}

ServerWindow* Display::GetRootWindow() {
  return root_.get();
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

void Display::OnDisplayClosed() {
  display_manager()->DestroyDisplay(this);
}

void Display::OnViewportMetricsChanged(const ViewportMetrics& old_metrics,
                                       const ViewportMetrics& new_metrics) {
  if (!root_) {
    root_.reset(window_server_->CreateServerWindow(
        display_manager()->GetAndAdvanceNextRootId(),
        ServerWindow::Properties()));
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels));
    root_->SetVisible(true);
    focus_controller_.reset(new FocusController(this, root_.get()));
    focus_controller_->AddObserver(this);
    InitWindowManagerDisplayRootsIfNecessary();
  } else {
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels));
    const gfx::Rect wm_bounds(root_->bounds().size());
    for (auto& pair : window_manager_display_root_map_)
      pair.second->root()->SetBounds(wm_bounds);
  }
  display_manager()->OnDisplayUpdate(this);
}

void Display::OnCompositorFrameDrawn() {
  std::set<ServerWindow*> windows;
  windows.swap(windows_needing_frame_destruction_);
  for (ServerWindow* window : windows) {
    window->RemoveObserver(this);
    window->DestroySurfacesScheduledForDestruction();
  }
}

bool Display::CanHaveActiveChildren(ServerWindow* window) const {
  return window && activation_parents_.Contains(window);
}

void Display::OnActivationChanged(ServerWindow* old_active_window,
                                  ServerWindow* new_active_window) {
  DCHECK_NE(new_active_window, old_active_window);
  if (new_active_window && new_active_window->parent()) {
    // Raise the new active window.
    // TODO(sad): Let the WM dictate whether to raise the window or not?
    new_active_window->parent()->StackChildAtTop(new_active_window);
  }
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

void Display::OnWindowDestroyed(ServerWindow* window) {
  windows_needing_frame_destruction_.erase(window);
  window->RemoveObserver(this);
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
}  // namespace mus
