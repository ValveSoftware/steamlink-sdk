// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_tree.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "services/ui/ws/default_access_policy.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/event_matcher.h"
#include "services/ui/ws/focus_controller.h"
#include "services/ui/ws/operation.h"
#include "services/ui/ws/platform_display.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"
#include "services/ui/ws/server_window_observer.h"
#include "services/ui/ws/user_display_manager.h"
#include "services/ui/ws/window_manager_display_root.h"
#include "services/ui/ws/window_manager_state.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree_binding.h"
#include "ui/display/display.h"
#include "ui/platform_window/mojo/ime_type_converters.h"
#include "ui/platform_window/text_input_state.h"

using mojo::Array;
using mojo::InterfaceRequest;
using mojo::String;

namespace ui {
namespace ws {

class TargetedEvent : public ServerWindowObserver {
 public:
  TargetedEvent(ServerWindow* target, const ui::Event& event)
      : target_(target), event_(ui::Event::Clone(event)) {
    target_->AddObserver(this);
  }
  ~TargetedEvent() override {
    if (target_)
      target_->RemoveObserver(this);
  }

  ServerWindow* target() { return target_; }
  std::unique_ptr<ui::Event> TakeEvent() { return std::move(event_); }

 private:
  // ServerWindowObserver:
  void OnWindowDestroyed(ServerWindow* window) override {
    DCHECK_EQ(target_, window);
    target_->RemoveObserver(this);
    target_ = nullptr;
  }

  ServerWindow* target_;
  std::unique_ptr<ui::Event> event_;

