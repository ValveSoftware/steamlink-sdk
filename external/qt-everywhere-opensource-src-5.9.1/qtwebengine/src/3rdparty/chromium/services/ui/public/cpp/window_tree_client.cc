// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/window_tree_client.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/common/util.h"
#include "services/ui/public/cpp/in_flight_change.h"
#include "services/ui/public/cpp/input_event_handler.h"
#include "services/ui/public/cpp/surface_id_handler.h"
#include "services/ui/public/cpp/window_drop_target.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/cpp/window_observer.h"
#include "services/ui/public/cpp/window_private.h"
#include "services/ui/public/cpp/window_tracker.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "services/ui/public/cpp/window_tree_client_observer.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/window_manager_window_tree_factory.mojom.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

Id MakeTransportId(ClientSpecificId client_id, ClientSpecificId local_id) {
  return (client_id << 16) | local_id;
}

// Helper function to get the device_scale_factor() of the display::Display
// with |display_id|.
float ScaleFactorForDisplay(int64_t display_id) {
  // TODO(riajiang): Change to use display::GetDisplayWithDisplayId() after
  // https://codereview.chromium.org/2361283002/ is landed.
  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  auto iter = std::find_if(displays.begin(), displays.end(),
                           [display_id](const display::Display& display) {
                             return display.id() == display_id;
                           });
  if (iter != displays.end())
    return iter->device_scale_factor();
  return 1.f;
}

// Helper called to construct a local window object from transport data.
Window* AddWindowToClient(WindowTreeClient* client,
                          Window* parent,
                          const mojom::WindowDataPtr& window_data) {
  // We don't use the ctor that takes a WindowTreeClient here, since it will
  // call back to the service and attempt to create a new window.
  Window* window = WindowPrivate::LocalCreate();
  WindowPrivate private_window(window);
  private_window.set_client(client);
  private_window.set_server_id(window_data->window_id);
  private_window.set_visible(window_data->visible);
  private_window.set_properties(
      window_data->properties
          .To<std::map<std::string, std::vector<uint8_t>>>());
  client->AddWindow(window);
  private_window.LocalSetBounds(
      gfx::Rect(),
      gfx::ConvertRectToDIP(ScaleFactorForDisplay(window->display_id()),
                            window_data->bounds));
  if (parent)
    WindowPrivate(parent).LocalAddChild(window);
  return window;
}

struct WindowTreeClient::CurrentDragState {
  // The current change id of the current drag an drop ipc.
  uint32_t change_id;

  // The effect to return when we send our finish signal.
  uint32_t completed_action;

  // Callback executed when a drag initiated by PerformDragDrop() is completed.
  base::Callback<void(bool, uint32_t)> on_finished;
};

WindowTreeClient::WindowTreeClient(
    WindowTreeClientDelegate* delegate,
    WindowManagerDelegate* window_manager_delegate,
    mojo::InterfaceRequest<mojom::WindowTreeClient> request)
    : client_id_(0),
      next_window_id_(1),
      next_change_id_(1),
      delegate_(delegate),
      window_manager_delegate_(window_manager_delegate),
      capture_window_(nullptr),
      focused_window_(nullptr),
      binding_(this),
      tree_(nullptr),
      in_destructor_(false),
      weak_factory_(this) {
  // Allow for a null request in tests.
  if (request.is_pending())
    binding_.Bind(std::move(request));
  if (window_manager_delegate)
    window_manager_delegate->SetWindowManagerClient(this);
}

WindowTreeClient::~WindowTreeClient() {
  in_destructor_ = true;

  for (auto& observer : observers_)
    observer.OnWillDestroyClient(this);

  std::vector<Window*> non_owned;
  WindowTracker tracker;
  while (!windows_.empty()) {
    IdToWindowMap::iterator it = windows_.begin();
    if (it->second->WasCreatedByThisClient()) {
      it->second->Destroy();
    } else {
      tracker.Add(it->second);
      windows_.erase(it);
    }
  }

  // Delete the non-owned windows last. In the typical case these are roots. The
  // exception is the window manager and embed roots, which may know about
  // other random windows that it doesn't own.
  // NOTE: we manually delete as we're a friend.
  while (!tracker.windows().empty())
    delete tracker.windows().front();

  for (auto& observer : observers_)
    observer.OnDidDestroyClient(this);
}

void WindowTreeClient::ConnectViaWindowTreeFactory(
    service_manager::Connector* connector) {
  // The client id doesn't really matter, we use 101 purely for debugging.
  client_id_ = 101;

  mojom::WindowTreeFactoryPtr factory;
  connector->ConnectToInterface(ui::mojom::kServiceName, &factory);
  mojom::WindowTreePtr window_tree;
  factory->CreateWindowTree(GetProxy(&window_tree),
                            binding_.CreateInterfacePtrAndBind());
  SetWindowTree(std::move(window_tree));
}

void WindowTreeClient::ConnectAsWindowManager(
    service_manager::Connector* connector) {
  DCHECK(window_manager_delegate_);

  mojom::WindowManagerWindowTreeFactoryPtr factory;
  connector->ConnectToInterface(ui::mojom::kServiceName, &factory);
  mojom::WindowTreePtr window_tree;
  factory->CreateWindowTree(GetProxy(&window_tree),
                            binding_.CreateInterfacePtrAndBind());
  SetWindowTree(std::move(window_tree));
}

void WindowTreeClient::WaitForEmbed() {
  DCHECK(roots_.empty());
  // OnEmbed() is the first function called.
  binding_.WaitForIncomingMethodCall();
  // TODO(sky): deal with pipe being closed before we get OnEmbed().
}

void WindowTreeClient::DestroyWindow(Window* window) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<CrashInFlightChange>(window, ChangeType::DELETE_WINDOW));
  tree_->DeleteWindow(change_id, server_id(window));
}

void WindowTreeClient::AddChild(Window* parent, Id child_id) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<CrashInFlightChange>(parent, ChangeType::ADD_CHILD));
  tree_->AddWindow(change_id, parent->server_id(), child_id);
}

void WindowTreeClient::RemoveChild(Window* parent, Id child_id) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<CrashInFlightChange>(parent, ChangeType::REMOVE_CHILD));
  tree_->RemoveWindowFromParent(change_id, child_id);
}

