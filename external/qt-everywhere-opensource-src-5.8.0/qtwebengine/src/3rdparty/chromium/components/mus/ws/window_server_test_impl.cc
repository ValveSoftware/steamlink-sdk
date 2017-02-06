// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_server_test_impl.h"

#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_surface_manager.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_tree.h"

namespace mus {
namespace ws {

namespace {

bool WindowHasValidFrame(const ServerWindow* window) {
  const ServerWindowSurfaceManager* manager = window->surface_manager();
  return manager &&
         !manager->GetDefaultSurface()->last_submitted_frame_size().IsEmpty();
}

}  // namespace

WindowServerTestImpl::WindowServerTestImpl(
    WindowServer* window_server,
    mojo::InterfaceRequest<WindowServerTest> request)
    : window_server_(window_server), binding_(this, std::move(request)) {}

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
    const mojo::String& client_name,
    const EnsureClientHasDrawnWindowCallback& callback) {
  std::string name = client_name.To<std::string>();
  WindowTree* tree = window_server_->GetTreeWithClientName(name);
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
                 name, std::move(callback)));
}

}  // namespace ws
}  // namespace mus