  DISALLOW_COPY_AND_ASSIGN(TargetedEvent);
};

WindowTree::WindowTree(WindowServer* window_server,
                       const UserId& user_id,
                       ServerWindow* root,
                       std::unique_ptr<AccessPolicy> access_policy)
    : window_server_(window_server),
      user_id_(user_id),
      id_(window_server_->GetAndAdvanceNextClientId()),
      next_window_id_(1),
      access_policy_(std::move(access_policy)),
      event_ack_id_(0),
      window_manager_internal_(nullptr) {
  if (root)
    roots_.insert(root);
  access_policy_->Init(id_, this);
}

WindowTree::~WindowTree() {
  DestroyWindows();

  // We alert the WindowManagerState that we're destroying this state here
  // because WindowManagerState would attempt to use things that wouldn't have
  // been cleaned up by OnWindowDestroyingTreeImpl().
  if (window_manager_state_)
    window_manager_state_->OnWillDestroyTree(this);
}

void WindowTree::Init(std::unique_ptr<WindowTreeBinding> binding,
                      mojom::WindowTreePtr tree) {
  DCHECK(!binding_);
  binding_ = std::move(binding);

  if (roots_.empty())
    return;

  std::vector<const ServerWindow*> to_send;
  CHECK_EQ(1u, roots_.size());
  const ServerWindow* root = *roots_.begin();
  GetUnknownWindowsFrom(root, &to_send);

  Display* display = GetDisplay(root);
  int64_t display_id =
      display ? display->GetId() : display::Display::kInvalidDisplayID;
  const ServerWindow* focused_window =
      display ? display->GetFocusedWindow() : nullptr;
  if (focused_window)
    focused_window = access_policy_->GetWindowForFocusChange(focused_window);
  ClientWindowId focused_window_id;
  if (focused_window)
    IsWindowKnown(focused_window, &focused_window_id);

  const bool drawn = root->parent() && root->parent()->IsDrawn();
  client()->OnEmbed(id_, WindowToWindowData(to_send.front()), std::move(tree),
                    display_id, focused_window_id.id, drawn);
}

void WindowTree::ConfigureWindowManager() {
  // ConfigureWindowManager() should be called early on, before anything
  // else. |waiting_for_top_level_window_info_| must be null as if
  // |waiting_for_top_level_window_info_| is non-null it means we're about to
  // create an associated interface, which doesn't work with pause/resume.
  // TODO(sky): DCHECK temporary until 626869 is sorted out.
  DCHECK(!waiting_for_top_level_window_info_);
  DCHECK(!window_manager_internal_);
  window_manager_internal_ = binding_->GetWindowManager();
  window_manager_internal_->OnConnect(id_);
  window_manager_state_ = base::MakeUnique<WindowManagerState>(this);
}

const ServerWindow* WindowTree::GetWindow(const WindowId& id) const {
  if (id_ == id.client_id) {
    auto iter = created_window_map_.find(id);
    return iter == created_window_map_.end() ? nullptr : iter->second;
  }
  return window_server_->GetWindow(id);
}

bool WindowTree::IsWindowKnown(const ServerWindow* window,
                               ClientWindowId* id) const {
  if (!window)
    return false;
  auto iter = window_id_to_client_id_map_.find(window->id());
  if (iter == window_id_to_client_id_map_.end())
    return false;
  if (id)
    *id = iter->second;
  return true;
}

bool WindowTree::HasRoot(const ServerWindow* window) const {
  return roots_.count(window) > 0;
}

const ServerWindow* WindowTree::GetWindowByClientId(
    const ClientWindowId& id) const {
  auto iter = client_id_to_window_id_map_.find(id);
  return iter == client_id_to_window_id_map_.end() ? nullptr
                                                   : GetWindow(iter->second);
}

const Display* WindowTree::GetDisplay(const ServerWindow* window) const {
  return window ? display_manager()->GetDisplayContaining(window) : nullptr;
}

const WindowManagerDisplayRoot* WindowTree::GetWindowManagerDisplayRoot(
    const ServerWindow* window) const {
  return window ? display_manager()->GetWindowManagerDisplayRoot(window)
                : nullptr;
}

DisplayManager* WindowTree::display_manager() {
  return window_server_->display_manager();
}

const DisplayManager* WindowTree::display_manager() const {
  return window_server_->display_manager();
}

void WindowTree::PrepareForWindowServerShutdown() {
  window_manager_internal_client_binding_.reset();
  binding_->ResetClientForShutdown();
  if (window_manager_internal_)
    window_manager_internal_ = binding_->GetWindowManager();
}

void WindowTree::AddRootForWindowManager(const ServerWindow* root) {
  DCHECK(window_manager_internal_);
  const ClientWindowId client_window_id(WindowIdToTransportId(root->id()));
  DCHECK_EQ(0u, client_id_to_window_id_map_.count(client_window_id));
  client_id_to_window_id_map_[client_window_id] = root->id();
  window_id_to_client_id_map_[root->id()] = client_window_id;
  roots_.insert(root);

  Display* display = GetDisplay(root);
  DCHECK(display);

  window_manager_internal_->WmNewDisplayAdded(display->ToDisplay(),
                                              WindowToWindowData(root),
                                              root->parent()->IsDrawn());
}

void WindowTree::OnWindowDestroyingTreeImpl(WindowTree* tree) {
  if (event_source_wms_ && event_source_wms_->window_tree() == tree)
    event_source_wms_ = nullptr;

  // Notify our client if |tree| was embedded in any of our windows.
  for (const auto* tree_root : tree->roots_) {
    const bool owns_tree_root = tree_root->id().client_id == id_;
    if (owns_tree_root) {
      client()->OnEmbeddedAppDisconnected(
          ClientWindowIdForWindow(tree_root).id);
    }
  }
}

void WindowTree::OnWmDisplayModified(const display::Display& display) {
  window_manager_internal_->WmDisplayModified(display);
}

void WindowTree::NotifyChangeCompleted(
    uint32_t change_id,
    mojom::WindowManagerErrorCode error_code) {
  client()->OnChangeCompleted(
      change_id, error_code == mojom::WindowManagerErrorCode::SUCCESS);
}

bool WindowTree::SetCapture(const ClientWindowId& client_window_id) {
  ServerWindow* window = GetWindowByClientId(client_window_id);
  WindowManagerDisplayRoot* display_root = GetWindowManagerDisplayRoot(window);
  ServerWindow* current_capture_window =
      display_root ? display_root->window_manager_state()->capture_window()
                   : nullptr;
  if (window && window->IsDrawn() && display_root &&
      display_root->window_manager_state()->IsActive() &&
      access_policy_->CanSetCapture(window) &&
      (!current_capture_window ||
       access_policy_->CanSetCapture(current_capture_window))) {
    Operation op(this, window_server_, OperationType::SET_CAPTURE);
    return display_root->window_manager_state()->SetCapture(window, id_);
  }
  return false;
}

bool WindowTree::ReleaseCapture(const ClientWindowId& client_window_id) {
  ServerWindow* window = GetWindowByClientId(client_window_id);
  WindowManagerDisplayRoot* display_root = GetWindowManagerDisplayRoot(window);
  ServerWindow* current_capture_window =
      display_root ? display_root->window_manager_state()->capture_window()
                   : nullptr;
  if (!window || !display_root ||
      !display_root->window_manager_state()->IsActive() ||
      (current_capture_window &&
       !access_policy_->CanSetCapture(current_capture_window)) ||
      window != current_capture_window) {
    return false;
  }
  Operation op(this, window_server_, OperationType::RELEASE_CAPTURE);
  return display_root->window_manager_state()->SetCapture(nullptr,
                                                          kInvalidClientId);
}

bool WindowTree::NewWindow(
    const ClientWindowId& client_window_id,
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  if (!IsValidIdForNewWindow(client_window_id))
    return false;
  const WindowId window_id = GenerateNewWindowId();
  DCHECK(!GetWindow(window_id));
  ServerWindow* window =
      window_server_->CreateServerWindow(window_id, properties);
  created_window_map_[window_id] = window;
  client_id_to_window_id_map_[client_window_id] = window_id;
  window_id_to_client_id_map_[window_id] = client_window_id;
  return true;
}

bool WindowTree::AddWindow(const ClientWindowId& parent_id,
                           const ClientWindowId& child_id) {
  ServerWindow* parent = GetWindowByClientId(parent_id);
  ServerWindow* child = GetWindowByClientId(child_id);
  if (parent && child && child->parent() != parent &&
      !child->Contains(parent) && access_policy_->CanAddWindow(parent, child)) {
    Operation op(this, window_server_, OperationType::ADD_WINDOW);
    parent->Add(child);
    return true;
  }
  return false;
}

bool WindowTree::AddTransientWindow(const ClientWindowId& window_id,
                                    const ClientWindowId& transient_window_id) {
  ServerWindow* window = GetWindowByClientId(window_id);
  ServerWindow* transient_window = GetWindowByClientId(transient_window_id);
  if (window && transient_window && !transient_window->Contains(window) &&
      access_policy_->CanAddTransientWindow(window, transient_window)) {
    Operation op(this, window_server_, OperationType::ADD_TRANSIENT_WINDOW);
    return window->AddTransientWindow(transient_window);
  }
  return false;
}

bool WindowTree::DeleteWindow(const ClientWindowId& window_id) {
  ServerWindow* window = GetWindowByClientId(window_id);
  if (!window)
    return false;

  if (roots_.count(window) > 0) {
    // Deleting a root behaves as an unembed.
    window_server_->OnTreeMessagedClient(id_);
    RemoveRoot(window, RemoveRootReason::UNEMBED);
    return true;
  }

  if (!access_policy_->CanDeleteWindow(window) &&
      !ShouldRouteToWindowManager(window)) {
    return false;
  }

  // Have the owner of the tree service the actual delete.
  WindowTree* tree = window_server_->GetTreeWithId(window->id().client_id);
  return tree && tree->DeleteWindowImpl(this, window);
}

bool WindowTree::SetModal(const ClientWindowId& window_id) {
  ServerWindow* window = GetWindowByClientId(window_id);
  if (window && access_policy_->CanSetModal(window)) {
    WindowManagerDisplayRoot* display_root =
        GetWindowManagerDisplayRoot(window);
    if (window->transient_parent()) {
      window->SetModal();
    } else if (user_id_ != InvalidUserId()) {
      if (display_root)
        display_root->window_manager_state()->AddSystemModalWindow(window);
    } else {
      return false;
    }
    if (display_root)
      display_root->window_manager_state()->ReleaseCaptureBlockedByModalWindow(
          window);
    return true;
  }
  return false;
}

std::vector<const ServerWindow*> WindowTree::GetWindowTree(
    const ClientWindowId& window_id) const {
  const ServerWindow* window = GetWindowByClientId(window_id);
  std::vector<const ServerWindow*> windows;
  if (window)
    GetWindowTreeImpl(window, &windows);
  return windows;
}

bool WindowTree::SetWindowVisibility(const ClientWindowId& window_id,
                                     bool visible) {
  ServerWindow* window = GetWindowByClientId(window_id);
  if (!window || !access_policy_->CanChangeWindowVisibility(window))
    return false;
  if (window->visible() == visible)
    return true;
  Operation op(this, window_server_, OperationType::SET_WINDOW_VISIBILITY);
  window->SetVisible(visible);
  return true;
}

bool WindowTree::SetWindowOpacity(const ClientWindowId& window_id,
                                  float opacity) {
  ServerWindow* window = GetWindowByClientId(window_id);
  if (!window || !access_policy_->CanChangeWindowOpacity(window))
    return false;
  if (window->opacity() == opacity)
    return true;
  Operation op(this, window_server_, OperationType::SET_WINDOW_OPACITY);
  window->SetOpacity(opacity);
  return true;
}

bool WindowTree::SetFocus(const ClientWindowId& window_id) {
  ServerWindow* window = GetWindowByClientId(window_id);
  ServerWindow* currently_focused = window_server_->GetFocusedWindow();
  if (!currently_focused && !window) {
    DVLOG(1) << "SetFocus failure, no focused window to clear.";
    return false;
  }

  Display* display = GetDisplay(window);
  if (window && (!display || !window->can_focus() || !window->IsDrawn())) {
    DVLOG(1) << "SetFocus failure, window cannot be focused.";
    return false;
  }

  if (!access_policy_->CanSetFocus(window)) {
    DVLOG(1) << "SetFocus failure, blocked by access policy.";
    return false;
  }

  Operation op(this, window_server_, OperationType::SET_FOCUS);
  bool success = window_server_->SetFocusedWindow(window);
  if (!success) {
    DVLOG(1) << "SetFocus failure, could not SetFocusedWindow.";
  }
  return success;
}

bool WindowTree::Embed(const ClientWindowId& window_id,
                       mojom::WindowTreeClientPtr client,
                       uint32_t flags) {
  if (!client || !CanEmbed(window_id))
    return false;
  ServerWindow* window = GetWindowByClientId(window_id);
  PrepareForEmbed(window);
  // When embedding we don't know the user id of where the TreeClient came
  // from. Use an invalid id, which limits what the client is able to do.
  window_server_->EmbedAtWindow(window, InvalidUserId(), std::move(client),
                                flags,
                                base::WrapUnique(new DefaultAccessPolicy));
  return true;
}

void WindowTree::DispatchInputEvent(ServerWindow* target,
                                    const ui::Event& event) {
  if (event_ack_id_) {
    // This is currently waiting for an event ack. Add it to the queue.
    event_queue_.push(base::MakeUnique<TargetedEvent>(target, event));
    // TODO(sad): If the |event_queue_| grows too large, then this should notify
    // Display, so that it can stop sending events.
    return;
  }

  // If there are events in the queue, then store this new event in the queue,
  // and dispatch the latest event from the queue instead that still has a live
  // target.
  if (!event_queue_.empty()) {
    event_queue_.push(base::MakeUnique<TargetedEvent>(target, event));
    return;
  }

  DispatchInputEventImpl(target, event);
}

bool WindowTree::IsWaitingForNewTopLevelWindow(uint32_t wm_change_id) {
  return waiting_for_top_level_window_info_ &&
         waiting_for_top_level_window_info_->wm_change_id == wm_change_id;
}

void WindowTree::OnWindowManagerCreatedTopLevelWindow(
    uint32_t wm_change_id,
    uint32_t client_change_id,
    const ServerWindow* window) {
  DCHECK(IsWaitingForNewTopLevelWindow(wm_change_id));
  std::unique_ptr<WaitingForTopLevelWindowInfo>
      waiting_for_top_level_window_info(
          std::move(waiting_for_top_level_window_info_));
  binding_->SetIncomingMethodCallProcessingPaused(false);
  // We were paused, so the id should still be valid.
  DCHECK(IsValidIdForNewWindow(
      waiting_for_top_level_window_info->client_window_id));
  client_id_to_window_id_map_[waiting_for_top_level_window_info
                                  ->client_window_id] = window->id();
  window_id_to_client_id_map_[window->id()] =
      waiting_for_top_level_window_info->client_window_id;
  roots_.insert(window);
  Display* display = GetDisplay(window);
  int64_t display_id =
      display ? display->GetId() : display::Display::kInvalidDisplayID;
  const bool drawn = window->parent() && window->parent()->IsDrawn();
  client()->OnTopLevelCreated(client_change_id, WindowToWindowData(window),
                              display_id, drawn);
}

void WindowTree::AddActivationParent(const ClientWindowId& window_id) {
  ServerWindow* window = GetWindowByClientId(window_id);
  if (window) {
    Display* display = GetDisplay(window);
    if (display)
      display->AddActivationParent(window);
    else
      DVLOG(1) << "AddActivationParent window not associated with display";
  } else {
    DVLOG(1) << "AddActivationParent supplied invalid window id";
  }
}

void WindowTree::OnChangeCompleted(uint32_t change_id, bool success) {
  client()->OnChangeCompleted(change_id, success);
}

void WindowTree::OnAccelerator(uint32_t accelerator_id,
                               const ui::Event& event,
                               bool needs_ack) {
  DCHECK(window_manager_internal_);
  if (needs_ack)
    GenerateEventAckId();
  else
    DCHECK_EQ(0u, event_ack_id_);
  // TODO(moshayedi): crbug.com/617167. Don't clone even once we map
  // mojom::Event directly to ui::Event.
  window_manager_internal_->OnAccelerator(event_ack_id_, accelerator_id,
                                          ui::Event::Clone(event));
}

void WindowTree::OnDisplayDestroying(int64_t display_id) {
  DCHECK(window_manager_internal_);
  window_manager_internal_->WmDisplayRemoved(display_id);
}

void WindowTree::ClientJankinessChanged(WindowTree* tree) {
  tree->janky_ = !tree->janky_;
  // Don't inform the client if it is the source of jank (which generally only
  // happens while debugging).
  if (window_manager_internal_ && tree != this) {
    window_manager_internal_->WmClientJankinessChanged(
        tree->id(), tree->janky());
  }
}

void WindowTree::ProcessWindowBoundsChanged(const ServerWindow* window,
                                            const gfx::Rect& old_bounds,
                                            const gfx::Rect& new_bounds,
                                            bool originated_change) {
  ClientWindowId client_window_id;
  if (originated_change || !IsWindowKnown(window, &client_window_id))
    return;
  client()->OnWindowBoundsChanged(client_window_id.id, old_bounds, new_bounds);
}

void WindowTree::ProcessClientAreaChanged(
    const ServerWindow* window,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas,
    bool originated_change) {
  ClientWindowId client_window_id;
  if (originated_change || !IsWindowKnown(window, &client_window_id))
    return;
  client()->OnClientAreaChanged(
      client_window_id.id, new_client_area,
      std::vector<gfx::Rect>(new_additional_client_areas));
}

void WindowTree::ProcessWillChangeWindowHierarchy(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent,
    bool originated_change) {
  if (originated_change)
    return;

  const bool old_drawn = window->IsDrawn();
  const bool new_drawn =
      window->visible() && new_parent && new_parent->IsDrawn();
  if (old_drawn == new_drawn)
    return;

  NotifyDrawnStateChanged(window, new_drawn);
}

void WindowTree::ProcessWindowPropertyChanged(
    const ServerWindow* window,
    const std::string& name,
    const std::vector<uint8_t>* new_data,
    bool originated_change) {
  if (originated_change)
    return;

  ClientWindowId client_window_id;
  if (!IsWindowKnown(window, &client_window_id))
    return;

  Array<uint8_t> data(nullptr);
  if (new_data)
    data = Array<uint8_t>::From(*new_data);

  client()->OnWindowSharedPropertyChanged(client_window_id.id, String(name),
                                          std::move(data));
}

void WindowTree::ProcessWindowHierarchyChanged(const ServerWindow* window,
                                               const ServerWindow* new_parent,
                                               const ServerWindow* old_parent,
                                               bool originated_change) {
  const bool knows_new = new_parent && IsWindowKnown(new_parent);
  if (originated_change || (window_server_->current_operation_type() ==
                            OperationType::DELETE_WINDOW) ||
      (window_server_->current_operation_type() == OperationType::EMBED) ||
      window_server_->DidTreeMessageClient(id_)) {
    return;
  }

  if (!access_policy_->ShouldNotifyOnHierarchyChange(window, &new_parent,
                                                     &old_parent)) {
    return;
  }
  // Inform the client of any new windows and update the set of windows we know
  // about.
  std::vector<const ServerWindow*> to_send;
  if (!IsWindowKnown(window))
    GetUnknownWindowsFrom(window, &to_send);
  const bool knows_old = old_parent && IsWindowKnown(old_parent);
  if (!knows_old && !knows_new)
    return;

  const ClientWindowId new_parent_client_window_id =
      knows_new ? ClientWindowIdForWindow(new_parent) : ClientWindowId();
  const ClientWindowId old_parent_client_window_id =
      knows_old ? ClientWindowIdForWindow(old_parent) : ClientWindowId();
  const ClientWindowId client_window_id =
      window ? ClientWindowIdForWindow(window) : ClientWindowId();
  client()->OnWindowHierarchyChanged(
      client_window_id.id, old_parent_client_window_id.id,
      new_parent_client_window_id.id, WindowsToWindowDatas(to_send));
  window_server_->OnTreeMessagedClient(id_);
}

void WindowTree::ProcessWindowReorder(const ServerWindow* window,
                                      const ServerWindow* relative_window,
                                      mojom::OrderDirection direction,
                                      bool originated_change) {
  DCHECK_EQ(window->parent(), relative_window->parent());
  ClientWindowId client_window_id, relative_client_window_id;
  if (originated_change || !IsWindowKnown(window, &client_window_id) ||
      !IsWindowKnown(relative_window, &relative_client_window_id) ||
      window_server_->DidTreeMessageClient(id_))
    return;

  // Do not notify ordering changes of the root windows, since the client
  // doesn't know about the ancestors of the roots, and so can't do anything
  // about this ordering change of the root.
  if (HasRoot(window) || HasRoot(relative_window))
    return;

  client()->OnWindowReordered(client_window_id.id, relative_client_window_id.id,
                              direction);
  window_server_->OnTreeMessagedClient(id_);
}

void WindowTree::ProcessWindowDeleted(ServerWindow* window,
                                      bool originated_change) {
  if (window->id().client_id == id_)
    created_window_map_.erase(window->id());

  ClientWindowId client_window_id;
  if (!IsWindowKnown(window, &client_window_id))
    return;

  if (HasRoot(window))
    RemoveRoot(window, RemoveRootReason::DELETED);
  else
    RemoveFromMaps(window);

  if (originated_change)
    return;

  client()->OnWindowDeleted(client_window_id.id);
  window_server_->OnTreeMessagedClient(id_);
}

void WindowTree::ProcessWillChangeWindowVisibility(const ServerWindow* window,
                                                   bool originated_change) {
  if (originated_change)
    return;

  ClientWindowId client_window_id;
  if (IsWindowKnown(window, &client_window_id)) {
    client()->OnWindowVisibilityChanged(client_window_id.id,
                                        !window->visible());
    return;
  }

  bool window_target_drawn_state;
  if (window->visible()) {
    // Window is being hidden, won't be drawn.
    window_target_drawn_state = false;
  } else {
    // Window is being shown. Window will be drawn if its parent is drawn.
    window_target_drawn_state = window->parent() && window->parent()->IsDrawn();
  }

  NotifyDrawnStateChanged(window, window_target_drawn_state);
}

void WindowTree::ProcessWindowOpacityChanged(const ServerWindow* window,
                                             float old_opacity,
                                             float new_opacity,
                                             bool originated_change) {
  if (originated_change)
    return;

  ClientWindowId client_window_id;
  if (IsWindowKnown(window, &client_window_id)) {
    client()->OnWindowOpacityChanged(client_window_id.id, old_opacity,
                                     new_opacity);
  }
}

void WindowTree::ProcessCursorChanged(const ServerWindow* window,
                                      mojom::Cursor cursor_id,
                                      bool originated_change) {
  if (originated_change)
    return;
  ClientWindowId client_window_id;
  if (!IsWindowKnown(window, &client_window_id))
    return;

  client()->OnWindowPredefinedCursorChanged(client_window_id.id, cursor_id);
}

void WindowTree::ProcessFocusChanged(const ServerWindow* old_focused_window,
                                     const ServerWindow* new_focused_window) {
  if (window_server_->current_operation_type() == OperationType::SET_FOCUS &&
      window_server_->IsOperationSource(id_)) {
    return;
  }
  const ServerWindow* window =
      new_focused_window
          ? access_policy_->GetWindowForFocusChange(new_focused_window)
          : nullptr;
  ClientWindowId client_window_id;
  // If the window isn't known we'll supply null, which is ok.
  IsWindowKnown(window, &client_window_id);
  client()->OnWindowFocused(client_window_id.id);
}

void WindowTree::ProcessTransientWindowAdded(
    const ServerWindow* window,
    const ServerWindow* transient_window,
    bool originated_change) {
  if (originated_change)
    return;

  ClientWindowId client_window_id, transient_client_window_id;
  if (!IsWindowKnown(window, &client_window_id) ||
      !IsWindowKnown(transient_window, &transient_client_window_id)) {
    return;
  }
  client()->OnTransientWindowAdded(client_window_id.id,
                                   transient_client_window_id.id);
}

void WindowTree::ProcessTransientWindowRemoved(
    const ServerWindow* window,
    const ServerWindow* transient_window,
    bool originated_change) {
  if (originated_change)
    return;
  ClientWindowId client_window_id, transient_client_window_id;
  if (!IsWindowKnown(window, &client_window_id) ||
      !IsWindowKnown(transient_window, &transient_client_window_id)) {
    return;
  }
  client()->OnTransientWindowRemoved(client_window_id.id,
                                     transient_client_window_id.id);
}

void WindowTree::ProcessWindowSurfaceChanged(ServerWindow* window,
                                             const cc::SurfaceId& surface_id,
                                             const gfx::Size& frame_size,
                                             float device_scale_factor) {
  ServerWindow* parent_window = window->parent();
  ClientWindowId client_window_id, parent_client_window_id;
  if (!IsWindowKnown(window, &client_window_id) ||
      !IsWindowKnown(parent_window, &parent_client_window_id) ||
      !created_window_map_.count(parent_window->id())) {
    return;
  }

  client()->OnWindowSurfaceChanged(client_window_id.id, surface_id, frame_size,
                                   device_scale_factor);
}

void WindowTree::SendToPointerWatcher(const ui::Event& event,
                                      ServerWindow* target_window) {
  if (!EventMatchesPointerWatcher(event))
    return;

  ClientWindowId client_window_id;
  // Ignore the return value from IsWindowKnown() as in the case of the client
  // not knowing the window we'll send 0, which corresponds to no window.
  IsWindowKnown(target_window, &client_window_id);
  client()->OnPointerEventObserved(ui::Event::Clone(event),
                                   client_window_id.id);
}

bool WindowTree::ShouldRouteToWindowManager(const ServerWindow* window) const {
  if (window_manager_state_)
    return false;  // We are the window manager, don't route to ourself.

  // If the client created this window, then do not route it through the WM.
  if (window->id().client_id == id_)
    return false;

  // If the client did not create the window, then it must be the root of the
  // client. If not, that means the client should not know about this window,
  // and so do not route the request to the WM.
  if (roots_.count(window) == 0)
    return false;

  // The WindowManager is attached to the root of the Display, if there isn't a
  // WindowManager attached no need to route to it.
  const WindowManagerDisplayRoot* display_root =
      GetWindowManagerDisplayRoot(window);
  if (!display_root)
    return false;

  // Route to the windowmanager if the windowmanager created the window.
  return display_root->window_manager_state()->window_tree()->id() ==
         window->id().client_id;
}

void WindowTree::ProcessCaptureChanged(const ServerWindow* new_capture,
                                       const ServerWindow* old_capture,
                                       bool originated_change) {
  ClientWindowId new_capture_window_client_id;
  ClientWindowId old_capture_window_client_id;
  const bool new_capture_window_known =
      IsWindowKnown(new_capture, &new_capture_window_client_id);
  const bool old_capture_window_known =
      IsWindowKnown(old_capture, &old_capture_window_client_id);
  if (!new_capture_window_known && !old_capture_window_known)
    return;

  if (originated_change && ((window_server_->current_operation_type() ==
                             OperationType::RELEASE_CAPTURE) ||
                            (window_server_->current_operation_type() ==
                             OperationType::SET_CAPTURE))) {
    return;
  }

  client()->OnCaptureChanged(new_capture_window_client_id.id,
                             old_capture_window_client_id.id);
}

ClientWindowId WindowTree::ClientWindowIdForWindow(
    const ServerWindow* window) const {
  auto iter = window_id_to_client_id_map_.find(window->id());
  DCHECK(iter != window_id_to_client_id_map_.end());
  return iter->second;
}

bool WindowTree::IsValidIdForNewWindow(const ClientWindowId& id) const {
  // Reserve 0 to indicate a null window.
  return client_id_to_window_id_map_.count(id) == 0u &&
         access_policy_->IsValidIdForNewWindow(id) && id != ClientWindowId();
}

WindowId WindowTree::GenerateNewWindowId() {
  // TODO(sky): deal with wrapping and uniqueness.
  return WindowId(id_, next_window_id_++);
}

bool WindowTree::CanReorderWindow(const ServerWindow* window,
                                  const ServerWindow* relative_window,
                                  mojom::OrderDirection direction) const {
  if (!window || !relative_window)
    return false;

  if (!window->parent() || window->parent() != relative_window->parent())
    return false;

  if (!access_policy_->CanReorderWindow(window, relative_window, direction))
    return false;

  const ServerWindow::Windows& children = window->parent()->children();
  const size_t child_i =
      std::find(children.begin(), children.end(), window) - children.begin();
  const size_t target_i =
      std::find(children.begin(), children.end(), relative_window) -
      children.begin();
  if ((direction == mojom::OrderDirection::ABOVE && child_i == target_i + 1) ||
      (direction == mojom::OrderDirection::BELOW && child_i + 1 == target_i)) {
    return false;
  }

  return true;
}

bool WindowTree::DeleteWindowImpl(WindowTree* source, ServerWindow* window) {
  DCHECK(window);
  DCHECK_EQ(window->id().client_id, id_);
  Operation op(source, window_server_, OperationType::DELETE_WINDOW);
  delete window;
  return true;
}

void WindowTree::GetUnknownWindowsFrom(
    const ServerWindow* window,
    std::vector<const ServerWindow*>* windows) {
  if (!access_policy_->CanGetWindowTree(window))
    return;

  // This function is called in the context of a hierarchy change when the
  // parent wasn't known. We need to tell the client about the window so that
  // it can set the parent correctly.
  windows->push_back(window);
  if (IsWindowKnown(window))
    return;

  // There are two cases where this gets hit:
  // . During init, in which case using the window id as the client id is
  //   fine.
  // . When a window is moved to a parent of a window we know about. This is
  //   only encountered for the WM or embed roots. We assume such clients want
  //   to see the real id of the window and are only created ClientWindowIds
  //   with the client_id.
  const ClientWindowId client_window_id(WindowIdToTransportId(window->id()));
  DCHECK_EQ(0u, client_id_to_window_id_map_.count(client_window_id));
  client_id_to_window_id_map_[client_window_id] = window->id();
  window_id_to_client_id_map_[window->id()] = client_window_id;
  if (!access_policy_->CanDescendIntoWindowForWindowTree(window))
    return;
  const ServerWindow::Windows& children = window->children();
  for (ServerWindow* child : children)
    GetUnknownWindowsFrom(child, windows);
}

bool WindowTree::RemoveFromMaps(const ServerWindow* window) {
  auto iter = window_id_to_client_id_map_.find(window->id());
  if (iter == window_id_to_client_id_map_.end())
    return false;

  client_id_to_window_id_map_.erase(iter->second);
  window_id_to_client_id_map_.erase(iter);
  return true;
}

void WindowTree::RemoveFromKnown(const ServerWindow* window,
                                 std::vector<ServerWindow*>* local_windows) {
  if (window->id().client_id == id_) {
    if (local_windows)
      local_windows->push_back(GetWindow(window->id()));
    return;
  }

  RemoveFromMaps(window);

  const ServerWindow::Windows& children = window->children();
  for (ServerWindow* child : children)
    RemoveFromKnown(child, local_windows);
}

void WindowTree::RemoveRoot(ServerWindow* window, RemoveRootReason reason) {
  DCHECK(roots_.count(window) > 0);
  roots_.erase(window);

  const ClientWindowId client_window_id(ClientWindowIdForWindow(window));

  // No need to do anything if we created the window.
  if (window->id().client_id == id_)
    return;

  if (reason == RemoveRootReason::EMBED) {
    client()->OnUnembed(client_window_id.id);
    client()->OnWindowDeleted(client_window_id.id);
    window_server_->OnTreeMessagedClient(id_);
  }

  // This client no longer knows about the window. Unparent any windows that
  // were parented to windows in the root.
  std::vector<ServerWindow*> local_windows;
  RemoveFromKnown(window, &local_windows);
  for (size_t i = 0; i < local_windows.size(); ++i)
    local_windows[i]->parent()->Remove(local_windows[i]);

  if (reason == RemoveRootReason::UNEMBED) {
    // Notify the owner of the window it no longer has a client embedded in it.
    // Owner is null in the case of the windowmanager unembedding itself from
    // a root.
    WindowTree* owning_tree =
        window_server_->GetTreeWithId(window->id().client_id);
    if (owning_tree) {
      DCHECK(owning_tree && owning_tree != this);
      owning_tree->client()->OnEmbeddedAppDisconnected(
          owning_tree->ClientWindowIdForWindow(window).id);
    }

    window->OnEmbeddedAppDisconnected();
  }
}

Array<mojom::WindowDataPtr> WindowTree::WindowsToWindowDatas(
    const std::vector<const ServerWindow*>& windows) {
  Array<mojom::WindowDataPtr> array(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    array[i] = WindowToWindowData(windows[i]);
  return array;
}

mojom::WindowDataPtr WindowTree::WindowToWindowData(
    const ServerWindow* window) {
  DCHECK(IsWindowKnown(window));
  const ServerWindow* parent = window->parent();
  // If the parent isn't known, it means the parent is not visible to us (not
  // in roots), and should not be sent over.
  if (parent && !IsWindowKnown(parent))
    parent = nullptr;
  mojom::WindowDataPtr window_data(mojom::WindowData::New());
  window_data->parent_id =
      parent ? ClientWindowIdForWindow(parent).id : ClientWindowId().id;
  window_data->window_id =
      window ? ClientWindowIdForWindow(window).id : ClientWindowId().id;
  window_data->bounds = window->bounds();
  window_data->properties =
      mojo::Map<String, Array<uint8_t>>::From(window->properties());
  window_data->visible = window->visible();
  return window_data;
}

void WindowTree::GetWindowTreeImpl(
    const ServerWindow* window,
    std::vector<const ServerWindow*>* windows) const {
  DCHECK(window);

  if (!access_policy_->CanGetWindowTree(window))
    return;

  windows->push_back(window);

  if (!access_policy_->CanDescendIntoWindowForWindowTree(window))
    return;

  const ServerWindow::Windows& children = window->children();
  for (ServerWindow* child : children)
    GetWindowTreeImpl(child, windows);
}

void WindowTree::NotifyDrawnStateChanged(const ServerWindow* window,
                                         bool new_drawn_value) {
  // Even though we don't know about window, it may be an ancestor of our root,
  // in which case the change may effect our roots drawn state.
  if (roots_.empty())
    return;

  for (auto* root : roots_) {
    if (window->Contains(root) && (new_drawn_value != root->IsDrawn())) {
      client()->OnWindowParentDrawnStateChanged(
          ClientWindowIdForWindow(root).id, new_drawn_value);
    }
  }
}

void WindowTree::DestroyWindows() {
  if (created_window_map_.empty())
    return;

  Operation op(this, window_server_, OperationType::DELETE_WINDOW);
  // If we get here from the destructor we're not going to get
  // ProcessWindowDeleted(). Copy the map and delete from the copy so that we
  // don't have to worry about whether |created_window_map_| changes or not.
  std::unordered_map<WindowId, ServerWindow*, WindowIdHash>
      created_window_map_copy;
  created_window_map_.swap(created_window_map_copy);
  // A sibling can be a transient parent of another window so we detach windows
  // from their transient parents to avoid double deletes.
  for (auto& pair : created_window_map_copy) {
    ServerWindow* transient_parent = pair.second->transient_parent();
    if (transient_parent)
      transient_parent->RemoveTransientWindow(pair.second);
  }

  for (auto& window_pair : created_window_map_copy)
    delete window_pair.second;
}

bool WindowTree::CanEmbed(const ClientWindowId& window_id) const {
  const ServerWindow* window = GetWindowByClientId(window_id);
  return window && access_policy_->CanEmbed(window);
}

void WindowTree::PrepareForEmbed(ServerWindow* window) {
  DCHECK(window);

  // Only allow a node to be the root for one client.
  WindowTree* existing_owner = window_server_->GetTreeWithRoot(window);

  Operation op(this, window_server_, OperationType::EMBED);
  RemoveChildrenAsPartOfEmbed(window);
  if (existing_owner) {
    // Never message the originating client.
    window_server_->OnTreeMessagedClient(id_);
    existing_owner->RemoveRoot(window, RemoveRootReason::EMBED);
  }
}

void WindowTree::RemoveChildrenAsPartOfEmbed(ServerWindow* window) {
  CHECK(window);
  while (!window->children().empty())
    window->Remove(window->children().front());
}

uint32_t WindowTree::GenerateEventAckId() {
  DCHECK(!event_ack_id_);
  // We do not want to create a sequential id for each event, because that can
  // leak some information to the client. So instead, manufacture the id
  // randomly.
  event_ack_id_ = 0x1000000 | (rand() & 0xffffff);
  return event_ack_id_;
}

void WindowTree::DispatchInputEventImpl(ServerWindow* target,
                                        const ui::Event& event) {
  GenerateEventAckId();
  WindowManagerDisplayRoot* display_root = GetWindowManagerDisplayRoot(target);
  DCHECK(display_root);
  event_source_wms_ = display_root->window_manager_state();
  // Should only get events from windows attached to a host.
  DCHECK(event_source_wms_);
  bool matched_pointer_watcher = EventMatchesPointerWatcher(event);
  client()->OnWindowInputEvent(
      event_ack_id_, ClientWindowIdForWindow(target).id,
      ui::Event::Clone(event), matched_pointer_watcher);
}

bool WindowTree::EventMatchesPointerWatcher(const ui::Event& event) const {
  if (!has_pointer_watcher_)
    return false;
  if (!event.IsPointerEvent())
    return false;
  if (pointer_watcher_want_moves_ && event.type() == ui::ET_POINTER_MOVED)
    return true;
  return event.type() == ui::ET_POINTER_DOWN ||
         event.type() == ui::ET_POINTER_UP ||
         event.type() == ui::ET_POINTER_WHEEL_CHANGED;
}

void WindowTree::NewWindow(
    uint32_t change_id,
    Id transport_window_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties) {
  std::map<std::string, std::vector<uint8_t>> properties;
  if (!transport_properties.is_null()) {
    properties =
        transport_properties.To<std::map<std::string, std::vector<uint8_t>>>();
  }
  client()->OnChangeCompleted(
      change_id, NewWindow(ClientWindowId(transport_window_id), properties));
}

void WindowTree::NewTopLevelWindow(
    uint32_t change_id,
    Id transport_window_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties) {
  // TODO(sky): rather than DCHECK, have this kill connection.
  DCHECK(!window_manager_internal_);  // Not valid for the windowmanager.
  DCHECK(!waiting_for_top_level_window_info_);
  const ClientWindowId client_window_id(transport_window_id);
  // TODO(sky): need a way for client to provide context to figure out display.
  Display* display = display_manager()->displays().empty()
                         ? nullptr
                         : *(display_manager()->displays().begin());
  // TODO(sky): move checks to accesspolicy.
  WindowManagerDisplayRoot* display_root =
      display && user_id_ != InvalidUserId()
          ? display->GetWindowManagerDisplayRootForUser(user_id_)
          : nullptr;
  if (!display_root ||
      display_root->window_manager_state()->window_tree() == this ||
      !IsValidIdForNewWindow(client_window_id)) {
    client()->OnChangeCompleted(change_id, false);
    return;
  }

  // The server creates the real window. Any further messages from the client
  // may try to alter the window. Pause incoming messages so that we know we
  // can't get a message for a window before the window is created. Once the
  // window is created we'll resume processing.
  binding_->SetIncomingMethodCallProcessingPaused(true);

  const uint32_t wm_change_id =
      window_server_->GenerateWindowManagerChangeId(this, change_id);

  waiting_for_top_level_window_info_.reset(
      new WaitingForTopLevelWindowInfo(client_window_id, wm_change_id));

  display_root->window_manager_state()
      ->window_tree()
      ->window_manager_internal_->WmCreateTopLevelWindow(
          wm_change_id, id_, std::move(transport_properties));
}

void WindowTree::DeleteWindow(uint32_t change_id, Id transport_window_id) {
  client()->OnChangeCompleted(
      change_id, DeleteWindow(ClientWindowId(transport_window_id)));
}

void WindowTree::AddWindow(uint32_t change_id, Id parent_id, Id child_id) {
  client()->OnChangeCompleted(change_id, AddWindow(ClientWindowId(parent_id),
                                                   ClientWindowId(child_id)));
}

void WindowTree::RemoveWindowFromParent(uint32_t change_id, Id window_id) {
  bool success = false;
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  if (window && window->parent() &&
      access_policy_->CanRemoveWindowFromParent(window)) {
    success = true;
    Operation op(this, window_server_,
                 OperationType::REMOVE_WINDOW_FROM_PARENT);
    window->parent()->Remove(window);
  }
  client()->OnChangeCompleted(change_id, success);
}

void WindowTree::AddTransientWindow(uint32_t change_id,
                                    Id window,
                                    Id transient_window) {
  client()->OnChangeCompleted(
      change_id, AddTransientWindow(ClientWindowId(window),
                                    ClientWindowId(transient_window)));
}

void WindowTree::RemoveTransientWindowFromParent(uint32_t change_id,
                                                 Id transient_window_id) {
  bool success = false;
  ServerWindow* transient_window =
      GetWindowByClientId(ClientWindowId(transient_window_id));
  if (transient_window && transient_window->transient_parent() &&
      access_policy_->CanRemoveTransientWindowFromParent(transient_window)) {
    success = true;
    Operation op(this, window_server_,
                 OperationType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT);
    transient_window->transient_parent()->RemoveTransientWindow(
        transient_window);
  }
  client()->OnChangeCompleted(change_id, success);
}

void WindowTree::SetModal(uint32_t change_id, Id window_id) {
  client()->OnChangeCompleted(change_id, SetModal(ClientWindowId(window_id)));
}

void WindowTree::ReorderWindow(uint32_t change_id,
                               Id window_id,
                               Id relative_window_id,
                               mojom::OrderDirection direction) {
  bool success = false;
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  ServerWindow* relative_window =
      GetWindowByClientId(ClientWindowId(relative_window_id));
  if (CanReorderWindow(window, relative_window, direction)) {
    success = true;
    Operation op(this, window_server_, OperationType::REORDER_WINDOW);
    window->Reorder(relative_window, direction);
    window_server_->ProcessWindowReorder(window, relative_window, direction);
  }
  client()->OnChangeCompleted(change_id, success);
}

void WindowTree::GetWindowTree(
    Id window_id,
    const base::Callback<void(Array<mojom::WindowDataPtr>)>& callback) {
  std::vector<const ServerWindow*> windows(
      GetWindowTree(ClientWindowId(window_id)));
  callback.Run(WindowsToWindowDatas(windows));
}

void WindowTree::SetCapture(uint32_t change_id, Id window_id) {
  client()->OnChangeCompleted(change_id, SetCapture(ClientWindowId(window_id)));
}

void WindowTree::ReleaseCapture(uint32_t change_id, Id window_id) {
  client()->OnChangeCompleted(change_id,
                              ReleaseCapture(ClientWindowId(window_id)));
}

void WindowTree::StartPointerWatcher(bool want_moves) {
  has_pointer_watcher_ = true;
  pointer_watcher_want_moves_ = want_moves;
}

void WindowTree::StopPointerWatcher() {
  has_pointer_watcher_ = false;
  pointer_watcher_want_moves_ = false;
}

void WindowTree::SetWindowBounds(uint32_t change_id,
                                 Id window_id,
                                 const gfx::Rect& bounds) {
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  if (window && ShouldRouteToWindowManager(window)) {
    const uint32_t wm_change_id =
        window_server_->GenerateWindowManagerChangeId(this, change_id);
    // |window_id| may be a client id, use the id from the window to ensure
    // the windowmanager doesn't get an id it doesn't know about.
    WindowManagerDisplayRoot* display_root =
        GetWindowManagerDisplayRoot(window);
    WindowTree* wm_tree = display_root->window_manager_state()->window_tree();
    wm_tree->window_manager_internal_->WmSetBounds(
        wm_change_id, wm_tree->ClientWindowIdForWindow(window).id,
        std::move(bounds));
    return;
  }

  // Only the owner of the window can change the bounds.
  bool success = window && access_policy_->CanSetWindowBounds(window);
  if (success) {
    Operation op(this, window_server_, OperationType::SET_WINDOW_BOUNDS);
    window->SetBounds(bounds);
  }
  client()->OnChangeCompleted(change_id, success);
}

void WindowTree::SetWindowVisibility(uint32_t change_id,
                                     Id transport_window_id,
                                     bool visible) {
  client()->OnChangeCompleted(
      change_id,
      SetWindowVisibility(ClientWindowId(transport_window_id), visible));
}

void WindowTree::SetWindowProperty(uint32_t change_id,
                                   Id transport_window_id,
                                   const mojo::String& name,
                                   mojo::Array<uint8_t> value) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  if (window && ShouldRouteToWindowManager(window)) {
    const uint32_t wm_change_id =
        window_server_->GenerateWindowManagerChangeId(this, change_id);
    WindowManagerDisplayRoot* display_root =
        GetWindowManagerDisplayRoot(window);
    WindowTree* wm_tree = display_root->window_manager_state()->window_tree();
    wm_tree->window_manager_internal_->WmSetProperty(
        wm_change_id, wm_tree->ClientWindowIdForWindow(window).id, name,
        std::move(value));
    return;
  }
  const bool success = window && access_policy_->CanSetWindowProperties(window);
  if (success) {
    Operation op(this, window_server_, OperationType::SET_WINDOW_PROPERTY);
    if (value.is_null()) {
      window->SetProperty(name, nullptr);
    } else {
      std::vector<uint8_t> data = value.To<std::vector<uint8_t>>();
      window->SetProperty(name, &data);
    }
  }
  client()->OnChangeCompleted(change_id, success);
}

void WindowTree::SetWindowOpacity(uint32_t change_id,
                                  Id window_id,
                                  float opacity) {
  client()->OnChangeCompleted(
      change_id, SetWindowOpacity(ClientWindowId(window_id), opacity));
}

void WindowTree::AttachCompositorFrameSink(
    Id transport_window_id,
    mojom::CompositorFrameSinkType type,
    cc::mojom::MojoCompositorFrameSinkRequest compositor_frame_sink,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  const bool success =
      window && access_policy_->CanSetWindowCompositorFrameSink(window, type);
  if (!success) {
    DVLOG(1) << "request to AttachCompositorFrameSink failed";
    return;
  }
  window->CreateCompositorFrameSink(type, gfx::kNullAcceleratedWidget, nullptr,
                                    nullptr, std::move(compositor_frame_sink),
                                    std::move(client));
}

void WindowTree::SetWindowTextInputState(Id transport_window_id,
                                         mojo::TextInputStatePtr state) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  bool success = window && access_policy_->CanSetWindowTextInputState(window);
  if (success)
    window->SetTextInputState(state.To<ui::TextInputState>());
}

void WindowTree::SetImeVisibility(Id transport_window_id,
                                  bool visible,
                                  mojo::TextInputStatePtr state) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  bool success = window && access_policy_->CanSetWindowTextInputState(window);
  if (success) {
    if (!state.is_null())
      window->SetTextInputState(state.To<ui::TextInputState>());

    Display* display = GetDisplay(window);
    if (display)
      display->SetImeVisibility(window, visible);
  }
}