void WindowTreeClient::AddTransientWindow(Window* window,
                                              Id transient_window_id) {
  DCHECK(tree_);
  const uint32_t change_id =
      ScheduleInFlightChange(base::MakeUnique<CrashInFlightChange>(
          window, ChangeType::ADD_TRANSIENT_WINDOW));
  tree_->AddTransientWindow(change_id, server_id(window), transient_window_id);
}

void WindowTreeClient::RemoveTransientWindowFromParent(Window* window) {
  DCHECK(tree_);
  const uint32_t change_id =
      ScheduleInFlightChange(base::MakeUnique<CrashInFlightChange>(
          window, ChangeType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT));
  tree_->RemoveTransientWindowFromParent(change_id, server_id(window));
}

void WindowTreeClient::SetModal(Window* window) {
  DCHECK(tree_);
  const uint32_t change_id =
      ScheduleInFlightChange(base::MakeUnique<InFlightSetModalChange>(window));
  tree_->SetModal(change_id, server_id(window));
}

void WindowTreeClient::Reorder(Window* window,
                                   Id relative_window_id,
                                   mojom::OrderDirection direction) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<CrashInFlightChange>(window, ChangeType::REORDER));
  tree_->ReorderWindow(change_id, server_id(window), relative_window_id,
                       direction);
}

bool WindowTreeClient::WasCreatedByThisClient(const Window* window) const {
  // Windows created via CreateTopLevelWindow() are not owned by us, but have
  // our client id. const_cast is required by set.
  return HiWord(server_id(window)) == client_id_ &&
         roots_.count(const_cast<Window*>(window)) == 0;
}

void WindowTreeClient::SetBounds(Window* window,
                                 const gfx::Rect& old_bounds,
                                 const gfx::Rect& bounds) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<InFlightBoundsChange>(window, old_bounds));
  tree_->SetWindowBounds(
      change_id, server_id(window),
      gfx::ConvertRectToPixel(ScaleFactorForDisplay(window->display_id()),
                              bounds));
}

void WindowTreeClient::SetCapture(Window* window) {
  // In order for us to get here we had to have exposed a window, which implies
  // we got a client.
  DCHECK(tree_);
  if (capture_window_ == window)
    return;
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<InFlightCaptureChange>(this, capture_window_));
  tree_->SetCapture(change_id, server_id(window));
  LocalSetCapture(window);
}

void WindowTreeClient::ReleaseCapture(Window* window) {
  // In order for us to get here we had to have exposed a window, which implies
  // we got a client.
  DCHECK(tree_);
  if (capture_window_ != window)
    return;
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<InFlightCaptureChange>(this, window));
  tree_->ReleaseCapture(change_id, server_id(window));
  LocalSetCapture(nullptr);
}

void WindowTreeClient::SetClientArea(
    Id window_id,
    const gfx::Insets& client_area,
    const std::vector<gfx::Rect>& additional_client_areas) {
  DCHECK(tree_);
  float device_scale_factor =
      ScaleFactorForDisplay(GetWindowByServerId(window_id)->display_id());
  std::vector<gfx::Rect> additional_client_areas_in_pixel;
  for (const gfx::Rect& area : additional_client_areas) {
    additional_client_areas_in_pixel.push_back(
        gfx::ConvertRectToPixel(device_scale_factor, area));
  }
  tree_->SetClientArea(
      window_id, gfx::ConvertInsetsToPixel(device_scale_factor, client_area),
      additional_client_areas_in_pixel);
}

void WindowTreeClient::SetHitTestMask(Id window_id, const gfx::Rect& mask) {
  DCHECK(tree_);
  tree_->SetHitTestMask(
      window_id,
      gfx::ConvertRectToPixel(
          ScaleFactorForDisplay(GetWindowByServerId(window_id)->display_id()),
          mask));
}

void WindowTreeClient::ClearHitTestMask(Id window_id) {
  DCHECK(tree_);
  tree_->SetHitTestMask(window_id, base::nullopt);
}

void WindowTreeClient::SetFocus(Window* window) {
  // In order for us to get here we had to have exposed a window, which implies
  // we got a client.
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<InFlightFocusChange>(this, focused_window_));
  tree_->SetFocus(change_id, window ? server_id(window) : 0);
  LocalSetFocus(window);
}

void WindowTreeClient::SetCanFocus(Id window_id, bool can_focus) {
  DCHECK(tree_);
  tree_->SetCanFocus(window_id, can_focus);
}

void WindowTreeClient::SetPredefinedCursor(Id window_id,
                                           ui::mojom::Cursor cursor_id) {
  DCHECK(tree_);

  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  // We make an inflight change thing here.
  const uint32_t change_id =
      ScheduleInFlightChange(base::MakeUnique<InFlightPredefinedCursorChange>(
          window, window->predefined_cursor()));
  tree_->SetPredefinedCursor(change_id, window_id, cursor_id);
}

void WindowTreeClient::SetVisible(Window* window, bool visible) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<InFlightVisibleChange>(window, !visible));
  tree_->SetWindowVisibility(change_id, server_id(window), visible);
}

void WindowTreeClient::SetOpacity(Window* window, float opacity) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<InFlightOpacityChange>(window, window->opacity()));
  tree_->SetWindowOpacity(change_id, server_id(window), opacity);
}

void WindowTreeClient::SetProperty(Window* window,
                                       const std::string& name,
                                       mojo::Array<uint8_t> data) {
  DCHECK(tree_);

  mojo::Array<uint8_t> old_value(nullptr);
  if (window->HasSharedProperty(name))
    old_value = mojo::Array<uint8_t>::From(window->properties_[name]);

  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<InFlightPropertyChange>(window, name, old_value));
  tree_->SetWindowProperty(change_id, server_id(window), mojo::String(name),
                           std::move(data));
}

void WindowTreeClient::SetWindowTextInputState(
    Id window_id,
    mojo::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetWindowTextInputState(window_id, std::move(state));
}

void WindowTreeClient::SetImeVisibility(Id window_id,
                                            bool visible,
                                            mojo::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetImeVisibility(window_id, visible, std::move(state));
}

void WindowTreeClient::Embed(Id window_id,
                             mojom::WindowTreeClientPtr client,
                             uint32_t flags,
                             const mojom::WindowTree::EmbedCallback& callback) {
  DCHECK(tree_);
  tree_->Embed(window_id, std::move(client), flags, callback);
}

