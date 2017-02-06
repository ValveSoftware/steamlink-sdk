// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window_tree_client.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/mus/common/util.h"
#include "components/mus/public/cpp/input_event_handler.h"
#include "components/mus/public/cpp/lib/in_flight_change.h"
#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tracker.h"
#include "components/mus/public/cpp/window_tree_client_delegate.h"
#include "components/mus/public/cpp/window_tree_client_observer.h"
#include "components/mus/public/interfaces/window_manager_window_tree_factory.mojom.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/display/mojo/display_type_converters.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

namespace mus {

void DeleteWindowTreeClient(WindowTreeClient* client) { delete client; }

Id MakeTransportId(ClientSpecificId client_id, ClientSpecificId local_id) {
  return (client_id << 16) | local_id;
}

Id server_id(Window* window) {
  return WindowPrivate(window).server_id();
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
  private_window.LocalSetBounds(gfx::Rect(), window_data->bounds);
  if (parent)
    WindowPrivate(parent).LocalAddChild(window);
  return window;
}

Window* BuildWindowTree(WindowTreeClient* client,
                        const mojo::Array<mojom::WindowDataPtr>& windows,
                        Window* initial_parent) {
  std::vector<Window*> parents;
  Window* root = NULL;
  Window* last_window = NULL;
  if (initial_parent)
    parents.push_back(initial_parent);
  for (size_t i = 0; i < windows.size(); ++i) {
    if (last_window && windows[i]->parent_id == server_id(last_window)) {
      parents.push_back(last_window);
    } else if (!parents.empty()) {
      while (server_id(parents.back()) != windows[i]->parent_id)
        parents.pop_back();
    }
    Window* window = AddWindowToClient(
        client, !parents.empty() ? parents.back() : NULL, windows[i]);
    if (!last_window)
      root = window;
    last_window = window;
  }
  return root;
}

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
      delete_on_no_roots_(!window_manager_delegate),
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

  std::vector<Window*> non_owned;
  WindowTracker tracker;
  while (!windows_.empty()) {
    IdToWindowMap::iterator it = windows_.begin();
    if (OwnsWindow(it->second)) {
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

  FOR_EACH_OBSERVER(WindowTreeClientObserver, observers_,
                    OnWillDestroyClient(this));

  delegate_->OnWindowTreeClientDestroyed(this);
}

void WindowTreeClient::ConnectViaWindowTreeFactory(
    shell::Connector* connector) {
  // Clients created with no root shouldn't delete automatically.
  delete_on_no_roots_ = false;

  // The client id doesn't really matter, we use 101 purely for debugging.
  client_id_ = 101;

  mojom::WindowTreeFactoryPtr factory;
  connector->ConnectToInterface("mojo:mus", &factory);
  mojom::WindowTreePtr window_tree;
  factory->CreateWindowTree(GetProxy(&window_tree),
                            binding_.CreateInterfacePtrAndBind());
  SetWindowTree(std::move(window_tree));
}

void WindowTreeClient::ConnectAsWindowManager(shell::Connector* connector) {
  DCHECK(window_manager_delegate_);

  mojom::WindowManagerWindowTreeFactoryPtr factory;
  connector->ConnectToInterface("mojo:mus", &factory);
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
  const uint32_t change_id = ScheduleInFlightChange(base::WrapUnique(
      new CrashInFlightChange(window, ChangeType::DELETE_WINDOW)));
  tree_->DeleteWindow(change_id, server_id(window));
}

void WindowTreeClient::AddChild(Window* parent, Id child_id) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new CrashInFlightChange(parent, ChangeType::ADD_CHILD)));
  tree_->AddWindow(change_id, parent->server_id(), child_id);
}

void WindowTreeClient::RemoveChild(Window* parent, Id child_id) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(base::WrapUnique(
      new CrashInFlightChange(parent, ChangeType::REMOVE_CHILD)));
  tree_->RemoveWindowFromParent(change_id, child_id);
}

void WindowTreeClient::AddTransientWindow(Window* window,
                                              Id transient_window_id) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(base::WrapUnique(
      new CrashInFlightChange(window, ChangeType::ADD_TRANSIENT_WINDOW)));
  tree_->AddTransientWindow(change_id, server_id(window), transient_window_id);
}

void WindowTreeClient::RemoveTransientWindowFromParent(Window* window) {
  DCHECK(tree_);
  const uint32_t change_id =
      ScheduleInFlightChange(base::WrapUnique(new CrashInFlightChange(
          window, ChangeType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT)));
  tree_->RemoveTransientWindowFromParent(change_id, server_id(window));
}

void WindowTreeClient::SetModal(Window* window) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightSetModalChange(window)));
  tree_->SetModal(change_id, server_id(window));
}

void WindowTreeClient::Reorder(Window* window,
                                   Id relative_window_id,
                                   mojom::OrderDirection direction) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new CrashInFlightChange(window, ChangeType::REORDER)));
  tree_->ReorderWindow(change_id, server_id(window), relative_window_id,
                       direction);
}

