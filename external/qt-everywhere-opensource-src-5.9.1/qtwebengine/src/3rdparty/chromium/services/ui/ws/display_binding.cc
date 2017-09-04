// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/display_binding.h"

#include "base/memory/ptr_util.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/window_manager_access_policy.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_tree.h"

namespace ui {
namespace ws {

DisplayBindingImpl::DisplayBindingImpl(mojom::WindowTreeHostRequest request,
                                       Display* display,
                                       const UserId& user_id,
                                       mojom::WindowTreeClientPtr client,
                                       WindowServer* window_server)
    : window_server_(window_server),
      user_id_(user_id),
      binding_(display, std::move(request)),
      client_(std::move(client)) {}

DisplayBindingImpl::~DisplayBindingImpl() {}

WindowTree* DisplayBindingImpl::CreateWindowTree(ServerWindow* root) {
  const uint32_t embed_flags = 0;
  WindowTree* tree = window_server_->EmbedAtWindow(
      root, user_id_, std::move(client_), embed_flags,
      base::WrapUnique(new WindowManagerAccessPolicy));
  tree->ConfigureWindowManager();
  return tree;
}

}  // namespace ws
}  // namespace ui
