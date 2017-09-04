// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_client.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/window_manager_window_tree_factory.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/mus/capture_synchronizer.h"
#include "ui/aura/mus/drag_drop_controller_mus.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/in_flight_change.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/surface_id_handler.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_client_observer.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

#if defined(HiWord)
#undef HiWord
#endif
#if defined(LoWord)
#undef LoWord
#endif

namespace aura {
namespace {

Id MakeTransportId(ClientSpecificId client_id, ClientSpecificId local_id) {
  return (client_id << 16) | local_id;
}

inline uint16_t HiWord(uint32_t id) {
  return static_cast<uint16_t>((id >> 16) & 0xFFFF);
}

inline uint16_t LoWord(uint32_t id) {
  return static_cast<uint16_t>(id & 0xFFFF);
}

struct WindowPortPropertyDataMus : public WindowPortPropertyData {
  std::string transport_name;
  std::unique_ptr<std::vector<uint8_t>> transport_value;
};

// Handles acknowledgment of an input event, either immediately when a nested
// message loop starts, or upon destruction.
class EventAckHandler : public base::MessageLoop::NestingObserver {
 public:
  explicit EventAckHandler(std::unique_ptr<EventResultCallback> ack_callback)
      : ack_callback_(std::move(ack_callback)) {
    DCHECK(ack_callback_);
    base::MessageLoop::current()->AddNestingObserver(this);
  }

  ~EventAckHandler() override {
    base::MessageLoop::current()->RemoveNestingObserver(this);
    if (ack_callback_) {
      ack_callback_->Run(handled_ ? ui::mojom::EventResult::HANDLED
                                  : ui::mojom::EventResult::UNHANDLED);
    }
  }

  void set_handled(bool handled) { handled_ = handled; }

  // base::MessageLoop::NestingObserver:
  void OnBeginNestedMessageLoop() override {
    // Acknowledge the event immediately if a nested message loop starts.
    // Otherwise we appear unresponsive for the life of the nested message loop.
    if (ack_callback_) {
      ack_callback_->Run(ui::mojom::EventResult::HANDLED);
      ack_callback_.reset();
    }
  }

 private:
  std::unique_ptr<EventResultCallback> ack_callback_;
  bool handled_ = false;

  DISALLOW_COPY_AND_ASSIGN(EventAckHandler);
};

WindowTreeHostMus* GetWindowTreeHostMus(Window* window) {
  return static_cast<WindowTreeHostMus*>(window->GetRootWindow()->GetHost());
}

WindowTreeHostMus* GetWindowTreeHostMus(WindowMus* window) {
  return GetWindowTreeHostMus(window->GetWindow());
}

bool IsInternalProperty(const void* key) {
  return key == client::kModalKey;
}

// Helper function to get the device_scale_factor() of the display::Display
// with |display_id|.
float ScaleFactorForDisplay(Window* window) {
  // TODO(riajiang): Change to use display::GetDisplayWithDisplayId() after
  // https://codereview.chromium.org/2361283002/ is landed.
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(window)
      .device_scale_factor();
}

}  // namespace

WindowTreeClient::WindowTreeClient(
    WindowTreeClientDelegate* delegate,
    WindowManagerDelegate* window_manager_delegate,
    mojo::InterfaceRequest<ui::mojom::WindowTreeClient> request)
    : client_id_(0),
      next_window_id_(1),
      next_change_id_(1),
      delegate_(delegate),
      window_manager_delegate_(window_manager_delegate),
      binding_(this),
      tree_(nullptr),
      in_destructor_(false),
      weak_factory_(this) {
  // Allow for a null request in tests.
  if (request.is_pending())
    binding_.Bind(std::move(request));
  client::GetTransientWindowClient()->AddObserver(this);
  if (window_manager_delegate)
    window_manager_delegate->SetWindowManagerClient(this);
}

WindowTreeClient::~WindowTreeClient() {
  in_destructor_ = true;

  for (WindowTreeClientObserver& observer : observers_)
    observer.OnWillDestroyClient(this);

  std::vector<Window*> non_owned;
  WindowTracker tracker;
  while (!windows_.empty()) {
    IdToWindowMap::iterator it = windows_.begin();
    if (WasCreatedByThisClient(it->second)) {
      const Id window_id = it->second->server_id();
      delete it->second->GetWindow();
      DCHECK_EQ(0u, windows_.count(window_id));
    } else {
      tracker.Add(it->second->GetWindow());
      windows_.erase(it);
    }
  }

  // Delete the non-owned windows last. In the typical case these are roots. The
  // exception is the window manager and embed roots, which may know about
  // other random windows that it doesn't own.
  while (!tracker.windows().empty())
    delete tracker.windows().front();

  for (WindowTreeClientObserver& observer : observers_)
    observer.OnDidDestroyClient(this);

  capture_synchronizer_.reset();

  client::GetTransientWindowClient()->RemoveObserver(this);
}

void WindowTreeClient::ConnectViaWindowTreeFactory(
    service_manager::Connector* connector) {
  // The client id doesn't really matter, we use 101 purely for debugging.
  client_id_ = 101;

  ui::mojom::WindowTreeFactoryPtr factory;
  connector->ConnectToInterface(ui::mojom::kServiceName, &factory);
  ui::mojom::WindowTreePtr window_tree;
  factory->CreateWindowTree(GetProxy(&window_tree),
                            binding_.CreateInterfacePtrAndBind());
  SetWindowTree(std::move(window_tree));
}

void WindowTreeClient::ConnectAsWindowManager(
    service_manager::Connector* connector) {
  DCHECK(window_manager_delegate_);

  ui::mojom::WindowManagerWindowTreeFactoryPtr factory;
  connector->ConnectToInterface(ui::mojom::kServiceName, &factory);
  ui::mojom::WindowTreePtr window_tree;
  factory->CreateWindowTree(GetProxy(&window_tree),
                            binding_.CreateInterfacePtrAndBind());
  SetWindowTree(std::move(window_tree));
}

void WindowTreeClient::SetClientArea(
    Window* window,
    const gfx::Insets& client_area,
    const std::vector<gfx::Rect>& additional_client_areas) {
  DCHECK(tree_);
  float device_scale_factor = ScaleFactorForDisplay(window);
  std::vector<gfx::Rect> additional_client_areas_in_pixel;
  for (const gfx::Rect& area : additional_client_areas) {
    additional_client_areas_in_pixel.push_back(
        gfx::ConvertRectToPixel(device_scale_factor, area));
  }
  tree_->SetClientArea(
      WindowMus::Get(window)->server_id(),
      gfx::ConvertInsetsToPixel(device_scale_factor, client_area),
      additional_client_areas_in_pixel);
}

void WindowTreeClient::SetHitTestMask(Window* window, const gfx::Rect& mask) {
  DCHECK(tree_);
  tree_->SetHitTestMask(
      WindowMus::Get(window)->server_id(),
      gfx::ConvertRectToPixel(ScaleFactorForDisplay(window), mask));
}

void WindowTreeClient::ClearHitTestMask(Window* window) {
  DCHECK(tree_);
  tree_->SetHitTestMask(WindowMus::Get(window)->server_id(), base::nullopt);
}

void WindowTreeClient::SetCanFocus(Window* window, bool can_focus) {
  DCHECK(tree_);
  DCHECK(window);
  tree_->SetCanFocus(WindowMus::Get(window)->server_id(), can_focus);
}

void WindowTreeClient::SetPredefinedCursor(WindowMus* window,
                                           ui::mojom::Cursor old_cursor,
                                           ui::mojom::Cursor new_cursor) {
  DCHECK(tree_);

  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<InFlightPredefinedCursorChange>(window, old_cursor));
  tree_->SetPredefinedCursor(change_id, window->server_id(), new_cursor);
}

