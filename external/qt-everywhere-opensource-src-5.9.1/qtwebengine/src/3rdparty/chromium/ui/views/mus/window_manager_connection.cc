// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_manager_connection.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_local.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/cpp/gpu_service.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/interfaces/event_matcher.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/os_exchange_data_provider_mus.h"
#include "ui/views/mus/clipboard_mus.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/pointer_watcher_event_router.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/mus/surface_context_factory.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/views_delegate.h"

namespace views {
namespace {

using WindowManagerConnectionPtr =
    base::ThreadLocalPointer<views::WindowManagerConnection>;

// Env is thread local so that aura may be used on multiple threads.
base::LazyInstance<WindowManagerConnectionPtr>::Leaky lazy_tls_ptr =
    LAZY_INSTANCE_INITIALIZER;

// Recursively finds the deepest visible window from |windows| that contains
// |screen_point|, when offsetting by the display origins from
// |display_origins|.
ui::Window* GetWindowFrom(const std::map<int64_t, gfx::Point>& display_origins,
                          const std::vector<ui::Window*>& windows,
                          const gfx::Point& screen_point) {
  for (ui::Window* window : windows) {
    if (!window->visible())
      continue;

    auto it = display_origins.find(window->display_id());
    if (it == display_origins.end())
      continue;

    gfx::Rect bounds_in_screen = window->GetBoundsInRoot();
    bounds_in_screen.Offset(it->second.x(), it->second.y());

    if (bounds_in_screen.Contains(screen_point)) {
      if (!window->children().empty()) {
        ui::Window* child =
            GetWindowFrom(display_origins, window->children(), screen_point);
        if (child)
          return child;
      }

      return window;
    }
  }
  return nullptr;
}

aura::Window* GetAuraWindowFromUiWindow(ui::Window* window) {
  if (!window)
    return nullptr;
  NativeWidgetMus* nw_mus = NativeWidgetMus::GetForWindow(window);
  return nw_mus
             ? static_cast<internal::NativeWidgetPrivate*>(nw_mus)
                   ->GetNativeView()
             : nullptr;
}

}  // namespace

WindowManagerConnection::~WindowManagerConnection() {
  // ~WindowTreeClient calls back to us (we're its delegate), destroy it while
  // we are still valid.
  client_.reset();
  ui::OSExchangeDataProviderFactory::SetFactory(nullptr);
  ui::Clipboard::DestroyClipboardForCurrentThread();
  gpu_service_.reset();
  lazy_tls_ptr.Pointer()->Set(nullptr);

  if (ViewsDelegate::GetInstance()) {
    ViewsDelegate::GetInstance()->set_native_widget_factory(
        ViewsDelegate::NativeWidgetFactory());
  }
}

// static
std::unique_ptr<WindowManagerConnection> WindowManagerConnection::Create(
    service_manager::Connector* connector,
    const service_manager::Identity& identity,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!lazy_tls_ptr.Pointer()->Get());
  WindowManagerConnection* connection =
      new WindowManagerConnection(connector, identity, std::move(task_runner));
  DCHECK(lazy_tls_ptr.Pointer()->Get());
  return base::WrapUnique(connection);
}

// static
WindowManagerConnection* WindowManagerConnection::Get() {
  WindowManagerConnection* connection = lazy_tls_ptr.Pointer()->Get();
  DCHECK(connection);
  return connection;
}

// static
bool WindowManagerConnection::Exists() {
  return !!lazy_tls_ptr.Pointer()->Get();
}

ui::Window* WindowManagerConnection::NewTopLevelWindow(
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  return client_->NewTopLevelWindow(&properties);
}