void WindowTreeClient::RequestClose(Window* window) {
  if (window_manager_internal_client_)
    window_manager_internal_client_->WmRequestClose(server_id(window));
}

void WindowTreeClient::AttachCompositorFrameSink(
    Id window_id,
    mojom::CompositorFrameSinkType type,
    cc::mojom::MojoCompositorFrameSinkRequest compositor_frame_sink,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  DCHECK(tree_);
  tree_->AttachCompositorFrameSink(
      window_id, type, std::move(compositor_frame_sink), std::move(client));
}

void WindowTreeClient::LocalSetCapture(Window* window) {
  if (capture_window_ == window)
    return;
  Window* lost_capture = capture_window_;
  capture_window_ = window;
  if (lost_capture) {
    for (auto& observer : *WindowPrivate(lost_capture).observers())
      observer.OnWindowLostCapture(lost_capture);
  }
  for (auto& observer : observers_)
    observer.OnWindowTreeCaptureChanged(window, lost_capture);
}

void WindowTreeClient::LocalSetFocus(Window* focused) {
  Window* blurred = focused_window_;
  // Update |focused_window_| before calling any of the observers, so that the
  // observers get the correct result from calling |Window::HasFocus()|,
  // |WindowTreeClient::GetFocusedWindow()| etc.
  focused_window_ = focused;
  if (blurred) {
    for (auto& observer : *WindowPrivate(blurred).observers())
      observer.OnWindowFocusChanged(focused, blurred);
  }
  if (focused) {
    for (auto& observer : *WindowPrivate(focused).observers())
      observer.OnWindowFocusChanged(focused, blurred);
  }
  for (auto& observer : observers_)
    observer.OnWindowTreeFocusChanged(focused, blurred);
}

void WindowTreeClient::AddWindow(Window* window) {
  DCHECK(windows_.find(server_id(window)) == windows_.end());
  windows_[server_id(window)] = window;
}

void WindowTreeClient::OnWindowDestroying(Window* window) {
  if (window == capture_window_) {
    // Normally the queue updates itself upon window destruction. However since
    // |window| is being destroyed, it will not be possible to notify its
    // observers of the lost capture. Update local state now.
    LocalSetCapture(nullptr);
  }
  // For |focused_window_| window destruction clears the entire focus state.
}

void WindowTreeClient::OnWindowDestroyed(Window* window) {
  windows_.erase(server_id(window));

  for (auto& entry : embedded_windows_) {
    auto it = entry.second.find(window);
    if (it != entry.second.end()) {
      entry.second.erase(it);
      break;
    }
  }

  // Remove any InFlightChanges associated with the window.
  std::set<uint32_t> in_flight_change_ids_to_remove;
  for (const auto& pair : in_flight_map_) {
    if (pair.second->window() == window)
      in_flight_change_ids_to_remove.insert(pair.first);
  }
  for (auto change_id : in_flight_change_ids_to_remove)
    in_flight_map_.erase(change_id);

  const bool was_root = roots_.erase(window) > 0;
  if (!in_destructor_ && was_root && roots_.empty() && is_from_embed_)
    delegate_->OnEmbedRootDestroyed(window);
}

Window* WindowTreeClient::GetWindowByServerId(Id id) {
  IdToWindowMap::const_iterator it = windows_.find(id);
  return it != windows_.end() ? it->second : NULL;
}

InFlightChange* WindowTreeClient::GetOldestInFlightChangeMatching(
    const InFlightChange& change) {
  for (const auto& pair : in_flight_map_) {
    if (pair.second->window() == change.window() &&
        pair.second->change_type() == change.change_type() &&
        pair.second->Matches(change)) {
      return pair.second.get();
    }
  }
  return nullptr;
}

uint32_t WindowTreeClient::ScheduleInFlightChange(
    std::unique_ptr<InFlightChange> change) {
  DCHECK(!change->window() ||
         windows_.count(change->window()->server_id()) > 0);
  const uint32_t change_id = next_change_id_++;
  in_flight_map_[change_id] = std::move(change);
  return change_id;
}

bool WindowTreeClient::ApplyServerChangeToExistingInFlightChange(
    const InFlightChange& change) {
  InFlightChange* existing_change = GetOldestInFlightChangeMatching(change);
  if (!existing_change)
    return false;

  existing_change->SetRevertValueFrom(change);
  return true;
}

void WindowTreeClient::BuildWindowTree(
    const mojo::Array<mojom::WindowDataPtr>& windows,
    Window* initial_parent) {
  for (const auto& window_data : windows) {
    Window* parent = window_data->parent_id == 0
                         ? nullptr
                         : GetWindowByServerId(window_data->parent_id);
    Window* existing_window = GetWindowByServerId(window_data->window_id);
    if (!existing_window)
      AddWindowToClient(this, parent, window_data);
    else if (parent)
      WindowPrivate(parent).LocalAddChild(existing_window);
  }
}

Window* WindowTreeClient::NewWindowImpl(
    NewWindowType type,
    const Window::SharedProperties* properties) {
  DCHECK(tree_);
  Window* window =
      new Window(this, MakeTransportId(client_id_, next_window_id_++));
  if (properties)
    window->properties_ = *properties;
  AddWindow(window);

  const uint32_t change_id =
      ScheduleInFlightChange(base::MakeUnique<CrashInFlightChange>(
          window, type == NewWindowType::CHILD
                      ? ChangeType::NEW_WINDOW
                      : ChangeType::NEW_TOP_LEVEL_WINDOW));
  mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties;
  if (properties) {
    transport_properties =
        mojo::Map<mojo::String, mojo::Array<uint8_t>>::From(*properties);
  }
  if (type == NewWindowType::CHILD) {
    tree_->NewWindow(change_id, server_id(window),
                     std::move(transport_properties));
  } else {
    roots_.insert(window);
    tree_->NewTopLevelWindow(change_id, server_id(window),
                             std::move(transport_properties));
  }
  return window;
}