void WindowTreeClient::SetWindowTextInputState(WindowMus* window,
                                               mojo::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetWindowTextInputState(window->server_id(), std::move(state));
}

void WindowTreeClient::SetImeVisibility(WindowMus* window,
                                        bool visible,
                                        mojo::TextInputStatePtr state) {
  DCHECK(tree_);
  tree_->SetImeVisibility(window->server_id(), visible, std::move(state));
}

void WindowTreeClient::Embed(
    Id window_id,
    ui::mojom::WindowTreeClientPtr client,
    uint32_t flags,
    const ui::mojom::WindowTree::EmbedCallback& callback) {
  DCHECK(tree_);
  tree_->Embed(window_id, std::move(client), flags, callback);
}

void WindowTreeClient::RequestClose(Window* window) {
  DCHECK(window);
  if (window_manager_internal_client_)
    window_manager_internal_client_->WmRequestClose(
        WindowMus::Get(window)->server_id());
}

void WindowTreeClient::AttachCompositorFrameSink(
    Id window_id,
    ui::mojom::CompositorFrameSinkType type,
    cc::mojom::MojoCompositorFrameSinkRequest compositor_frame_sink,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  DCHECK(tree_);
  tree_->AttachCompositorFrameSink(
      window_id, type, std::move(compositor_frame_sink), std::move(client));
}

void WindowTreeClient::RegisterWindowMus(WindowMus* window) {
  DCHECK(windows_.find(window->server_id()) == windows_.end());
  windows_[window->server_id()] = window;
}

WindowMus* WindowTreeClient::GetWindowByServerId(Id id) {
  IdToWindowMap::const_iterator it = windows_.find(id);
  return it != windows_.end() ? it->second : nullptr;
}

bool WindowTreeClient::WasCreatedByThisClient(const WindowMus* window) const {
  // Windows created via CreateTopLevelWindow() are not owned by us, but have
  // our client id. const_cast is required by set.
  return HiWord(window->server_id()) == client_id_ &&
         roots_.count(const_cast<WindowMus*>(window)) == 0;
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
    const mojo::Array<ui::mojom::WindowDataPtr>& windows) {
  for (const auto& window_data : windows) {
    WindowMus* parent = window_data->parent_id == kInvalidServerId
                            ? nullptr
                            : GetWindowByServerId(window_data->parent_id);
    WindowMus* existing_window = GetWindowByServerId(window_data->window_id);
    if (!existing_window)
      NewWindowFromWindowData(parent, window_data);
    else if (parent)
      parent->AddChildFromServer(existing_window);
  }
}

std::unique_ptr<WindowPortMus> WindowTreeClient::CreateWindowPortMus(
    const ui::mojom::WindowDataPtr& window_data,
    WindowMusType window_mus_type) {
  std::unique_ptr<WindowPortMus> window_port_mus(
      base::MakeUnique<WindowPortMus>(this, window_mus_type));
  window_port_mus->set_server_id(window_data->window_id);
  RegisterWindowMus(window_port_mus.get());
  return window_port_mus;
}

void WindowTreeClient::SetLocalPropertiesFromServerProperties(
    WindowMus* window,
    const ui::mojom::WindowDataPtr& window_data) {
  for (auto& pair : window_data->properties) {
    if (pair.second.is_null()) {
      window->SetPropertyFromServer(pair.first, nullptr);
    } else {
      std::vector<uint8_t> stl_value = pair.second.To<std::vector<uint8_t>>();
      window->SetPropertyFromServer(pair.first, &stl_value);
    }
  }
}

std::unique_ptr<WindowTreeHostMus> WindowTreeClient::CreateWindowTreeHost(
    WindowMusType window_mus_type,
    const ui::mojom::WindowDataPtr& window_data,
    int64_t display_id) {
  std::unique_ptr<WindowPortMus> window_port =
      CreateWindowPortMus(window_data, window_mus_type);
  roots_.insert(window_port.get());
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      base::MakeUnique<WindowTreeHostMus>(std::move(window_port), this,
                                          display_id);
  if (!window_data.is_null()) {
    SetLocalPropertiesFromServerProperties(
        WindowMus::Get(window_tree_host->window()), window_data);
  }
  return window_tree_host;
}

