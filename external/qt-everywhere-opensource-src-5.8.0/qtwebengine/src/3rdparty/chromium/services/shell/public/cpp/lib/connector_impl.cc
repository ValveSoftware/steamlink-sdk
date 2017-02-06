// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/lib/connector_impl.h"

#include "base/memory/ptr_util.h"
#include "services/shell/public/cpp/identity.h"
#include "services/shell/public/cpp/lib/connection_impl.h"

namespace shell {

Connector::ConnectParams::ConnectParams(const Identity& target)
    : target_(target) {}

Connector::ConnectParams::ConnectParams(const std::string& name)
    : target_(name, mojom::kInheritUserID) {}

Connector::ConnectParams::~ConnectParams() {}

ConnectorImpl::ConnectorImpl(mojom::ConnectorPtrInfo unbound_state)
    : unbound_state_(std::move(unbound_state)) {}

ConnectorImpl::ConnectorImpl(mojom::ConnectorPtr connector)
    : connector_(std::move(connector)) {
  thread_checker_.reset(new base::ThreadChecker);
  connector_.set_connection_error_handler(
      base::Bind(&ConnectorImpl::OnConnectionError, base::Unretained(this)));
}

ConnectorImpl::~ConnectorImpl() {}

void ConnectorImpl::OnConnectionError() {
  DCHECK(thread_checker_->CalledOnValidThread());
  connector_.reset();
}

std::unique_ptr<Connection> ConnectorImpl::Connect(const std::string& name) {
  ConnectParams params(name);
  return Connect(&params);
}

std::unique_ptr<Connection> ConnectorImpl::Connect(ConnectParams* params) {
  // Bind this object to the current thread the first time it is used to
  // connect.
  if (!connector_.is_bound()) {
    if (!unbound_state_.is_valid()) {
      // It's possible to get here when the link to the shell has been severed
      // (and so the connector pipe has been closed) but the app has chosen not
      // to quit.
      return nullptr;
    }
    connector_.Bind(std::move(unbound_state_));
    connector_.set_connection_error_handler(
        base::Bind(&ConnectorImpl::OnConnectionError, base::Unretained(this)));
    thread_checker_.reset(new base::ThreadChecker);
  }
  DCHECK(thread_checker_->CalledOnValidThread());

  DCHECK(params);
  // We allow all interfaces on outgoing connections since we are presumably in
  // a position to know who we're talking to.
  CapabilityRequest request;
  request.interfaces.insert("*");
  mojom::InterfaceProviderPtr local_interfaces;
  mojom::InterfaceProviderRequest local_request = GetProxy(&local_interfaces);
  mojom::InterfaceProviderPtr remote_interfaces;
  mojom::InterfaceProviderRequest remote_request = GetProxy(&remote_interfaces);
  std::unique_ptr<internal::ConnectionImpl> registry(
      new internal::ConnectionImpl(
          params->target().name(), params->target(), mojom::kInvalidInstanceID,
          request, Connection::State::PENDING));
  if (params->exposed_interfaces()) {
    params->exposed_interfaces()->Bind(std::move(local_request));
    registry->set_exposed_interfaces(params->exposed_interfaces());
  } else {
    std::unique_ptr<InterfaceRegistry> exposed_interfaces(
        new InterfaceRegistry(registry.get()));
    exposed_interfaces->Bind(std::move(local_request));
    registry->SetExposedInterfaces(std::move(exposed_interfaces));
  }
  if (params->remote_interfaces()) {
    params->remote_interfaces()->Bind(std::move(remote_interfaces));
    registry->set_remote_interfaces(params->remote_interfaces());
  } else {
    std::unique_ptr<InterfaceProvider> remote_interface_provider(
        new InterfaceProvider);
    remote_interface_provider->Bind(std::move(remote_interfaces));
    registry->SetRemoteInterfaces(std::move(remote_interface_provider));
  }

  mojom::ShellClientPtr shell_client;
  mojom::PIDReceiverRequest pid_receiver_request;
  params->TakeClientProcessConnection(&shell_client, &pid_receiver_request);
  mojom::ClientProcessConnectionPtr client_process_connection;
  if (shell_client.is_bound() && pid_receiver_request.is_pending()) {
    client_process_connection = mojom::ClientProcessConnection::New();
    client_process_connection->shell_client =
        shell_client.PassInterface().PassHandle();
    client_process_connection->pid_receiver_request =
        pid_receiver_request.PassMessagePipe();
  } else if (shell_client.is_bound() || pid_receiver_request.is_pending()) {
    NOTREACHED() << "If one of shell_client or pid_receiver_request is valid, "
                 << "both must be valid.";
    return std::move(registry);
  }
  connector_->Connect(mojom::Identity::From(params->target()),
                      std::move(remote_request), std::move(local_interfaces),
                      std::move(client_process_connection),
                      registry->GetConnectCallback());
  return std::move(registry);
}

std::unique_ptr<Connector> ConnectorImpl::Clone() {
  mojom::ConnectorPtr connector;
  connector_->Clone(GetProxy(&connector));
  return base::WrapUnique(new ConnectorImpl(connector.PassInterface()));
}

}  // namespace shell