void WindowTree::OnWindowInputEventAck(uint32_t event_id,
                                       mojom::EventResult result) {
  if (event_ack_id_ == 0 || event_id != event_ack_id_) {
    // TODO(sad): Something bad happened. Kill the client?
    NOTIMPLEMENTED() << ": Wrong event acked. event_id=" << event_id
                     << ", event_ack_id_=" << event_ack_id_;
    DVLOG(1) << "OnWindowInputEventAck supplied unexpected event_id";
  }
  event_ack_id_ = 0;

  if (janky_)
    event_source_wms_->window_tree()->ClientJankinessChanged(this);

  WindowManagerState* event_source_wms = event_source_wms_;
  event_source_wms_ = nullptr;
  if (event_source_wms)
    event_source_wms->OnEventAck(this, result);

  if (!event_queue_.empty()) {
    DCHECK(!event_ack_id_);
    ServerWindow* target = nullptr;
    std::unique_ptr<ui::Event> event;
    do {
      std::unique_ptr<TargetedEvent> targeted_event =
          std::move(event_queue_.front());
      event_queue_.pop();
      target = targeted_event->target();
      event = targeted_event->TakeEvent();
    } while (!event_queue_.empty() && !GetDisplay(target));
    if (target)
      DispatchInputEventImpl(target, *event);
  }
}

