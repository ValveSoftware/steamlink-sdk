// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_TREE_FACTORY_H_
#define COMPONENTS_MUS_WS_WINDOW_TREE_FACTORY_H_

#include "base/macros.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/ws/user_id.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace mus {
namespace ws {

class WindowServer;

class WindowTreeFactory : public mus::mojom::WindowTreeFactory {
 public:
  WindowTreeFactory(WindowServer* window_server,
                    const UserId& user_id,
                    const std::string& client_name,
                    mojom::WindowTreeFactoryRequest request);
 private:
  ~WindowTreeFactory() override;

  // mus::mojom::WindowTreeFactory:
  void CreateWindowTree(mojo::InterfaceRequest<mojom::WindowTree> tree_request,
                        mojom::WindowTreeClientPtr client) override;

  WindowServer* window_server_;
  const UserId user_id_;
  const std::string client_name_;
  mojo::StrongBinding<mus::mojom::WindowTreeFactory> binding_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeFactory);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_TREE_FACTORY_H_
