// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/lib/connector_impl.h"

#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/lib/connection_impl.h"

namespace service_manager {

Connector::ConnectParams::ConnectParams(const Identity& target)
    : target_(target) {}

Connector::ConnectParams::ConnectParams(const std::string& name)
    : target_(name, mojom::kInheritUserID) {}

Connector::ConnectParams::~ConnectParams() {}

ConnectorImpl::ConnectorImpl(mojom::ConnectorPtrInfo unbound_state)
    : unbound_state_(std::move(unbound_state)) {
  thread_checker_.DetachFromThread();
}

ConnectorImpl::ConnectorImpl(mojom::ConnectorPtr connector)
    : connector_(std::move(connector)) {
  connector_.set_connection_error_handler(
      base::Bind(&ConnectorImpl::OnConnectionError, base::Unretained(this)));
}

ConnectorImpl::~ConnectorImpl() {}

void ConnectorImpl::OnConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  connector_.reset();
}

std::unique_ptr<Connection> ConnectorImpl::Connect(const std::string& name) {
  ConnectParams params(name);
  return Connect(&params);
}

std::unique_ptr<Connection> ConnectorImpl::Connect(ConnectParams* params) {
  if (!BindIfNecessary())
    return nullptr;

  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(params);

  mojom::InterfaceProviderPtr remote_interfaces;
  mojom::InterfaceProviderRequest remote_request = GetProxy(&remote_interfaces);
  std::unique_ptr<internal::ConnectionImpl> connection(
      new internal::ConnectionImpl(params->target(),
                                   Connection::State::PENDING));
  if (params->remote_interfaces()) {
    params->remote_interfaces()->Bind(std::move(remote_interfaces));
    connection->set_remote_interfaces(params->remote_interfaces());
  } else {
    std::unique_ptr<InterfaceProvider> remote_interface_provider(
        new InterfaceProvider);
    remote_interface_provider->Bind(std::move(remote_interfaces));
    connection->SetRemoteInterfaces(std::move(remote_interface_provider));
  }

  mojom::ServicePtr service;
  mojom::PIDReceiverRequest pid_receiver_request;
  params->TakeClientProcessConnection(&service, &pid_receiver_request);
  mojom::ClientProcessConnectionPtr client_process_connection;
  if (service.is_bound() && pid_receiver_request.is_pending()) {
    client_process_connection = mojom::ClientProcessConnection::New();
    client_process_connection->service =
        service.PassInterface().PassHandle();
    client_process_connection->pid_receiver_request =
        pid_receiver_request.PassMessagePipe();
  } else if (service.is_bound() || pid_receiver_request.is_pending()) {
    NOTREACHED() << "If one of service or pid_receiver_request is valid, "
                 << "both must be valid.";
    return std::move(connection);
  }
  connector_->Connect(params->target(), std::move(remote_request),
                      std::move(client_process_connection),
                      connection->GetConnectCallback());
  return std::move(connection);
}

std::unique_ptr<Connector> ConnectorImpl::Clone() {
  if (!BindIfNecessary())
    return nullptr;

  mojom::ConnectorPtr connector;
  mojom::ConnectorRequest request = GetProxy(&connector);
  connector_->Clone(std::move(request));
  return base::MakeUnique<ConnectorImpl>(connector.PassInterface());
}

void ConnectorImpl::BindRequest(mojom::ConnectorRequest request) {
  if (!BindIfNecessary())
    return;
  connector_->Clone(std::move(request));
}

bool ConnectorImpl::BindIfNecessary() {
  // Bind this object to the current thread the first time it is used to
  // connect.
  if (!connector_.is_bound()) {
    if (!unbound_state_.is_valid()) {
      // It's possible to get here when the link to the service manager has been
      // severed
      // (and so the connector pipe has been closed) but the app has chosen not
      // to quit.
      return false;
    }

    // Bind the ThreadChecker to this thread.
    DCHECK(thread_checker_.CalledOnValidThread());

    connector_.Bind(std::move(unbound_state_));
    connector_.set_connection_error_handler(
        base::Bind(&ConnectorImpl::OnConnectionError, base::Unretained(this)));
  }

  return true;
}

std::unique_ptr<Connector> Connector::Create(mojom::ConnectorRequest* request) {
  mojom::ConnectorPtr proxy;
  *request = mojo::GetProxy(&proxy);
  return base::MakeUnique<ConnectorImpl>(proxy.PassInterface());
}

}  // namespace service_manager