bool WindowTreeClient::OwnsWindow(Window* window) const {
  // Windows created via CreateTopLevelWindow() are not owned by us, but have
  // our client id.
  return HiWord(server_id(window)) == client_id_ &&
         roots_.count(window) == 0;
}

void WindowTreeClient::SetBounds(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& bounds) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightBoundsChange(window, old_bounds)));
  tree_->SetWindowBounds(change_id, server_id(window), bounds);
}

void WindowTreeClient::SetCapture(Window* window) {
  // In order for us to get here we had to have exposed a window, which implies
  // we got a client.
  DCHECK(tree_);
  if (capture_window_ == window)
    return;
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightCaptureChange(this, capture_window_)));
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
      base::WrapUnique(new InFlightCaptureChange(this, window)));
  tree_->ReleaseCapture(change_id, server_id(window));
  LocalSetCapture(nullptr);
}

void WindowTreeClient::SetClientArea(
    Id window_id,
    const gfx::Insets& client_area,
    const std::vector<gfx::Rect>& additional_client_areas) {
  DCHECK(tree_);
  tree_->SetClientArea(window_id, client_area, additional_client_areas);
}

void WindowTreeClient::SetHitTestMask(Id window_id, const gfx::Rect& mask) {
  DCHECK(tree_);
  tree_->SetHitTestMask(window_id, mask);
}

void WindowTreeClient::ClearHitTestMask(Id window_id) {
  DCHECK(tree_);
  tree_->SetHitTestMask(window_id, {});
}

void WindowTreeClient::SetFocus(Window* window) {
  // In order for us to get here we had to have exposed a window, which implies
  // we got a client.
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightFocusChange(this, focused_window_)));
  tree_->SetFocus(change_id, window ? server_id(window) : 0);
  LocalSetFocus(window);
}

void WindowTreeClient::SetCanFocus(Id window_id, bool can_focus) {
  DCHECK(tree_);
  tree_->SetCanFocus(window_id, can_focus);
}

void WindowTreeClient::SetPredefinedCursor(Id window_id,
                                               mus::mojom::Cursor cursor_id) {
  DCHECK(tree_);

  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  // We make an inflight change thing here.
  const uint32_t change_id = ScheduleInFlightChange(base::WrapUnique(
      new InFlightPredefinedCursorChange(window, window->predefined_cursor())));
  tree_->SetPredefinedCursor(change_id, window_id, cursor_id);
}

void WindowTreeClient::SetVisible(Window* window, bool visible) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightVisibleChange(window, !visible)));
  tree_->SetWindowVisibility(change_id, server_id(window), visible);
}

void WindowTreeClient::SetOpacity(Window* window, float opacity) {
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::WrapUnique(new InFlightOpacityChange(window, window->opacity())));
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
      base::WrapUnique(new InFlightPropertyChange(window, name, old_value)));
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

void WindowTreeClient::AttachSurface(
    Id window_id,
    mojom::SurfaceType type,
    mojo::InterfaceRequest<mojom::Surface> surface,
    mojom::SurfaceClientPtr client) {
  DCHECK(tree_);
  tree_->AttachSurface(window_id, type, std::move(surface), std::move(client));
}

void WindowTreeClient::LocalSetCapture(Window* window) {
  if (capture_window_ == window)
    return;
  Window* lost_capture = capture_window_;
  capture_window_ = window;
  if (lost_capture) {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(lost_capture).observers(),
                      OnWindowLostCapture(lost_capture));
  }
}

void WindowTreeClient::LocalSetFocus(Window* focused) {
  Window* blurred = focused_window_;
  // Update |focused_window_| before calling any of the observers, so that the
  // observers get the correct result from calling |Window::HasFocus()|,
  // |WindowTreeClient::GetFocusedWindow()| etc.
  focused_window_ = focused;
  if (blurred) {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(blurred).observers(),
                      OnWindowFocusChanged(focused, blurred));
  }
  if (focused) {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(focused).observers(),
                      OnWindowFocusChanged(focused, blurred));
  }
  FOR_EACH_OBSERVER(WindowTreeClientObserver, observers_,
                    OnWindowTreeFocusChanged(focused, blurred));
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

  if (roots_.erase(window) > 0 && roots_.empty() && delete_on_no_roots_ &&
      !in_destructor_) {
    delete this;
  }
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

