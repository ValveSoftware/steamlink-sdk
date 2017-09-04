// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/session/client_network_components.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "blimp/net/ssl_client_transport.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace client {

ClientNetworkComponents::ClientNetworkComponents(
    std::unique_ptr<NetworkEventObserver> network_observer)
    : connection_handler_(), network_observer_(std::move(network_observer)) {}

ClientNetworkComponents::~ClientNetworkComponents() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
}

void ClientNetworkComponents::Initialize() {
  io_thread_checker_.DetachFromThread();
  DCHECK(!connection_manager_);
  connection_manager_ = base::MakeUnique<ClientConnectionManager>(this);
}

void ClientNetworkComponents::ConnectWithAssignment(
    const Assignment& assignment) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(connection_manager_);

  connection_manager_->set_client_auth_token(assignment.client_auth_token);
  const char* transport_type = "UNKNOWN";
  switch (assignment.transport_protocol) {
    case Assignment::SSL:
      DCHECK(assignment.cert);
      connection_manager_->AddTransport(base::MakeUnique<SSLClientTransport>(
          assignment.engine_endpoint, std::move(assignment.cert), nullptr));
      transport_type = "SSL";
      break;
    case Assignment::TCP:
      connection_manager_->AddTransport(base::MakeUnique<TCPClientTransport>(
          assignment.engine_endpoint, nullptr));
      transport_type = "TCP";
      break;
    case Assignment::GRPC:
      // TODO(perumaal): Unimplemented as yet.
      // connection_manager_->AddTransport(
      // base::MakeUnique<GrpcClientTransport>(endpoint));
      transport_type = "GRPC";
      NOTIMPLEMENTED();
      break;
    case Assignment::UNKNOWN:
      LOG(FATAL) << "Unknown transport type.";
      break;
  }

  VLOG(1) << "Connecting to " << assignment.engine_endpoint.ToString() << " ("
          << transport_type << ")";

  connection_manager_->Connect();
}

BrowserConnectionHandler*
ClientNetworkComponents::GetBrowserConnectionHandler() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  return &connection_handler_;
}

void ClientNetworkComponents::HandleConnection(
    std::unique_ptr<BlimpConnection> connection) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  VLOG(1) << "Connection established.";
  connection->AddConnectionErrorObserver(this);
  connection_handler_.HandleConnection(std::move(connection));
  network_observer_->OnConnected();
}

void ClientNetworkComponents::OnConnectionError(int result) {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  if (result >= 0) {
    VLOG(1) << "Disconnected with reason: " << result;
  } else {
    VLOG(1) << "Connection error: " << net::ErrorToString(result);
  }
  network_observer_->OnDisconnected(result);
}

}  // namespace client
}  // namespace blimp