void WindowTree::SetClientArea(
    Id transport_window_id,
    const gfx::Insets& insets,
    mojo::Array<gfx::Rect> transport_additional_client_areas) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  if (!window || !access_policy_->CanSetClientArea(window))
    return;

  std::vector<gfx::Rect> additional_client_areas =
      transport_additional_client_areas.To<std::vector<gfx::Rect>>();
  window->SetClientArea(insets, additional_client_areas);
}

void WindowTree::SetHitTestMask(Id transport_window_id,
                                const base::Optional<gfx::Rect>& mask) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  if (!window || !access_policy_->CanSetHitTestMask(window)) {
    DVLOG(1) << "SetHitTestMask failed";
    return;
  }

  if (mask)
    window->SetHitTestMask(*mask);
  else
    window->ClearHitTestMask();
}

void WindowTree::SetCanAcceptDrops(Id window_id, bool accepts_drops) {
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  if (!window || !access_policy_->CanSetAcceptDrops(window)) {
    DVLOG(1) << "SetAcceptsDrops failed";
    return;
  }

  window->SetCanAcceptDrops(accepts_drops);
}

void WindowTree::Embed(Id transport_window_id,
                       mojom::WindowTreeClientPtr client,
                       uint32_t flags,
                       const EmbedCallback& callback) {
  callback.Run(
      Embed(ClientWindowId(transport_window_id), std::move(client), flags));
}

