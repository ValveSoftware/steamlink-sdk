// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_server.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/operation.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/user_activity_monitor.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "components/mus/ws/window_manager_access_policy.h"
#include "components/mus/ws/window_manager_display_root.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_manager_window_tree_factory.h"
#include "components/mus/ws/window_server_delegate.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"
#include "services/shell/public/cpp/connection.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace mus {
namespace ws {

WindowServer::WindowServer(
    WindowServerDelegate* delegate,
    const scoped_refptr<mus::SurfacesState>& surfaces_state)
    : delegate_(delegate),
      surfaces_state_(surfaces_state),
      next_client_id_(1),
      display_manager_(new DisplayManager(this, &user_id_tracker_)),
      current_operation_(nullptr),
      in_destructor_(false),
      next_wm_change_id_(0),
      window_manager_window_tree_factory_set_(this, &user_id_tracker_) {
  user_id_tracker_.AddObserver(this);
  OnUserIdAdded(user_id_tracker_.active_id());
}

WindowServer::~WindowServer() {
  in_destructor_ = true;

  // Destroys the window trees results in querying for the display. Tear down
  // the displays first so that the trees are notified of the display going
  // away while the display is still valid.
  display_manager_->DestroyAllDisplays();

  while (!tree_map_.empty())
    DestroyTree(tree_map_.begin()->second.get());

  display_manager_.reset();
}

ServerWindow* WindowServer::CreateServerWindow(
    const WindowId& id,
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  ServerWindow* window = new ServerWindow(this, id, properties);
  window->AddObserver(this);
  return window;
}

ClientSpecificId WindowServer::GetAndAdvanceNextClientId() {
  const ClientSpecificId id = next_client_id_++;
  DCHECK_LT(id, next_client_id_);
  return id;
}

WindowTree* WindowServer::EmbedAtWindow(
    ServerWindow* root,
    const UserId& user_id,
    mojom::WindowTreeClientPtr client,
    uint32_t flags,
    std::unique_ptr<AccessPolicy> access_policy) {
  std::unique_ptr<WindowTree> tree_ptr(
      new WindowTree(this, user_id, root, std::move(access_policy)));
  WindowTree* tree = tree_ptr.get();
  if (flags & mojom::kEmbedFlagEmbedderInterceptsEvents)
    tree->set_embedder_intercepts_events();

  mojom::WindowTreePtr window_tree_ptr;
  mojom::WindowTreeRequest window_tree_request = GetProxy(&window_tree_ptr);
  std::unique_ptr<WindowTreeBinding> binding =
      delegate_->CreateWindowTreeBinding(
          WindowServerDelegate::BindingType::EMBED, this, tree,
          &window_tree_request, &client);
  if (!binding) {
    binding.reset(new ws::DefaultWindowTreeBinding(
        tree, this, std::move(window_tree_request), std::move(client)));
  }

  AddTree(std::move(tree_ptr), std::move(binding), std::move(window_tree_ptr));
  OnTreeMessagedClient(tree->id());
  return tree;
}

void WindowServer::AddTree(std::unique_ptr<WindowTree> tree_impl_ptr,
                           std::unique_ptr<WindowTreeBinding> binding,
                           mojom::WindowTreePtr tree_ptr) {
  CHECK_EQ(0u, tree_map_.count(tree_impl_ptr->id()));
  WindowTree* tree = tree_impl_ptr.get();
  tree_map_[tree->id()] = std::move(tree_impl_ptr);
  tree->Init(std::move(binding), std::move(tree_ptr));
}

WindowTree* WindowServer::CreateTreeForWindowManager(
    const UserId& user_id,
    mojom::WindowTreeRequest window_tree_request,
    mojom::WindowTreeClientPtr window_tree_client) {
  std::unique_ptr<WindowTree> window_tree(new WindowTree(
      this, user_id, nullptr, base::WrapUnique(new WindowManagerAccessPolicy)));
  std::unique_ptr<WindowTreeBinding> window_tree_binding =
      delegate_->CreateWindowTreeBinding(
          WindowServerDelegate::BindingType::WINDOW_MANAGER, this,
          window_tree.get(), &window_tree_request, &window_tree_client);
  if (!window_tree_binding) {
    window_tree_binding.reset(new DefaultWindowTreeBinding(
        window_tree.get(), this, std::move(window_tree_request),
        std::move(window_tree_client)));
  }
  WindowTree* window_tree_ptr = window_tree.get();
  AddTree(std::move(window_tree), std::move(window_tree_binding), nullptr);
  window_tree_ptr->ConfigureWindowManager();
  return window_tree_ptr;
}

void WindowServer::DestroyTree(WindowTree* tree) {
  std::unique_ptr<WindowTree> tree_ptr;
  {
    auto iter = tree_map_.find(tree->id());
    DCHECK(iter != tree_map_.end());
    tree_ptr = std::move(iter->second);
    tree_map_.erase(iter);
  }

  // Notify remaining connections so that they can cleanup.
  for (auto& pair : tree_map_)
    pair.second->OnWindowDestroyingTreeImpl(tree);

  // Notify the hosts, taking care to only notify each host once.
  std::set<Display*> displays_notified;
  for (auto* root : tree->roots()) {
    // WindowTree holds its roots as a const, which is right as WindowTree
    // doesn't need to modify the window. OTOH we do. We could look up the
    // window using the id to get non-const version, but instead we cast.
    Display* display =
        display_manager_->GetDisplayContaining(const_cast<ServerWindow*>(root));
    if (display && displays_notified.count(display) == 0) {
      display->OnWillDestroyTree(tree);
      displays_notified.insert(display);
    }
  }

  window_manager_window_tree_factory_set_.DeleteFactoryAssociatedWithTree(tree);

  // Remove any requests from the client that resulted in a call to the window
  // manager and we haven't gotten a response back yet.
  std::set<uint32_t> to_remove;
  for (auto& pair : in_flight_wm_change_map_) {
    if (pair.second.client_id == tree->id())
      to_remove.insert(pair.first);
  }
  for (uint32_t id : to_remove)
    in_flight_wm_change_map_.erase(id);
}

WindowTree* WindowServer::GetTreeWithId(ClientSpecificId client_id) {
  auto iter = tree_map_.find(client_id);
  return iter == tree_map_.end() ? nullptr : iter->second.get();
}

WindowTree* WindowServer::GetTreeWithClientName(
    const std::string& client_name) {
  for (const auto& entry : tree_map_) {
    if (entry.second->name() == client_name)
      return entry.second.get();
  }
  return nullptr;
}

ServerWindow* WindowServer::GetWindow(const WindowId& id) {
  // kInvalidClientId is used for Display and WindowManager nodes.
  if (id.client_id == kInvalidClientId) {
    for (Display* display : display_manager_->displays()) {
      ServerWindow* window = display->GetRootWithId(id);
      if (window)
        return window;
    }
  }
  WindowTree* tree = GetTreeWithId(id.client_id);
  return tree ? tree->GetWindow(id) : nullptr;
}

void WindowServer::SchedulePaint(ServerWindow* window,
                                 const gfx::Rect& bounds) {
  Display* display = display_manager_->GetDisplayContaining(window);
  if (display)
    display->SchedulePaint(window, bounds);
}

void WindowServer::OnTreeMessagedClient(ClientSpecificId id) {
  if (current_operation_)
    current_operation_->MarkTreeAsMessaged(id);
}

bool WindowServer::DidTreeMessageClient(ClientSpecificId id) const {
  return current_operation_ && current_operation_->DidMessageTree(id);
}

const WindowTree* WindowServer::GetTreeWithRoot(
    const ServerWindow* window) const {
  if (!window)
    return nullptr;
  for (auto& pair : tree_map_) {
    if (pair.second->HasRoot(window))
      return pair.second.get();
  }
  return nullptr;
}

void WindowServer::OnFirstWindowManagerWindowTreeFactoryReady() {
  if (display_manager_->has_active_or_pending_displays())
    return;

  // We've been supplied a WindowManagerFactory and no displays have been
  // created yet. Treat this as a signal to create a Display.
  // TODO(sky): we need a better way to determine this, most likely a switch.
  delegate_->CreateDefaultDisplays();
}

UserActivityMonitor* WindowServer::GetUserActivityMonitorForUser(
    const UserId& user_id) {
  DCHECK_GT(activity_monitor_map_.count(user_id), 0u);
  return activity_monitor_map_[user_id].get();
}

bool WindowServer::SetFocusedWindow(ServerWindow* window) {
  // TODO(sky): this should fail if there is modal dialog active and |window|
  // is outside that.
  ServerWindow* currently_focused = GetFocusedWindow();
  Display* focused_display =
      currently_focused
          ? display_manager_->GetDisplayContaining(currently_focused)
          : nullptr;
  if (!window)
    return focused_display ? focused_display->SetFocusedWindow(nullptr) : true;

  Display* display = display_manager_->GetDisplayContaining(window);
  DCHECK(display);  // It's assumed callers do validation before calling this.
  const bool result = display->SetFocusedWindow(window);
  // If the focus actually changed, and focus was in another display, then we
  // need to notify the previously focused display so that it cleans up state
  // and notifies appropriately.
  if (result && display->GetFocusedWindow() && display != focused_display &&
      focused_display) {
    const bool cleared_focus = focused_display->SetFocusedWindow(nullptr);
    DCHECK(cleared_focus);
  }
  return result;
}

ServerWindow* WindowServer::GetFocusedWindow() {
  for (Display* display : display_manager_->displays()) {
    ServerWindow* focused_window = display->GetFocusedWindow();
    if (focused_window)
      return focused_window;
  }
  return nullptr;
}

uint32_t WindowServer::GenerateWindowManagerChangeId(
    WindowTree* source,
    uint32_t client_change_id) {
  const uint32_t wm_change_id = next_wm_change_id_++;
  in_flight_wm_change_map_[wm_change_id] = {source->id(), client_change_id};
  return wm_change_id;
}

void WindowServer::WindowManagerChangeCompleted(
    uint32_t window_manager_change_id,
    bool success) {
  InFlightWindowManagerChange change;
  if (!GetAndClearInFlightWindowManagerChange(window_manager_change_id,
                                              &change)) {
    return;
  }

  WindowTree* tree = GetTreeWithId(change.client_id);
  tree->OnChangeCompleted(change.client_change_id, success);
}

void WindowServer::WindowManagerCreatedTopLevelWindow(
    WindowTree* wm_tree,
    uint32_t window_manager_change_id,
    const ServerWindow* window) {
  InFlightWindowManagerChange change;
  if (!GetAndClearInFlightWindowManagerChange(window_manager_change_id,
                                              &change)) {
    return;
  }
  if (!window) {
    WindowManagerSentBogusMessage();
    return;
  }

  WindowTree* tree = GetTreeWithId(change.client_id);
  // The window manager should have created the window already, and it should
  // be ready for embedding.
  if (!tree->IsWaitingForNewTopLevelWindow(window_manager_change_id) ||
      !window || window->id().client_id != wm_tree->id() ||
      !window->children().empty() || GetTreeWithRoot(window)) {
    WindowManagerSentBogusMessage();
    return;
  }

  tree->OnWindowManagerCreatedTopLevelWindow(window_manager_change_id,
                                             change.client_change_id, window);
}

void WindowServer::ProcessWindowBoundsChanged(const ServerWindow* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowBoundsChanged(window, old_bounds, new_bounds,
                                            IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessClientAreaChanged(
    const ServerWindow* window,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessClientAreaChanged(window, new_client_area,
                                          new_additional_client_areas,
                                          IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessLostCapture(const ServerWindow* window) {
  for (auto& pair : tree_map_)
    pair.second->ProcessLostCapture(window, IsOperationSource(pair.first));
}

void WindowServer::ProcessWillChangeWindowHierarchy(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWillChangeWindowHierarchy(
        window, new_parent, old_parent, IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessWindowHierarchyChanged(
    const ServerWindow* window,
    const ServerWindow* new_parent,
    const ServerWindow* old_parent) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowHierarchyChanged(window, new_parent, old_parent,
                                               IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessWindowReorder(const ServerWindow* window,
                                        const ServerWindow* relative_window,
                                        const mojom::OrderDirection direction) {
  // We'll probably do a bit of reshuffling when we add a transient window.
  if ((current_operation_type() == OperationType::ADD_TRANSIENT_WINDOW) ||
      (current_operation_type() ==
       OperationType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT)) {
    return;
  }
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowReorder(window, relative_window, direction,
                                      IsOperationSource(pair.first));
  }
}

void WindowServer::ProcessWindowDeleted(const ServerWindow* window) {
  for (auto& pair : tree_map_)
    pair.second->ProcessWindowDeleted(window, IsOperationSource(pair.first));
}

void WindowServer::ProcessWillChangeWindowPredefinedCursor(ServerWindow* window,
                                                           int32_t cursor_id) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessCursorChanged(window, cursor_id,
                                      IsOperationSource(pair.first));
  }
}

void WindowServer::SendToEventObservers(const ui::Event& event,
                                        const UserId& user_id,
                                        WindowTree* ignore_tree) {
  for (auto& pair : tree_map_) {
    WindowTree* tree = pair.second.get();
    if (tree->user_id() == user_id && tree != ignore_tree)
      tree->SendToEventObserver(event);
  }
}

void WindowServer::SetPaintCallback(
    const base::Callback<void(ServerWindow*)>& callback) {
  DCHECK(delegate_->IsTestConfig()) << "Paint callbacks are expensive, and "
                                    << "allowed only in tests.";
  DCHECK(window_paint_callback_.is_null() || callback.is_null());
  window_paint_callback_ = callback;
}

bool WindowServer::GetAndClearInFlightWindowManagerChange(
    uint32_t window_manager_change_id,
    InFlightWindowManagerChange* change) {
  // There are valid reasons as to why we wouldn't know about the id. The
  // most likely is the client disconnected before the response from the window
  // manager came back.
  auto iter = in_flight_wm_change_map_.find(window_manager_change_id);
  if (iter == in_flight_wm_change_map_.end())
    return false;

  *change = iter->second;
  in_flight_wm_change_map_.erase(iter);
  return true;
}

void WindowServer::PrepareForOperation(Operation* op) {
  // Should only ever have one change in flight.
  CHECK(!current_operation_);
  current_operation_ = op;
}

void WindowServer::FinishOperation() {
  // PrepareForOperation/FinishOperation should be balanced.
  CHECK(current_operation_);
  current_operation_ = nullptr;
}

void WindowServer::UpdateNativeCursorFromMouseLocation(ServerWindow* window) {
  WindowManagerDisplayRoot* display_root =
      display_manager_->GetWindowManagerDisplayRoot(window);
  if (display_root) {
    EventDispatcher* event_dispatcher =
        display_root->window_manager_state()->event_dispatcher();
    event_dispatcher->UpdateCursorProviderByLastKnownLocation();
    int32_t cursor_id = 0;
    if (event_dispatcher->GetCurrentMouseCursor(&cursor_id))
      display_root->display()->UpdateNativeCursor(cursor_id);
  }
}

void WindowServer::UpdateNativeCursorIfOver(ServerWindow* window) {
  WindowManagerDisplayRoot* display_root =
      display_manager_->GetWindowManagerDisplayRoot(window);
  if (!display_root)
    return;

  EventDispatcher* event_dispatcher =
      display_root->window_manager_state()->event_dispatcher();
  if (window != event_dispatcher->mouse_cursor_source_window())
    return;

  event_dispatcher->UpdateNonClientAreaForCurrentWindow();
  int32_t cursor_id = 0;
  if (event_dispatcher->GetCurrentMouseCursor(&cursor_id))
    display_root->display()->UpdateNativeCursor(cursor_id);
}

mus::SurfacesState* WindowServer::GetSurfacesState() {
  return surfaces_state_.get();
}

void WindowServer::OnScheduleWindowPaint(ServerWindow* window) {
  if (in_destructor_)
    return;

  SchedulePaint(window, gfx::Rect(window->bounds().size()));
  if (!window_paint_callback_.is_null())
    window_paint_callback_.Run(window);
}

const ServerWindow* WindowServer::GetRootWindow(
    const ServerWindow* window) const {
  const Display* display = display_manager_->GetDisplayContaining(window);
  return display ? display->root_window() : nullptr;
}

void WindowServer::ScheduleSurfaceDestruction(ServerWindow* window) {
  Display* display = display_manager_->GetDisplayContaining(window);
  if (display)
    display->ScheduleSurfaceDestruction(window);
}

void WindowServer::OnWindowDestroyed(ServerWindow* window) {
  ProcessWindowDeleted(window);
}

void WindowServer::OnWillChangeWindowHierarchy(ServerWindow* window,
                                               ServerWindow* new_parent,
                                               ServerWindow* old_parent) {
  if (in_destructor_)
    return;

  ProcessWillChangeWindowHierarchy(window, new_parent, old_parent);
}

void WindowServer::OnWindowHierarchyChanged(ServerWindow* window,
                                            ServerWindow* new_parent,
                                            ServerWindow* old_parent) {
  if (in_destructor_)
    return;

  WindowManagerDisplayRoot* display_root =
      display_manager_->GetWindowManagerDisplayRoot(window);
  if (display_root)
    display_root->window_manager_state()
        ->ReleaseCaptureBlockedByAnyModalWindow();

  ProcessWindowHierarchyChanged(window, new_parent, old_parent);

  // TODO(beng): optimize.
  if (old_parent)
    SchedulePaint(old_parent, gfx::Rect(old_parent->bounds().size()));
  if (new_parent)
    SchedulePaint(new_parent, gfx::Rect(new_parent->bounds().size()));

  UpdateNativeCursorFromMouseLocation(window);
}

void WindowServer::OnWindowBoundsChanged(ServerWindow* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  if (in_destructor_)
    return;

  ProcessWindowBoundsChanged(window, old_bounds, new_bounds);
  if (!window->parent())
    return;

  SchedulePaint(window->parent(), old_bounds);
  SchedulePaint(window->parent(), new_bounds);

  UpdateNativeCursorFromMouseLocation(window);
}

void WindowServer::OnWindowClientAreaChanged(
    ServerWindow* window,
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& new_additional_client_areas) {
  if (in_destructor_)
    return;

  ProcessClientAreaChanged(window, new_client_area,
                           new_additional_client_areas);

  UpdateNativeCursorIfOver(window);
}

void WindowServer::OnWindowReordered(ServerWindow* window,
                                     ServerWindow* relative,
                                     mojom::OrderDirection direction) {
  ProcessWindowReorder(window, relative, direction);
  if (!in_destructor_)
    SchedulePaint(window, gfx::Rect(window->bounds().size()));
  UpdateNativeCursorFromMouseLocation(window);
}

void WindowServer::OnWillChangeWindowVisibility(ServerWindow* window) {
  if (in_destructor_)
    return;

  // Need to repaint if the window was drawn (which means it's in the process of
  // hiding) or the window is transitioning to drawn.
  if (window->parent() &&
      (window->IsDrawn() ||
       (!window->visible() && window->parent()->IsDrawn()))) {
    SchedulePaint(window->parent(), window->bounds());
  }

  for (auto& pair : tree_map_) {
    pair.second->ProcessWillChangeWindowVisibility(
        window, IsOperationSource(pair.first));
  }
}

void WindowServer::OnWindowOpacityChanged(ServerWindow* window,
                                          float old_opacity,
                                          float new_opacity) {
  DCHECK(!in_destructor_);

  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowOpacityChanged(window, old_opacity, new_opacity,
                                             IsOperationSource(pair.first));
  }
}

void WindowServer::OnWindowVisibilityChanged(ServerWindow* window) {
  if (in_destructor_)
    return;

  WindowManagerDisplayRoot* display_root =
      display_manager_->GetWindowManagerDisplayRoot(window);
  if (display_root)
    display_root->window_manager_state()->ReleaseCaptureBlockedByModalWindow(
        window);
}

void WindowServer::OnWindowPredefinedCursorChanged(ServerWindow* window,
                                                   int32_t cursor_id) {
  if (in_destructor_)
    return;

  ProcessWillChangeWindowPredefinedCursor(window, cursor_id);

  UpdateNativeCursorIfOver(window);
}

void WindowServer::OnWindowNonClientCursorChanged(ServerWindow* window,
                                                  int32_t cursor_id) {
  if (in_destructor_)
    return;

  UpdateNativeCursorIfOver(window);
}

void WindowServer::OnWindowSharedPropertyChanged(
    ServerWindow* window,
    const std::string& name,
    const std::vector<uint8_t>* new_data) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessWindowPropertyChanged(window, name, new_data,
                                              IsOperationSource(pair.first));
  }
}

void WindowServer::OnWindowTextInputStateChanged(
    ServerWindow* window,
    const ui::TextInputState& state) {
  Display* display = display_manager_->GetDisplayContaining(window);
  display->UpdateTextInputState(window, state);
}

void WindowServer::OnTransientWindowAdded(ServerWindow* window,
                                          ServerWindow* transient_child) {
  for (auto& pair : tree_map_) {
    pair.second->ProcessTransientWindowAdded(window, transient_child,
                                             IsOperationSource(pair.first));
  }
}

void WindowServer::OnTransientWindowRemoved(ServerWindow* window,
                                            ServerWindow* transient_child) {
  // If we're deleting a window, then this is a superfluous message.
  if (current_operation_type() == OperationType::DELETE_WINDOW)
    return;
  for (auto& pair : tree_map_) {
    pair.second->ProcessTransientWindowRemoved(window, transient_child,
                                               IsOperationSource(pair.first));
  }
}

void WindowServer::OnFirstDisplayReady() {
  delegate_->OnFirstDisplayReady();
}

void WindowServer::OnNoMoreDisplays() {
  delegate_->OnNoMoreDisplays();
}

bool WindowServer::GetFrameDecorationsForUser(
    const UserId& user_id,
    mojom::FrameDecorationValuesPtr* values) {
  WindowManagerState* window_manager_state =
      window_manager_window_tree_factory_set_.GetWindowManagerStateForUser(
          user_id);
  if (!window_manager_state)
    return false;
  if (values && window_manager_state->got_frame_decoration_values())
    *values = window_manager_state->frame_decoration_values().Clone();
  return window_manager_state->got_frame_decoration_values();
}

WindowManagerState* WindowServer::GetWindowManagerStateForUser(
    const UserId& user_id) {
  return window_manager_window_tree_factory_set_.GetWindowManagerStateForUser(
      user_id);
}

void WindowServer::OnActiveUserIdChanged(const UserId& previously_active_id,
                                         const UserId& active_id) {}

void WindowServer::OnUserIdAdded(const UserId& id) {
  activity_monitor_map_[id] = base::MakeUnique<UserActivityMonitor>(nullptr);
}

void WindowServer::OnUserIdRemoved(const UserId& id) {
  activity_monitor_map_.erase(id);
}

}  // namespace ws
}  // namespace mus
