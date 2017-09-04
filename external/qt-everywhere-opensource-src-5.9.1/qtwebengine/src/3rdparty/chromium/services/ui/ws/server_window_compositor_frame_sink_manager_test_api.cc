// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window_compositor_frame_sink_manager_test_api.h"

#include "services/ui/ws/server_window.h"

namespace ui {
namespace ws {

ServerWindowCompositorFrameSinkManagerTestApi::
    ServerWindowCompositorFrameSinkManagerTestApi(
        ServerWindowCompositorFrameSinkManager* manager)
    : manager_(manager) {}

ServerWindowCompositorFrameSinkManagerTestApi::
    ~ServerWindowCompositorFrameSinkManagerTestApi() {}

void ServerWindowCompositorFrameSinkManagerTestApi::
    CreateEmptyDefaultCompositorFrameSink() {
  manager_->type_to_compositor_frame_sink_map_
      [mojom::CompositorFrameSinkType::DEFAULT] = CompositorFrameSinkData();
}

void ServerWindowCompositorFrameSinkManagerTestApi::
    DestroyDefaultCompositorFrameSink() {
  manager_->type_to_compositor_frame_sink_map_.erase(
      mojom::CompositorFrameSinkType::DEFAULT);
}

void EnableHitTest(ServerWindow* window) {
  ServerWindowCompositorFrameSinkManagerTestApi test_api(
      window->GetOrCreateCompositorFrameSinkManager());
  test_api.CreateEmptyDefaultCompositorFrameSink();
}

void DisableHitTest(ServerWindow* window) {
  ServerWindowCompositorFrameSinkManagerTestApi test_api(
      window->GetOrCreateCompositorFrameSinkManager());
  test_api.DestroyDefaultCompositorFrameSink();
}

}  // namespace ws
}  // namespace ui
