// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_SHELL_CONNECTION_H_
#define SERVICES_SHELL_PUBLIC_CPP_SHELL_CONNECTION_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/core.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"

namespace shell {

class Connector;

// Encapsulates a connection to the Mojo Shell in two parts:
// - a bound InterfacePtr to mojom::Shell, the primary mechanism
//   by which the instantiating application interacts with other services
//   brokered by the Mojo Shell.
// - a bound InterfaceRequest of mojom::ShellClient, an interface
//   used by the Mojo Shell to inform this application of lifecycle events and
//   inbound connections brokered by it.
//
// This class should be used in two scenarios:
// - During early startup to bind the mojom::ShellClientRequest obtained from
//   the Mojo Shell, typically in response to either MojoMain() or main().
// - In an implementation of mojom::ShellClientFactory to bind the
//   mojom::ShellClientRequest passed via StartApplication. In this scenario
//   there can be many instances of this class per process.
//
// Instances of this class are constructed with an implementation of the Shell
// Client Lib's ShellClient interface. See documentation in shell_client.h
// for details.
//
class ShellConnection : public mojom::ShellClient {
 public:
  // Creates a new ShellConnection bound to |request|. This connection may be
  // used immediately to make outgoing connections via connector().  Does not
  // take ownership of |client|, which must remain valid for the lifetime of
  // ShellConnection.
  ShellConnection(shell::ShellClient* client,
                  mojom::ShellClientRequest request);

  ~ShellConnection() override;

  Connector* connector() { return connector_.get(); }
  const Identity& identity() { return identity_; }

  // TODO(rockot): Remove this. http://crbug.com/594852.
  void set_initialize_handler(const base::Closure& callback);

  // TODO(rockot): Remove this once we get rid of app tests.
  void SetAppTestConnectorForTesting(mojom::ConnectorPtr connector);

  // Specify a function to be called when the connection to the shell is lost.
  // Note that if connection has already been lost, then |closure| is called
  // immediately.
  void SetConnectionLostClosure(const base::Closure& closure);

 private:
  // mojom::ShellClient:
  void Initialize(mojom::IdentityPtr identity,
                  uint32_t id,
                  const InitializeCallback& callback) override;
  void AcceptConnection(mojom::IdentityPtr source,
                        uint32_t source_id,
                        mojom::InterfaceProviderRequest remote_interfaces,
                        mojom::InterfaceProviderPtr local_interfaces,
                        mojom::CapabilityRequestPtr allowed_capabilities,
                        const mojo::String& name) override;

  void OnConnectionError();

  // A callback called when Initialize() is run.
  base::Closure initialize_handler_;

  // We track the lifetime of incoming connection registries as it more
  // convenient for the client.
  ScopedVector<Connection> incoming_connections_;

  // A pending Connector request which will eventually be passed to the shell.
  mojom::ConnectorRequest pending_connector_request_;

  shell::ShellClient* client_;
  mojo::Binding<mojom::ShellClient> binding_;
  std::unique_ptr<Connector> connector_;
  shell::Identity identity_;
  bool should_run_connection_lost_closure_ = false;

  base::Closure connection_lost_closure_;

  DISALLOW_COPY_AND_ASSIGN(ShellConnection);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_SHELL_CONNECTION_H_
