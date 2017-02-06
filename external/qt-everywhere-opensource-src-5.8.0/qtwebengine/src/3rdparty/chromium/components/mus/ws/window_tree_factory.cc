// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_tree_factory.h"

#include "base/memory/ptr_util.h"
#include "components/mus/ws/default_access_policy.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"

namespace mus {
namespace ws {

WindowTreeFactory::WindowTreeFactory(WindowServer* window_server,
                                     const UserId& user_id,
                                     const std::string& client_name,
                                     mojom::WindowTreeFactoryRequest request)
    : window_server_(window_server),
      user_id_(user_id),
      client_name_(client_name),
      binding_(this, std::move(request)) {}

WindowTreeFactory::~WindowTreeFactory() {}

void WindowTreeFactory::CreateWindowTree(
    mojo::InterfaceRequest<mojom::WindowTree> tree_request,
    mojom::WindowTreeClientPtr client) {
  std::unique_ptr<ws::WindowTree> service(
      new ws::WindowTree(window_server_, user_id_, nullptr,
                         base::WrapUnique(new DefaultAccessPolicy)));
  std::unique_ptr<ws::DefaultWindowTreeBinding> binding(
      new ws::DefaultWindowTreeBinding(service.get(), window_server_,
                                       std::move(tree_request),
                                       std::move(client)));
  service->set_name(client_name_);
  window_server_->AddTree(std::move(service), std::move(binding), nullptr);
}

}  // namespace ws
}  // namespace mus