WindowMus* WindowTreeClient::NewWindowFromWindowData(
    WindowMus* parent,
    const ui::mojom::WindowDataPtr& window_data) {
  // This function is only called for windows coming from other clients.
  std::unique_ptr<WindowPortMus> window_port_mus(
      CreateWindowPortMus(window_data, WindowMusType::OTHER));
  WindowPortMus* window_port_mus_ptr = window_port_mus.get();
  Window* window = new Window(nullptr, std::move(window_port_mus));
  WindowMus* window_mus = window_port_mus_ptr;
  window->Init(ui::LAYER_NOT_DRAWN);
  SetLocalPropertiesFromServerProperties(window_mus, window_data);
  window_mus->SetBoundsFromServer(window_data->bounds);
  if (parent)
    parent->AddChildFromServer(window_port_mus_ptr);
  if (window_data->visible)
    window_mus->SetVisibleFromServer(true);
  return window_port_mus_ptr;
}

void WindowTreeClient::SetWindowTree(ui::mojom::WindowTreePtr window_tree_ptr) {
  tree_ptr_ = std::move(window_tree_ptr);
  WindowTreeConnectionEstablished(tree_ptr_.get());
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

void WindowTreeClient::WindowTreeConnectionEstablished(
    ui::mojom::WindowTree* window_tree) {
  tree_ = window_tree;

  drag_drop_controller_ = base::MakeUnique<DragDropControllerMus>(this, tree_);
  capture_synchronizer_ = base::MakeUnique<CaptureSynchronizer>(
      this, tree_, delegate_->GetCaptureClient());
  focus_synchronizer_ = base::MakeUnique<FocusSynchronizer>(this, tree_);
}

void WindowTreeClient::OnConnectionLost() {
  delegate_->OnLostConnection(this);
}

bool WindowTreeClient::HandleInternalPropertyChanged(WindowMus* window,
                                                     const void* key) {
  if (key != client::kModalKey)
    return false;

  if (window->GetWindow()->GetProperty(client::kModalKey) ==
      ui::MODAL_TYPE_NONE) {
    // TODO: shouldn't early return, but explicitly tell server to turn off
    // modality. http://crbug.com/660073.
    return true;
  }

  const uint32_t change_id =
      ScheduleInFlightChange(base::MakeUnique<InFlightSetModalChange>(window));
  // TODO: this is subtly different that explicitly specifying a type.
  // http://crbug.com/660073.
  tree_->SetModal(change_id, window->server_id());
  return true;
}

void WindowTreeClient::OnEmbedImpl(ui::mojom::WindowTree* window_tree,
                                   ClientSpecificId client_id,
                                   ui::mojom::WindowDataPtr root_data,
                                   int64_t display_id,
                                   Id focused_window_id,
                                   bool drawn) {
  // WARNING: this is only called if WindowTreeClient was created as the
  // result of an embedding.
  client_id_ = client_id;
  WindowTreeConnectionEstablished(window_tree);

  DCHECK(roots_.empty());
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      CreateWindowTreeHost(WindowMusType::EMBED, root_data, display_id);

  focus_synchronizer_->SetFocusFromServer(
      GetWindowByServerId(focused_window_id));

  delegate_->OnEmbed(std::move(window_tree_host));
}

WindowTreeHost* WindowTreeClient::WmNewDisplayAddedImpl(
    const display::Display& display,
    ui::mojom::WindowDataPtr root_data,
    bool parent_drawn) {
  DCHECK(window_manager_delegate_);

  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      CreateWindowTreeHost(WindowMusType::DISPLAY, root_data, display.id());

  WindowTreeHost* window_tree_host_ptr = window_tree_host.get();
  window_manager_delegate_->OnWmNewDisplay(std::move(window_tree_host),
                                           display);
  return window_tree_host_ptr;
}

std::unique_ptr<EventResultCallback>
WindowTreeClient::CreateEventResultCallback(int32_t event_id) {
  return base::MakeUnique<EventResultCallback>(
      base::Bind(&ui::mojom::WindowTree::OnWindowInputEventAck,
                 base::Unretained(tree_), event_id));
}

void WindowTreeClient::OnReceivedCursorLocationMemory(
    mojo::ScopedSharedBufferHandle handle) {
  cursor_location_mapping_ = handle->Map(sizeof(base::subtle::Atomic32));
  DCHECK(cursor_location_mapping_);
}

void WindowTreeClient::SetWindowBoundsFromServer(
    WindowMus* window,
    const gfx::Rect& revert_bounds) {
  if (IsRoot(window)) {
    GetWindowTreeHostMus(window)->SetBoundsFromServer(revert_bounds);
    return;
  }

  window->SetBoundsFromServer(revert_bounds);
}

void WindowTreeClient::SetWindowVisibleFromServer(WindowMus* window,
                                                  bool visible) {
  if (!IsRoot(window)) {
    window->SetVisibleFromServer(visible);
    return;
  }

  std::unique_ptr<WindowMusChangeData> data =
      window->PrepareForServerVisibilityChange(visible);
  WindowTreeHostMus* window_tree_host = GetWindowTreeHostMus(window);
  if (visible)
    window_tree_host->Show();
  else
    window_tree_host->Hide();
}

void WindowTreeClient::ScheduleInFlightBoundsChange(
    WindowMus* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<InFlightBoundsChange>(this, window, old_bounds));
  tree_->SetWindowBounds(change_id, window->server_id(), new_bounds);
}