Window* WindowTreeClient::NewWindowImpl(
    NewWindowType type,
    const Window::SharedProperties* properties) {
  DCHECK(tree_);
  Window* window =
      new Window(this, MakeTransportId(client_id_, next_window_id_++));
  if (properties)
    window->properties_ = *properties;
  AddWindow(window);

  const uint32_t change_id = ScheduleInFlightChange(base::WrapUnique(
      new CrashInFlightChange(window, type == NewWindowType::CHILD
                                          ? ChangeType::NEW_WINDOW
                                          : ChangeType::NEW_TOP_LEVEL_WINDOW)));
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
  delete this;
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
    FOR_EACH_OBSERVER(WindowTreeClientObserver, observers_,
                      OnWindowTreeFocusChanged(focused_window_, nullptr));
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

////////////////////////////////////////////////////////////////////////////////
// WindowTreeClient, WindowTreeClient implementation:

void WindowTreeClient::SetDeleteOnNoRoots(bool value) {
  delete_on_no_roots_ = value;
}

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

void WindowTreeClient::SetEventObserver(mojom::EventMatcherPtr matcher) {
  if (matcher.is_null()) {
    has_event_observer_ = false;
    tree_->SetEventObserver(nullptr, 0u);
  } else {
    has_event_observer_ = true;
    event_observer_id_++;
    tree_->SetEventObserver(std::move(matcher), event_observer_id_);
  }
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

void WindowTreeClient::OnEmbed(ClientSpecificId client_id,
                                   mojom::WindowDataPtr root_data,
                                   mojom::WindowTreePtr tree,
                                   int64_t display_id,
                                   Id focused_window_id,
                                   bool drawn) {
  DCHECK(!tree_ptr_);
  tree_ptr_ = std::move(tree);
  tree_ptr_.set_connection_error_handler(
      base::Bind(&DeleteWindowTreeClient, this));

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
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(window).observers(),
                      OnWindowEmbeddedAppDisconnected(window));
  }
}

void WindowTreeClient::OnUnembed(Id window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  delegate_->OnUnembed(window);
  WindowPrivate(window).LocalDestroy();
}

void WindowTreeClient::OnLostCapture(Id window_id) {
  Window* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightCaptureChange reset_change(this, nullptr);
  if (ApplyServerChangeToExistingInFlightChange(reset_change))
    return;

  LocalSetCapture(nullptr);
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

  InFlightBoundsChange new_change(window, new_bounds);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  WindowPrivate(window).LocalSetBounds(old_bounds, new_bounds);
}

void WindowTreeClient::OnClientAreaChanged(
    uint32_t window_id,
    const gfx::Insets& new_client_area,
    mojo::Array<gfx::Rect> new_additional_client_areas) {
  Window* window = GetWindowByServerId(window_id);
  if (window) {
    WindowPrivate(window).LocalSetClientArea(
        new_client_area,
        new_additional_client_areas.To<std::vector<gfx::Rect>>());
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

  BuildWindowTree(this, windows, initial_parent);

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
                                          uint32_t event_observer_id) {
  DCHECK(event);
  Window* window = GetWindowByServerId(window_id);  // May be null.

  // Non-zero event_observer_id means it matched an event observer on the
  // server.
  if (event_observer_id != 0 && has_event_observer_ &&
      event_observer_id == event_observer_id_)
    delegate_->OnEventObserved(*event.get(), window);

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
    window->input_event_handler_->OnWindowInputEvent(
        window, ui::MouseEvent(*event->AsPointerEvent()), &ack_callback);
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

void WindowTreeClient::OnEventObserved(std::unique_ptr<ui::Event> event,
                                       uint32_t event_observer_id) {
  DCHECK(event);
  if (has_event_observer_ && event_observer_id == event_observer_id_)
    delegate_->OnEventObserved(*event.get(), nullptr /* target */);
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

  FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(window).observers(),
                    OnRequestClose(window));
}

void WindowTreeClient::OnConnect(ClientSpecificId client_id) {
  client_id_ = client_id;
}

void WindowTreeClient::WmNewDisplayAdded(mojom::DisplayPtr display,
                                         mojom::WindowDataPtr root_data,
                                         bool parent_drawn) {
  WmNewDisplayAddedImpl(display.To<display::Display>(), std::move(root_data),
                        parent_drawn);
}

void WindowTreeClient::WmSetBounds(uint32_t change_id,
                                       Id window_id,
                                       const gfx::Rect& transit_bounds) {
  Window* window = GetWindowByServerId(window_id);
  bool result = false;
  if (window) {
    DCHECK(window_manager_delegate_);
    gfx::Rect bounds = transit_bounds;
    result = window_manager_delegate_->OnWmSetBounds(window, &bounds);
    if (result) {
      // If the resulting bounds differ return false. Returning false ensures
      // the client applies the bounds we set below.
      result = bounds == transit_bounds;
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

void WindowTreeClient::OnAccelerator(uint32_t id,
                                     std::unique_ptr<ui::Event> event) {
  DCHECK(event);
  window_manager_delegate_->OnAccelerator(id, *event.get());
}

void WindowTreeClient::SetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  if (window_manager_internal_client_) {
    window_manager_internal_client_->WmSetFrameDecorationValues(
        std::move(values));
  }
}

void WindowTreeClient::SetNonClientCursor(Window* window,
                                              mus::mojom::Cursor cursor_id) {
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
    window_manager_internal_client_->SetUnderlaySurfaceOffsetAndExtendedHitArea(
        server_id(window), offset.x(), offset.y(), hit_area);
  }
}

}  // namespace mus
