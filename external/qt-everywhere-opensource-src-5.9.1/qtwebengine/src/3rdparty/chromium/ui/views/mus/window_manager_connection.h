// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_WINDOW_MANAGER_CONNECTION_H_
#define UI_VIEWS_MUS_WINDOW_MANAGER_CONNECTION_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "ui/views/mus/mus_export.h"
#include "ui/views/mus/screen_mus_delegate.h"
#include "ui/views/widget/widget.h"

namespace service_manager {
class Connector;
}

namespace ui {
class GpuService;
}

namespace views {
class NativeWidget;
class PointerWatcherEventRouter;
class ScreenMus;
class SurfaceContextFactory;
namespace internal {
class NativeWidgetDelegate;
}

namespace test {
class WindowManagerConnectionTestApi;
}

// Provides configuration to mus in views. This consists of the following:
// . Provides a Screen implementation backed by mus.
// . Provides a Clipboard implementation backed by mus.
// . Creates and owns a WindowTreeClient.
// . Registers itself as the factory for creating NativeWidgets so that a
//   NativeWidgetMus is created.
// WindowManagerConnection is a singleton and should be created early on.
//
// TODO(sky): this name is now totally confusing. Come up with a better one.
class VIEWS_MUS_EXPORT WindowManagerConnection
    : public NON_EXPORTED_BASE(ui::WindowTreeClientDelegate),
      public ScreenMusDelegate,
      public ui::OSExchangeDataProviderFactory::Factory {
 public:
  ~WindowManagerConnection() override;

  // |io_task_runner| is used by the gpu service. If no task runner is provided,
  // then a new thread is created and used by ui::GpuService.
  static std::unique_ptr<WindowManagerConnection> Create(
      service_manager::Connector* connector,
      const service_manager::Identity& identity,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner = nullptr);
  static WindowManagerConnection* Get();
  static bool Exists();

  PointerWatcherEventRouter* pointer_watcher_event_router() {
    return pointer_watcher_event_router_.get();
  }
  service_manager::Connector* connector() { return connector_; }
  ui::GpuService* gpu_service() { return gpu_service_.get(); }
  ui::WindowTreeClient* client() { return client_.get(); }

  ui::Window* NewTopLevelWindow(
      const std::map<std::string, std::vector<uint8_t>>& properties);

  NativeWidget* CreateNativeWidgetMus(
      const std::map<std::string, std::vector<uint8_t>>& properties,
      const Widget::InitParams& init_params,
      internal::NativeWidgetDelegate* delegate);

  const std::set<ui::Window*>& GetRoots() const;

 private:
  friend class test::WindowManagerConnectionTestApi;

  WindowManagerConnection(
      service_manager::Connector* connector,
      const service_manager::Identity& identity,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Exposed for tests.
  ui::Window* GetUiWindowAtScreenPoint(const gfx::Point& point);

  // ui::WindowTreeClientDelegate:
  void OnEmbed(ui::Window* root) override;
  void OnLostConnection(ui::WindowTreeClient* client) override;
  void OnEmbedRootDestroyed(ui::Window* root) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              ui::Window* target) override;

  // ScreenMusDelegate:
  void OnWindowManagerFrameValuesChanged() override;
  gfx::Point GetCursorScreenPoint() override;
  aura::Window* GetWindowAtScreenPoint(const gfx::Point& point) override;

  // ui:OSExchangeDataProviderFactory::Factory:
  std::unique_ptr<OSExchangeData::Provider> BuildProvider() override;

  service_manager::Connector* connector_;
  service_manager::Identity identity_;
  std::unique_ptr<ScreenMus> screen_;
  std::unique_ptr<ui::WindowTreeClient> client_;
  std::unique_ptr<ui::GpuService> gpu_service_;
  std::unique_ptr<PointerWatcherEventRouter> pointer_watcher_event_router_;
  std::unique_ptr<SurfaceContextFactory> compositor_context_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnection);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_WINDOW_MANAGER_CONNECTION_H_
