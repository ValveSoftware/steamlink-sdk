// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/client_connection_manager.h"

#include "base/logging.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/protocol_version.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_message_processor.h"
#include "blimp/net/blimp_transport.h"
#include "blimp/net/browser_connection_handler.h"
#include "blimp/net/connection_handler.h"
#include "net/base/net_errors.h"

namespace blimp {

ClientConnectionManager::ClientConnectionManager(
    ConnectionHandler* connection_handler)
    : connection_handler_(connection_handler), weak_factory_(this) {
  DCHECK(connection_handler_);
}

ClientConnectionManager::~ClientConnectionManager() {}

void ClientConnectionManager::AddTransport(
    std::unique_ptr<BlimpTransport> transport) {
  DCHECK(transport);
  transports_.push_back(std::move(transport));
}

void ClientConnectionManager::Connect() {
  // A |transport| added first is used first. When it fails to connect,
  // the next transport is used.
  DCHECK(!transports_.empty());
  Connect(0);
}

void ClientConnectionManager::Connect(int transport_index) {
  DVLOG(1) << "ClientConnectionManager::Connect(" << transport_index << ")";
  if (static_cast<size_t>(transport_index) < transports_.size()) {
    transports_[transport_index]->Connect(
        base::Bind(&ClientConnectionManager::OnConnectResult,
                   base::Unretained(this), transport_index));
  } else {
    // TODO(haibinlu): add an error reporting path out for this.
    LOG(WARNING) << "All transports failed to connect";
  }
}

void ClientConnectionManager::OnConnectResult(int transport_index, int result) {
  DCHECK_NE(result, net::ERR_IO_PENDING);
  const auto& transport = transports_[transport_index];
  if (result == net::OK) {
    std::unique_ptr<BlimpConnection> connection = transport->TakeConnection();
    connection->AddConnectionErrorObserver(this);
    SendAuthenticationMessage(std::move(connection));
  } else {
    DVLOG(1) << "Transport " << transport->GetName()
             << " failed to connect:" << net::ErrorToString(result);
    Connect(transport_index + 1);
  }
}

void ClientConnectionManager::SendAuthenticationMessage(
    std::unique_ptr<BlimpConnection> connection) {
  DVLOG(1) << "Sending authentication message.";
  connection->GetOutgoingMessageProcessor()->ProcessMessage(
      CreateStartConnectionMessage(client_token_, kProtocolVersion),
      base::Bind(&ClientConnectionManager::OnAuthenticationMessageSent,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(connection))));
}

void ClientConnectionManager::OnAuthenticationMessageSent(
    std::unique_ptr<BlimpConnection> connection,
    int result) {
  DVLOG(1) << "AuthenticationMessageSent, result=" << result;
  if (result != net::OK) {
    // If a write error occurred, just throw away |connection|.
    // We don't need to propagate the error code here because the connection
    // will already have done so via the ErrorObserver object.
    return;
  }
  connection_handler_->HandleConnection(std::move(connection));
}

void ClientConnectionManager::OnConnectionError(int error) {
  // TODO(kmarshall): implement reconnection logic.
  VLOG(0) << "Connection dropped, error=" << error;
}

}  // namespace blimp
