// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/window_server_test_impl.h"

#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree.h"

namespace ui {
namespace ws {

namespace {

bool WindowHasValidFrame(const ServerWindow* window) {
  const ServerWindowCompositorFrameSinkManager* manager =
      window->compositor_frame_sink_manager();
  return manager &&
         !manager->GetLatestFrameSize(mojom::CompositorFrameSinkType::DEFAULT)
              .IsEmpty();
}

}  // namespace

WindowServerTestImpl::WindowServerTestImpl(WindowServer* window_server)
    : window_server_(window_server) {}

WindowServerTestImpl::~WindowServerTestImpl() {}

void WindowServerTestImpl::OnWindowPaint(
    const std::string& name,
    const EnsureClientHasDrawnWindowCallback& cb,
    ServerWindow* window) {
  WindowTree* tree = window_server_->GetTreeWithClientName(name);
  if (!tree)
    return;
  if (tree->HasRoot(window) && WindowHasValidFrame(window)) {
    cb.Run(true);
    window_server_->SetPaintCallback(base::Callback<void(ServerWindow*)>());
  }
}

void WindowServerTestImpl::EnsureClientHasDrawnWindow(
    const std::string& client_name,
    const EnsureClientHasDrawnWindowCallback& callback) {
  WindowTree* tree = window_server_->GetTreeWithClientName(client_name);
  if (tree) {
    for (const ServerWindow* window : tree->roots()) {
      if (WindowHasValidFrame(window)) {
        callback.Run(true);
        return;
      }
    }
  }

  window_server_->SetPaintCallback(
      base::Bind(&WindowServerTestImpl::OnWindowPaint, base::Unretained(this),
                 client_name, std::move(callback)));
}

}  // namespace ws
}  // namespace ui