NativeWidget* WindowManagerConnection::CreateNativeWidgetMus(
    const std::map<std::string, std::vector<uint8_t>>& props,
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
  // TYPE_CONTROL widgets require a NativeWidgetAura. So we let this fall
  // through, so that the default NativeWidgetPrivate::CreateNativeWidget() is
  // used instead.
  if (init_params.type == Widget::InitParams::TYPE_CONTROL)
    return nullptr;
  std::map<std::string, std::vector<uint8_t>> properties = props;
  NativeWidgetMus::ConfigurePropertiesForNewWindow(init_params, &properties);
  properties[ui::mojom::WindowManager::kAppID_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(identity_.name());
  return new NativeWidgetMus(delegate, NewTopLevelWindow(properties),
                             ui::mojom::CompositorFrameSinkType::DEFAULT);
}

const std::set<ui::Window*>& WindowManagerConnection::GetRoots() const {
  return client_->GetRoots();
}

WindowManagerConnection::WindowManagerConnection(
    service_manager::Connector* connector,
    const service_manager::Identity& identity,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : connector_(connector), identity_(identity) {
  lazy_tls_ptr.Pointer()->Set(this);

  gpu_service_ = ui::GpuService::Create(connector, std::move(task_runner));
  compositor_context_factory_ =
      base::MakeUnique<views::SurfaceContextFactory>(gpu_service_.get());
  aura::Env::GetInstance()->set_context_factory(
      compositor_context_factory_.get());
  client_ = base::MakeUnique<ui::WindowTreeClient>(this, nullptr, nullptr);
  client_->ConnectViaWindowTreeFactory(connector_);

  pointer_watcher_event_router_ =
      base::MakeUnique<PointerWatcherEventRouter>(client_.get());

  screen_ = base::MakeUnique<ScreenMus>(this);
  screen_->Init(connector);

  std::unique_ptr<ClipboardMus> clipboard = base::MakeUnique<ClipboardMus>();
  clipboard->Init(connector);
  ui::Clipboard::SetClipboardForCurrentThread(std::move(clipboard));

  ui::OSExchangeDataProviderFactory::SetFactory(this);

  ViewsDelegate::GetInstance()->set_native_widget_factory(base::Bind(
      &WindowManagerConnection::CreateNativeWidgetMus,
      base::Unretained(this),
      std::map<std::string, std::vector<uint8_t>>()));
}

ui::Window* WindowManagerConnection::GetUiWindowAtScreenPoint(
    const gfx::Point& point) {
  std::map<int64_t, gfx::Point> display_origins;
  for (display::Display& d : display::Screen::GetScreen()->GetAllDisplays())
    display_origins[d.id()] = d.bounds().origin();

  const std::set<ui::Window*>& roots = GetRoots();
  std::vector<ui::Window*> windows;
  std::copy(roots.begin(), roots.end(), std::back_inserter(windows));
  return GetWindowFrom(display_origins, windows, point);
}

void WindowManagerConnection::OnEmbed(ui::Window* root) {}

void WindowManagerConnection::OnLostConnection(ui::WindowTreeClient* client) {
  DCHECK_EQ(client, client_.get());
  client_.reset();
}

void WindowManagerConnection::OnEmbedRootDestroyed(ui::Window* root) {
  // Not called for WindowManagerConnection as WindowTreeClient isn't created by
  // way of an Embed().
  NOTREACHED();
}

void WindowManagerConnection::OnPointerEventObserved(
    const ui::PointerEvent& event,
    ui::Window* target) {
  pointer_watcher_event_router_->OnPointerEventObserved(event, target);
}

void WindowManagerConnection::OnWindowManagerFrameValuesChanged() {
  if (client_)
    NativeWidgetMus::NotifyFrameChanged(client_.get());
}

gfx::Point WindowManagerConnection::GetCursorScreenPoint() {
  return client_->GetCursorScreenPoint();
}

aura::Window* WindowManagerConnection::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  return GetAuraWindowFromUiWindow(GetUiWindowAtScreenPoint(point));
}

std::unique_ptr<OSExchangeData::Provider>
WindowManagerConnection::BuildProvider() {
  return base::MakeUnique<aura::OSExchangeDataProviderMus>();
}

}  // namespace views