void WindowTreeClient::OnWindowMusCreated(WindowMus* window) {
  if (window->server_id() != kInvalidServerId)
    return;

  window->set_server_id(MakeTransportId(client_id_, next_window_id_++));
  RegisterWindowMus(window);

  const bool create_top_level = !window_manager_delegate_ && IsRoot(window);

  mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties;
  std::set<const void*> property_keys =
      window->GetWindow()->GetAllPropertKeys();
  PropertyConverter* property_converter = delegate_->GetPropertyConverter();
  for (const void* key : property_keys) {
    std::string transport_name;
    std::unique_ptr<std::vector<uint8_t>> transport_value;
    if (!property_converter->ConvertPropertyForTransport(
            window->GetWindow(), key, &transport_name, &transport_value)) {
      continue;
    }
    if (!transport_value) {
      transport_properties[transport_name] = mojo::Array<uint8_t>(nullptr);
    } else {
      transport_properties[transport_name] =
          mojo::Array<uint8_t>::From(*transport_value);
    }
  }

  const uint32_t change_id =
      ScheduleInFlightChange(base::MakeUnique<CrashInFlightChange>(
          window, create_top_level ? ChangeType::NEW_TOP_LEVEL_WINDOW
                                   : ChangeType::NEW_WINDOW));
  if (create_top_level) {
    tree_->NewTopLevelWindow(change_id, window->server_id(),
                             std::move(transport_properties));
  } else {
    tree_->NewWindow(change_id, window->server_id(),
                     std::move(transport_properties));
  }
}

void WindowTreeClient::OnWindowMusDestroyed(WindowMus* window) {
  if (focus_synchronizer_->focused_window() == window)
    focus_synchronizer_->OnFocusedWindowDestroyed();

  // TODO: decide how to deal with windows not owned by this client.
  if (WasCreatedByThisClient(window) || IsRoot(window)) {
    const uint32_t change_id =
        ScheduleInFlightChange(base::MakeUnique<CrashInFlightChange>(
            window, ChangeType::DELETE_WINDOW));
    tree_->DeleteWindow(change_id, window->server_id());
  }

  windows_.erase(window->server_id());

  for (auto& entry : embedded_windows_) {
    auto it = entry.second.find(window->GetWindow());
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
    delegate_->OnEmbedRootDestroyed(window->GetWindow());
}

void WindowTreeClient::OnWindowMusBoundsChanged(WindowMus* window,
                                                const gfx::Rect& old_bounds,
                                                const gfx::Rect& new_bounds) {
  // Changes to bounds of root windows are routed through
  // OnWindowTreeHostBoundsWillChange(). Any bounds that happen here are a side
  // effect of those and can be ignored.
  if (IsRoot(window))
    return;

  ScheduleInFlightBoundsChange(window, old_bounds, new_bounds);
}

void WindowTreeClient::OnWindowMusAddChild(WindowMus* parent,
                                           WindowMus* child) {
  // TODO: add checks to ensure this can work.
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<CrashInFlightChange>(parent, ChangeType::ADD_CHILD));
  tree_->AddWindow(change_id, parent->server_id(), child->server_id());
}

void WindowTreeClient::OnWindowMusRemoveChild(WindowMus* parent,
                                              WindowMus* child) {
  // TODO: add checks to ensure this can work.
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<CrashInFlightChange>(parent, ChangeType::REMOVE_CHILD));
  tree_->RemoveWindowFromParent(change_id, child->server_id());
}

void WindowTreeClient::OnWindowMusMoveChild(WindowMus* parent,
                                            size_t current_index,
                                            size_t dest_index) {
  // TODO: add checks to ensure this can work, e.g. we own the parent.
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<CrashInFlightChange>(parent, ChangeType::REORDER));
  WindowMus* window =
      WindowMus::Get(parent->GetWindow()->children()[current_index]);
  WindowMus* relative_window = nullptr;
  ui::mojom::OrderDirection direction;
  if (dest_index < current_index) {
    relative_window =
        WindowMus::Get(parent->GetWindow()->children()[dest_index]);
    direction = ui::mojom::OrderDirection::BELOW;
  } else {
    relative_window =
        WindowMus::Get(parent->GetWindow()->children()[dest_index]);
    direction = ui::mojom::OrderDirection::ABOVE;
  }
  tree_->ReorderWindow(change_id, window->server_id(),
                       relative_window->server_id(), direction);
}

void WindowTreeClient::OnWindowMusSetVisible(WindowMus* window, bool visible) {
  // TODO: add checks to ensure this can work.
  DCHECK(tree_);
  const uint32_t change_id = ScheduleInFlightChange(
      base::MakeUnique<InFlightVisibleChange>(this, window, !visible));
  tree_->SetWindowVisibility(change_id, window->server_id(), visible);
}

std::unique_ptr<WindowPortPropertyData>
WindowTreeClient::OnWindowMusWillChangeProperty(WindowMus* window,
                                                const void* key) {
  if (IsInternalProperty(key))
    return nullptr;

  std::unique_ptr<WindowPortPropertyDataMus> data(
      base::MakeUnique<WindowPortPropertyDataMus>());
  if (!delegate_->GetPropertyConverter()->ConvertPropertyForTransport(
          window->GetWindow(), key, &data->transport_name,
          &data->transport_value)) {
    return nullptr;
  }
  return std::move(data);
}

