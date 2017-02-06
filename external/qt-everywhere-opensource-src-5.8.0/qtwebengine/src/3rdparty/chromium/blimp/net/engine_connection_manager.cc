// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/engine_connection_manager.h"

#include "base/logging.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_transport.h"
#include "net/base/net_errors.h"

namespace blimp {

EngineConnectionManager::EngineConnectionManager(
    ConnectionHandler* connection_handler)
    : connection_handler_(connection_handler) {
  DCHECK(connection_handler_);
}

EngineConnectionManager::~EngineConnectionManager() {}

void EngineConnectionManager::AddTransport(
    std::unique_ptr<BlimpTransport> transport) {
  BlimpTransport* transport_ptr = transport.get();
  transports_.push_back(std::move(transport));
  Connect(transport_ptr);
}

void EngineConnectionManager::Connect(BlimpTransport* transport) {
  transport->Connect(base::Bind(&EngineConnectionManager::OnConnectResult,
                                base::Unretained(this),
                                base::Unretained(transport)));
}

void EngineConnectionManager::OnConnectResult(BlimpTransport* transport,
                                              int result) {
  // Expects engine transport to be reliably, thus |result| is always net::OK.
  CHECK(result == net::OK) << "Transport failure:" << transport->GetName();
  connection_handler_->HandleConnection(transport->TakeConnection());
  Connect(transport);
}

}  // namespace blimp
