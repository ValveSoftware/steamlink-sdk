// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_manager_delegate.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "ui/display/display.h"
#include "ui/display/test/test_screen.h"

namespace ui {
namespace test {

class TestWM : public service_manager::Service,
               public ui::WindowTreeClientDelegate,
               public ui::WindowManagerDelegate {
 public:
  TestWM() {}
  ~TestWM() override { display::Screen::SetScreenInstance(nullptr); }

 private:
  // service_manager::Service:
  void OnStart() override {
    test_screen_ = base::MakeUnique<display::test::TestScreen>();
    display::Screen::SetScreenInstance(test_screen_.get());
    window_tree_client_.reset(new ui::WindowTreeClient(this, this));
    window_tree_client_->ConnectAsWindowManager(context()->connector());
  }

  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    return false;
  }

  // ui::WindowTreeClientDelegate:
  void OnEmbed(ui::Window* root) override {
    // WindowTreeClients configured as the window manager should never get
    // OnEmbed().
    NOTREACHED();
  }
  void OnLostConnection(WindowTreeClient* client) override {
    window_tree_client_.reset();
  }
  void OnEmbedRootDestroyed(ui::Window* root) override {
    // WindowTreeClients configured as the window manager should never get
    // OnEmbedRootDestroyed().
    NOTREACHED();
  }
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              ui::Window* target) override {
    // Don't care.
  }

  // ui::WindowManagerDelegate:
  void SetWindowManagerClient(ui::WindowManagerClient* client) override {
    window_manager_client_ = client;
  }
  bool OnWmSetBounds(ui::Window* window, gfx::Rect* bounds) override {
    return true;
  }
  bool OnWmSetProperty(
      ui::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override {
    return true;
  }
  ui::Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override {
    ui::Window* window = root_->window_tree()->NewWindow(properties);
    window->SetBounds(gfx::Rect(10, 10, 500, 500));
    root_->AddChild(window);
    return window;
  }
  void OnWmClientJankinessChanged(const std::set<Window*>& client_windows,
                                  bool janky) override {
    // Don't care.
  }
  void OnWmNewDisplay(Window* window,
                      const display::Display& display) override {
    // Only handles a single root.
    DCHECK(!root_);
    root_ = window;
    DCHECK(window_manager_client_);
    window_manager_client_->AddActivationParent(root_);
    ui::mojom::FrameDecorationValuesPtr frame_decoration_values =
        ui::mojom::FrameDecorationValues::New();
    frame_decoration_values->max_title_bar_button_width = 0;
    window_manager_client_->SetFrameDecorationValues(
        std::move(frame_decoration_values));
  }
  void OnWmDisplayRemoved(ui::Window* window) override { window->Destroy(); }
  void OnWmDisplayModified(const display::Display& display) override {}
  void OnWmPerformMoveLoop(Window* window,
                           mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override {
    // Don't care.
  }
  void OnWmCancelMoveLoop(Window* window) override {}

  // Dummy screen required to be the screen instance.
  std::unique_ptr<display::test::TestScreen> test_screen_;

  ui::Window* root_ = nullptr;
  ui::WindowManagerClient* window_manager_client_ = nullptr;
  std::unique_ptr<ui::WindowTreeClient> window_tree_client_;

  DISALLOW_COPY_AND_ASSIGN(TestWM);
};

}  // namespace test
}  // namespace ui

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(new ui::test::TestWM);
  return runner.Run(service_request_handle);
}