void WindowTree::SetFocus(uint32_t change_id, Id transport_window_id) {
  client()->OnChangeCompleted(change_id,
                              SetFocus(ClientWindowId(transport_window_id)));
}

void WindowTree::SetCanFocus(Id transport_window_id, bool can_focus) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  // TODO(sky): there should be an else case (it shouldn't route to wm and
  // policy allows, then set_can_focus).
  if (window && ShouldRouteToWindowManager(window))
    window->set_can_focus(can_focus);
}

void WindowTree::SetCanAcceptEvents(Id transport_window_id,
                                    bool can_accept_events) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  // TODO(riajiang): check |event_queue_| is empty for |window|.
  if (window && access_policy_->CanSetAcceptEvents(window))
    window->set_can_accept_events(can_accept_events);
}

void WindowTree::SetPredefinedCursor(uint32_t change_id,
                                     Id transport_window_id,
                                     ui::mojom::Cursor cursor_id) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));

  // Only the owner of the window can change the bounds.
  bool success = window && access_policy_->CanSetCursorProperties(window);
  if (success) {
    Operation op(this, window_server_,
                 OperationType::SET_WINDOW_PREDEFINED_CURSOR);
    window->SetPredefinedCursor(cursor_id);
  }
  client()->OnChangeCompleted(change_id, success);
}

