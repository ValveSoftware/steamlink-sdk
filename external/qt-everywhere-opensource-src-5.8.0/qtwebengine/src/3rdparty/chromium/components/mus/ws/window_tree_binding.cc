// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_binding.h"

#include "base/bind.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_tree.h"

namespace mus {
namespace ws {

WindowTreeBinding::WindowTreeBinding(mojom::WindowTreeClient* client)
    : client_(client) {}

WindowTreeBinding::~WindowTreeBinding() {}

DefaultWindowTreeBinding::DefaultWindowTreeBinding(
    WindowTree* tree,
    WindowServer* window_server,
    mojom::WindowTreeRequest service_request,
    mojom::WindowTreeClientPtr client)
    : WindowTreeBinding(client.get()),
      binding_(tree, std::move(service_request)),
      client_(std::move(client)) {
  // Both |window_server| and |tree| outlive us.
  binding_.set_connection_error_handler(
      base::Bind(&WindowServer::DestroyTree, base::Unretained(window_server),
                 base::Unretained(tree)));
}

DefaultWindowTreeBinding::DefaultWindowTreeBinding(
    WindowTree* tree,
    mojom::WindowTreeClientPtr client)
    : WindowTreeBinding(client.get()),
      binding_(tree),
      client_(std::move(client)) {}

DefaultWindowTreeBinding::~DefaultWindowTreeBinding() {}

void DefaultWindowTreeBinding::SetIncomingMethodCallProcessingPaused(
    bool paused) {
  if (paused)
    binding_.PauseIncomingMethodCallProcessing();
  else
    binding_.ResumeIncomingMethodCallProcessing();
}

mojom::WindowTreePtr DefaultWindowTreeBinding::CreateInterfacePtrAndBind() {
  DCHECK(!binding_.is_bound());
  return binding_.CreateInterfacePtrAndBind();
}

mojom::WindowManager* DefaultWindowTreeBinding::GetWindowManager() {
  client_->GetWindowManager(
      GetProxy(&window_manager_internal_, client_.associated_group()));
  return window_manager_internal_.get();
}

}  // namespace ws
}  // namespace mus