void WindowTreeClient::SetWindowTree(mojom::WindowTreePtr window_tree_ptr) {
  tree_ptr_ = std::move(window_tree_ptr);
  tree_ = tree_ptr_.get();

  tree_ptr_->GetCursorLocationMemory(
      base::Bind(&WindowTreeClient::OnReceivedCursorLocationMemory,
                 weak_factory_.GetWeakPtr()));

  tree_ptr_.set_connection_error_handler(base::Bind(
      &WindowTreeClient::OnConnectionLost, weak_factory_.GetWeakPtr()));

  if (window_manager_delegate_) {
    tree_ptr_->GetWindowManagerClient(GetProxy(&window_manager_internal_client_,
                                               tree_ptr_.associated_group()));
  }
}

void WindowTreeClient::OnConnectionLost() {
  delegate_->OnLostConnection(this);
}

void WindowTreeClient::OnEmbedImpl(mojom::WindowTree* window_tree,
                                       ClientSpecificId client_id,
                                       mojom::WindowDataPtr root_data,
                                       int64_t display_id,
                                       Id focused_window_id,
                                       bool drawn) {
  // WARNING: this is only called if WindowTreeClient was created as the
  // result of an embedding.
  tree_ = window_tree;
  client_id_ = client_id;

  DCHECK(roots_.empty());
  Window* root = AddWindowToClient(this, nullptr, root_data);
  WindowPrivate(root).LocalSetDisplay(display_id);
  roots_.insert(root);

  focused_window_ = GetWindowByServerId(focused_window_id);

  WindowPrivate(root).LocalSetParentDrawn(drawn);

  delegate_->OnEmbed(root);

  if (focused_window_) {
    for (auto& observer : observers_)
      observer.OnWindowTreeFocusChanged(focused_window_, nullptr);
  }
}

void WindowTreeClient::WmNewDisplayAddedImpl(const display::Display& display,
                                             mojom::WindowDataPtr root_data,
                                             bool parent_drawn) {
  DCHECK(window_manager_delegate_);

  Window* root = AddWindowToClient(this, nullptr, root_data);
  WindowPrivate(root).LocalSetDisplay(display.id());
  WindowPrivate(root).LocalSetParentDrawn(parent_drawn);
  roots_.insert(root);

  window_manager_delegate_->OnWmNewDisplay(root, display);
}

void WindowTreeClient::OnReceivedCursorLocationMemory(
    mojo::ScopedSharedBufferHandle handle) {
  cursor_location_mapping_ = handle->Map(sizeof(base::subtle::Atomic32));
  DCHECK(cursor_location_mapping_);
}