void WindowTreeClient::OnWindowMusPropertyChanged(
    WindowMus* window,
    const void* key,
    std::unique_ptr<WindowPortPropertyData> data) {
  if (HandleInternalPropertyChanged(window, key) || !data)
    return;

  WindowPortPropertyDataMus* data_mus =
      static_cast<WindowPortPropertyDataMus*>(data.get());

  std::string transport_name;
  std::unique_ptr<std::vector<uint8_t>> transport_value;
  if (!delegate_->GetPropertyConverter()->ConvertPropertyForTransport(
          window->GetWindow(), key, &transport_name, &transport_value)) {
    return;
  }
  DCHECK_EQ(transport_name, data_mus->transport_name);

  mojo::Array<uint8_t> transport_value_mojo(nullptr);
  if (transport_value) {
    transport_value_mojo.resize(transport_value->size());
    if (transport_value->size()) {
      memcpy(&transport_value_mojo.front(), &(transport_value->front()),
             transport_value->size());
    }
  }
  const uint32_t change_id =
      ScheduleInFlightChange(base::MakeUnique<InFlightPropertyChange>(
          window, transport_name, std::move(data_mus->transport_value)));
  tree_->SetWindowProperty(change_id, window->server_id(),
                           mojo::String(transport_name),
                           std::move(transport_value_mojo));
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

std::set<Window*> WindowTreeClient::GetRoots() {
  std::set<Window*> roots;
  for (WindowMus* window : roots_)
    roots.insert(window->GetWindow());
  return roots;
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

void WindowTreeClient::PerformWindowMove(
    Window* window,
    ui::mojom::MoveLoopSource source,
    const gfx::Point& cursor_location,
    const base::Callback<void(bool)>& callback) {
  DCHECK(on_current_move_finished_.is_null());
  on_current_move_finished_ = callback;

  WindowMus* window_mus = WindowMus::Get(window);
  current_move_loop_change_ = ScheduleInFlightChange(
      base::MakeUnique<InFlightDragChange>(window_mus, ChangeType::MOVE_LOOP));
  // Tell the window manager to take over moving us.
  tree_->PerformWindowMove(current_move_loop_change_, window_mus->server_id(),
                           source, cursor_location);
}

void WindowTreeClient::CancelWindowMove(Window* window) {
  tree_->CancelWindowMove(WindowMus::Get(window)->server_id());
}

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
                               ui::mojom::WindowDataPtr root_data,
                               ui::mojom::WindowTreePtr tree,
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
  WindowMus* window = GetWindowByServerId(window_id);
  if (window)
    window->NotifyEmbeddedAppDisconnected();
}

void WindowTreeClient::OnUnembed(Id window_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  delegate_->OnUnembed(window->GetWindow());
  delete window;
}

void WindowTreeClient::OnCaptureChanged(Id new_capture_window_id,
                                        Id old_capture_window_id) {
  WindowMus* new_capture_window = GetWindowByServerId(new_capture_window_id);
  WindowMus* lost_capture_window = GetWindowByServerId(old_capture_window_id);
  if (!new_capture_window && !lost_capture_window)
    return;

  InFlightCaptureChange change(this, capture_synchronizer_.get(),
                               new_capture_window);
  if (ApplyServerChangeToExistingInFlightChange(change))
    return;

  capture_synchronizer_->SetCaptureFromServer(new_capture_window);
}

void WindowTreeClient::OnTopLevelCreated(uint32_t change_id,
                                         ui::mojom::WindowDataPtr data,
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

  WindowMus* window = change->window();
  WindowTreeHostMus* window_tree_host = GetWindowTreeHostMus(window);

  // Drawn state and display-id always come from the server (they can't be
  // modified locally).
  window_tree_host->set_display_id(display_id);

  // The default visibilty is false, we only need update visibility if it
  // differs from that.
  if (data->visible) {
    InFlightVisibleChange visible_change(this, window, data->visible);
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(visible_change);
    if (current_change)
      current_change->SetRevertValueFrom(visible_change);
    else
      SetWindowVisibleFromServer(window, true);
  }

  const gfx::Rect bounds(data->bounds);
  {
    InFlightBoundsChange bounds_change(this, window, bounds);
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(bounds_change);
    if (current_change)
      current_change->SetRevertValueFrom(bounds_change);
    else if (window->GetWindow()->bounds() != bounds)
      SetWindowBoundsFromServer(window, bounds);
  }

  // There is currently no API to bulk set properties, so we iterate over each
  // property individually.
  std::map<std::string, std::vector<uint8_t>> properties =
      data->properties.To<std::map<std::string, std::vector<uint8_t>>>();
  for (const auto& pair : properties) {
    std::unique_ptr<std::vector<uint8_t>> revert_value(
        base::MakeUnique<std::vector<uint8_t>>(pair.second));
    InFlightPropertyChange property_change(window, pair.first,
                                           std::move(revert_value));
    InFlightChange* current_change =
        GetOldestInFlightChangeMatching(property_change);
    if (current_change) {
      current_change->SetRevertValueFrom(property_change);
    } else {
      window->SetPropertyFromServer(pair.first, &pair.second);
    }
  }

  // Top level windows should not have a parent.
  DCHECK_EQ(0u, data->parent_id);
}

void WindowTreeClient::OnWindowBoundsChanged(Id window_id,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightBoundsChange new_change(this, window, new_bounds);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  SetWindowBoundsFromServer(window, new_bounds);
}

void WindowTreeClient::OnClientAreaChanged(
    uint32_t window_id,
    const gfx::Insets& new_client_area,
    mojo::Array<gfx::Rect> new_additional_client_areas) {
  // TODO: client area.
  // TODO(riajiang): Convert from pixel to DIP. (http://crbug.com/600815)
  /*
  Window* window = GetWindowByServerId(window_id);
  if (window) {
    WindowPrivate(window).LocalSetClientArea(
        new_client_area,
        new_additional_client_areas.To<std::vector<gfx::Rect>>());
  }
  */
}

void WindowTreeClient::OnTransientWindowAdded(uint32_t window_id,
                                              uint32_t transient_window_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  WindowMus* transient_window = GetWindowByServerId(transient_window_id);
  // window or transient_window or both may be null if a local delete occurs
  // with an in flight add from the server.
  if (window && transient_window)
    window->AddTransientChildFromServer(transient_window);
}

void WindowTreeClient::OnTransientWindowRemoved(uint32_t window_id,
                                                uint32_t transient_window_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  WindowMus* transient_window = GetWindowByServerId(transient_window_id);
  // window or transient_window or both may be null if a local delete occurs
  // with an in flight delete from the server.
  if (window && transient_window)
    window->RemoveTransientChildFromServer(transient_window);
}

void WindowTreeClient::OnWindowHierarchyChanged(
    Id window_id,
    Id old_parent_id,
    Id new_parent_id,
    mojo::Array<ui::mojom::WindowDataPtr> windows) {
  const bool was_window_known = GetWindowByServerId(window_id) != nullptr;

  BuildWindowTree(windows);

  // If the window was not known, then BuildWindowTree() will have created it
  // and parented the window.
  if (!was_window_known)
    return;

  WindowMus* new_parent = GetWindowByServerId(new_parent_id);
  WindowMus* old_parent = GetWindowByServerId(old_parent_id);
  WindowMus* window = GetWindowByServerId(window_id);
  if (new_parent)
    new_parent->AddChildFromServer(window);
  else
    old_parent->RemoveChildFromServer(window);
}

void WindowTreeClient::OnWindowReordered(Id window_id,
                                         Id relative_window_id,
                                         ui::mojom::OrderDirection direction) {
  WindowMus* window = GetWindowByServerId(window_id);
  WindowMus* relative_window = GetWindowByServerId(relative_window_id);
  WindowMus* parent = WindowMus::Get(window->GetWindow()->parent());
  if (window && relative_window && parent &&
      parent == WindowMus::Get(relative_window->GetWindow()->parent())) {
    parent->ReorderFromServer(window, relative_window, direction);
  }
}

void WindowTreeClient::OnWindowDeleted(Id window_id) {
  // TODO(sky): decide how best to deal with this. It seems we should let the
  // delegate do the actualy deletion.
  delete GetWindowByServerId(window_id)->GetWindow();
}

void WindowTreeClient::OnWindowVisibilityChanged(Id window_id, bool visible) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightVisibleChange new_change(this, window, visible);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  SetWindowVisibleFromServer(window, visible);
}