void WindowTree::GetWindowManagerClient(
    mojo::AssociatedInterfaceRequest<mojom::WindowManagerClient> internal) {
  if (!access_policy_->CanSetWindowManager() || !window_manager_internal_ ||
      window_manager_internal_client_binding_) {
    return;
  }
  window_manager_internal_client_binding_.reset(
      new mojo::AssociatedBinding<mojom::WindowManagerClient>(
          this, std::move(internal)));
}

void WindowTree::GetCursorLocationMemory(
    const GetCursorLocationMemoryCallback& callback) {
  callback.Run(
      window_server_->display_manager()->GetUserDisplayManager(user_id_)->
      GetCursorLocationMemory());
}

void WindowTree::PerformDragDrop(
    uint32_t change_id,
    Id source_window_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> drag_data,
    uint32_t drag_operation) {
  ServerWindow* window = GetWindowByClientId(ClientWindowId(source_window_id));
  bool success = window && access_policy_->CanInitiateDragLoop(window);
  if (!success || !ShouldRouteToWindowManager(window)) {
    // We need to fail this move loop change, otherwise the client will just be
    // waiting for |change_id|.
    DVLOG(1) << "PerformDragDrop failed (access denied).";
    OnChangeCompleted(change_id, false);
    return;
  }

  WindowManagerDisplayRoot* display_root = GetWindowManagerDisplayRoot(window);
  if (!display_root) {
    // The window isn't parented. There's nothing to do.
    DVLOG(1) << "PerformDragDrop failed (window unparented).";
    OnChangeCompleted(change_id, false);
    return;
  }

  if (window_server_->in_move_loop() || window_server_->in_drag_loop()) {
    // Either the window manager is servicing a window drag or we're servicing
    // a drag and drop operation. We can't start a second drag.
    DVLOG(1) << "PerformDragDrop failed (already performing a drag).";
    OnChangeCompleted(change_id, false);
    return;
  }

  // TODO(erg): Dealing with |drag_representation| is hard, so we're going to
  // deal with that later.

  // Here, we need to dramatically change how the mouse pointer works. Once
  // we've started a drag drop operation, cursor events don't go to windows as
  // normal.
  WindowManagerState* wms = display_root->window_manager_state();
  window_server_->StartDragLoop(change_id, window, this);
  wms->SetDragDropSourceWindow(this, window, this, std::move(drag_data),
                               drag_operation);
}

