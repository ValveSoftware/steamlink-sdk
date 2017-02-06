// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_host_factory.h"

#include "components/mus/gles2/gpu_state.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/window_server.h"

namespace mus {
namespace ws {

WindowTreeHostFactory::WindowTreeHostFactory(
    WindowServer* window_server,
    const UserId& user_id,
    const PlatformDisplayInitParams& platform_display_init_params)
    : window_server_(window_server),
      user_id_(user_id),
      platform_display_init_params_(platform_display_init_params) {}

WindowTreeHostFactory::~WindowTreeHostFactory() {}

void WindowTreeHostFactory::AddBinding(
    mojom::WindowTreeHostFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void WindowTreeHostFactory::CreateWindowTreeHost(
    mojom::WindowTreeHostRequest host,
    mojom::WindowTreeClientPtr tree_client) {
  Display* display = new Display(window_server_, platform_display_init_params_);
  std::unique_ptr<DisplayBindingImpl> display_binding(
      new DisplayBindingImpl(std::move(host), display, user_id_,
                             std::move(tree_client), window_server_));
  display->Init(std::move(display_binding));
}

}  // namespace ws
}  // namespace mus