void WindowTreeClient::OnWindowOpacityChanged(Id window_id,
                                              float old_opacity,
                                              float new_opacity) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightOpacityChange new_change(window, new_opacity);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  window->SetOpacityFromServer(new_opacity);
}

void WindowTreeClient::OnWindowParentDrawnStateChanged(Id window_id,
                                                       bool drawn) {
  // TODO: route to WindowTreeHost.
  /*
  Window* window = GetWindowByServerId(window_id);
  if (window)
    WindowPrivate(window).LocalSetParentDrawn(drawn);
  */
}

void WindowTreeClient::OnWindowSharedPropertyChanged(
    Id window_id,
    const mojo::String& name,
    mojo::Array<uint8_t> transport_data) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  std::unique_ptr<std::vector<uint8_t>> data;
  if (!transport_data.is_null()) {
    data = base::MakeUnique<std::vector<uint8_t>>(
        transport_data.To<std::vector<uint8_t>>());
  }
  InFlightPropertyChange new_change(window, name, std::move(data));
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  if (!transport_data.is_null()) {
    data = base::MakeUnique<std::vector<uint8_t>>(
        transport_data.To<std::vector<uint8_t>>());
  }
  window->SetPropertyFromServer(name, data.get());
}

void WindowTreeClient::OnWindowInputEvent(uint32_t event_id,
                                          Id window_id,
                                          std::unique_ptr<ui::Event> event,
                                          bool matches_pointer_watcher) {
  DCHECK(event);

  WindowMus* window = GetWindowByServerId(window_id);  // May be null.

  if (event->IsKeyEvent()) {
    DCHECK(!matches_pointer_watcher);  // PointerWatcher isn't for key events.
    if (!window || !window->GetWindow()->GetHost()) {
      tree_->OnWindowInputEventAck(event_id, ui::mojom::EventResult::UNHANDLED);
      return;
    }
    InputMethodMus* input_method = GetWindowTreeHostMus(window)->input_method();
    input_method->DispatchKeyEvent(event->AsKeyEvent(),
                                   CreateEventResultCallback(event_id));
    return;
  }

  if (matches_pointer_watcher && has_pointer_watcher_) {
    DCHECK(event->IsPointerEvent());
    delegate_->OnPointerEventObserved(*event->AsPointerEvent(),
                                      window ? window->GetWindow() : nullptr);
  }

  // TODO: deal with no window or host here. This could happen if during
  // dispatch a window is deleted or moved. In either case we still need to
  // dispatch. Most likely need the display id.
  if (!window || !window->GetWindow()->GetHost()) {
    tree_->OnWindowInputEventAck(event_id, ui::mojom::EventResult::UNHANDLED);
    return;
  }

  EventAckHandler ack_handler(CreateEventResultCallback(event_id));
  WindowTreeHostMus* host = GetWindowTreeHostMus(window);
  // TODO(moshayedi): crbug.com/617222. No need to convert to ui::MouseEvent or
  // ui::TouchEvent once we have proper support for pointer events.
  if (event->IsMousePointerEvent()) {
    if (event->type() == ui::ET_POINTER_WHEEL_CHANGED) {
      ui::MouseWheelEvent mapped_event(*event->AsPointerEvent());
      host->SendEventToProcessor(&mapped_event);
    } else {
      ui::MouseEvent mapped_event(*event->AsPointerEvent());
      host->SendEventToProcessor(&mapped_event);
    }
  } else if (event->IsTouchPointerEvent()) {
    ui::TouchEvent mapped_event(*event->AsPointerEvent());
    host->SendEventToProcessor(&mapped_event);
  } else {
    host->SendEventToProcessor(event.get());
  }
  ack_handler.set_handled(event->handled());
}

void WindowTreeClient::OnPointerEventObserved(std::unique_ptr<ui::Event> event,
                                              uint32_t window_id) {
  DCHECK(event);
  DCHECK(event->IsPointerEvent());
  if (!has_pointer_watcher_)
    return;

  WindowMus* target_window = GetWindowByServerId(window_id);
  delegate_->OnPointerEventObserved(
      *event->AsPointerEvent(),
      target_window ? target_window->GetWindow() : nullptr);
}

void WindowTreeClient::OnWindowFocused(Id focused_window_id) {
  WindowMus* focused_window = GetWindowByServerId(focused_window_id);
  InFlightFocusChange new_change(this, focus_synchronizer_.get(),
                                 focused_window);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  focus_synchronizer_->SetFocusFromServer(focused_window);
}