void WindowTree::CancelDragDrop(Id window_id) {
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  if (!window) {
    DVLOG(1) << "CancelDragDrop failed (no window)";
    return;
  }

  if (window != window_server_->GetCurrentDragLoopWindow()) {
    DVLOG(1) << "CancelDragDrop failed (not the drag loop window)";
    return;
  }

  if (window_server_->GetCurrentDragLoopInitiator() != this) {
    DVLOG(1) << "CancelDragDrop failed (not the drag initiator)";
    return;
  }

  WindowManagerDisplayRoot* display_root = GetWindowManagerDisplayRoot(window);
  if (!display_root) {
    DVLOG(1) << "CancelDragDrop failed (no such window manager display root)";
    return;
  }

  WindowManagerState* wms = display_root->window_manager_state();
  wms->CancelDragDrop();
}

void WindowTree::PerformWindowMove(uint32_t change_id,
                                   Id window_id,
                                   ui::mojom::MoveLoopSource source,
                                   const gfx::Point& cursor) {
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  bool success = window && access_policy_->CanInitiateMoveLoop(window);
  if (!success || !ShouldRouteToWindowManager(window)) {
    // We need to fail this move loop change, otherwise the client will just be
    // waiting for |change_id|.
    DVLOG(1) << "PerformWindowMove failed (access denied).";
    OnChangeCompleted(change_id, false);
    return;
  }

  WindowManagerDisplayRoot* display_root = GetWindowManagerDisplayRoot(window);
  if (!display_root) {
    // The window isn't parented. There's nothing to do.
    DVLOG(1) << "PerformWindowMove failed (window unparented).";
    OnChangeCompleted(change_id, false);
    return;
  }

  if (window_server_->in_move_loop() || window_server_->in_drag_loop()) {
    // Either the window manager is servicing a window drag or we're servicing
    // a drag and drop operation. We can't start a second drag.
    DVLOG(1) << "PerformWindowMove failed (already performing a drag).";
    OnChangeCompleted(change_id, false);
    return;
  }

  // When we perform a window move loop, we give the window manager non client
  // capture. Because of how the capture public interface currently works,
  // SetCapture() will check whether the mouse cursor is currently in the
  // non-client area and if so, will redirect messages to the window
  // manager. (And normal window movement relies on this behaviour.)
  WindowManagerState* wms = display_root->window_manager_state();
  wms->SetCapture(window, wms->window_tree()->id());

  const uint32_t wm_change_id =
      window_server_->GenerateWindowManagerChangeId(this, change_id);
  window_server_->StartMoveLoop(wm_change_id, window, this, window->bounds());
  wms->window_tree()->window_manager_internal_->WmPerformMoveLoop(
      wm_change_id, wms->window_tree()->ClientWindowIdForWindow(window).id,
      source, cursor);
}

void WindowTree::CancelWindowMove(Id window_id) {
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  bool success = window && access_policy_->CanInitiateMoveLoop(window);
  if (!success) {
    DVLOG(1) << "CancelWindowMove failed (no window / access denied)";
    return;
  }

  if (window != window_server_->GetCurrentMoveLoopWindow()) {
    DVLOG(1) << "CancelWindowMove failed (not the move loop window)";
    return;
  }

  if (window_server_->GetCurrentMoveLoopInitiator() != this) {
    DVLOG(1) << "CancelWindowMove failed (not the move loop initiator)";
    return;
  }

  WindowManagerDisplayRoot* display_root = GetWindowManagerDisplayRoot(window);
  if (!display_root) {
    DVLOG(1) << "CancelWindowMove failed (no such window manager display root)";
    return;
  }

  WindowManagerState* wms = display_root->window_manager_state();
  wms->window_tree()->window_manager_internal_->WmCancelMoveLoop(
      window_server_->GetCurrentMoveLoopChangeId());
}

