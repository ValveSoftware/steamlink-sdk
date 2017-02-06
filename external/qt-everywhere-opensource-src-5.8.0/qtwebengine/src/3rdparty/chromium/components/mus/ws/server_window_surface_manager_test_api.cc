// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_window_surface_manager_test_api.h"

#include "components/mus/ws/server_window.h"

namespace mus {
namespace ws {

ServerWindowSurfaceManagerTestApi::ServerWindowSurfaceManagerTestApi(
    ServerWindowSurfaceManager* manager)
    : manager_(manager) {}

ServerWindowSurfaceManagerTestApi::~ServerWindowSurfaceManagerTestApi() {}

void ServerWindowSurfaceManagerTestApi::CreateEmptyDefaultSurface() {
  manager_->type_to_surface_map_[mojom::SurfaceType::DEFAULT] = nullptr;
}

void ServerWindowSurfaceManagerTestApi::DestroyDefaultSurface() {
  manager_->type_to_surface_map_.erase(mojom::SurfaceType::DEFAULT);
}

void EnableHitTest(ServerWindow* window) {
  ServerWindowSurfaceManagerTestApi test_api(
      window->GetOrCreateSurfaceManager());
  test_api.CreateEmptyDefaultSurface();
}

void DisableHitTest(ServerWindow* window) {
  ServerWindowSurfaceManagerTestApi test_api(
      window->GetOrCreateSurfaceManager());
  test_api.DestroyDefaultSurface();
}

}  // namespace ws
}  // namespace mus
