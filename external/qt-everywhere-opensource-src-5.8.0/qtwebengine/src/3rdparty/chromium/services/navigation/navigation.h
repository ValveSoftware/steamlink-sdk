// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_NAVIGATION_H_
#define SERVICES_NAVIGATION_NAVIGATION_H_

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/navigation/public/interfaces/view.mojom.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection_ref.h"

namespace content {
class BrowserContext;
}

namespace navigation {

class Navigation : public shell::ShellClient,
                   public shell::InterfaceFactory<mojom::ViewFactory>,
                   public mojom::ViewFactory {
 public:
  Navigation();
  ~Navigation() override;

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t instance_id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // shell::InterfaceFactory<mojom::ViewFactory>:
  void Create(shell::Connection* connection,
              mojom::ViewFactoryRequest request) override;

  // mojom::ViewFactory:
  void CreateView(mojom::ViewClientPtr client,
                  mojom::ViewRequest request) override;

  void ViewFactoryLost();

  shell::Connector* connector_ = nullptr;
  std::string client_user_id_;

  shell::ShellConnectionRefFactory ref_factory_;
  std::set<std::unique_ptr<shell::ShellConnectionRef>> refs_;

  mojo::BindingSet<mojom::ViewFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(Navigation);
};

}  // navigation

#endif  // SERVICES_NAVIGATION_NAVIGATION_H_
