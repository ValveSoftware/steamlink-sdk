// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "components/mus/public/cpp/window_tree_client_delegate.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/cpp/application_runner.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/shell_client.h"
#include "ui/display/display.h"
#include "ui/display/mojo/display_type_converters.h"

namespace mus {
namespace test {

class TestWM : public shell::ShellClient,
               public mus::WindowTreeClientDelegate,
               public mus::WindowManagerDelegate {
 public:
  TestWM() {}
  ~TestWM() override { delete window_tree_client_; }

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override {
    window_tree_client_ = new mus::WindowTreeClient(this, this, nullptr);
    window_tree_client_->ConnectAsWindowManager(connector);
  }
  bool AcceptConnection(shell::Connection* connection) override {
    return true;
  }

  // mus::WindowTreeClientDelegate:
  void OnEmbed(mus::Window* root) override {
    // WindowTreeClients configured as the window manager should never get
    // OnEmbed().
    NOTREACHED();
  }
  void OnWindowTreeClientDestroyed(mus::WindowTreeClient* client) override {
    window_tree_client_ = nullptr;
  }
  void OnEventObserved(const ui::Event& event, mus::Window* target) override {
    // Don't care.
  }

  // mus::WindowManagerDelegate:
  void SetWindowManagerClient(mus::WindowManagerClient* client) override {
    window_manager_client_ = client;
  }
  bool OnWmSetBounds(mus::Window* window, gfx::Rect* bounds) override {
    return true;
  }
  bool OnWmSetProperty(
      mus::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override {
    return true;
  }
  mus::Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override {
    mus::Window* window = root_->window_tree()->NewWindow(properties);
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
    mus::mojom::FrameDecorationValuesPtr frame_decoration_values =
        mus::mojom::FrameDecorationValues::New();
    frame_decoration_values->max_title_bar_button_width = 0;
    window_manager_client_->SetFrameDecorationValues(
        std::move(frame_decoration_values));
  }
  void OnAccelerator(uint32_t id, const ui::Event& event) override {
    // Don't care.
  }

  mus::Window* root_ = nullptr;
  mus::WindowManagerClient* window_manager_client_ = nullptr;
  // See WindowTreeClient for details on ownership.
  mus::WindowTreeClient* window_tree_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestWM);
};

}  // namespace test
}  // namespace mus

MojoResult MojoMain(MojoHandle shell_handle) {
  shell::ApplicationRunner runner(new mus::test::TestWM);
  return runner.Run(shell_handle);
}