void WindowTreeClient::OnWindowPredefinedCursorChanged(
    Id window_id,
    ui::mojom::Cursor cursor) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;

  InFlightPredefinedCursorChange new_change(window, cursor);
  if (ApplyServerChangeToExistingInFlightChange(new_change))
    return;

  window->SetPredefinedCursorFromServer(cursor);
}

void WindowTreeClient::OnWindowSurfaceChanged(
    Id window_id,
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size,
    float device_scale_factor) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window)
    return;
  std::unique_ptr<SurfaceInfo> surface_info(base::MakeUnique<SurfaceInfo>());
  surface_info->surface_id = surface_id;
  surface_info->frame_size = frame_size;
  surface_info->device_scale_factor = device_scale_factor;
  window->SetSurfaceIdFromServer(std::move(surface_info));
}

void WindowTreeClient::OnDragDropStart(
    mojo::Map<mojo::String, mojo::Array<uint8_t>> mime_data) {
  drag_drop_controller_->OnDragDropStart(
      mime_data.To<std::map<std::string, std::vector<uint8_t>>>());
}

void WindowTreeClient::OnDragEnter(Id window_id,
                                   uint32_t key_state,
                                   const gfx::Point& position,
                                   uint32_t effect_bitmask,
                                   const OnDragEnterCallback& callback) {
  callback.Run(drag_drop_controller_->OnDragEnter(
      GetWindowByServerId(window_id), key_state, position, effect_bitmask));
}

void WindowTreeClient::OnDragOver(Id window_id,
                                  uint32_t key_state,
                                  const gfx::Point& position,
                                  uint32_t effect_bitmask,
                                  const OnDragOverCallback& callback) {
  callback.Run(drag_drop_controller_->OnDragOver(
      GetWindowByServerId(window_id), key_state, position, effect_bitmask));
}

void WindowTreeClient::OnDragLeave(Id window_id) {
  drag_drop_controller_->OnDragLeave(GetWindowByServerId(window_id));
}

void WindowTreeClient::OnDragDropDone() {
  drag_drop_controller_->OnDragDropDone();
}

void WindowTreeClient::OnCompleteDrop(Id window_id,
                                      uint32_t key_state,
                                      const gfx::Point& position,
                                      uint32_t effect_bitmask,
                                      const OnCompleteDropCallback& callback) {
  callback.Run(drag_drop_controller_->OnCompleteDrop(
      GetWindowByServerId(window_id), key_state, position, effect_bitmask));
}

void WindowTreeClient::OnPerformDragDropCompleted(uint32_t change_id,
                                                  bool success,
                                                  uint32_t action_taken) {
  if (drag_drop_controller_->DoesChangeIdMatchDragChangeId(change_id)) {
    OnChangeCompleted(change_id, success);
    drag_drop_controller_->OnPerformDragDropCompleted(action_taken);
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
}

void WindowTreeClient::GetWindowManager(
    mojo::AssociatedInterfaceRequest<WindowManager> internal) {
  window_manager_internal_.reset(
      new mojo::AssociatedBinding<ui::mojom::WindowManager>(
          this, std::move(internal)));
}

void WindowTreeClient::RequestClose(uint32_t window_id) {
  WindowMus* window = GetWindowByServerId(window_id);
  if (!window || !IsRoot(window))
    return;

  window->GetWindow()->delegate()->OnRequestClose();
}

void WindowTreeClient::OnConnect(ClientSpecificId client_id) {
  client_id_ = client_id;
}

void WindowTreeClient::WmNewDisplayAdded(const display::Display& display,
                                         ui::mojom::WindowDataPtr root_data,
                                         bool parent_drawn) {
  WmNewDisplayAddedImpl(display, std::move(root_data), parent_drawn);
}

void WindowTreeClient::WmDisplayRemoved(int64_t display_id) {
  DCHECK(window_manager_delegate_);
  // TODO: route to WindowTreeHost.
  /*
  for (Window* root : roots_) {
    if (root->display_id() == display_id) {
      window_manager_delegate_->OnWmDisplayRemoved(root);
      return;
    }
  }
  */
}

void WindowTreeClient::WmDisplayModified(const display::Display& display) {
  DCHECK(window_manager_delegate_);
  // TODO(sky): this should likely route to WindowTreeHost.
  window_manager_delegate_->OnWmDisplayModified(display);
}

// TODO(riajiang): Convert between pixel and DIP for window bounds properly.
// (http://crbug.com/646942)
void WindowTreeClient::WmSetBounds(uint32_t change_id,
                                   Id window_id,
                                   const gfx::Rect& transit_bounds) {
  WindowMus* window = GetWindowByServerId(window_id);
  bool result = false;
  if (window) {
    DCHECK(window_manager_delegate_);
    gfx::Rect bounds = transit_bounds;
    // TODO: this needs to trigger scheduling a bounds change on |window|.
    result =
        window_manager_delegate_->OnWmSetBounds(window->GetWindow(), &bounds);
    if (result) {
      // If the resulting bounds differ return false. Returning false ensures
      // the client applies the bounds we set below.
      result = bounds == transit_bounds;
      window->SetBoundsFromServer(bounds);
    }
  }
  if (window_manager_internal_client_)
    window_manager_internal_client_->WmResponse(change_id, result);
}

void WindowTreeClient::WmSetProperty(uint32_t change_id,
                                     Id window_id,
                                     const mojo::String& name,
                                     mojo::Array<uint8_t> transit_data) {
  WindowMus* window = GetWindowByServerId(window_id);
  bool result = false;
  if (window) {
    DCHECK(window_manager_delegate_);
    std::unique_ptr<std::vector<uint8_t>> data;
    if (!transit_data.is_null()) {
      data.reset(
          new std::vector<uint8_t>(transit_data.To<std::vector<uint8_t>>()));
    }
    result = window_manager_delegate_->OnWmSetProperty(window->GetWindow(),
                                                       name, &data);
    if (result) {
      delegate_->GetPropertyConverter()->SetPropertyFromTransportValue(
          window->GetWindow(), name, data.get());
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
        change_id, WindowMus::Get(window)->server_id());
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
                                         ui::mojom::MoveLoopSource source,
                                         const gfx::Point& cursor_location) {
  if (!window_manager_delegate_ || current_wm_move_loop_change_ != 0) {
    OnWmMoveLoopCompleted(change_id, false);
    return;
  }

  current_wm_move_loop_change_ = change_id;
  current_wm_move_loop_window_id_ = window_id;
  WindowMus* window = GetWindowByServerId(window_id);
  if (window) {
    window_manager_delegate_->OnWmPerformMoveLoop(
        window->GetWindow(), source, cursor_location,
        base::Bind(&WindowTreeClient::OnWmMoveLoopCompleted,
                   weak_factory_.GetWeakPtr(), change_id));
  } else {
    OnWmMoveLoopCompleted(change_id, false);
  }
}

void WindowTreeClient::WmCancelMoveLoop(uint32_t change_id) {
  if (!window_manager_delegate_ || change_id != current_wm_move_loop_change_)
    return;

  WindowMus* window = GetWindowByServerId(current_wm_move_loop_window_id_);
  if (window)
    window_manager_delegate_->OnWmCancelMoveLoop(window->GetWindow());
}

void WindowTreeClient::OnAccelerator(uint32_t ack_id,
                                     uint32_t accelerator_id,
                                     std::unique_ptr<ui::Event> event) {
  DCHECK(event);
  const ui::mojom::EventResult result =
      window_manager_delegate_->OnAccelerator(accelerator_id, *event.get());
  if (ack_id && window_manager_internal_client_)
    window_manager_internal_client_->OnAcceleratorAck(ack_id, result);
}

void WindowTreeClient::SetFrameDecorationValues(
    ui::mojom::FrameDecorationValuesPtr values) {
  if (window_manager_internal_client_) {
    window_manager_internal_client_->WmSetFrameDecorationValues(
        std::move(values));
  }
}

void WindowTreeClient::SetNonClientCursor(Window* window,
                                          ui::mojom::Cursor cursor_id) {
  window_manager_internal_client_->WmSetNonClientCursor(
      WindowMus::Get(window)->server_id(), cursor_id);
}

void WindowTreeClient::AddAccelerator(
    uint32_t id,
    ui::mojom::EventMatcherPtr event_matcher,
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
  if (window_manager_internal_client_) {
    window_manager_internal_client_->AddActivationParent(
        WindowMus::Get(window)->server_id());
  }
}

void WindowTreeClient::RemoveActivationParent(Window* window) {
  if (window_manager_internal_client_) {
    window_manager_internal_client_->RemoveActivationParent(
        WindowMus::Get(window)->server_id());
  }
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
        WindowMus::Get(window)->server_id(), offset.x(), offset.y(),
        gfx::ConvertInsetsToDIP(ScaleFactorForDisplay(window), hit_area));
  }
}