void WindowTreeClient::OnWmMoveLoopCompleted(uint32_t change_id,
                                             bool completed) {
  if (window_manager_internal_client_)
    window_manager_internal_client_->WmResponse(change_id, completed);

  if (change_id == current_wm_move_loop_change_) {
    current_wm_move_loop_change_ = 0;
    current_wm_move_loop_window_id_ = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeClient, WindowTreeClient implementation:

const std::set<Window*>& WindowTreeClient::GetRoots() {
  return roots_;
}

Window* WindowTreeClient::GetFocusedWindow() {
  return focused_window_;
}

void WindowTreeClient::ClearFocus() {
  if (!focused_window_)
    return;

  SetFocus(nullptr);
}

gfx::Point WindowTreeClient::GetCursorScreenPoint() {
  // We raced initialization. Return (0, 0).
  if (!cursor_location_memory())
    return gfx::Point();

  base::subtle::Atomic32 location =
      base::subtle::NoBarrier_Load(cursor_location_memory());
  return gfx::Point(static_cast<int16_t>(location >> 16),
                    static_cast<int16_t>(location & 0xFFFF));
}

void WindowTreeClient::StartPointerWatcher(bool want_moves) {
  if (has_pointer_watcher_)
    StopPointerWatcher();
  has_pointer_watcher_ = true;
  tree_->StartPointerWatcher(want_moves);
}

void WindowTreeClient::StopPointerWatcher() {
  DCHECK(has_pointer_watcher_);
  tree_->StopPointerWatcher();
  has_pointer_watcher_ = false;
}

void WindowTreeClient::PerformDragDrop(
    Window* window,
    const std::map<std::string, std::vector<uint8_t>>& drag_data,
    int drag_operation,
    const gfx::Point& cursor_location,
    const SkBitmap& bitmap,
    const base::Callback<void(bool, uint32_t)>& callback) {
  DCHECK(!current_drag_state_);

  // TODO(erg): Pass |cursor_location| and |bitmap| in PerformDragDrop() when
  // we start showing an image representation of the drag under the cursor.

  if (window->drop_target()) {
    // To minimize the number of round trips, copy the drag drop data to our
    // handler here, instead of forcing mus to send this same data back.
    OnDragDropStart(
        mojo::Map<mojo::String, mojo::Array<uint8_t>>::From(drag_data));
  }

  uint32_t current_drag_change = ScheduleInFlightChange(
      base::MakeUnique<InFlightDragChange>(window, ChangeType::DRAG_LOOP));
  current_drag_state_.reset(new CurrentDragState{
      current_drag_change, ui::mojom::kDropEffectNone, callback});

  tree_->PerformDragDrop(
      current_drag_change, window->server_id(),
      mojo::Map<mojo::String, mojo::Array<uint8_t>>::From(drag_data),
      drag_operation);
}

void WindowTreeClient::CancelDragDrop(Window* window) {
  // Server will clean up drag and fail the in-flight change.
  tree_->CancelDragDrop(window->server_id());
}

void WindowTreeClient::PerformWindowMove(
    Window* window,
    ui::mojom::MoveLoopSource source,
    const gfx::Point& cursor_location,
    const base::Callback<void(bool)>& callback) {
  DCHECK(on_current_move_finished_.is_null());
  on_current_move_finished_ = callback;

  current_move_loop_change_ = ScheduleInFlightChange(
      base::MakeUnique<InFlightDragChange>(window, ChangeType::MOVE_LOOP));
  // Tell the window manager to take over moving us.
  tree_->PerformWindowMove(current_move_loop_change_, window->server_id(),
                           source, cursor_location);
}

void WindowTreeClient::CancelWindowMove(Window* window) {
  tree_->CancelWindowMove(window->server_id());
}

Window* WindowTreeClient::NewWindow(
    const Window::SharedProperties* properties) {
  return NewWindowImpl(NewWindowType::CHILD, properties);
}

Window* WindowTreeClient::NewTopLevelWindow(
    const Window::SharedProperties* properties) {
  Window* window = NewWindowImpl(NewWindowType::TOP_LEVEL, properties);
  // Assume newly created top level windows are drawn by default, otherwise
  // requests to focus will fail. We will get the real value in
  // OnTopLevelCreated().
  window->LocalSetParentDrawn(true);
  return window;
}

#if !defined(NDEBUG)
std::string WindowTreeClient::GetDebugWindowHierarchy() const {
  std::string result;
  for (Window* root : roots_)
    BuildDebugInfo(std::string(), root, &result);
  return result;
}

void WindowTreeClient::BuildDebugInfo(const std::string& depth,
                                      Window* window,
                                      std::string* result) const {
  std::string name = window->GetName();
  *result += base::StringPrintf(
      "%sid=%d visible=%s bounds=%d,%d %dx%d %s\n", depth.c_str(),
      window->server_id(), window->visible() ? "true" : "false",
      window->bounds().x(), window->bounds().y(), window->bounds().width(),
      window->bounds().height(), !name.empty() ? name.c_str() : "(no name)");
  for (Window* child : window->children())
    BuildDebugInfo(depth + "  ", child, result);
}
#endif  // !defined(NDEBUG)

////////////////////////////////////////////////////////////////////////////////
// WindowTreeClient, WindowTreeClient implementation:

void WindowTreeClient::AddObserver(WindowTreeClientObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowTreeClient::RemoveObserver(WindowTreeClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WindowTreeClient::SetCanAcceptDrops(Id window_id, bool can_accept_drops) {
  DCHECK(tree_);
  tree_->SetCanAcceptDrops(window_id, can_accept_drops);
}

void WindowTreeClient::SetCanAcceptEvents(Id window_id,
                                          bool can_accept_events) {
  DCHECK(tree_);
  tree_->SetCanAcceptEvents(window_id, can_accept_events);
}

void WindowTreeClient::OnEmbed(ClientSpecificId client_id,
                               mojom::WindowDataPtr root_data,
                               mojom::WindowTreePtr tree,
                               int64_t display_id,
                               Id focused_window_id,
                               bool drawn) {
  DCHECK(!tree_ptr_);
  tree_ptr_ = std::move(tree);

  is_from_embed_ = true;

  if (window_manager_delegate_) {
    tree_ptr_->GetWindowManagerClient(GetProxy(&window_manager_internal_client_,
                                               tree_ptr_.associated_group()));
  }

  OnEmbedImpl(tree_ptr_.get(), client_id, std::move(root_data), display_id,
              focused_window_id, drawn);
}

void WindowTreeClient::OnEmbeddedAppDisconnected(Id window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (window) {
    for (auto& observer : *WindowPrivate(window).observers())
      observer.OnWindowEmbeddedAppDisconnected(window);
  }
}

void WindowTreeClient::OnUnembed(Id window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  delegate_->OnUnembed(window);
  WindowPrivate(window).LocalDestroy();
}

void WindowTreeClient::OnCaptureChanged(Id new_capture_window_id,
                                        Id old_capture_window_id) {
  Window* new_capture_window = GetWindowByServerId(new_capture_window_id);
  Window* lost_capture_window = GetWindowByServerId(old_capture_window_id);
  if (!new_capture_window && !lost_capture_window)
    return;

  InFlightCaptureChange change(this, new_capture_window);
  if (ApplyServerChangeToExistingInFlightChange(change))
    return;

  LocalSetCapture(new_capture_window);
}

void WindowTreeClient::OnTopLevelCreated(uint32_t change_id,
                                             mojom::WindowDataPtr data,
                                             int64_t display_id,
                                             bool drawn) {
  // The server ack'd the top level window we created and supplied the state
  // of the window at the time the server created it. For properties we do not
  // have changes in flight for we can update them immediately. For properties
  // with changes in flight we set the revert value from the server.

  if (!in_flight_map_.count(change_id)) {
    // The window may have been destroyed locally before the server could finish
    // creating the window, and before the server received the notification that
    // the window has been destroyed.
    return;
  }
  std::unique_ptr<InFlightChange> change(std::move(in_flight_map_[change_id]));
  in_flight_map_.erase(change_id);

  Window* window = change->window();
  WindowPrivate window_private(window);

  // Drawn state and display-id always come from the server (they can't be
  // modified locally).
  window_private.LocalSetParentDrawn(drawn);
  window_private.LocalSetDisplay(display_id);

  // The default visibilty is false, we only need update visibility if it
  // differs from that.
  if (data->visible) {
    InFlightVisibleChange visible_change(window, data->visible);
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(visible_change);
    if (current_change)
      current_change->SetRevertValueFrom(visible_change);
    else
      window_private.LocalSetVisible(true);
  }

  const gfx::Rect bounds(data->bounds);
  {
    InFlightBoundsChange bounds_change(window, bounds);
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(bounds_change);
    if (current_change)
      current_change->SetRevertValueFrom(bounds_change);
    else if (window->bounds() != bounds)
      window_private.LocalSetBounds(window->bounds(), bounds);
  }

  // There is currently no API to bulk set properties, so we iterate over each
  // property individually.
  Window::SharedProperties properties =
      data->properties.To<std::map<std::string, std::vector<uint8_t>>>();
  for (const auto& pair : properties) {
    InFlightPropertyChange property_change(
        window, pair.first, mojo::Array<uint8_t>::From(pair.second));
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(property_change);
    if (current_change)
      current_change->SetRevertValueFrom(property_change);
    else
      window_private.LocalSetSharedProperty(pair.first, &(pair.second));
  }

  // Top level windows should not have a parent.
  DCHECK_EQ(0u, data->parent_id);
}

void WindowTreeClient::OnWindowBoundsChanged(Id window_id,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  float device_scale_factor = ScaleFactorForDisplay(window->display_id());
  gfx::Rect old_bounds_in_dip =
      gfx::ConvertRectToDIP(device_scale_factor, old_bounds);
  gfx::Rect new_bounds_in_dip =
      gfx::ConvertRectToDIP(device_scale_factor, new_bounds);

  InFlightBoundsChange new_change(window, new_bounds_in_dip);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;
  WindowPrivate(window).LocalSetBounds(old_bounds_in_dip, new_bounds_in_dip);
}

void WindowTreeClient::OnClientAreaChanged(
    uint32_t window_id,
    const gfx::Insets& new_client_area,
    mojo::Array<gfx::Rect> new_additional_client_areas) {
  Window* window = GetWindowByServerId(window_id);
  if (window) {
    float device_scale_factor = ScaleFactorForDisplay(window->display_id());
    std::vector<gfx::Rect> new_additional_client_areas_in_dip;
    for (const gfx::Rect& area : new_additional_client_areas) {
      new_additional_client_areas_in_dip.push_back(
          gfx::ConvertRectToDIP(device_scale_factor, area));
    }
    WindowPrivate(window).LocalSetClientArea(
        gfx::ConvertInsetsToDIP(device_scale_factor, new_client_area),
        new_additional_client_areas_in_dip);
  }
}

void WindowTreeClient::OnTransientWindowAdded(
    uint32_t window_id,
    uint32_t transient_window_id) {
  Window* window = GetWindowByServerId(window_id);
  Window* transient_window = GetWindowByServerId(transient_window_id);
  // window or transient_window or both may be null if a local delete occurs
  // with an in flight add from the server.
  if (window && transient_window)
    WindowPrivate(window).LocalAddTransientWindow(transient_window);
}

void WindowTreeClient::OnTransientWindowRemoved(
    uint32_t window_id,
    uint32_t transient_window_id) {
  Window* window = GetWindowByServerId(window_id);
  Window* transient_window = GetWindowByServerId(transient_window_id);
  // window or transient_window or both may be null if a local delete occurs
  // with an in flight delete from the server.
  if (window && transient_window)
    WindowPrivate(window).LocalRemoveTransientWindow(transient_window);
}

void WindowTreeClient::OnWindowHierarchyChanged(
    Id window_id,
    Id old_parent_id,
    Id new_parent_id,
    mojo::Array<mojom::WindowDataPtr> windows) {
  Window* initial_parent =
      windows.size() ? GetWindowByServerId(windows[0]->parent_id) : NULL;

  const bool was_window_known = GetWindowByServerId(window_id) != nullptr;

  BuildWindowTree(windows, initial_parent);

  // If the window was not known, then BuildWindowTree() will have created it
  // and parented the window.
  if (!was_window_known)
    return;

  Window* new_parent = GetWindowByServerId(new_parent_id);
  Window* old_parent = GetWindowByServerId(old_parent_id);
  Window* window = GetWindowByServerId(window_id);
  if (new_parent)
    WindowPrivate(new_parent).LocalAddChild(window);
  else
    WindowPrivate(old_parent).LocalRemoveChild(window);
}

void WindowTreeClient::OnWindowReordered(Id window_id,
                                             Id relative_window_id,
                                             mojom::OrderDirection direction) {
  Window* window = GetWindowByServerId(window_id);
  Window* relative_window = GetWindowByServerId(relative_window_id);
  if (window && relative_window)
    WindowPrivate(window).LocalReorder(relative_window, direction);
}

void WindowTreeClient::OnWindowDeleted(Id window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (window)
    WindowPrivate(window).LocalDestroy();
}

Window* WindowTreeClient::GetCaptureWindow() {
  return capture_window_;
}

void WindowTreeClient::OnWindowVisibilityChanged(Id window_id,
                                                     bool visible) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightVisibleChange new_change(window, visible);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window).LocalSetVisible(visible);
}

void WindowTreeClient::OnWindowOpacityChanged(Id window_id,
                                                  float old_opacity,
                                                  float new_opacity) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightOpacityChange new_change(window, new_opacity);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window).LocalSetOpacity(new_opacity);
}

