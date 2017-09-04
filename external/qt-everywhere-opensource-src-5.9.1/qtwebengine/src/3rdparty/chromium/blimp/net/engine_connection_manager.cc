// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/engine_connection_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_transport.h"
#include "blimp/net/message_port.h"
#include "blimp/net/tcp_engine_transport.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"

namespace blimp {

EngineConnectionManager::EngineConnectionManager(
    ConnectionHandler* connection_handler,
    net::NetLog* net_log)
    : connection_handler_(connection_handler), net_log_(net_log) {
  DCHECK(connection_handler_);
}

EngineConnectionManager::~EngineConnectionManager() {}

void EngineConnectionManager::ConnectTransport(
    net::IPEndPoint* ip_endpoint,
    EngineTransportType transport_type) {
  switch (transport_type) {
    case EngineTransportType::TCP: {
      transport_ = base::MakeUnique<TCPEngineTransport>(*ip_endpoint, net_log_);
      break;
    }

    case EngineTransportType::GRPC: {
      NOTIMPLEMENTED();
      // TODO(perumaal): Unimplemented as yet.
      // transport_ =
      //      base::MakeUnique<GrpcEngineTransport>(ip_endpoint.ToString());
      break;
    }
  }

  Connect();
  transport_->GetLocalAddress(ip_endpoint);
}

void EngineConnectionManager::Connect() {
  transport_->Connect(base::Bind(&EngineConnectionManager::OnConnectResult,
                                 base::Unretained(this)));
}

void EngineConnectionManager::OnConnectResult(int result) {
  CHECK_EQ(net::OK, result) << "Transport failure:" << transport_->GetName();
  connection_handler_->HandleConnection(transport_->MakeConnection());
  Connect();
}

}  // namespace blimp