void WindowTreeClient::OnWindowTreeHostBoundsWillChange(
    WindowTreeHostMus* window_tree_host,
    const gfx::Rect& bounds) {
  ScheduleInFlightBoundsChange(WindowMus::Get(window_tree_host->window()),
                               window_tree_host->GetBounds(), bounds);
}

std::unique_ptr<WindowPortMus> WindowTreeClient::CreateWindowPortForTopLevel() {
  std::unique_ptr<WindowPortMus> window_port =
      base::MakeUnique<WindowPortMus>(this, WindowMusType::TOP_LEVEL);
  roots_.insert(window_port.get());
  return window_port;
}

void WindowTreeClient::OnWindowTreeHostCreated(
    WindowTreeHostMus* window_tree_host) {
  // All WindowTreeHosts are destroyed before this, so we don't need to unset
  // the DragDropClient.
  client::SetDragDropClient(window_tree_host->window(),
                            drag_drop_controller_.get());
}

void WindowTreeClient::OnTransientChildWindowAdded(Window* parent,
                                                   Window* transient_child) {
  if (WindowMus::Get(parent)->OnTransientChildAdded(
          WindowMus::Get(transient_child)) == WindowMus::ChangeSource::SERVER) {
    return;
  }
  // The change originated from client code and needs to be sent to the server.
  DCHECK(tree_);
  WindowMus* parent_mus = WindowMus::Get(parent);
  const uint32_t change_id =
      ScheduleInFlightChange(base::MakeUnique<CrashInFlightChange>(
          parent_mus, ChangeType::ADD_TRANSIENT_WINDOW));
  tree_->AddTransientWindow(change_id, parent_mus->server_id(),
                            WindowMus::Get(transient_child)->server_id());
}

void WindowTreeClient::OnTransientChildWindowRemoved(Window* parent,
                                                     Window* transient_child) {
  if (WindowMus::Get(parent)->OnTransientChildRemoved(
          WindowMus::Get(transient_child)) == WindowMus::ChangeSource::SERVER) {
    return;
  }
  // The change originated from client code and needs to be sent to the server.
  DCHECK(tree_);
  WindowMus* child_mus = WindowMus::Get(transient_child);
  const uint32_t change_id =
      ScheduleInFlightChange(base::MakeUnique<CrashInFlightChange>(
          child_mus, ChangeType::REMOVE_TRANSIENT_WINDOW_FROM_PARENT));
  tree_->RemoveTransientWindowFromParent(change_id, child_mus->server_id());
}

uint32_t WindowTreeClient::CreateChangeIdForDrag(WindowMus* window) {
  return ScheduleInFlightChange(
      base::MakeUnique<InFlightDragChange>(window, ChangeType::DRAG_LOOP));
}

uint32_t WindowTreeClient::CreateChangeIdForCapture(WindowMus* window) {
  return ScheduleInFlightChange(base::MakeUnique<InFlightCaptureChange>(
      this, capture_synchronizer_.get(), window));
}

uint32_t WindowTreeClient::CreateChangeIdForFocus(WindowMus* window) {
  return ScheduleInFlightChange(base::MakeUnique<InFlightFocusChange>(
      this, focus_synchronizer_.get(), window));
}

}  // namespace aura