void WindowTree::AddAccelerator(uint32_t id,
                                mojom::EventMatcherPtr event_matcher,
                                const AddAcceleratorCallback& callback) {
  DCHECK(window_manager_state_);
  const bool success =
      window_manager_state_->event_dispatcher()->AddAccelerator(
          id, std::move(event_matcher));
  callback.Run(success);
}

void WindowTree::RemoveAccelerator(uint32_t id) {
  window_manager_state_->event_dispatcher()->RemoveAccelerator(id);
}

void WindowTree::AddActivationParent(Id transport_window_id) {
  AddActivationParent(ClientWindowId(transport_window_id));
}

void WindowTree::RemoveActivationParent(Id transport_window_id) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  if (window) {
    Display* display = GetDisplay(window);
    if (display)
      display->RemoveActivationParent(window);
    else
      DVLOG(1) << "RemoveActivationParent window not associated with display";
  } else {
    DVLOG(1) << "RemoveActivationParent supplied invalid window id";
  }
}

void WindowTree::ActivateNextWindow() {
  DCHECK(window_manager_state_);
  if (window_server_->user_id_tracker()->active_id() != user_id_)
    return;

  ServerWindow* focused_window = window_server_->GetFocusedWindow();
  if (focused_window) {
    WindowManagerDisplayRoot* display_root =
        GetWindowManagerDisplayRoot(focused_window);
    if (display_root->window_manager_state() != window_manager_state_.get()) {
      // We aren't active.
      return;
    }
    display_root->display()->ActivateNextWindow();
    return;
  }
  // Use the first display.
  std::set<Display*> displays = window_server_->display_manager()->displays();
  if (displays.empty())
    return;

  (*displays.begin())->ActivateNextWindow();
}

void WindowTree::SetUnderlaySurfaceOffsetAndExtendedHitArea(
    Id window_id,
    int32_t x_offset,
    int32_t y_offset,
    const gfx::Insets& hit_area) {
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  if (!window)
    return;

  window->SetUnderlayOffset(gfx::Vector2d(x_offset, y_offset));
  window->set_extended_hit_test_region(hit_area);
}

void WindowTree::WmResponse(uint32_t change_id, bool response) {
  if (window_server_->in_move_loop() &&
      window_server_->GetCurrentMoveLoopChangeId() == change_id) {
    ServerWindow* window = window_server_->GetCurrentMoveLoopWindow();

    if (window->id().client_id != id_) {
      window_server_->WindowManagerSentBogusMessage();
      window = nullptr;
    } else {
      WindowManagerDisplayRoot* display_root =
          GetWindowManagerDisplayRoot(window);
      if (display_root) {
        WindowManagerState* wms = display_root->window_manager_state();
        // Clear the implicit capture.
        wms->SetCapture(nullptr, false);
      }
    }

    if (!response && window) {
      // Our move loop didn't succeed, which means that we must restore the
      // original bounds of the window.
      window->SetBounds(window_server_->GetCurrentMoveLoopRevertBounds());
    }

    window_server_->EndMoveLoop();
  }

  window_server_->WindowManagerChangeCompleted(change_id, response);
}

void WindowTree::WmRequestClose(Id transport_window_id) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  WindowTree* tree = window_server_->GetTreeWithRoot(window);
  if (tree && tree != this) {
    tree->client()->RequestClose(tree->ClientWindowIdForWindow(window).id);
  }
  // TODO(sky): think about what else case means.
}

void WindowTree::WmSetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  DCHECK(window_manager_state_);
  window_manager_state_->SetFrameDecorationValues(std::move(values));
}

void WindowTree::WmSetNonClientCursor(uint32_t window_id,
                                      mojom::Cursor cursor_id) {
  DCHECK(window_manager_state_);
  ServerWindow* window = GetWindowByClientId(ClientWindowId(window_id));
  if (window) {
    window->SetNonClientCursor(cursor_id);
  } else {
    DVLOG(1) << "trying to update non-client cursor of invalid window";
  }
}

void WindowTree::OnWmCreatedTopLevelWindow(uint32_t change_id,
                                           Id transport_window_id) {
  ServerWindow* window =
      GetWindowByClientId(ClientWindowId(transport_window_id));
  if (window && window->id().client_id != id_) {
    DVLOG(1) << "OnWmCreatedTopLevelWindow supplied invalid window id";
    window_server_->WindowManagerSentBogusMessage();
    window = nullptr;
  }
  window_server_->WindowManagerCreatedTopLevelWindow(this, change_id, window);
}

void WindowTree::OnAcceleratorAck(uint32_t event_id,
                                  mojom::EventResult result) {
  if (event_ack_id_ == 0 || event_id != event_ack_id_) {
    DVLOG(1) << "OnAcceleratorAck supplied invalid event_id";
    window_server_->WindowManagerSentBogusMessage();
    return;
  }
  event_ack_id_ = 0;
  DCHECK(window_manager_state_);
  window_manager_state_->OnAcceleratorAck(result);
}

bool WindowTree::HasRootForAccessPolicy(const ServerWindow* window) const {
  return HasRoot(window);
}

bool WindowTree::IsWindowKnownForAccessPolicy(
    const ServerWindow* window) const {
  return IsWindowKnown(window);
}

bool WindowTree::IsWindowRootOfAnotherTreeForAccessPolicy(
    const ServerWindow* window) const {
  WindowTree* tree = window_server_->GetTreeWithRoot(window);
  return tree && tree != this;
}

void WindowTree::OnDragCompleted(bool success, uint32_t action_taken) {
  DCHECK(window_server_->in_drag_loop());

  if (window_server_->GetCurrentDragLoopInitiator() != this)
    return;

  uint32_t change_id = window_server_->GetCurrentDragLoopChangeId();
  ServerWindow* window = window_server_->GetCurrentDragLoopWindow();
  WindowManagerDisplayRoot* display_root = GetWindowManagerDisplayRoot(window);
  if (!display_root)
    return;

  window_server_->EndDragLoop();
  WindowManagerState* wms = display_root->window_manager_state();
  wms->EndDragDrop();

  client()->OnPerformDragDropCompleted(change_id, success, action_taken);
}

ServerWindow* WindowTree::GetWindowById(const WindowId& id) {
  return GetWindow(id);
}

DragTargetConnection* WindowTree::GetDragTargetForWindow(
    const ServerWindow* window) {
  if (!window)
    return nullptr;
  DragTargetConnection* connection = window_server_->GetTreeWithRoot(window);
  if (connection)
    return connection;
  return window_server_->GetTreeWithId(window->id().client_id);
}

void WindowTree::PerformOnDragDropStart(
    mojo::Map<mojo::String, mojo::Array<uint8_t>> mime_data) {
  client()->OnDragDropStart(std::move(mime_data));
}

void WindowTree::PerformOnDragEnter(
    const ServerWindow* window,
    uint32_t event_flags,
    const gfx::Point& cursor_offset,
    uint32_t effect_bitmask,
    const base::Callback<void(uint32_t)>& callback) {
  ClientWindowId client_window_id;
  if (!IsWindowKnown(window, &client_window_id)) {
    NOTREACHED();
    callback.Run(0);
    return;
  }
  client()->OnDragEnter(client_window_id.id, event_flags, cursor_offset,
                        effect_bitmask, callback);
}

void WindowTree::PerformOnDragOver(
    const ServerWindow* window,
    uint32_t event_flags,
    const gfx::Point& cursor_offset,
    uint32_t effect_bitmask,
    const base::Callback<void(uint32_t)>& callback) {
  ClientWindowId client_window_id;
  if (!IsWindowKnown(window, &client_window_id)) {
    NOTREACHED();
    callback.Run(0);
    return;
  }
  client()->OnDragOver(client_window_id.id, event_flags, cursor_offset,
                       effect_bitmask, callback);
}

void WindowTree::PerformOnDragLeave(const ServerWindow* window) {
  ClientWindowId client_window_id;
  if (!IsWindowKnown(window, &client_window_id)) {
    NOTREACHED();
    return;
  }
  client()->OnDragLeave(client_window_id.id);
}

void WindowTree::PerformOnCompleteDrop(
    const ServerWindow* window,
    uint32_t event_flags,
    const gfx::Point& cursor_offset,
    uint32_t effect_bitmask,
    const base::Callback<void(uint32_t)>& callback) {
  ClientWindowId client_window_id;
  if (!IsWindowKnown(window, &client_window_id)) {
    NOTREACHED();
    callback.Run(0);
    return;
  }
  client()->OnCompleteDrop(client_window_id.id, event_flags, cursor_offset,
                           effect_bitmask, callback);
}

void WindowTree::PerformOnDragDropDone() {
  client()->OnDragDropDone();
}

}  // namespace ws
}  // namespace ui
