// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mus/render_widget_window_tree_client_factory.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "content/common/render_widget_window_tree_client_factory.mojom.h"
#include "content/public/common/mojo_shell_connection.h"
#include "content/renderer/mus/render_widget_mus_connection.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "url/gurl.h"

namespace content {

namespace {

// This object's lifetime is managed by MojoShellConnection because it's a
// registered with it.
class RenderWidgetWindowTreeClientFactoryImpl
    : public shell::ShellClient,
      public shell::InterfaceFactory<
          mojom::RenderWidgetWindowTreeClientFactory>,
      public mojom::RenderWidgetWindowTreeClientFactory {
 public:
  RenderWidgetWindowTreeClientFactoryImpl() {
    DCHECK(MojoShellConnection::GetForProcess());
  }

  ~RenderWidgetWindowTreeClientFactoryImpl() override {}

 private:
  // shell::ShellClient implementation:
  bool AcceptConnection(shell::Connection* connection) override {
    connection->AddInterface<mojom::RenderWidgetWindowTreeClientFactory>(this);
    return true;
  }

  // shell::InterfaceFactory<mojom::RenderWidgetWindowTreeClientFactory>:
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<mojom::RenderWidgetWindowTreeClientFactory>
                  request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::RenderWidgetWindowTreeClientFactory implementation.
  void CreateWindowTreeClientForRenderWidget(
      uint32_t routing_id,
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) override {
    RenderWidgetMusConnection* connection =
        RenderWidgetMusConnection::GetOrCreate(routing_id);
    connection->Bind(std::move(request));
  }

  mojo::BindingSet<mojom::RenderWidgetWindowTreeClientFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetWindowTreeClientFactoryImpl);
};

}  // namespace

void CreateRenderWidgetWindowTreeClientFactory() {
  MojoShellConnection::GetForProcess()->AddEmbeddedShellClient(
      base::WrapUnique(new RenderWidgetWindowTreeClientFactoryImpl));
}

}  // namespace content
