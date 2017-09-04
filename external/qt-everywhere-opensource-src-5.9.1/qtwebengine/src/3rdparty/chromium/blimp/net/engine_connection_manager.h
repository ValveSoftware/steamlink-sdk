// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_ENGINE_CONNECTION_MANAGER_H_
#define BLIMP_NET_ENGINE_CONNECTION_MANAGER_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "blimp/net/blimp_net_export.h"
#include "blimp/net/connection_handler.h"
#include "net/base/ip_endpoint.h"
#include "net/log/net_log.h"

namespace blimp {

class BlimpConnection;
class BlimpTransport;
class BlimpEngineTransport;

// Coordinates the channel creation and authentication workflows for
// incoming (Engine) network connections.
//
// TODO(kmarshall): Add rate limiting and abuse handling logic.
class BLIMP_NET_EXPORT EngineConnectionManager {
 public:
  enum class EngineTransportType { TCP, GRPC };

  // Caller is responsible for ensuring that |connection_handler| outlives
  // |this|.
  explicit EngineConnectionManager(ConnectionHandler* connection_handler,
                                   net::NetLog* net_log);

  ~EngineConnectionManager();

  // Adds a transport and accepts new BlimpConnections from it as fast as they
  // arrive. The |ip_endpoint| will receive the actual port-number if the
  // provided one is not available for listening.
  void ConnectTransport(net::IPEndPoint* ip_endpoint,
                        EngineTransportType transport_type);

 private:
  // Invokes transport_->Connect to listen for a connection.
  void Connect();

  // Callback invoked by |transport_| to indicate that it has a connection
  // ready to be authenticated.
  void OnConnectResult(int result);

  ConnectionHandler* connection_handler_;
  net::NetLog* net_log_;
  std::unique_ptr<BlimpEngineTransport> transport_;

  DISALLOW_COPY_AND_ASSIGN(EngineConnectionManager);
};

}  // namespace blimp

#endif  // BLIMP_NET_ENGINE_CONNECTION_MANAGER_H_