void WindowTreeClient::OnWindowParentDrawnStateChanged(Id window_id,
                                                           bool drawn) {
  Window* window = GetWindowByServerId(window_id);
  if (window)
    WindowPrivate(window).LocalSetParentDrawn(drawn);
}

void WindowTreeClient::OnWindowSharedPropertyChanged(
    Id window_id,
    const mojo::String& name,
    mojo::Array<uint8_t> new_data) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightPropertyChange new_change(window, name, new_data);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window).LocalSetSharedProperty(name, std::move(new_data));
}

void WindowTreeClient::OnWindowInputEvent(uint32_t event_id,
                                          Id window_id,
                                          std::unique_ptr<ui::Event> event,
                                          bool matches_pointer_watcher) {
  DCHECK(event);
  Window* window = GetWindowByServerId(window_id);  // May be null.

  if (matches_pointer_watcher && has_pointer_watcher_) {
    DCHECK(event->IsPointerEvent());
    delegate_->OnPointerEventObserved(*event->AsPointerEvent(), window);
  }

  if (!window || !window->input_event_handler_) {
    tree_->OnWindowInputEventAck(event_id, mojom::EventResult::UNHANDLED);
    return;
  }

  std::unique_ptr<base::Callback<void(mojom::EventResult)>> ack_callback(
      new base::Callback<void(mojom::EventResult)>(
          base::Bind(&mojom::WindowTree::OnWindowInputEventAck,
                     base::Unretained(tree_), event_id)));

  // TODO(moshayedi): crbug.com/617222. No need to convert to ui::MouseEvent or
  // ui::TouchEvent once we have proper support for pointer events.
  if (event->IsMousePointerEvent()) {
    if (event->type() == ui::ET_POINTER_WHEEL_CHANGED) {
      window->input_event_handler_->OnWindowInputEvent(
          window, ui::MouseWheelEvent(*event->AsPointerEvent()), &ack_callback);
    } else {
      window->input_event_handler_->OnWindowInputEvent(
          window, ui::MouseEvent(*event->AsPointerEvent()), &ack_callback);
    }
  } else if (event->IsTouchPointerEvent()) {
    window->input_event_handler_->OnWindowInputEvent(
        window, ui::TouchEvent(*event->AsPointerEvent()), &ack_callback);
  } else {
    window->input_event_handler_->OnWindowInputEvent(window, *event.get(),
                                                     &ack_callback);
  }

  // The handler did not take ownership of the callback, so we send the ack,
  // marking the event as not consumed.
  if (ack_callback)
    ack_callback->Run(mojom::EventResult::UNHANDLED);
}

void WindowTreeClient::OnPointerEventObserved(std::unique_ptr<ui::Event> event,
                                              uint32_t window_id) {
  DCHECK(event);
  DCHECK(event->IsPointerEvent());
  if (has_pointer_watcher_) {
    Window* target_window = GetWindowByServerId(window_id);
    delegate_->OnPointerEventObserved(*event->AsPointerEvent(), target_window);
  }
}

