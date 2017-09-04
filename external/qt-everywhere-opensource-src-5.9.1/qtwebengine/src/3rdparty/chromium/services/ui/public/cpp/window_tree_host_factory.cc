// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/window_tree_host_factory.h"

#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "services/ui/public/interfaces/constants.mojom.h"

namespace ui {

std::unique_ptr<WindowTreeClient> CreateWindowTreeHost(
    mojom::WindowTreeHostFactory* factory,
    WindowTreeClientDelegate* delegate,
    mojom::WindowTreeHostPtr* host,
    WindowManagerDelegate* window_manager_delegate) {
  mojom::WindowTreeClientPtr tree_client;
  std::unique_ptr<WindowTreeClient> window_tree_client =
      base::MakeUnique<WindowTreeClient>(delegate, window_manager_delegate,
                                         GetProxy(&tree_client));
  factory->CreateWindowTreeHost(GetProxy(host), std::move(tree_client));
  return window_tree_client;
}

std::unique_ptr<WindowTreeClient> CreateWindowTreeHost(
    service_manager::Connector* connector,
    WindowTreeClientDelegate* delegate,
    mojom::WindowTreeHostPtr* host,
    WindowManagerDelegate* window_manager_delegate) {
  mojom::WindowTreeHostFactoryPtr factory;
  connector->ConnectToInterface(ui::mojom::kServiceName, &factory);
  return CreateWindowTreeHost(factory.get(), delegate, host,
                              window_manager_delegate);
}

}  // namespace ui
