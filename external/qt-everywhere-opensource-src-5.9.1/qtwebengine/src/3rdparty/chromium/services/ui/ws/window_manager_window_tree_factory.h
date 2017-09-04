// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_WINDOW_MANAGER_WINDOW_TREE_FACTORY_H_
#define SERVICES_UI_WS_WINDOW_MANAGER_WINDOW_TREE_FACTORY_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/window_manager_window_tree_factory.mojom.h"
#include "services/ui/ws/user_id.h"

namespace ui {
namespace ws {

class ServerWindow;
class WindowManagerWindowTreeFactorySet;
class WindowServer;
class WindowTree;

// Implementation of mojom::WindowManagerWindowTreeFactory.
class WindowManagerWindowTreeFactory
    : public mojom::WindowManagerWindowTreeFactory {
 public:
  WindowManagerWindowTreeFactory(
      WindowManagerWindowTreeFactorySet* window_manager_window_tree_factory_set,
      const UserId& user_id,
      mojo::InterfaceRequest<mojom::WindowManagerWindowTreeFactory> request);
  ~WindowManagerWindowTreeFactory() override;

  const UserId& user_id() const { return user_id_; }

  WindowTree* window_tree() { return window_tree_; }

  // mojom::WindowManagerWindowTreeFactory:
  void CreateWindowTree(mojom::WindowTreeRequest window_tree_request,
                        mojom::WindowTreeClientPtr window_tree_client) override;

 private:
  // Used by tests.
  WindowManagerWindowTreeFactory(WindowManagerWindowTreeFactorySet* registry,
                                 const UserId& user_id);

  WindowServer* GetWindowServer();

  void SetWindowTree(WindowTree* window_tree);

  WindowManagerWindowTreeFactorySet* window_manager_window_tree_factory_set_;
  const UserId user_id_;
  mojo::Binding<mojom::WindowManagerWindowTreeFactory> binding_;

  // Owned by WindowServer.
  WindowTree* window_tree_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerWindowTreeFactory);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_WINDOW_MANAGER_WINDOW_TREE_FACTORY_H_