void WindowTreeClient::OnWindowFocused(Id focused_window_id) {
  Window* focused_window = GetWindowByServerId(focused_window_id);
  InFlightFocusChange new_change(this, focused_window);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  LocalSetFocus(focused_window);
}

void WindowTreeClient::OnWindowPredefinedCursorChanged(
    Id window_id,
    mojom::Cursor cursor) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightPredefinedCursorChange new_change(window, cursor);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window).LocalSetPredefinedCursor(cursor);
}

void WindowTreeClient::OnWindowSurfaceChanged(
    Id window_id,
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size,
    float device_scale_factor) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;
  std::unique_ptr<SurfaceInfo> surface_info(base::MakeUnique<SurfaceInfo>());
  surface_info->surface_id = surface_id;
  surface_info->frame_size = frame_size;
  surface_info->device_scale_factor = device_scale_factor;
  WindowPrivate(window).LocalSetSurfaceId(std::move(surface_info));
}

void WindowTreeClient::OnDragDropStart(
    mojo::Map<mojo::String, mojo::Array<uint8_t>> mime_data) {
  mime_drag_data_ = std::move(mime_data);
}

void WindowTreeClient::OnDragEnter(Id window_id,
                                   uint32_t key_state,
                                   const gfx::Point& position,
                                   uint32_t effect_bitmask,
                                   const OnDragEnterCallback& callback) {
  Window* window = GetWindowByServerId(window_id);
  if (!window || !window->drop_target()) {
    callback.Run(mojom::kDropEffectNone);
    return;
  }

  if (!base::ContainsKey(drag_entered_windows_, window_id)) {
    window->drop_target()->OnDragDropStart(
        mime_drag_data_.To<std::map<std::string, std::vector<uint8_t>>>());
    drag_entered_windows_.insert(window_id);
  }

  uint32_t ret =
      window->drop_target()->OnDragEnter(key_state, position, effect_bitmask);
  callback.Run(ret);
}

void WindowTreeClient::OnDragOver(Id window_id,
                                  uint32_t key_state,
                                  const gfx::Point& position,
                                  uint32_t effect_bitmask,
                                  const OnDragOverCallback& callback) {
  Window* window = GetWindowByServerId(window_id);
  if (!window || !window->drop_target()) {
    callback.Run(mojom::kDropEffectNone);
    return;
  }

  uint32_t ret =
      window->drop_target()->OnDragOver(key_state, position, effect_bitmask);
  callback.Run(ret);
}

void WindowTreeClient::OnDragLeave(Id window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (!window || !window->drop_target())
    return;

  window->drop_target()->OnDragLeave();
}

void WindowTreeClient::OnDragDropDone() {
  for (Id id : drag_entered_windows_) {
    Window* window = GetWindowByServerId(id);
    if (!window || !window->drop_target())
      continue;
    window->drop_target()->OnDragDropDone();
  }
  drag_entered_windows_.clear();
}

void WindowTreeClient::OnCompleteDrop(Id window_id,
                                      uint32_t key_state,
                                      const gfx::Point& position,
                                      uint32_t effect_bitmask,
                                      const OnCompleteDropCallback& callback) {
  Window* window = GetWindowByServerId(window_id);
  if (!window || !window->drop_target()) {
    callback.Run(mojom::kDropEffectNone);
    return;
  }

  uint32_t ret = window->drop_target()->OnCompleteDrop(key_state, position,
                                                       effect_bitmask);
  callback.Run(ret);
}

void WindowTreeClient::OnPerformDragDropCompleted(uint32_t change_id,
                                                  bool success,
                                                  uint32_t action_taken) {
  if (current_drag_state_ && change_id == current_drag_state_->change_id) {
    current_drag_state_->completed_action = action_taken;
    OnChangeCompleted(change_id, success);
  }
}

void WindowTreeClient::OnChangeCompleted(uint32_t change_id, bool success) {
  std::unique_ptr<InFlightChange> change(std::move(in_flight_map_[change_id]));
  in_flight_map_.erase(change_id);
  if (!change)
    return;

  if (!success)
    change->ChangeFailed();

  InFlightChange* next_change = GetOldestInFlightChangeMatching(*change);
  if (next_change) {
    if (!success)
      next_change->SetRevertValueFrom(*change);
  } else if (!success) {
    change->Revert();
  }

  if (change_id == current_move_loop_change_) {
    current_move_loop_change_ = 0;
    on_current_move_finished_.Run(success);
    on_current_move_finished_.Reset();
  }

  if (current_drag_state_ && change_id == current_drag_state_->change_id) {
    OnDragDropDone();

    current_drag_state_->on_finished.Run(success,
                                         current_drag_state_->completed_action);
    current_drag_state_.reset();
  }
}

void WindowTreeClient::GetWindowManager(
    mojo::AssociatedInterfaceRequest<WindowManager> internal) {
  window_manager_internal_.reset(
      new mojo::AssociatedBinding<mojom::WindowManager>(this,
                                                        std::move(internal)));
}

void WindowTreeClient::RequestClose(uint32_t window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (!window || !IsRoot(window))
    return;

  for (auto& observer : *WindowPrivate(window).observers())
    observer.OnRequestClose(window);
}

void WindowTreeClient::OnConnect(ClientSpecificId client_id) {
  client_id_ = client_id;
}

void WindowTreeClient::WmNewDisplayAdded(const display::Display& display,
                                         mojom::WindowDataPtr root_data,
                                         bool parent_drawn) {
  WmNewDisplayAddedImpl(display, std::move(root_data), parent_drawn);
}

void WindowTreeClient::WmDisplayRemoved(int64_t display_id) {
  DCHECK(window_manager_delegate_);

  for (Window* root : roots_) {
    if (root->display_id() == display_id) {
      window_manager_delegate_->OnWmDisplayRemoved(root);
      return;
    }
  }
}

void WindowTreeClient::WmDisplayModified(const display::Display& display) {
  DCHECK(window_manager_delegate_);
  window_manager_delegate_->OnWmDisplayModified(display);
}

