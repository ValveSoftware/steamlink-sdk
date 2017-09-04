// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/mus_client.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/interfaces/event_matcher.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/gpu_service.h"
#include "ui/aura/mus/mus_context_factory.h"
#include "ui/aura/mus/os_exchange_data_provider_mus.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/clipboard_mus.h"
#include "ui/views/mus/desktop_window_tree_host_mus.h"
#include "ui/views/mus/screen_mus.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/wm_state.h"

namespace views {

// static
MusClient* MusClient::instance_ = nullptr;

MusClient::~MusClient() {
  // ~WindowTreeClient calls back to us (we're its delegate), destroy it while
  // we are still valid.
  window_tree_client_.reset();
  ui::OSExchangeDataProviderFactory::SetFactory(nullptr);
  ui::Clipboard::DestroyClipboardForCurrentThread();
  gpu_service_.reset();

  if (ViewsDelegate::GetInstance()) {
    ViewsDelegate::GetInstance()->set_native_widget_factory(
        ViewsDelegate::NativeWidgetFactory());
  }

  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

// static
bool MusClient::ShouldCreateDesktopNativeWidgetAura(
    const Widget::InitParams& init_params) {
  // TYPE_CONTROL and child widgets require a NativeWidgetAura.
  return init_params.type != Widget::InitParams::TYPE_CONTROL &&
         !init_params.child;
}

NativeWidget* MusClient::CreateNativeWidget(
    const Widget::InitParams& init_params,
    internal::NativeWidgetDelegate* delegate) {
  if (!ShouldCreateDesktopNativeWidgetAura(init_params)) {
    // A null return value results in creating NativeWidgetAura.
    return nullptr;
  }

  DesktopNativeWidgetAura* native_widget =
      new DesktopNativeWidgetAura(delegate);
  if (init_params.desktop_window_tree_host) {
    native_widget->SetDesktopWindowTreeHost(
        base::WrapUnique(init_params.desktop_window_tree_host));
  } else {
    native_widget->SetDesktopWindowTreeHost(
        base::MakeUnique<DesktopWindowTreeHostMus>(delegate, native_widget,
                                                   init_params));
  }
  return native_widget;
}

MusClient::MusClient(service_manager::Connector* connector,
                     const service_manager::Identity& identity,
                     scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : connector_(connector), identity_(identity) {
  DCHECK(!instance_);
  instance_ = this;
  // TODO(msw): Avoid this... use some default value? Allow clients to extend?
  property_converter_ = base::MakeUnique<aura::PropertyConverter>();

  wm_state_ = base::MakeUnique<wm::WMState>();

  gpu_service_ = aura::GpuService::Create(connector, std::move(io_task_runner));
  compositor_context_factory_ =
      base::MakeUnique<aura::MusContextFactory>(gpu_service_.get());
  aura::Env::GetInstance()->set_context_factory(
      compositor_context_factory_.get());
  window_tree_client_ =
      base::MakeUnique<aura::WindowTreeClient>(this, nullptr, nullptr);
  aura::Env::GetInstance()->SetWindowTreeClient(window_tree_client_.get());
  window_tree_client_->ConnectViaWindowTreeFactory(connector_);

  // TODO: wire up PointerWatcherEventRouter. http://crbug.com/663526.

  screen_ = base::MakeUnique<ScreenMus>(this);
  screen_->Init(connector);

  std::unique_ptr<ClipboardMus> clipboard = base::MakeUnique<ClipboardMus>();
  clipboard->Init(connector);
  ui::Clipboard::SetClipboardForCurrentThread(std::move(clipboard));

  ui::OSExchangeDataProviderFactory::SetFactory(this);

  ViewsDelegate::GetInstance()->set_native_widget_factory(
      base::Bind(&MusClient::CreateNativeWidget, base::Unretained(this)));
}

void MusClient::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  NOTREACHED();
}

void MusClient::OnLostConnection(aura::WindowTreeClient* client) {}

void MusClient::OnEmbedRootDestroyed(aura::Window* root) {
  // Not called for MusClient as WindowTreeClient isn't created by
  // way of an Embed().
  NOTREACHED();
}

void MusClient::OnPointerEventObserved(const ui::PointerEvent& event,
                                       aura::Window* target) {
  // TODO: wire up PointerWatcherEventRouter. http://crbug.com/663526.
  NOTIMPLEMENTED();
}

void MusClient::OnWindowManagerFrameValuesChanged() {
  // TODO: wire up client area. http://crbug.com/663525.
  NOTIMPLEMENTED();
}

aura::client::CaptureClient* MusClient::GetCaptureClient() {
  return wm::CaptureController::Get();
}

aura::PropertyConverter* MusClient::GetPropertyConverter() {
  return property_converter_.get();
}

gfx::Point MusClient::GetCursorScreenPoint() {
  return window_tree_client_->GetCursorScreenPoint();
}

aura::Window* MusClient::GetWindowAtScreenPoint(const gfx::Point& point) {
  for (aura::Window* root : window_tree_client_->GetRoots()) {
    aura::WindowTreeHost* window_tree_host = root->GetHost();
    if (!window_tree_host)
      continue;
    // TODO: this likely gets z-order wrong. http://crbug.com/663606.
    gfx::Point relative_point(point);
    window_tree_host->ConvertPointFromNativeScreen(&relative_point);
    if (gfx::Rect(root->bounds().size()).Contains(relative_point))
      return root->GetTopWindowContainingPoint(relative_point);
  }
  return nullptr;
}

std::unique_ptr<OSExchangeData::Provider> MusClient::BuildProvider() {
  return base::MakeUnique<aura::OSExchangeDataProviderMus>();
}

}  // namespace views
