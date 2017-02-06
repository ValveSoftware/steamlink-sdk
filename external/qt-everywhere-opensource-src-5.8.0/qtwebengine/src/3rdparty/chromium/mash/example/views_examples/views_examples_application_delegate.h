// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_
#define MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/cpp/tracing_impl.h"

namespace views {
class AuraInit;
class WindowManagerConnection;
}

class ViewsExamplesApplicationDelegate
    : public shell::ShellClient,
      public mash::mojom::Launchable,
      public shell::InterfaceFactory<mash::mojom::Launchable> {
 public:
  ViewsExamplesApplicationDelegate();
  ~ViewsExamplesApplicationDelegate() override;

 private:
  // shell::ShellClient:
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(shell::Connection* connection) override;

  // mash::mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override;

  // shell::InterfaceFactory<mash::mojom::Launchable>:
  void Create(shell::Connection* connection,
              mash::mojom::LaunchableRequest request) override;

  mojo::BindingSet<mash::mojom::Launchable> bindings_;

  mojo::TracingImpl tracing_;
  std::unique_ptr<views::AuraInit> aura_init_;
  std::unique_ptr<views::WindowManagerConnection> window_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(ViewsExamplesApplicationDelegate);
};

#endif  // MASH_EXAMPLE_VIEWS_EXAMPLES_APPLICATION_DELEGATE_H_