void WindowTreeClient::WmSetBounds(uint32_t change_id,
                                       Id window_id,
                                       const gfx::Rect& transit_bounds) {
  Window* window = GetWindowByServerId(window_id);
  bool result = false;
  if (window) {
    DCHECK(window_manager_delegate_);
    gfx::Rect transit_bounds_in_dip = gfx::ConvertRectToDIP(
        ScaleFactorForDisplay(window->display_id()), transit_bounds);
    gfx::Rect bounds = transit_bounds_in_dip;
    result = window_manager_delegate_->OnWmSetBounds(window, &bounds);
    if (result) {
      // If the resulting bounds differ return false. Returning false ensures
      // the client applies the bounds we set below.
      result = bounds == transit_bounds_in_dip;
      window->SetBounds(bounds);
    }
  }
  if (window_manager_internal_client_)
    window_manager_internal_client_->WmResponse(change_id, result);
}

void WindowTreeClient::WmSetProperty(uint32_t change_id,
                                     Id window_id,
                                     const mojo::String& name,
                                     mojo::Array<uint8_t> transit_data) {
  Window* window = GetWindowByServerId(window_id);
  bool result = false;
  if (window) {
    DCHECK(window_manager_delegate_);
    std::unique_ptr<std::vector<uint8_t>> data;
    if (!transit_data.is_null()) {
      data.reset(
          new std::vector<uint8_t>(transit_data.To<std::vector<uint8_t>>()));
    }
    result = window_manager_delegate_->OnWmSetProperty(window, name, &data);
    if (result) {
      // If the resulting bounds differ return false. Returning false ensures
      // the client applies the bounds we set below.
      window->SetSharedPropertyInternal(name, data.get());
    }
  }
  if (window_manager_internal_client_)
    window_manager_internal_client_->WmResponse(change_id, result);
}

void WindowTreeClient::WmCreateTopLevelWindow(
    uint32_t change_id,
    ClientSpecificId requesting_client_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties) {
  std::map<std::string, std::vector<uint8_t>> properties =
      transport_properties.To<std::map<std::string, std::vector<uint8_t>>>();
  Window* window =
      window_manager_delegate_->OnWmCreateTopLevelWindow(&properties);
  embedded_windows_[requesting_client_id].insert(window);
  if (window_manager_internal_client_) {
    window_manager_internal_client_->OnWmCreatedTopLevelWindow(
        change_id, server_id(window));
  }
}

void WindowTreeClient::WmClientJankinessChanged(ClientSpecificId client_id,
                                                bool janky) {
  if (window_manager_delegate_) {
    auto it = embedded_windows_.find(client_id);
    CHECK(it != embedded_windows_.end());
    window_manager_delegate_->OnWmClientJankinessChanged(
        embedded_windows_[client_id], janky);
  }
}

void WindowTreeClient::WmPerformMoveLoop(uint32_t change_id,
                                         Id window_id,
                                         mojom::MoveLoopSource source,
                                         const gfx::Point& cursor_location) {
  if (!window_manager_delegate_ || current_wm_move_loop_change_ != 0) {
    OnWmMoveLoopCompleted(change_id, false);
    return;
  }

  current_wm_move_loop_change_ = change_id;
  current_wm_move_loop_window_id_ = window_id;
  Window* window = GetWindowByServerId(window_id);
  if (window) {
    window_manager_delegate_->OnWmPerformMoveLoop(
        window, source, cursor_location,
        base::Bind(&WindowTreeClient::OnWmMoveLoopCompleted,
                   weak_factory_.GetWeakPtr(), change_id));
  } else {
    OnWmMoveLoopCompleted(change_id, false);
  }
}

void WindowTreeClient::WmCancelMoveLoop(uint32_t change_id) {
  if (!window_manager_delegate_ || change_id != current_wm_move_loop_change_)
    return;

  Window* window = GetWindowByServerId(current_wm_move_loop_window_id_);
  if (window)
    window_manager_delegate_->OnWmCancelMoveLoop(window);
}

void WindowTreeClient::OnAccelerator(uint32_t ack_id,
                                     uint32_t accelerator_id,
                                     std::unique_ptr<ui::Event> event) {
  DCHECK(event);
  const mojom::EventResult result =
      window_manager_delegate_->OnAccelerator(accelerator_id, *event.get());
  if (ack_id && window_manager_internal_client_)
    window_manager_internal_client_->OnAcceleratorAck(ack_id, result);
}

void WindowTreeClient::SetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  if (window_manager_internal_client_) {
    window_manager_internal_client_->WmSetFrameDecorationValues(
        std::move(values));
  }
}

void WindowTreeClient::SetNonClientCursor(Window* window,
                                          ui::mojom::Cursor cursor_id) {
  window_manager_internal_client_->WmSetNonClientCursor(server_id(window),
                                                        cursor_id);
}

void WindowTreeClient::AddAccelerator(
    uint32_t id,
    mojom::EventMatcherPtr event_matcher,
    const base::Callback<void(bool)>& callback) {
  if (window_manager_internal_client_) {
    window_manager_internal_client_->AddAccelerator(
        id, std::move(event_matcher), callback);
  }
}

void WindowTreeClient::RemoveAccelerator(uint32_t id) {
  if (window_manager_internal_client_) {
    window_manager_internal_client_->RemoveAccelerator(id);
  }
}

void WindowTreeClient::AddActivationParent(Window* window) {
  if (window_manager_internal_client_)
    window_manager_internal_client_->AddActivationParent(server_id(window));
}

void WindowTreeClient::RemoveActivationParent(Window* window) {
  if (window_manager_internal_client_)
    window_manager_internal_client_->RemoveActivationParent(server_id(window));
}

void WindowTreeClient::ActivateNextWindow() {
  if (window_manager_internal_client_)
    window_manager_internal_client_->ActivateNextWindow();
}

void WindowTreeClient::SetUnderlaySurfaceOffsetAndExtendedHitArea(
    Window* window,
    const gfx::Vector2d& offset,
    const gfx::Insets& hit_area) {
  if (window_manager_internal_client_) {
    // TODO(riajiang): Figure out if |offset| needs to be converted.
    // (http://crbugs.com/646932)
    window_manager_internal_client_->SetUnderlaySurfaceOffsetAndExtendedHitArea(
        server_id(window), offset.x(), offset.y(),
        gfx::ConvertInsetsToDIP(ScaleFactorForDisplay(window->display_id()),
                                hit_area));
  }
}

}  // namespace ui
